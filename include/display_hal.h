#ifndef CIBORGUE_DISPLAY_HAL_H
#define CIBORGUE_DISPLAY_HAL_H

#include <Arduino.h>
#include <vector>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
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

struct SpringF {
    float val = 0.0f;
    float vel = 0.0f;
    float target = 0.0f;

    void init(float v) {
        val = v;
        target = v;
        vel = 0.0f;
    }

    void step(float stiffness, float damping) {
        float force = (target - val) * stiffness;
        vel = (vel + force) * damping;
        val += vel;
    }
};

static inline float clampf01(float t) {
    return t <= 0.0f ? 0.0f : (t >= 1.0f ? 1.0f : t);
}

static inline float easeOutBackF(float t) {
    float u = clampf01(t);
    const float c1 = 1.35f;
    const float c3 = c1 + 1.0f;
    float m = u - 1.0f;
    return 1.0f + c3 * (m * m * m) + c1 * (m * m);
}

static inline uint16_t packRgb565(float r, float g, float b) {
    uint8_t ir = (uint8_t)constrain((int)(r + 0.5f), 0, 255);
    uint8_t ig = (uint8_t)constrain((int)(g + 0.5f), 0, 255);
    uint8_t ib = (uint8_t)constrain((int)(b + 0.5f), 0, 255);
    return (uint16_t)(((ir & 0xF8) << 8) | ((ig & 0xFC) << 3) | (ib >> 3));
}

enum MouthKey : uint8_t {
    MK_NEUTRAL = 0,
    MK_SPEAK,
    MK_LISTEN,
    MK_THINK,
    MK_HAPPY_OPEN,
    MK_SURPRISED,
    MK_SAD,
    MK_ANGRY,
    MK_WINK,
    MK_SLEEPY,
    MK_FEAR,
    MK_DISGUST,
    MK_CONTEMPT,
    MK_SHAME,
    MK_DESPAIR,
    MK_MALICE
};

/**
 * Motor leve de transições suaves (~280-320ms a 50 FPS) para LovyanGFX no Core 0.
 * Interpola cor RGB565, dimensões, pálpebras contínuas, intensidade de detalhes e morph elástico da boca.
 */
struct FaceMorphEngine {
    SpringF colR, colG, colB;
    SpringF eyeW, leftH, rightH, offX, offY, shakeAmp;
    SpringF lTopL, lTopR, rTopL, rTopR;
    SpringF lBotL, lBotR, rBotL, rBotR;
    SpringF botArc, fearCorner;
    SpringF fearW, shameW, despairW;

    MouthKey prevMouth = MK_NEUTRAL;
    MouthKey currMouth = MK_NEUTRAL;
    float mouthT = 1.0f;

    FaceMorphEngine() {
        colR.init(0.0f); colG.init(190.0f); colB.init(255.0f);
        eyeW.init(68.0f); leftH.init(74.0f); rightH.init(74.0f);
        offX.init(0.0f); offY.init(0.0f); shakeAmp.init(0.0f);
        lTopL.init(0.0f); lTopR.init(0.0f); rTopL.init(0.0f); rTopR.init(0.0f);
        lBotL.init(0.0f); lBotR.init(0.0f); rBotL.init(0.0f); rBotR.init(0.0f);
        botArc.init(0.0f); fearCorner.init(0.0f);
        fearW.init(0.0f); shameW.init(0.0f); despairW.init(0.0f);
    }

    static MouthKey resolveMouthKey(RobotState st, FaceEmotion emo) {
        if (st == STATE_SPEAK)  return MK_SPEAK;
        if (st == STATE_LISTEN) return MK_LISTEN;
        if (st == STATE_THINK)  return MK_THINK;
        switch (emo) {
            case EMO_HAPPY:
            case EMO_LOVE:      return MK_HAPPY_OPEN;
            case EMO_SURPRISED: return MK_SURPRISED;
            case EMO_SAD:       return MK_SAD;
            case EMO_ANGRY:     return MK_ANGRY;
            case EMO_WINK:      return MK_WINK;
            case EMO_SLEEPY:    return MK_SLEEPY;
            case EMO_FEAR:      return MK_FEAR;
            case EMO_DISGUST:   return MK_DISGUST;
            case EMO_CONTEMPT:  return MK_CONTEMPT;
            case EMO_SHAME:     return MK_SHAME;
            case EMO_DESPAIR:   return MK_DESPAIR;
            case EMO_MALICE:    return MK_MALICE;
            default:            return MK_NEUTRAL;
        }
    }

