#ifndef CIBORGUE_PERSONALITY_MANAGER_H
#define CIBORGUE_PERSONALITY_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <time.h>
#include "config.h"
#include "robot_types.h"
#include "display_hal.h"
#include "sfx_synth.h"

/**
 * Sistema equilibrado de Personalidade, Humor e Vínculo persistido na Flash NVS ("cib_pet").
 * Protege a Flash com rate-limit mínimo de escrita (PET_NVS_MIN_SAVE_MS) entre eventos contínuos.
 */
class PersonalityManager {
private:
    Preferences prefs;
    DisplayHAL& display;
    SfxSynth& sfx;
    volatile RobotState& currentState;
    FaceEmotion& currentEmotion;

    PetState state;
    bool dirty = false;
    unsigned long lastInteractionMs = 0;
    unsigned long lastDecayMs = 0;
    unsigned long lastSaveMs = 0;
    struct Arm { uint16_t trials = 0; uint16_t replies = 0; };
    struct Period {
        uint16_t pets = 0;
        uint16_t chats = 0;
        Arm arms[3];
    };
    Period periods[4];
    bool learningDirty = false;
    uint32_t lastLearningSave = 0;
    uint32_t chirpDue = 0;
    uint32_t rewardDue = 0;
    uint32_t chirpTimes[3] = {};
    uint8_t chirpCount = 0;
    int8_t rewardPeriod = -1;
    uint8_t rewardArm = 1;
    uint8_t selectedArm = 1;
    int8_t selectedPeriod = -1;
    uint32_t reactionIdleAt = 0;
    bool reactionActive = false;
    bool reactionWaiting = false;

    int localPeriod() const {
        const time_t now = time(nullptr);
        if (now < 1704067200) return -1;
        struct tm local;
        return localtime_r(&now, &local) ? local.tm_hour / 6 : -1;
    }

    void scheduleChirp(uint32_t now) {
        static constexpr uint32_t intervals[] = {240000, 480000, 720000};
        selectedPeriod = localPeriod();
        selectedArm = 1;
        if (selectedPeriod >= 0) {
            const auto& slot = periods[selectedPeriod];
            if (esp_random() % 10 == 0) {
                selectedArm = (esp_random() & 1) ? 0 : 2;
            } else {
                // Small prior favors the default. Activity permits shorter
                // intervals to win when their observed response rate is higher.
                const float activity = min(20, (int)slot.pets + (int)slot.chats) / 20.0f;
                auto score = [&](int i) {
                    const auto& arm = slot.arms[i];
                    const float prior = i == 1 ? 1.0f : 0.7f;
                    const float penalty = i == 0 ? 0.12f * (1.0f - activity) : 0.0f;
                    return (arm.replies + prior) / (arm.trials + 3.0f) - penalty;
                };
                for (uint8_t i : {uint8_t(0), uint8_t(2)}) {
                    if (score(i) > score(selectedArm)) selectedArm = i;
                }
            }
        }
        chirpDue = now + intervals[selectedArm];
    }

    void finishReward(bool replied) {
        if (rewardPeriod < 0) return;
        auto& arm = periods[rewardPeriod].arms[rewardArm];
        if (arm.trials >= 4096) {
            arm.trials /= 2;
            arm.replies /= 2;
        }
        arm.trials++;
        if (replied) arm.replies++;
        rewardPeriod = -1;
        learningDirty = true;
    }

    void recordInteraction(bool pet) {
        const uint32_t now = millis();
        if (rewardPeriod >= 0 && int32_t(now - rewardDue) <= 0) finishReward(true);
        const int period = localPeriod();
        if (period < 0) return;
        auto& slot = periods[period];
        if (slot.pets >= 4096 || slot.chats >= 4096) {
            slot.pets /= 2;
            slot.chats /= 2;
        }
        if (pet) slot.pets++;
        else slot.chats++;
        learningDirty = true;
    }

    void syncToDisplay() const {
        display.syncPersonality(state, getMoodLabel());
    }

public:
    PersonalityManager(DisplayHAL& d, SfxSynth& s, volatile RobotState& st, FaceEmotion& emo)
        : display(d), sfx(s), currentState(st), currentEmotion(emo) {}

