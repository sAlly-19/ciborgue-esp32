#ifndef CIBORGUE_LED_CONTROLLER_H
#define CIBORGUE_LED_CONTROLLER_H

#include <Arduino.h>
#include "config.h"
#include "robot_types.h"

class LedController {
private:
    uint8_t customR = 0, customG = 120, customB = 255;
    uint8_t currentR = 0, currentG = 0, currentB = 0;
    LedEffectMode effectMode = LED_MODE_STATE;
    unsigned long lastUpdate = 0;
    int animStep = 10;
    bool animUp = true;
    uint16_t rainbowHue = 0;
    volatile float brightnessScale = 1.0f;

    void writeHardware(uint8_t r, uint8_t g, uint8_t b) {
        if (r == 0 && g == 0 && b == 0) {
            currentR = 0;
            currentG = 0;
            currentB = 0;
            neopixelWrite(PIN_RGB_LED, 0, 0, 0);
            return;
        }
        float scale = brightnessScale;
        uint8_t sr = (uint8_t)constrain((int)(r * scale), 0, 255);
        uint8_t sg = (uint8_t)constrain((int)(g * scale), 0, 255);
        uint8_t sb = (uint8_t)constrain((int)(b * scale), 0, 255);
        currentR = sr;
        currentG = sg;
        currentB = sb;
        neopixelWrite(PIN_RGB_LED, sr, sg, sb);
    }

    void hsvToRgb(uint16_t h, uint8_t s, uint8_t v, uint8_t &r, uint8_t &g, uint8_t &b) {
        uint8_t region = h / 43;
        uint8_t remainder = (h - (region * 43)) * 6;
        uint8_t p = (v * (255 - s)) >> 8;
        uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
        uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

        switch (region) {
            case 0:  r = v; g = t; b = p; break;
            case 1:  r = q; g = v; b = p; break;
            case 2:  r = p; g = v; b = t; break;
            case 3:  r = p; g = q; b = v; break;
            case 4:  r = t; g = p; b = v; break;
            default: r = v; g = p; b = q; break;
        }
    }

public:
    void begin() {
        // Pisca rápido de inicialização
        writeHardware(40, 0, 0); delay(120);
        writeHardware(0, 40, 0); delay(120);
        writeHardware(0, 0, 40); delay(120);
        writeHardware(0, 0, 0);
    }

    void setStateMode() {
        effectMode = LED_MODE_STATE;
    }

    void setBrightnessScale(float scale) {
        if (scale < 0.0f) scale = 0.0f;
        if (scale > 1.0f) scale = 1.0f;
        brightnessScale = scale;
    }

    void turnOff() {
        writeHardware(0, 0, 0);
    }

    void setCustomColor(uint8_t r, uint8_t g, uint8_t b, LedEffectMode mode = LED_MODE_SOLID) {
        customR = r;
        customG = g;
        customB = b;
        effectMode = mode;
        if (mode == LED_MODE_SOLID) {
            writeHardware(r, g, b);
        }
        const char* modeName = (mode == LED_MODE_RAINBOW) ? "RAINBOW" :
                               (mode == LED_MODE_PULSE)   ? "PULSE" :
                               (mode == LED_MODE_ALERT)   ? "ALERT" : "SOLID";
        Serial.printf("EVT:LED=%d,%d,%d,%s\n", r, g, b, modeName);
    }