    void step(RobotState st, FaceEmotion emo, unsigned long now, bool blinking) {
        // 1. Alvo de cor RGB888 para fusão suave
        float tr = 0.0f, tg = 190.0f, tb = 255.0f;
        if (st == STATE_LISTEN) {
            tr = 0.0f; tg = 255.0f; tb = 0.0f;
        } else if (st == STATE_THINK) {
            tr = 188.0f + 45.0f * sinf((float)now * 0.0055f);
            tg = 50.0f;
            tb = 255.0f;
        } else if (st == STATE_ALERT) {
            tr = 255.0f;
            tg = 140.0f + 55.0f * sinf((float)now * 0.012f);
            tb = 0.0f;
        } else {
            switch (emo) {
                case EMO_ANGRY:    tr = 255.0f; tg = 0.0f;   tb = 0.0f;   break;
                case EMO_LOVE:     tr = 255.0f; tg = 0.0f;   tb = 255.0f; break;
                case EMO_FEAR:     tr = 148.0f; tg = 212.0f; tb = 255.0f; break;
                case EMO_DISGUST:  tr = 148.0f; tg = 232.0f; tb = 64.0f;  break;
                case EMO_CONTEMPT: tr = 154.0f; tg = 174.0f; tb = 228.0f; break;
                case EMO_SHAME:    tr = 255.0f; tg = 154.0f; tb = 186.0f; break;
                case EMO_DESPAIR:  tr = 114.0f; tg = 114.0f; tb = 255.0f; break;
                case EMO_MALICE:   tr = 194.0f; tg = 70.0f;  tb = 255.0f; break;
                default:           tr = 0.0f;   tg = 190.0f; tb = 255.0f; break;
            }
        }
        colR.target = tr; colG.target = tg; colB.target = tb;
        colR.step(0.14f, 0.78f);
        colG.step(0.14f, 0.78f);
        colB.step(0.14f, 0.78f);

        // 2. Alvos de geometria e pálpebras por emoção
        float tw = 68.0f, tlh = 74.0f, trh = 74.0f;
        float tox = 0.0f, toy = 0.0f, tshk = 0.0f;
        float tl1 = 0.0f, tl2 = 0.0f, tr1 = 0.0f, tr2 = 0.0f;
        float bl1 = 0.0f, bl2 = 0.0f, br1 = 0.0f, br2 = 0.0f;
        float tarc = 0.0f, tfear = 0.0f;
        float tfw = 0.0f, tsw = 0.0f, tdw = 0.0f;

        switch (emo) {
            case EMO_HAPPY:
                tarc = 1.0f;
                break;
            case EMO_SAD:
                tl1 = 0.42f; tl2 = 0.06f; tr1 = 0.06f; tr2 = 0.42f;
                toy = 4.0f;
                break;
            case EMO_ANGRY:
                tl1 = 0.06f; tl2 = 0.42f; tr1 = 0.42f; tr2 = 0.06f;
                tshk = 1.5f;
                break;
            case EMO_SURPRISED:
                tlh = 84.0f; trh = 84.0f;
                break;
            case EMO_WINK:
                trh = 6.0f;
                break;
            case EMO_SLEEPY:
                tlh = 18.0f; trh = 18.0f;
                break;
            case EMO_FEAR:
                tw = 62.0f; tlh = 80.0f; trh = 80.0f;
                tfear = 1.0f; tfw = 1.0f; tshk = 1.0f;
                break;
            case EMO_DISGUST:
                tlh = 36.0f; trh = 50.0f;
                tl1 = 0.14f; tl2 = 0.44f; tr1 = 0.42f; tr2 = 0.16f;
                bl1 = 0.06f; bl2 = 0.36f; br1 = 0.28f; br2 = 0.05f;
                break;
            case EMO_CONTEMPT:
                tox = 6.0f;
                tl1 = 0.47f; tl2 = 0.47f; tr1 = 0.47f; tr2 = 0.25f;
                break;
            case EMO_SHAME:
                tw = 58.0f; tlh = 54.0f; trh = 54.0f;
                tox = -5.0f; toy = 4.0f;
                tl1 = 0.33f; tl2 = 0.04f; tr1 = 0.04f; tr2 = 0.33f;
                tsw = 1.0f;
                break;
            case EMO_DESPAIR:
                tlh = 78.0f; trh = 78.0f;
                tl1 = 0.41f; tl2 = 0.04f; tr1 = 0.04f; tr2 = 0.41f;
                tdw = 1.0f; tshk = 2.0f;
                break;
            case EMO_MALICE:
                tlh = 58.0f; trh = 58.0f;
                tl1 = 0.06f; tl2 = 0.47f; tr1 = 0.47f; tr2 = 0.06f;
                tarc = 0.7917f;
                break;
            default:
                break;
        }

        if (blinking && emo != EMO_SLEEPY) {
            tlh = 6.0f;
            trh = 6.0f;
        }

        const float gk = blinking ? 0.42f : 0.19f;
        const float gd = blinking ? 0.60f : 0.68f;
        eyeW.target = tw; leftH.target = tlh; rightH.target = trh;
        offX.target = tox; offY.target = toy; shakeAmp.target = tshk;
        eyeW.step(0.19f, 0.68f); leftH.step(gk, gd); rightH.step(gk, gd);
        offX.step(0.18f, 0.74f); offY.step(0.18f, 0.74f); shakeAmp.step(0.18f, 0.76f);

        lTopL.target = tl1; lTopR.target = tl2; rTopL.target = tr1; rTopR.target = tr2;
        lBotL.target = bl1; lBotR.target = bl2; rBotL.target = br1; rBotR.target = br2;
        botArc.target = tarc; fearCorner.target = tfear;
        lTopL.step(0.19f, 0.68f); lTopR.step(0.19f, 0.68f);
        rTopL.step(0.19f, 0.68f); rTopR.step(0.19f, 0.68f);
        lBotL.step(0.19f, 0.68f); lBotR.step(0.19f, 0.68f);
        rBotL.step(0.19f, 0.68f); rBotR.step(0.19f, 0.68f);
        botArc.step(0.19f, 0.68f); fearCorner.step(0.19f, 0.68f);

        fearW.target = tfw; shameW.target = tsw; despairW.target = tdw;
        fearW.step(0.20f, 0.78f); shameW.step(0.20f, 0.78f); despairW.step(0.20f, 0.78f);

        // 3. Progresso do morph elástico da boca (~260ms em 50 FPS)
        MouthKey nextMk = resolveMouthKey(st, emo);
        if (nextMk != currMouth) {
            prevMouth = currMouth;
            currMouth = nextMk;
            mouthT = 0.0f;
        }
        if (mouthT < 1.0f) {
            float spd = (currMouth == MK_SPEAK) ? 0.14f : 0.075f;
            mouthT = min(1.0f, mouthT + spd);
        }
    }

    uint16_t packColor() const {
        return packRgb565(colR.val, colG.val, colB.val);
    }
};

class DisplayHAL {
private:
    RobotState currentState = STATE_IDLE;
    FaceEmotion currentEmotion = EMO_NEUTRAL;
    String netBadge = "";
    unsigned long lastFrame = 0;
    unsigned long lastBlink = 0;
    bool isBlinking = false;
    FaceMorphEngine faceMorph;

    SemaphoreHandle_t stateMux = nullptr;
    DisplayWidgetType activeWidget = WIDGET_NONE;
    unsigned long widgetUntilMs = 0;
    unsigned long widgetDurationMs = WIDGET_DEFAULT_DURATION_MS;
    bool deskClockActive = false;
    bool alarmRinging = false;

    // Snapshots para os cards full-screen e pílula topo
    String clockTime = "00:00";
    String clockDate = "--/--/----";
    String clockWeekday = "";

    WeatherCache weatherCache;

    std::vector<String> taskTitles;
    std::vector<bool> taskDone;
    int taskDoneCount = 0;
    int taskTotalCount = 0;

    bool pomoActive = false;
    bool pomoIsFocus = true;
    int pomoRemS = 0;
    int pomoTotalS = 1500;
    String pomoGoal = "Foco";

    bool timerActive = false;
    int timerRemS = 0;
    int timerTotalS = 60;
    String timerLabel = "Timer";

    PetState petState;
    String moodLabel = "Tranquilo";

    String alarmTimeStr = "07:00";
    String alarmLabel = "Alarme";
    String nextAlarmHint = "";

    String pcAction = "";
    String pcParam = "";

    void lockState() const {
        if (stateMux) xSemaphoreTake(stateMux, portMAX_DELAY);
    }

    void unlockState() const {
        if (stateMux) xSemaphoreGive(stateMux);
    }

#if ENABLE_PHYSICAL_DISPLAY
    LGFX_Estardyn_JD9853 tft;
    LGFX_Sprite canvas{&tft};

