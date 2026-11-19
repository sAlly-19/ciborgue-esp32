#ifndef CIBORGUE_TOUCH_SENSOR_H
#define CIBORGUE_TOUCH_SENSOR_H

#include <Arduino.h>
#include "config.h"
#include "robot_types.h"
#include "led_controller.h"
#include "display_hal.h"
#include "sfx_synth.h"
#include "personality_manager.h"

extern void notifyUserActivity();
extern bool handlePriorityTapAction();

class TouchHeadSensor {
private:
    LedController& led;
    DisplayHAL& display;
    SfxSynth& sfx;
    PersonalityManager& personality;
    volatile RobotState& currentState;
    FaceEmotion& currentEmotion;

    float baseline = 0.0f;
    bool calibrated = false;
    int consecutiveHits = 0;
    unsigned long lastCheckMillis = 0;
    unsigned long lastPetReactionMillis = 0;
    int phraseIndex = 0;

    const char* PET_PHRASES[4] = {
        "Awn, que carinho gostoso nos meus circuitos!",
        "Hehehe, assim meu processador até esquenta de tanta felicidade!",
        "Adoro quando você faz carinho na minha cabeça!",
        "Carinho detectado! Meus níveis de alegria subiram para cem por cento!"
    };

public:
    TouchHeadSensor(LedController& l, DisplayHAL& d, SfxSynth& s, PersonalityManager& pm,
                    volatile RobotState& st, FaceEmotion& emo)
        : led(l), display(d), sfx(s), personality(pm), currentState(st), currentEmotion(emo) {}

    void begin() {
#if ENABLE_TOUCH_SENSOR
        // Calibra a linha de base capacitiva do GPIO 1 no boot
        uint32_t sum = 0;
        int validSamples = 0;
        for (int i = 0; i < 12; i++) {
            uint32_t v = touchRead(PIN_TOUCH_HEAD);
            if (v > 0) {
                sum += v;
                validSamples++;
            }
            delay(8);
        }
        if (validSamples > 0) {
            baseline = (float)sum / (float)validSamples;
            calibrated = true;
            Serial.printf("EVT:LOG=Sensor Touch de Carinho (GPIO %d) calibrado (baseline=%.0f)\n",
                          PIN_TOUCH_HEAD, baseline);
        }
#endif
    }

    void triggerPetReaction(bool speakOutLoud = true, bool fromPhysicalTouch = false) {
        notifyUserActivity();
        if (fromPhysicalTouch && handlePriorityTapAction()) {
            lastPetReactionMillis = millis();
            return;
        }
        lastPetReactionMillis = millis();
        FaceEmotion emo = personality.onPet();
        currentEmotion = emo;
        display.setEmotion(emo);
        if (emo == EMO_SHAME) {
            led.setCustomColor(255, 110, 170, LED_MODE_PULSE);
            sfx.play(SFX_SHAME);
        } else {
            led.setCustomColor(255, 50, 150, LED_MODE_PULSE);
            sfx.play(SFX_LOVE);
        }

        Serial.printf("EVT:EMOTION=%s\n", emotionToString(emo));
        Serial.println("EVT:PET=1");

        if (speakOutLoud && currentState != STATE_SPEAK && currentState != STATE_THINK) {
            const char* phrase = (emo == EMO_SHAME)
                ? "Ai... assim eu fico até sem graça com tanto carinho!"
                : PET_PHRASES[phraseIndex % 4];
            phraseIndex++;
            Serial.printf("EVT:SAY=%s\n", phrase);
        }
    }

    void resetAfterWake() {
        consecutiveHits = 0;
        lastCheckMillis = millis();
    }

    void update() {
#if ENABLE_TOUCH_SENSOR
        if (!calibrated) return;
        unsigned long now = millis();
        if (now - lastCheckMillis < 40) return; // Amostragem a 25 Hz
        lastCheckMillis = now;

        // Evita disparo dentro do cooldown curto, mas permite toque prioritário durante alarme/widget/desk-clock
        bool canInterrupt = handlePriorityTapActionCheckOnly();
        if (!canInterrupt) {
            if (currentState == STATE_SPEAK || currentState == STATE_THINK) return;
            if (now - lastPetReactionMillis < 7000) return;
        } else if (now - lastPetReactionMillis < 900) {
            return;
        }

        float val = (float)touchRead(PIN_TOUCH_HEAD);
        if (val <= 100.0f) return;

        // No ESP32-S3, o valor capacitivo sobe significativamente quando o dedo toca o pino/pad
        float threshold = max(baseline + 8500.0f, baseline * 1.42f);
        if (val > threshold) {
            consecutiveHits++;
            if (consecutiveHits >= 4) { // 160ms de toque contínuo confirmado
                consecutiveHits = 0;
                Serial.printf("EVT:LOG=Toque físico detectado no GPIO %d (leitura=%.0f, base=%.0f)\n",
                              PIN_TOUCH_HEAD, val, baseline);
                triggerPetReaction(true, true);
            }
        } else {
            consecutiveHits = 0;
            // Adaptação lenta de deriva térmica/umidade
            baseline = 0.995f * baseline + 0.005f * val;
        }
#endif
    }

    bool handlePriorityTapActionCheckOnly() const {
        return display.isDeskClockActive() || display.hasActiveCard() || (currentState == STATE_ALERT);
    }
};

#endif // CIBORGUE_TOUCH_SENSOR_H
