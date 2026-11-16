#ifndef CIBORGUE_TOUCH_SENSOR_H
#define CIBORGUE_TOUCH_SENSOR_H

#include <Arduino.h>
#include "config.h"
#include "robot_types.h"
#include "led_controller.h"
#include "display_hal.h"

class TouchHeadSensor {
private:
    LedController& led;
    DisplayHAL& display;
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
    TouchHeadSensor(LedController& l, DisplayHAL& d, volatile RobotState& st, FaceEmotion& emo)
        : led(l), display(d), currentState(st), currentEmotion(emo) {}

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

    void triggerPetReaction(bool speakOutLoud = true) {
        lastPetReactionMillis = millis();
        currentEmotion = EMO_LOVE;
        display.setEmotion(EMO_LOVE);
        led.setCustomColor(255, 50, 150, LED_MODE_PULSE);

        Serial.println("EVT:EMOTION=LOVE");
        Serial.println("EVT:PET=1");

        if (speakOutLoud && currentState != STATE_SPEAK && currentState != STATE_THINK) {
            const char* phrase = PET_PHRASES[phraseIndex % 4];
            phraseIndex++;
            display.setSubtitle(phrase);
            Serial.printf("EVT:SAY=%s\n", phrase);
        }
    }

    void update() {
#if ENABLE_TOUCH_SENSOR
        if (!calibrated) return;
        unsigned long now = millis();
        if (now - lastCheckMillis < 40) return; // Amostragem a 25 Hz
        lastCheckMillis = now;

        // Evita disparo durante fala ou dentro do cooldown de 7 segundos
        if (currentState == STATE_SPEAK || currentState == STATE_THINK) return;
        if (now - lastPetReactionMillis < 7000) return;

        float val = (float)touchRead(PIN_TOUCH_HEAD);
        if (val <= 100.0f) return;

        // No ESP32-S3, o valor capacitivo sobe significativamente quando o dedo toca o pino/pad
        float threshold = max(baseline + 8500.0f, baseline * 1.42f);
        if (val > threshold) {
            consecutiveHits++;
            if (consecutiveHits >= 4) { // 160ms de toque contínuo confirmado
                consecutiveHits = 0;
                Serial.printf("EVT:LOG=Carinho físico detectado no GPIO %d (leitura=%.0f, base=%.0f)\n",
                              PIN_TOUCH_HEAD, val, baseline);
                triggerPetReaction(true);
            }
        } else {
            consecutiveHits = 0;
            // Adaptação lenta de deriva térmica/umidade
            baseline = 0.995f * baseline + 0.005f * val;
        }
#endif
    }
};

#endif // CIBORGUE_TOUCH_SENSOR_H