    void drawCardFrame(uint16_t accent, const char* title, const char* badgeRight, float remainRatio = -1.0f) {
        canvas.fillRoundRect(8, 8, FACE_SCREEN_W - 16, FACE_SCREEN_H - 16, 18, 0x0883);
        canvas.drawRoundRect(8, 8, FACE_SCREEN_W - 16, FACE_SCREEN_H - 16, 18, accent);
        canvas.setTextColor(accent, 0x0883);
        canvas.setTextSize(1);
        canvas.setCursor(20, 20);
        canvas.print(title);
        if (badgeRight && badgeRight[0]) {
            canvas.setTextColor(0x94B2, 0x0883);
            int bw = (int)strlen(badgeRight) * 6;
            canvas.setCursor(FACE_SCREEN_W - 22 - bw, 20);
            canvas.print(badgeRight);
        }
        canvas.drawFastHLine(18, 34, FACE_SCREEN_W - 36, 0x18E3);

        if (remainRatio >= 0.0f) {
            int barW = FACE_SCREEN_W - 56;
            int fillW = max(4, (int)(barW * constrain(remainRatio, 0.0f, 1.0f)));
            canvas.fillRoundRect(28, FACE_SCREEN_H - 17, barW, 3, 1, 0x18E3);
            canvas.fillRoundRect(28, FACE_SCREEN_H - 17, fillW, 3, 1, accent);
        }
    }

    void drawTopPill(unsigned long now, bool hasPomo, bool isFocus, int pRem, int pTot,
                     bool hasTmr, int tRem, int tTot, const String& badge) {
        char txt[36] = {0};
        uint16_t accent = 0x05FF;
        float progress = -1.0f;

        if (hasPomo) {
            int mm = max(0, pRem) / 60;
            int ss = max(0, pRem) % 60;
            accent = isFocus ? 0x3DFD : 0x4EE8;
            snprintf(txt, sizeof(txt), "%s %02d:%02d", isFocus ? "FOCO" : "PAUSA", mm, ss);
            if (pTot > 0) progress = 1.0f - constrain((float)pRem / (float)pTot, 0.0f, 1.0f);
        } else if (hasTmr) {
            int mm = max(0, tRem) / 60;
            int ss = max(0, tRem) % 60;
            accent = 0xFD20;
            snprintf(txt, sizeof(txt), "TIMER %02d:%02d", mm, ss);
            if (tTot > 0) progress = 1.0f - constrain((float)tRem / (float)tTot, 0.0f, 1.0f);
        } else if (badge.length() > 0) {
            if (badge == "AP") {
                accent = 0x05FF;
                snprintf(txt, sizeof(txt), "AP: Ciborgue-Setup");
            } else {
                accent = 0xFD20;
                snprintf(txt, sizeof(txt), "%s", badge.substring(0, 18).c_str());
            }
        } else {
            return;
        }

        int pillW = min(200, max(108, (int)strlen(txt) * 6 + 32));
        int px = (FACE_SCREEN_W - pillW) / 2;
        canvas.fillRoundRect(px, 6, pillW, 20, 10, 0x0883);
        canvas.drawRoundRect(px, 6, pillW, 20, 10, accent);
        canvas.fillCircle(px + 11, 16, 3, accent);
        canvas.setTextColor(0xF7BE, 0x0883);
        canvas.setTextSize(1);
        canvas.setCursor(px + 20, 12);
        canvas.print(txt);

        if (progress >= 0.0f) {
            int trackW = pillW - 22;
            int fw = max(3, (int)(trackW * progress));
            canvas.fillRoundRect(px + 11, 22, trackW, 2, 1, 0x18E3);
            canvas.fillRoundRect(px + 11, 22, fw, 2, 1, accent);
        }
        (void)now;
    }