    void begin() {
        prefs.begin("cib_pet", true);
        state.affection = (int8_t)constrain((int)prefs.getChar("affection", 65), 0, 100);
        state.energy    = (int8_t)constrain((int)prefs.getChar("energy", 85), 0, 100);
        state.mood      = (int8_t)constrain((int)prefs.getChar("mood", 15), -50, 50);
        state.petCount  = prefs.getUInt("pets", 0);
        state.chatCount = prefs.getUInt("chats", 0);
        if (prefs.getBytesLength("learn_v1") == sizeof(periods)) {
            Period saved[4];
            if (prefs.getBytes("learn_v1", saved, sizeof(saved)) == sizeof(saved)) {
                bool valid = true;
                for (const auto& period : saved) {
                    for (const auto& arm : period.arms) valid &= arm.replies <= arm.trials;
                }
                if (valid) memcpy(periods, saved, sizeof(periods));
            }
        }
        prefs.end();

        unsigned long now = millis();
        lastInteractionMs = now;
        lastDecayMs = now;
        lastSaveMs = lastLearningSave = now;
        dirty = false;
        scheduleChirp(now);
        syncToDisplay();
    }

    void saveToNvs(bool force = false) {
        if (!dirty && !learningDirty) return;
        unsigned long now = millis();
        const bool savePersonality = dirty && (force || now - lastSaveMs >= PET_NVS_MIN_SAVE_MS);
        const bool saveLearning = learningDirty && now - lastLearningSave >= PET_NVS_MIN_SAVE_MS;
        if (!savePersonality && !saveLearning) return;

        if (!prefs.begin("cib_pet", false)) return;
        if (savePersonality) {
            prefs.putChar("affection", state.affection);
            prefs.putChar("energy", state.energy);
            prefs.putChar("mood", state.mood);
            prefs.putUInt("pets", state.petCount);
            prefs.putUInt("chats", state.chatCount);
            lastSaveMs = now;
            dirty = false;
        }
        if (saveLearning) {
            learningDirty = prefs.putBytes("learn_v1", periods, sizeof(periods)) != sizeof(periods);
            lastLearningSave = now;
        }
        prefs.end();
    }

    const PetState& getState() const { return state; }

    const char* getMoodLabel() const {
        if (state.mood >= 30 && state.affection >= 75) return "Radiante";
        if (state.mood >= 10)  return "Animado";
        if (state.energy <= 35) return "Sonolento";
        if (state.mood <= -15) return "Carente";
        if (state.mood < 0)    return "Entediado";
        return "Tranquilo";
    }

    FaceEmotion onPet() {
        recordInteraction(true);
        lastInteractionMs = millis();
        state.affection = (int8_t)min(100, (int)state.affection + 6);
        state.mood      = (int8_t)min(50,  (int)state.mood + 8);
        state.energy    = (int8_t)min(100, (int)state.energy + 2);
        state.petCount++;
        dirty = true;
        saveToNvs(false);
        syncToDisplay();
        emitPersonalityPacket();

        if (state.affection >= 85 && (esp_random() % 4) == 0) {
            return EMO_SHAME;
        }
        return state.affection >= 65 ? EMO_LOVE : EMO_HAPPY;
    }

    void onInteraction() {
        recordInteraction(false);
        lastInteractionMs = millis();
        state.affection = (int8_t)min(100, (int)state.affection + 2);
        state.mood      = (int8_t)min(50,  (int)state.mood + 3);
        state.energy    = (int8_t)max(10,  (int)state.energy - 1);
        state.chatCount++;
        dirty = true;
        saveToNvs(false);
        syncToDisplay();
        emitPersonalityPacket();
    }

    void onPomodoroCompleted() {
        lastInteractionMs = millis();
        state.affection = (int8_t)min(100, (int)state.affection + 5);
        state.mood      = (int8_t)min(50,  (int)state.mood + 8);
        dirty = true;
        saveToNvs(true);
        syncToDisplay();
        emitPersonalityPacket();
    }

    void onRested() {
        state.energy = (int8_t)min(100, (int)state.energy + 18);
        state.mood   = (int8_t)min(50,  (int)state.mood + 4);
        dirty = true;
        saveToNvs(false);
        syncToDisplay();
        emitPersonalityPacket();
    }

    String buildContextForGemini() const {
        char buf[160];
        snprintf(buf, sizeof(buf),
                 "\n[HUMOR ATUAL DO CIBORGUE]: Afeto %d/100, Energia %d/100, Humor %d (%s). Reaja de forma coerente.",
                 (int)state.affection, (int)state.energy, (int)state.mood, getMoodLabel());
        return String(buf);
    }