    String setNamedColor(String colorName, String effectName = "solido") {
        colorName.toLowerCase();
        colorName.trim();
        effectName.toLowerCase();
        effectName.trim();

        uint8_t r = 0, g = 120, b = 255;
        if (colorName.indexOf("vermelh") >= 0)      { r = 255; g = 0;   b = 0;   }
        else if (colorName.indexOf("verde") >= 0)   { r = 0;   g = 255; b = 20;  }
        else if (colorName.indexOf("azul") >= 0)    { r = 0;   g = 80;  b = 255; }
        else if (colorName.indexOf("amarel") >= 0)  { r = 255; g = 200; b = 0;   }
        else if (colorName.indexOf("rox") >= 0 || colorName.indexOf("violet") >= 0) { r = 180; g = 0; b = 255; }
        else if (colorName.indexOf("cian") >= 0 || colorName.indexOf("turques") >= 0) { r = 0; g = 255; b = 255; }
        else if (colorName.indexOf("laranj") >= 0)  { r = 255; g = 70;  b = 0;   }
        else if (colorName.indexOf("rosa") >= 0 || colorName.indexOf("pink") >= 0) { r = 255; g = 20; b = 140; }
        else if (colorName.indexOf("branc") >= 0)   { r = 220; g = 220; b = 220; }
        else if (colorName.indexOf("apag") >= 0 || colorName.indexOf("deslig") >= 0 || colorName.indexOf("preto") >= 0) {
            r = 0; g = 0; b = 0;
        }

        LedEffectMode mode = LED_MODE_SOLID;
        if (effectName.indexOf("arco") >= 0 || effectName.indexOf("rainbow") >= 0 || colorName.indexOf("arco") >= 0) {
            mode = LED_MODE_RAINBOW;
        } else if (effectName.indexOf("puls") >= 0 || effectName.indexOf("respir") >= 0) {
            mode = LED_MODE_PULSE;
        } else if (effectName.indexOf("alert") >= 0 || effectName.indexOf("pisc") >= 0) {
            mode = LED_MODE_ALERT;
        }

        setCustomColor(r, g, b, mode);
        return String("LED ajustado para cor ") + colorName + " com efeito " + effectName + ".";
    }

    void update(RobotState state) {
        unsigned long now = millis();
        if (now - lastUpdate < 20) return; // ~50 FPS sem bloquear o processador
        lastUpdate = now;

        // Se estiver em modo customizado (quando o estado é IDLE ou CUSTOM)
        if (effectMode != LED_MODE_STATE && (state == STATE_IDLE || state == STATE_CUSTOM)) {
            if (effectMode == LED_MODE_SOLID) {
                writeHardware(customR, customG, customB);
                return;
            } else if (effectMode == LED_MODE_PULSE) {
                if (animUp) { animStep += 2; if (animStep >= 100) animUp = false; }
                else        { animStep -= 2; if (animStep <= 8)   animUp = true;  }
                float scale = animStep / 100.0f;
                writeHardware((uint8_t)(customR * scale), (uint8_t)(customG * scale), (uint8_t)(customB * scale));
                return;
            } else if (effectMode == LED_MODE_RAINBOW) {
                rainbowHue = (rainbowHue + 2) & 0xFF;
                uint8_t r, g, b;
                hsvToRgb(rainbowHue, 255, 90, r, g, b);
                writeHardware(r, g, b);
                return;
            } else if (effectMode == LED_MODE_ALERT) {
                bool on = ((now / 150) % 2) == 0;
                writeHardware(on ? customR : 0, on ? customG : 0, on ? customB : 0);
                return;
            }
        }

        // Animação padrão por estado da máquina
        switch (state) {
            case STATE_IDLE: {
                if (animUp) { animStep += 1; if (animStep >= 55) animUp = false; }
                else        { animStep -= 1; if (animStep <= 5)  animUp = true;  }
                writeHardware(0, (uint8_t)(animStep / 4), (uint8_t)animStep);
                break;
            }
            case STATE_LISTEN: {
                writeHardware(0, 95, 20);
                break;
            }
            case STATE_THINK: {
                if (animUp) { animStep += 4; if (animStep >= 100) animUp = false; }
                else        { animStep -= 4; if (animStep <= 12)  animUp = true;  }
                writeHardware((uint8_t)animStep, 0, (uint8_t)min(255, (int)(animStep * 1.3)));
                break;
            }
            case STATE_SPEAK: {
                int val = 25 + (rand() % 75);
                writeHardware(0, (uint8_t)val, (uint8_t)min(255, (int)(val * 1.25)));
                break;
            }
            case STATE_ALERT: {
                bool flash = ((now / 160) % 2) == 0;
                writeHardware(flash ? 140 : 15, flash ? 45 : 0, 0);
                break;
            }
            case STATE_PORTAL: {
                // Pulso duplo azul-royal/ciano indicando Portal Cativo Wi-Fi aberto
                unsigned long phase = now % 1200UL;
                bool pulseOn = (phase < 160UL) || (phase >= 280UL && phase < 440UL);
                writeHardware(0, pulseOn ? 110 : 10, pulseOn ? 255 : 40);
                break;
            }
            default:
                break;
        }
    }

    uint8_t getR() const { return currentR; }
    uint8_t getG() const { return currentG; }
    uint8_t getB() const { return currentB; }
};

#endif // CIBORGUE_LED_CONTROLLER_H