    void drawCardContent(DisplayWidgetType wtype, unsigned long now, float remainRatio,
                         const String& cTime, const String& cDate, const String& cWd,
                         const WeatherCache& wc, const std::vector<String>& titles,
                         const std::vector<bool>& doneFlags, int dCnt, int tCnt,
                         bool isFocus, int pRem, int pTot, const String& pGoal,
                         int tRem, int tTot, const String& tLbl,
                         const PetState& ps, const String& mLabel,
                         const String& aTime, const String& aLbl, const String& aHint,
                         const String& pcAct, const String& pcPrm) {
        if (wtype == WIDGET_ALARM_RING) {
            bool flash = ((now / 260) % 2) == 0;
            uint16_t acc = flash ? 0xFD20 : 0xF800;
            drawCardFrame(acc, "DESPERTADOR", "ATIVO", -1.0f);
            // Ícone vetorial de sino animado
            int tilt = flash ? -3 : 3;
            canvas.fillCircle(148 + tilt, 72, 16, acc);
            canvas.fillRect(130 + tilt, 72, 36, 12, acc);
            canvas.fillCircle(148 - tilt, 88, 5, 0xFFFF);
            canvas.setTextColor(0xFFFF, 0x0883);
            canvas.setTextSize(4);
            canvas.setCursor(88, 104);
            canvas.print(aTime);
            canvas.setTextSize(2);
            canvas.setTextColor(0xFE60, 0x0883);
            canvas.setCursor(28, 150);
            canvas.print(aLbl.substring(0, 19));
            canvas.setTextSize(1);
            canvas.setTextColor(0x94B2, 0x0883);
            canvas.setCursor(44, 194);
            canvas.print("Toque no GPIO 1 ou BOOT para parar");
            return;
        }

        if (wtype == WIDGET_DESK_CLOCK) {
            struct tm ti;
            char hm[8] = "--:--";
            char ss[6] = "";
            char dt[16] = "--/--/----";
            if (getLocalTime(&ti, 0)) {
                snprintf(hm, sizeof(hm), "%02d:%02d", ti.tm_hour, ti.tm_min);
                snprintf(ss, sizeof(ss), ":%02d", ti.tm_sec);
                snprintf(dt, sizeof(dt), "%02d/%02d/%04d", ti.tm_mday, ti.tm_mon + 1, ti.tm_year + 1900);
            } else {
                strncpy(hm, cTime.c_str(), sizeof(hm) - 1);
                strncpy(dt, cDate.c_str(), sizeof(dt) - 1);
            }
            drawCardFrame(0x3DFD, "RELOGIO DE MESA", "ATIVO", -1.0f);
            canvas.setTextColor(0xFFFF, 0x0883);
            canvas.setTextSize(5);
            canvas.setCursor(42, 62);
            canvas.print(hm);
            if (ss[0]) {
                canvas.setTextSize(2);
                canvas.setTextColor(0x3DFD, 0x0883);
                canvas.setCursor(204, 80);
                canvas.print(ss);
            }
            canvas.setTextSize(1);
            canvas.setTextColor(0xAD55, 0x0883);
            canvas.setCursor(46, 122);
            canvas.print(dt);
            if (wc.valid) {
                char wbuf[36];
                snprintf(wbuf, sizeof(wbuf), "%.1fC | Umid %d%%", wc.temp, wc.hum);
                canvas.fillRoundRect(22, 152, 152, 26, 8, 0x10A4);
                canvas.setTextColor(0xFE60, 0x10A4);
                canvas.setCursor(32, 161);
                canvas.print(wbuf);
            }
            if (aHint.length() > 0) {
                canvas.fillRoundRect(182, 152, 92, 26, 8, 0x10A4);
                canvas.setTextColor(0x4EE8, 0x10A4);
                canvas.setCursor(190, 161);
                canvas.print(String("ALM ") + aHint);
            }
            canvas.setTextColor(0x7BEF, 0x0883);
            canvas.setCursor(54, 198);
            canvas.print("Toque ou diga 'sair do relogio'");
            return;
        }

        if (wtype == WIDGET_CLOCK) {
            drawCardFrame(0x4EE8, "HORARIO OFICIAL", "NTP", remainRatio);
            canvas.setTextColor(0x4EE8, 0x0883);
            canvas.setTextSize(5);
            canvas.setCursor(70, 68);
            canvas.print(cTime);
            canvas.setTextSize(2);
            canvas.setTextColor(0xFFFF, 0x0883);
            canvas.setCursor(46, 130);
            canvas.print(cWd.substring(0, 18));
            canvas.setTextSize(1);
            canvas.setTextColor(0x94B2, 0x0883);
            canvas.setCursor(114, 166);
            canvas.print(cDate);
            return;
        }

        if (wtype == WIDGET_WEATHER) {
            drawCardFrame(0x3DFD, wc.city.substring(0, 18).c_str(), "CLIMA", remainRatio);
            // Sol / nuvem vetorial simples
            canvas.fillCircle(64, 86, 20, 0xFE60);
            if (wc.code >= 2) {
                canvas.fillRoundRect(54, 84, 42, 20, 10, 0xCE79);
                canvas.fillCircle(68, 82, 12, 0xCE79);
            }
            canvas.setTextColor(0xFFFF, 0x0883);
            canvas.setTextSize(4);
            canvas.setCursor(124, 68);
            canvas.printf("%.1fC", wc.temp);
            canvas.setTextSize(1);
            canvas.setTextColor(0x94B2, 0x0883);
            canvas.setCursor(124, 108);
            canvas.print(wc.desc.substring(0, 24));
            canvas.fillRoundRect(22, 146, 120, 40, 8, 0x10A4);
            canvas.setTextColor(0xAD55, 0x10A4);
            canvas.setCursor(32, 154);
            canvas.print("SENSACAO");
            canvas.setTextSize(2);
            canvas.setTextColor(0xFFFF, 0x10A4);
            canvas.setCursor(32, 166);
            canvas.printf("%.1fC", wc.feels);
            canvas.fillRoundRect(154, 146, 120, 40, 8, 0x10A4);
            canvas.setTextSize(1);
            canvas.setTextColor(0xAD55, 0x10A4);
            canvas.setCursor(164, 154);
            canvas.print("UMIDADE");
            canvas.setTextSize(2);
            canvas.setTextColor(0x3DFD, 0x10A4);
            canvas.setCursor(164, 166);
            canvas.printf("%d%%", wc.hum);
            return;
        }

        if (wtype == WIDGET_TASKS) {
            char cntBuf[16];
            snprintf(cntBuf, sizeof(cntBuf), "%d/%d", dCnt, tCnt);
            drawCardFrame(0x3DFD, "TAREFAS NVS", cntBuf, remainRatio);
            if (titles.empty()) {
                canvas.drawCircle(148, 104, 20, 0x3DFD);
                canvas.drawLine(138, 104, 145, 112, 0x3DFD);
                canvas.drawLine(145, 112, 159, 96, 0x3DFD);
                canvas.setTextColor(0x94B2, 0x0883);
                canvas.setTextSize(1);
                canvas.setCursor(80, 142);
                canvas.print("Nenhuma tarefa pendente");
                return;
            }
            int y = 46;
            int shown = min((int)titles.size(), 5);
            for (int i = 0; i < shown; i++) {
                bool dn = (i < (int)doneFlags.size()) ? doneFlags[i] : false;
                canvas.fillRoundRect(20, y, FACE_SCREEN_W - 40, 26, 6, 0x10A4);
                if (dn) {
                    canvas.fillRoundRect(28, y + 6, 14, 14, 4, 0x4EE8);
                    canvas.setTextColor(0x7BEF, 0x10A4);
                } else {
                    canvas.drawRoundRect(28, y + 6, 14, 14, 4, 0x3DFD);
                    canvas.setTextColor(0xFFFF, 0x10A4);
                }
                canvas.setTextSize(1);
                canvas.setCursor(50, y + 9);
                canvas.print(titles[i].substring(0, 34));
                y += 31;
            }
            return;
        }

        if (wtype == WIDGET_POMODORO || wtype == WIDGET_TIMER) {
            bool isPomo = (wtype == WIDGET_POMODORO);
            uint16_t acc = isPomo ? (isFocus ? 0x3DFD : 0x4EE8) : 0xFD20;
            const char* hdr = isPomo ? (isFocus ? "POMODORO FOCO" : "POMODORO PAUSA") : "TIMER ATIVO";
            int rem = isPomo ? pRem : tRem;
            int tot = max(1, isPomo ? pTot : tTot);
            const String& lbl = isPomo ? pGoal : tLbl;
            float prog = 1.0f - constrain((float)rem / (float)tot, 0.0f, 1.0f);

            drawCardFrame(acc, hdr, isPomo ? "ESTUDO" : "REGRESSIVO", remainRatio);
            // Anel circular à esquerda
            canvas.fillArc(76, 122, 34, 42, 0.0f, 360.0f, 0x18E3);
            if (prog > 0.01f) {
                canvas.fillArc(76, 122, 34, 42, 270.0f, 270.0f + (360.0f * prog), acc);
            }
            canvas.setTextColor(0xFFFF, 0x0883);
            canvas.setTextSize(3);
            canvas.setCursor(132, 86);
            canvas.printf("%02d:%02d", max(0, rem) / 60, max(0, rem) % 60);
            canvas.setTextSize(1);
            canvas.setTextColor(0xAD55, 0x0883);
            canvas.setCursor(132, 126);
            canvas.print(lbl.substring(0, 22));
            return;
        }

        if (wtype == WIDGET_MOOD) {
            drawCardFrame(0xF81F, "HUMOR E PERSONALIDADE", mLabel.c_str(), remainRatio);
            auto drawStatBar = [&](int y, const char* name, int val, int maxVal, uint16_t col) {
                canvas.setTextSize(1);
                canvas.setTextColor(0xCE79, 0x0883);
                canvas.setCursor(22, y);
                canvas.print(name);
                canvas.setCursor(FACE_SCREEN_W - 48, y);
                canvas.printf("%d%%", constrain(val, 0, maxVal));
                int bw = FACE_SCREEN_W - 44;
                int fw = max(4, (bw * constrain(val, 0, maxVal)) / maxVal);
                canvas.fillRoundRect(22, y + 12, bw, 10, 5, 0x18E3);
                canvas.fillRoundRect(22, y + 12, fw, 10, 5, col);
            };
            drawStatBar(48,  "AFETO / VINCULO", ps.affection, 100, 0xF81F);
            drawStatBar(88,  "ENERGIA",         ps.energy,    100, 0x4EE8);
            drawStatBar(128, "HUMOR ATUAL",     (int)ps.mood + 50, 100, 0x3DFD);
            canvas.setTextColor(0x94B2, 0x0883);
            canvas.setCursor(22, 178);
            canvas.printf("Carinhos: %lu   Conversas: %lu", (unsigned long)ps.petCount, (unsigned long)ps.chatCount);
            return;
        }

        if (wtype == WIDGET_PC_CONTROL) {
            drawCardFrame(0x4EE8, "CONTROLE PC WINDOWS", "ACAO", remainRatio);
            canvas.drawRoundRect(118, 56, 60, 42, 6, 0x4EE8);
            canvas.drawFastHLine(134, 104, 28, 0x4EE8);
            canvas.setTextColor(0xFFFF, 0x0883);
            canvas.setTextSize(2);
            canvas.setCursor(30, 126);
            canvas.print(pcAct.substring(0, 18));
            canvas.setTextSize(1);
            canvas.setTextColor(0x94B2, 0x0883);
            canvas.setCursor(30, 160);
            canvas.print(pcPrm.substring(0, 36));
            return;
        }
    }