    String getStateJson() const {
        JsonDocument doc;
        doc["affection"]  = state.affection;
        doc["energy"]     = state.energy;
        doc["mood"]       = state.mood;
        doc["label"]      = getMoodLabel();
        doc["pet_count"]  = state.petCount;
        doc["chat_count"] = state.chatCount;
        String out;
        serializeJson(doc, out);
        return out;
    }

    void emitPersonalityPacket() const {
        Serial.printf("EVT:PERSONALITY=%s\n", getStateJson().c_str());
    }

    void showMoodCard() {
        syncToDisplay();
        display.showMoodWidget();
        emitPersonalityPacket();

        JsonDocument card;
        card["type"]       = "MOOD";
        card["affection"]  = state.affection;
        card["energy"]     = state.energy;
        card["mood"]       = state.mood;
        card["label"]      = getMoodLabel();
        card["pet_count"]  = state.petCount;
        card["chat_count"] = state.chatCount;
        String out;
        serializeJson(card, out);
        Serial.printf("EVT:WIDGET=%s\n", out.c_str());
    }

    void trackReaction(FaceEmotion emotion, bool awake) {
        reactionActive = awake && emotion != EMO_NEUTRAL && currentState != STATE_ALERT;
        reactionWaiting = reactionActive;
        reactionIdleAt = millis();
    }

    float reactionGain(bool pet) const {
        const int period = localPeriod();
        if (period < 0) return 0.75f;
        const auto& slot = periods[period];
        const unsigned count = pet ? slot.pets : slot.chats;
        return 0.65f + min(20U, count) * 0.01f;
    }

    void update(PowerStage powerStage, bool blocked) {
        unsigned long now = millis();

        if ((now - lastInteractionMs) >= PET_DECAY_INTERVAL_MS &&
            (now - lastDecayMs) >= PET_DECAY_INTERVAL_MS) {
            lastDecayMs = now;
            state.mood      = (int8_t)max(-30, (int)state.mood - 2);
            state.energy    = (int8_t)max(25,  (int)state.energy - 2);
            state.affection = (int8_t)max(20,  (int)state.affection - 1);
            dirty = true;
            syncToDisplay();
            emitPersonalityPacket();
        }

        if (dirty || learningDirty) saveToNvs();

        const bool awake = powerStage == PWR_AWAKE;
        if (!awake) {
            reactionActive = false;
            rewardPeriod = -1;
            return;
        }
        if (reactionActive) {
            if (currentState != STATE_IDLE) {
                reactionWaiting = true;
            } else {
                if (reactionWaiting) {
                    reactionWaiting = false;
                    reactionIdleAt = now;
                }
                if (!blocked && now - reactionIdleAt >= PET_REACTION_HOLD_MS) {
                    reactionActive = false;
                    if (currentEmotion != EMO_NEUTRAL) {
                        currentEmotion = EMO_NEUTRAL;
                        display.setEmotion(EMO_NEUTRAL);
                        Serial.println("EVT:EMOTION=NEUTRAL");
                    }
                }
            }
        }

        if (blocked || currentState == STATE_ALERT || !sfx.idleChirpEnabled()) {
            rewardPeriod = -1;
            return;
        }
        if (rewardPeriod >= 0 && int32_t(now - rewardDue) >= 0) {
            if (currentState == STATE_IDLE) finishReward(false);
            else rewardPeriod = -1;
        }
        if (currentState != STATE_IDLE || sfx.isBusy() ||
            now - lastInteractionMs < 20000 || int32_t(now - chirpDue) < 0) return;
        if (chirpCount == 3 && now - chirpTimes[0] < 3600000) return;
        if (!sfx.play(SFX_IDLE_CHIRP, 0.55f)) return;
        if (chirpCount == 3) {
            chirpTimes[0] = chirpTimes[1];
            chirpTimes[1] = chirpTimes[2];
            chirpCount = 2;
        }
        chirpTimes[chirpCount++] = now;
        rewardPeriod = localPeriod();
        if (rewardPeriod != selectedPeriod) rewardPeriod = -1;
        rewardArm = selectedArm;
        rewardDue = now + 30000;
        scheduleChirp(now);
    }
};

#endif // CIBORGUE_PERSONALITY_MANAGER_H
