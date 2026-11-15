#ifndef CIBORGUE_DISPLAY_HAL_H
#define CIBORGUE_DISPLAY_HAL_H

#include <Arduino.h>
#include "config.h"
#include "robot_types.h"

#if ENABLE_PHYSICAL_DISPLAY
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// Driver configurado para Estardyn 2.01" TFT LCD 240x296 (Controlador JD9853 SPI 8 Pinos)
class LGFX_Estardyn_JD9853 : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 _panel_instance;
    lgfx::Bus_SPI      _bus_instance;
    lgfx::Light_PWM    _light_instance;

public:
    LGFX_Estardyn_JD9853(void) {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host   = SPI2_HOST;
            cfg.spi_mode   = 0;
            cfg.freq_write = 40000000;
            cfg.freq_read  = 16000000;
            cfg.spi_3wire  = false;
            cfg.use_lock   = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk   = PIN_TFT_SCK;
            cfg.pin_mosi   = PIN_TFT_MOSI;
            cfg.pin_miso   = -1;
            cfg.pin_dc     = PIN_TFT_DC;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs           = PIN_TFT_CS;
            cfg.pin_rst          = PIN_TFT_RES;
            cfg.pin_busy         = -1;
            cfg.panel_width      = TFT_NATIVE_WIDTH;   // 240
            cfg.panel_height     = TFT_NATIVE_HEIGHT;  // 296
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 0;
            cfg.invert           = true;
            cfg.rgb_order        = false;
            _panel_instance.config(cfg);
        }
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl      = PIN_TFT_BLK;
            cfg.invert      = false;
            cfg.freq        = 12000;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }
        setPanel(&_panel_instance);
    }
};
#endif

class DisplayHAL {
private:
    RobotState currentState = STATE_IDLE;
    FaceEmotion currentEmotion = EMO_NEUTRAL;
    String statusSubtitle = "Ciborgue Pronto";
    unsigned long lastFrame = 0;
    unsigned long lastBlink = 0;
    bool isBlinking = false;

#if ENABLE_PHYSICAL_DISPLAY
    LGFX_Estardyn_JD9853 tft;
    LGFX_Sprite canvas(&tft);
#endif

public:
    void begin() {
#if ENABLE_PHYSICAL_DISPLAY
        tft.init();
        tft.setRotation(1); // Paisagem Horizontal: 296x240
        tft.setBrightness(220);
        canvas.setPsram(true);
        canvas.createSprite(FACE_SCREEN_W, FACE_SCREEN_H);
        Serial.println("EVT:LOG=Display Estardyn 2.01\" JD9853 (296x240) inicializado!");
#else
        Serial.println("EVT:LOG=Display em Modo Simulador Virtual 296x240 no PC (ENABLE_PHYSICAL_DISPLAY=0)");
#endif
    }

    void setState(RobotState s) { currentState = s; }
    void setEmotion(FaceEmotion e) { currentEmotion = e; }
    void setSubtitle(const String& text) { statusSubtitle = text; }

    void update() {
        unsigned long now = millis();
        if (now - lastFrame < 33) return;
        lastFrame = now;

        if (!isBlinking && (now - lastBlink > 3800)) {
            isBlinking = true;
            lastBlink = now;
        } else if (isBlinking && (now - lastBlink > 140)) {
            isBlinking = false;
            lastBlink = now;
        }

#if ENABLE_PHYSICAL_DISPLAY
        canvas.fillScreen(TFT_BLACK);

        uint16_t eyeColor = 0x05FF; // Ciano brilhante
        if (currentState == STATE_LISTEN)      eyeColor = 0x07E0;
        else if (currentState == STATE_THINK)  eyeColor = 0xB99F;
        else if (currentState == STATE_ALERT)  eyeColor = 0xFD20;
        else if (currentEmotion == EMO_ANGRY)  eyeColor = 0xF800;
        else if (currentEmotion == EMO_LOVE)   eyeColor = 0xF81F;

        int eyeW = 68;
        int eyeH = isBlinking ? 6 : 74;
        if (currentEmotion == EMO_SLEEPY)    eyeH = 18;
        if (currentEmotion == EMO_SURPRISED) eyeH = 84;

        int leftX  = 56;
        int rightX = 172;
        int centerY = 82;

        // 1. Olhos Cozmo/Eilik
        canvas.fillRoundRect(leftX, centerY - eyeH / 2, eyeW, eyeH, 18, eyeColor);
        int rightH = (currentEmotion == EMO_WINK) ? 6 : eyeH;
        canvas.fillRoundRect(rightX, centerY - rightH / 2, eyeW, rightH, 18, eyeColor);

        if (currentEmotion == EMO_HAPPY && !isBlinking) {
            canvas.fillCircle(leftX + eyeW / 2,  centerY + 38, 44, TFT_BLACK);
            canvas.fillCircle(rightX + eyeW / 2, centerY + 38, 44, TFT_BLACK);
        }

        // 2. Boca Expressiva Eilik/BMO + LipSync (y = 152)
        int mouthX = FACE_SCREEN_W / 2;
        int mouthY = 152;
        if (currentState == STATE_SPEAK) {
            int openW = 24 + ((now / 70) % 28);
            int openH = 10 + ((now / 55) % 22);
            canvas.fillRoundRect(mouthX - openW / 2, mouthY - openH / 2, openW, openH, 8, eyeColor);
            canvas.fillRoundRect(mouthX - openW / 2 + 3, mouthY - openH / 2 + 3, openW - 6, openH - 6, 6, 0x3008);
            canvas.fillEllipse(mouthX, mouthY + openH / 4, openW / 3, openH / 4, 0xF98C); // Língua BMO
        } else if (currentEmotion == EMO_HAPPY || currentEmotion == EMO_LOVE) {
            canvas.fillCircle(mouthX, mouthY - 4, 22, eyeColor);
            canvas.fillRect(mouthX - 24, mouthY - 28, 48, 24, TFT_BLACK);
            canvas.fillEllipse(mouthX, mouthY + 8, 12, 6, 0xF98C);
        } else if (currentEmotion == EMO_SURPRISED) {
            canvas.fillCircle(mouthX, mouthY, 14, eyeColor);
            canvas.fillCircle(mouthX, mouthY, 9, TFT_BLACK);
        } else {
            canvas.fillRoundRect(mouthX - 18, mouthY, 36, 5, 2, eyeColor);
        }

        // 3. Barra inferior discreta
        canvas.drawFastHLine(12, 204, FACE_SCREEN_W - 24, 0x2124);
        canvas.setTextColor(0xAD55, TFT_BLACK);
        canvas.setTextSize(1);
        canvas.setCursor(14, 214);
        canvas.print(statusSubtitle.substring(0, 44));

        canvas.pushSprite(0, 0);
#endif
    }
};

#endif // CIBORGUE_DISPLAY_HAL_H