    void drawTopCutQuad(int x, int y, int w, int h, float tl, float tr) {
        int yL = y + (int)(h * clampf01(tl));
        int yR = y + (int)(h * clampf01(tr));
        int x1 = x - 2, y1 = y - 2, x2 = x + w + 2;
        canvas.fillTriangle(x1, y1, x2, y1, x2, yR, TFT_BLACK);
        canvas.fillTriangle(x1, y1, x2, yR, x1, yL, TFT_BLACK);
    }

    void drawBotCutQuad(int x, int y, int w, int h, float bl, float br) {
        int ybL = y + h - (int)(h * clampf01(bl));
        int ybR = y + h - (int)(h * clampf01(br));
        int x1 = x - 2, yb = y + h + 2, x2 = x + w + 2;
        canvas.fillTriangle(x1, yb, x2, yb, x2, ybR, TFT_BLACK);
        canvas.fillTriangle(x1, yb, x2, ybR, x1, ybL, TFT_BLACK);
    }

    void drawSingleEyeMorph(int ex, int centerY, int ew, int eh, bool isLeft,
                            uint16_t eyeColor, int shakeX, unsigned long now) {
        int ey = centerY - eh / 2;
        int rad = min(18, max(2, eh / 2));
        canvas.fillRoundRect(ex, ey, ew, eh, rad, eyeColor);

        if (eh > 16) {
            float tl = isLeft ? faceMorph.lTopL.val : faceMorph.rTopL.val;
            float tr = isLeft ? faceMorph.lTopR.val : faceMorph.rTopR.val;
            if (max(tl, tr) > 0.01f) {
                drawTopCutQuad(ex, ey, ew, eh, tl, tr);
            }

            float fc = clampf01(faceMorph.fearCorner.val);
            if (fc > 0.01f) {
                if (isLeft) {
                    canvas.fillTriangle(
                        ex - 2, ey - 2,
                        ex + (int)((ew * 0.5f) * fc), ey - 2,
                        ex - 2, ey + (int)(24.0f * fc),
                        TFT_BLACK
                    );
                } else {
                    canvas.fillTriangle(
                        ex + ew + 2, ey - 2,
                        ex + ew - (int)((ew * 0.5f) * fc), ey - 2,
                        ex + ew + 2, ey + (int)(24.0f * fc),
                        TFT_BLACK
                    );
                }
            }

            float ba = max(0.0f, min(1.12f, faceMorph.botArc.val));
            if (ba > 0.01f) {
                int arcCy = centerY + (int)(86.0f - 48.0f * ba);
                int arcR = (int)(44.0f - 2.0f * clampf01(1.0f - ba));
                canvas.fillCircle(ex + ew / 2, arcCy, arcR, TFT_BLACK);
            }

            float bl = isLeft ? faceMorph.lBotL.val : faceMorph.rBotL.val;
            float br = isLeft ? faceMorph.lBotR.val : faceMorph.rBotR.val;
            if (max(bl, br) > 0.01f) {
                drawBotCutQuad(ex, ey, ew, eh, bl, br);
            }
        }

        if (eh > 22) {
            float fw = clampf01(faceMorph.fearW.val);
            if (fw > 0.10f) {
                int pr = max(2, (int)(8.0f * fw));
                canvas.fillCircle(ex + ew / 2 + shakeX, centerY, pr, TFT_BLACK);
                if (!isLeft) {
                    int dropY = centerY - 26 + (int)((now / 60) % 6);
                    int dr = max(1, (int)(4.0f * fw));
                    canvas.fillCircle(ex + ew + 8, dropY + 5, dr, 0x5FFF);
                    canvas.fillTriangle(
                        ex + ew + 8, dropY - dr,
                        ex + ew + 8 - dr, dropY + dr,
                        ex + ew + 8 + dr, dropY + dr,
                        0x5FFF
                    );
                }
            }

            float sw = clampf01(faceMorph.shameW.val);
            if (sw > 0.10f) {
                int bx = isLeft ? (ex + 10) : (ex + ew - 10);
                canvas.fillEllipse(bx, centerY + 38, max(2, (int)(16.0f * sw)), max(1, (int)(8.0f * sw)), 0xF995);
            }

            float dw = clampf01(faceMorph.despairW.val);
            if (dw > 0.10f) {
                int rcx = ex + ew / 2;
                int r1 = max(2, (int)(14.0f * dw));
                int r2 = max(3, (int)(22.0f * dw));
                canvas.drawCircle(rcx, centerY + 4, r1, TFT_BLACK);
                canvas.drawCircle(rcx, centerY + 4, r2, TFT_BLACK);
                int tearOff = (int)((now / 35) % 24);
                int tx = isLeft ? (ex + 14) : (ex + ew - 14);
                int tStep = isLeft ? tearOff : ((tearOff + 10) % 24);
                canvas.fillCircle(tx, centerY + 28 + tStep, max(1, (int)(5.0f * dw)), 0x5E5F);
            }
        }
    }

