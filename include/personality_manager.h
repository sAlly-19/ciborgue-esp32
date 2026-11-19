#ifndef CIBORGUE_PERSONALITY_MANAGER_H
#define CIBORGUE_PERSONALITY_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
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
    unsigned long nextAutonomousAt = 0;

    void scheduleNextAutonomous() {
        unsigned long span = PET_AUTONOMOUS_MAX_MS - PET_AUTONOMOUS_MIN_MS;
        unsigned long offset = (span > 0) ? (unsigned long)(esp_random() % span) : 0;
        nextAutonomousAt = millis() + PET_AUTONOMOUS_MIN_MS + offset;
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
        prefs.end();

        unsigned long now = millis();
        lastInteractionMs = now;
        lastDecayMs = now;
        lastSaveMs = now;
        dirty = false;
        scheduleNextAutonomous();
        syncToDisplay();
    }

    void saveToNvs(bool force = false) {
        if (!dirty) return;
        unsigned long now = millis();
        if (!force && (now - lastSaveMs) < PET_NVS_MIN_SAVE_MS) return;

        prefs.begin("cib_pet", false);
        prefs.putChar("affection", state.affection);
        prefs.putChar("energy", state.energy);
        prefs.putChar("mood", state.mood);
        prefs.putUInt("pets", state.petCount);
        prefs.putUInt("chats", state.chatCount);
        prefs.end();

        lastSaveMs = now;
        dirty = false;
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
        return EMO_LOVE;
    }

    void onInteraction() {
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

    void update(bool blocked) {
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

        if (dirty && (now - lastSaveMs) >= PET_NVS_MIN_SAVE_MS) {
            saveToNvs(false);
        }

        if (blocked || currentState != STATE_IDLE) {
            return;
        }

        if ((long)(now - nextAutonomousAt) >= 0) {
            scheduleNextAutonomous();
            FaceEmotion nextEmo = EMO_NEUTRAL;
            uint32_t r = esp_random() % 3;
            if (state.energy <= 35) {
                nextEmo = (r == 0) ? EMO_SLEEPY : EMO_DESPAIR;
            } else if (state.mood >= 28) {
                if (r == 0) nextEmo = EMO_HAPPY;
                else if (r == 1) nextEmo = EMO_WINK;
                else nextEmo = EMO_MALICE;
            } else if (state.mood <= -12) {
                nextEmo = (r == 0) ? EMO_SAD : EMO_CONTEMPT;
            } else {
                nextEmo = (r == 0) ? EMO_WINK : ((r == 1) ? EMO_HAPPY : EMO_NEUTRAL);
            }

            if (nextEmo != currentEmotion) {
                currentEmotion = nextEmo;
                display.setEmotion(nextEmo);
                Serial.printf("EVT:EMOTION=%s\n", emotionToString(nextEmo));
                if (nextEmo == EMO_MALICE) sfx.play(SFX_MALICE, 0.45f);
                else if (nextEmo == EMO_WINK) sfx.play(SFX_WINK, 0.45f);
            }
        }
    }
};

#endif // CIBORGUE_PERSONALITY_MANAGER_H