    void drawMouthShapeMorph(MouthKey mk, int mouthX, int mouthY, float scale,
                             uint16_t eyeColor, unsigned long now, int shakeX) {
        if (scale < 0.15f) return;
        auto sx = [scale](float d) -> int { return (int)roundf(d * scale); };
        auto sw = [scale](float d) -> int { return max(1, (int)roundf(d * scale)); };

        switch (mk) {
            case MK_SPEAK: {
                int openW = sw((float)(24 + ((now / 70) % 28)));
                int openH = sw((float)(10 + ((now / 55) % 22)));
                int rr = max(2, sw(8));
                canvas.fillRoundRect(mouthX - openW / 2, mouthY - openH / 2, openW, openH, rr, eyeColor);
                if (openW > 8 && openH > 8) {
                    canvas.fillRoundRect(mouthX - openW / 2 + 3, mouthY - openH / 2 + 3, openW - 6, openH - 6, max(1, rr - 2), 0x3008);
                    canvas.fillEllipse(mouthX, mouthY + openH / 4, max(2, openW / 3), max(1, openH / 4), 0xF98C);
                }
                break;
            }
            case MK_LISTEN: {
                int ph = sw(10.0f + 4.0f * abs(sinf((float)now * 0.008f)));
                int pw = sw(32.0f);
                canvas.fillRoundRect(mouthX - pw / 2, mouthY - ph / 2, pw, ph, max(2, sw(5)), eyeColor);
                if (pw > 8 && ph > 6) {
                    canvas.fillRoundRect(mouthX - pw / 2 + 3, mouthY - ph / 2 + 3, pw - 6, ph - 6, max(1, sw(3)), 0x3008);
                }
                break;
            }
            case MK_THINK: {
                canvas.drawLine(mouthX + sx(-14), mouthY + sx(2), mouthX + sx(-4), mouthY + sx(-3), eyeColor);
                canvas.drawLine(mouthX + sx(-4), mouthY + sx(-3), mouthX + sx(5), mouthY + sx(4), eyeColor);
                canvas.drawLine(mouthX + sx(5), mouthY + sx(4), mouthX + sx(15), mouthY + sx(-1), eyeColor);
                canvas.drawLine(mouthX + sx(-14), mouthY + sx(3), mouthX + sx(-4), mouthY + sx(-2), eyeColor);
                break;
            }
            case MK_HAPPY_OPEN: {
                int r = sw(22);
                canvas.fillCircle(mouthX, mouthY + sx(-4), r, eyeColor);
                canvas.fillRect(mouthX - r - 2, mouthY + sx(-28), (r + 2) * 2, max(2, sw(24)), TFT_BLACK);
                canvas.fillEllipse(mouthX, mouthY + sx(8), sw(12), sw(6), 0xF98C);
                break;
            }
            case MK_SURPRISED: {
                canvas.fillCircle(mouthX, mouthY, sw(14), eyeColor);
                int ir = (int)roundf(9.0f * scale);
                if (ir > 1) canvas.fillCircle(mouthX, mouthY, ir, TFT_BLACK);
                break;
            }
            case MK_SAD: {
                canvas.drawLine(mouthX + sx(-18), mouthY + sx(7), mouthX, mouthY + sx(-4), eyeColor);
                canvas.drawLine(mouthX, mouthY + sx(-4), mouthX + sx(18), mouthY + sx(7), eyeColor);
                canvas.drawLine(mouthX + sx(-18), mouthY + sx(8), mouthX, mouthY + sx(-3), eyeColor);
                canvas.drawLine(mouthX, mouthY + sx(-3), mouthX + sx(18), mouthY + sx(8), eyeColor);
                break;
            }
            case MK_ANGRY: {
                canvas.drawLine(mouthX + sx(-20), mouthY + sx(4), mouthX + sx(-7), mouthY + sx(-2), eyeColor);
                canvas.drawLine(mouthX + sx(-7), mouthY + sx(-2), mouthX + sx(7), mouthY + sx(4), eyeColor);
                canvas.drawLine(mouthX + sx(7), mouthY + sx(4), mouthX + sx(20), mouthY + sx(-2), eyeColor);
                break;
            }
            case MK_FEAR: {
                int fx = mouthX + shakeX;
                int fw = sw(36), fh = sw(14);
                canvas.fillRoundRect(fx - fw / 2, mouthY - fh / 2, fw, fh, max(2, sw(6)), eyeColor);
                if (fw > 8 && fh > 6) {
                    canvas.fillRoundRect(fx - fw / 2 + 3, mouthY - fh / 2 + 3, fw - 6, fh - 6, max(1, sw(4)), 0x3008);
                    canvas.drawFastHLine(fx - sw(13), mouthY, sw(26), 0xFFFF);
                    canvas.drawFastVLine(fx + sx(-4), mouthY - sw(3), sw(6), 0xFFFF);
                    canvas.drawFastVLine(fx + sx(5), mouthY - sw(3), sw(6), 0xFFFF);
                }
                break;
            }
            case MK_DISGUST: {
                canvas.drawLine(mouthX + sx(-18), mouthY + sx(2), mouthX + sx(-6), mouthY + sx(-5), eyeColor);
                canvas.drawLine(mouthX + sx(-6), mouthY + sx(-5), mouthX + sx(6), mouthY + sx(4), eyeColor);
                canvas.drawLine(mouthX + sx(6), mouthY + sx(4), mouthX + sx(18), mouthY + sx(-2), eyeColor);
                canvas.drawLine(mouthX + sx(-18), mouthY + sx(3), mouthX + sx(-6), mouthY + sx(-4), eyeColor);
                canvas.fillEllipse(mouthX + sx(10), mouthY + sx(8), sw(6), sw(8), 0xF98C);
                break;
            }
            case MK_CONTEMPT: {
                int mw = sw(22), mh = max(2, sw(4));
                canvas.fillRoundRect(mouthX + sx(-16), mouthY + sx(2), mw, mh, max(1, mh / 2), eyeColor);
                canvas.drawLine(mouthX + sx(5), mouthY + sx(3), mouthX + sx(19), mouthY + sx(-5), eyeColor);
                canvas.drawLine(mouthX + sx(5), mouthY + sx(4), mouthX + sx(19), mouthY + sx(-4), eyeColor);
                break;
            }
            case MK_SHAME: {
                canvas.drawLine(mouthX + sx(-10), mouthY, mouthX + sx(-5), mouthY + sx(4), eyeColor);
                canvas.drawLine(mouthX + sx(-5), mouthY + sx(4), mouthX, mouthY, eyeColor);
                canvas.drawLine(mouthX, mouthY, mouthX + sx(5), mouthY + sx(4), eyeColor);
                canvas.drawLine(mouthX + sx(5), mouthY + sx(4), mouthX + sx(10), mouthY, eyeColor);
                break;
            }
            case MK_DESPAIR: {
                int r1 = sw(20), r2 = sw(16);
                int dx = mouthX + shakeX;
                canvas.fillCircle(dx, mouthY + sx(12), r1, eyeColor);
                if (r2 > 2) canvas.fillCircle(dx, mouthY + sx(12), r2, 0x3008);
                canvas.fillRect(dx - r1 - 2, mouthY + sx(12), (r1 + 2) * 2, r1 + 2, TFT_BLACK);
                canvas.fillEllipse(dx, mouthY + sx(8), sw(9), sw(4), 0xF98C);
                break;
            }
            case MK_MALICE: {
                int r1 = sw(22), r2 = sw(19);
                canvas.fillCircle(mouthX, mouthY + sx(-6), r1, eyeColor);
                canvas.fillRect(mouthX - r1 - 2, mouthY + sx(-28), (r1 + 2) * 2, max(2, sw(24)), TFT_BLACK);
                if (r2 > 2) canvas.fillCircle(mouthX, mouthY + sx(-9), r2, TFT_BLACK);
                canvas.fillTriangle(
                    mouthX + sx(11), mouthY + sx(-1),
                    mouthX + sx(16), mouthY + sx(-1),
                    mouthX + sx(14), mouthY + sx(7),
                    0xFFFF
                );
                break;
            }
            default: {
                int mw = sw(36), mh = max(2, sw(5));
                canvas.fillRoundRect(mouthX - mw / 2, mouthY, mw, mh, max(1, mh / 2), eyeColor);
                break;
            }
        }
    }
#endif

public:
    void begin() {
        if (!stateMux) {
            stateMux = xSemaphoreCreateMutex();
        }
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
    void setSubtitle(const String& text) { (void)text; } // Tela 100% pura: sem legenda escrita na tela
    void setNetBadge(const String& badge) { lockState(); netBadge = badge; unlockState(); }

    void setDeskClock(bool active) {
        lockState();
        deskClockActive = active;
        if (!active && activeWidget == WIDGET_DESK_CLOCK) {
            activeWidget = WIDGET_NONE;
        }
        unlockState();
    }

    bool isDeskClockActive() const {
        lockState();
        bool v = deskClockActive;
        unlockState();
        return v;
    }

    bool hasActiveCard() const {
        lockState();
        bool v = (activeWidget != WIDGET_NONE) || alarmRinging;
        unlockState();
        return v;
    }

    void dismissOverlay() {
        lockState();
        if (!alarmRinging) {
            activeWidget = WIDGET_NONE;
            widgetUntilMs = 0;
        }
        unlockState();
    }

    void setWeatherCache(const WeatherCache& wc) {
        lockState();
        weatherCache = wc;
        unlockState();
    }

    void setNextAlarmHint(const String& hhmm) {
        lockState();
        nextAlarmHint = hhmm;
        unlockState();
    }

    void syncPomodoro(bool active, bool isFocus, int remS, int totalS, const String& goal) {
        lockState();
        pomoActive = active;
        pomoIsFocus = isFocus;
        pomoRemS = remS;
        pomoTotalS = totalS;
        pomoGoal = goal;
        unlockState();
    }

    void syncTimer(bool active, int remS, int totalS, const String& label) {
        lockState();
        timerActive = active;
        timerRemS = remS;
        timerTotalS = totalS;
        timerLabel = label;
        unlockState();
    }

    void syncTasksSummary(const std::vector<String>& titles, const std::vector<bool>& doneFlags, int doneCount, int totalCount) {
        lockState();
        taskTitles = titles;
        taskDone = doneFlags;
        taskDoneCount = doneCount;
        taskTotalCount = totalCount;
        unlockState();
    }

    void syncPersonality(const PetState& ps, const String& label) {
        lockState();
        petState = ps;
        moodLabel = label;
        unlockState();
    }

    void showClockWidget(const String& t, const String& d, const String& wd, unsigned long durMs = WIDGET_DEFAULT_DURATION_MS) {
        lockState();
        clockTime = t; clockDate = d; clockWeekday = wd;
        activeWidget = WIDGET_CLOCK;
        widgetDurationMs = durMs;
        widgetUntilMs = millis() + durMs;
        unlockState();
    }

    void showWeatherWidget(const WeatherCache& wc, unsigned long durMs = WIDGET_DEFAULT_DURATION_MS) {
        lockState();
        weatherCache = wc;
        activeWidget = WIDGET_WEATHER;
        widgetDurationMs = durMs;
        widgetUntilMs = millis() + durMs;
        unlockState();
    }

    void showTasksWidget(unsigned long durMs = WIDGET_DEFAULT_DURATION_MS) {
        lockState();
        activeWidget = WIDGET_TASKS;
        widgetDurationMs = durMs;
        widgetUntilMs = millis() + durMs;
        unlockState();
    }

    void showPomodoroWidget(unsigned long durMs = WIDGET_DEFAULT_DURATION_MS) {
        lockState();
        activeWidget = WIDGET_POMODORO;
        widgetDurationMs = durMs;
        widgetUntilMs = millis() + durMs;
        unlockState();
    }

    void showTimerWidget(unsigned long durMs = WIDGET_DEFAULT_DURATION_MS) {
        lockState();
        activeWidget = WIDGET_TIMER;
        widgetDurationMs = durMs;
        widgetUntilMs = millis() + durMs;
        unlockState();
    }

    void showMoodWidget(unsigned long durMs = WIDGET_DEFAULT_DURATION_MS) {
        lockState();
        activeWidget = WIDGET_MOOD;
        widgetDurationMs = durMs;
        widgetUntilMs = millis() + durMs;
        unlockState();
    }

    void showAlarmRingWidget(const String& hhmm, const String& label) {
        lockState();
        alarmRinging = true;
        alarmTimeStr = hhmm;
        alarmLabel = label;
        activeWidget = WIDGET_ALARM_RING;
        widgetUntilMs = 0;
        unlockState();
    }

    void clearAlarmRingWidget() {
        lockState();
        alarmRinging = false;
        if (activeWidget == WIDGET_ALARM_RING) {
            activeWidget = WIDGET_NONE;
            widgetUntilMs = 0;
        }
        unlockState();
    }

    void showPcWidget(const String& act, const String& param, unsigned long durMs = 5000UL) {
        lockState();
        pcAction = act; pcParam = param;
        activeWidget = WIDGET_PC_CONTROL;
        widgetDurationMs = durMs;
        widgetUntilMs = millis() + durMs;
        unlockState();
    }

    void setBrightness(uint8_t level) {
#if ENABLE_PHYSICAL_DISPLAY
        tft.setBrightness(level);
#else
        (void)level;
#endif
    }

    void update() {
        unsigned long now = millis();
        if (now - lastFrame < 20) return;
        lastFrame = now;

        if (!isBlinking && (now - lastBlink > 3800)) {
            isBlinking = true;
            lastBlink = now;
        } else if (isBlinking && (now - lastBlink > 140)) {
            isBlinking = false;
            lastBlink = now;
        }

        faceMorph.step(currentState, currentEmotion, now, isBlinking);

#if ENABLE_PHYSICAL_DISPLAY
        DisplayWidgetType curW = WIDGET_NONE;
        unsigned long wUntil = 0, wDur = WIDGET_DEFAULT_DURATION_MS;
        bool dClock = false, aRing = false;
        String cTime, cDate, cWd, pGoal, tLbl, mLbl, aTime, aLbl, aHint, pcAct, pcPrm, badge;
        WeatherCache wc;
        std::vector<String> tTitles;
        std::vector<bool> tDone;
        int dCnt = 0, tCnt = 0, pRem = 0, pTot = 1500, tRem = 0, tTot = 60;
        bool hasPomo = false, isFocus = true, hasTmr = false;
        PetState ps;

        lockState();
        if (activeWidget != WIDGET_NONE && widgetUntilMs > 0 && (long)(now - widgetUntilMs) >= 0) {
            activeWidget = WIDGET_NONE;
            widgetUntilMs = 0;
        }
        curW = alarmRinging ? WIDGET_ALARM_RING : activeWidget;
        wUntil = widgetUntilMs; wDur = max(1UL, widgetDurationMs);
        dClock = deskClockActive; aRing = alarmRinging;
        cTime = clockTime; cDate = clockDate; cWd = clockWeekday;
        wc = weatherCache; tTitles = taskTitles; tDone = taskDone;
        dCnt = taskDoneCount; tCnt = taskTotalCount;
        hasPomo = pomoActive; isFocus = pomoIsFocus; pRem = pomoRemS; pTot = pomoTotalS; pGoal = pomoGoal;
        hasTmr = timerActive; tRem = timerRemS; tTot = timerTotalS; tLbl = timerLabel;
        ps = petState; mLbl = moodLabel;
        aTime = alarmTimeStr; aLbl = alarmLabel; aHint = nextAlarmHint;
        pcAct = pcAction; pcPrm = pcParam; badge = netBadge;
        unlockState();

        canvas.fillScreen(TFT_BLACK);

        if (curW != WIDGET_NONE || dClock) {
            DisplayWidgetType effective = (curW != WIDGET_NONE) ? curW : WIDGET_DESK_CLOCK;
            float remainRatio = (wUntil > 0 && wUntil > now && effective != WIDGET_DESK_CLOCK && effective != WIDGET_ALARM_RING)
                                ? (float)(wUntil - now) / (float)wDur : -1.0f;
            drawCardContent(effective, now, remainRatio,
                            cTime, cDate, cWd, wc, tTitles, tDone, dCnt, tCnt,
                            isFocus, pRem, pTot, pGoal, tRem, tTot, tLbl,
                            ps, mLbl, aTime, aLbl, aHint, pcAct, pcPrm);
            canvas.pushSprite(0, 0);
            return;
        }

        uint16_t eyeColor = faceMorph.packColor();
        int eyeW   = max(20, (int)roundf(faceMorph.eyeW.val));
        int leftH  = max(4,  (int)roundf(faceMorph.leftH.val));
        int rightH = max(4,  (int)roundf(faceMorph.rightH.val));

        int shakeAmp = (int)roundf(faceMorph.shakeAmp.val);
        int shakeX = (shakeAmp > 0) ? (((int)((now / 42) % 3) - 1) * shakeAmp) : 0;

        int offX = (int)roundf(faceMorph.offX.val);
        int offY = (int)roundf(faceMorph.offY.val);
        int centerXOffset = (68 - eyeW) / 2;
        int leftX   = 56 + centerXOffset + offX + shakeX;
        int rightX  = 172 + centerXOffset + offX + shakeX;
        int centerY = 96 + offY;

        // 1. Olhos Cozmo/Eilik com Morph Contínuo
        drawSingleEyeMorph(leftX, centerY, eyeW, leftH, true, eyeColor, shakeX, now);
        drawSingleEyeMorph(rightX, centerY, eyeW, rightH, false, eyeColor, shakeX, now);

        // 2. Boca Expressiva Eilik/BMO com Morph Elástico + LipSync (y = 166)
        int mouthX = FACE_SCREEN_W / 2;
        int mouthY = 166;
        if (faceMorph.mouthT >= 0.999f || faceMorph.prevMouth == faceMorph.currMouth) {
            drawMouthShapeMorph(faceMorph.currMouth, mouthX, mouthY, 1.0f, eyeColor, now, shakeX);
        } else if (faceMorph.mouthT < 0.40f) {
            float sc = 1.0f - (faceMorph.mouthT / 0.40f) * 0.72f;
            drawMouthShapeMorph(faceMorph.prevMouth, mouthX, mouthY, sc, eyeColor, now, shakeX);
        } else {
            float u = (faceMorph.mouthT - 0.40f) / 0.60f;
            float sc = max(0.25f, min(1.15f, 0.28f + 0.72f * easeOutBackF(u)));
            drawMouthShapeMorph(faceMorph.currMouth, mouthX, mouthY, sc, eyeColor, now, shakeX);
        }

        // 3. Pílula discreta no topo (apenas se Pomodoro, Timer ou AP/OFFLINE ativos)
        drawTopPill(now, hasPomo, isFocus, pRem, pTot, hasTmr, tRem, tTot, badge);

        canvas.pushSprite(0, 0);
#else
        lockState();
        if (activeWidget != WIDGET_NONE && widgetUntilMs > 0 && (long)(now - widgetUntilMs) >= 0) {
            activeWidget = WIDGET_NONE;
            widgetUntilMs = 0;
        }
        unlockState();
#endif
    }
};

#endif // CIBORGUE_DISPLAY_HAL_H
