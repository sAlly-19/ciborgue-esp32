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

class DisplayHAL {
private:
    RobotState currentState = STATE_IDLE;
    FaceEmotion currentEmotion = EMO_NEUTRAL;
    String netBadge = "";
    unsigned long lastFrame = 0;
    unsigned long lastBlink = 0;
    bool isBlinking = false;

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

        uint16_t eyeColor = 0x05FF; // Ciano brilhante
        if (currentState == STATE_LISTEN)      eyeColor = 0x07E0;
        else if (currentState == STATE_THINK)  eyeColor = 0xB99F;
        else if (currentState == STATE_ALERT)  eyeColor = 0xFD20;
        else if (currentEmotion == EMO_ANGRY)    eyeColor = 0xF800;
        else if (currentEmotion == EMO_LOVE)     eyeColor = 0xF81F;
        else if (currentEmotion == EMO_FEAR)     eyeColor = 0x96BF;
        else if (currentEmotion == EMO_DISGUST)  eyeColor = 0x9748;
        else if (currentEmotion == EMO_CONTEMPT) eyeColor = 0x9D7C;
        else if (currentEmotion == EMO_SHAME)    eyeColor = 0xFCD7;
        else if (currentEmotion == EMO_DESPAIR)  eyeColor = 0x739F;
        else if (currentEmotion == EMO_MALICE)   eyeColor = 0xC23F;

        int eyeW = 68;
        int eyeH = isBlinking ? 6 : 74;
        if (currentEmotion == EMO_SLEEPY)    eyeH = 18;
        if (currentEmotion == EMO_SURPRISED) eyeH = 84;
        if (!isBlinking) {
            if (currentEmotion == EMO_FEAR)         { eyeW = 62; eyeH = 80; }
            else if (currentEmotion == EMO_SHAME)   { eyeW = 58; eyeH = 54; }
            else if (currentEmotion == EMO_DESPAIR) { eyeH = 78; }
            else if (currentEmotion == EMO_MALICE)  { eyeH = 58; }
        }

        int shakeX = 0;
        if (currentEmotion == EMO_FEAR)         shakeX = (int)((now / 45) % 3) - 1;
        else if (currentEmotion == EMO_DESPAIR) shakeX = ((int)((now / 40) % 3) - 1) * 2;

        int leftX  = 56 + shakeX;
        int rightX = 172 + shakeX;
        int centerY = 96;
        if (currentEmotion == EMO_CONTEMPT)     { leftX += 6; rightX += 6; }
        else if (currentEmotion == EMO_SHAME)   { leftX -= 5; rightX -= 5; centerY += 4; }

        // 1. Olhos Cozmo/Eilik
        int leftH = (currentEmotion == EMO_DISGUST && !isBlinking) ? 36 : eyeH;
        canvas.fillRoundRect(leftX, centerY - leftH / 2, eyeW, leftH, 18, eyeColor);
        int rightH = (currentEmotion == EMO_WINK) ? 6 : ((currentEmotion == EMO_DISGUST && !isBlinking) ? 50 : eyeH);
        canvas.fillRoundRect(rightX, centerY - rightH / 2, eyeW, rightH, 18, eyeColor);

        if (currentEmotion == EMO_HAPPY && !isBlinking) {
            canvas.fillCircle(leftX + eyeW / 2,  centerY + 38, 44, TFT_BLACK);
            canvas.fillCircle(rightX + eyeW / 2, centerY + 38, 44, TFT_BLACK);
        } else if (!isBlinking) {
            if (currentEmotion == EMO_FEAR) {
                canvas.fillTriangle(leftX - 2, centerY - leftH / 2 - 2, leftX + eyeW / 2, centerY - leftH / 2 - 2, leftX - 2, centerY - leftH / 2 + 24, TFT_BLACK);
                canvas.fillTriangle(rightX + eyeW + 2, centerY - rightH / 2 - 2, rightX + eyeW / 2, centerY - rightH / 2 - 2, rightX + eyeW + 2, centerY - rightH / 2 + 24, TFT_BLACK);
                canvas.fillCircle(leftX + eyeW / 2 + shakeX, centerY, 8, TFT_BLACK);
                canvas.fillCircle(rightX + eyeW / 2 + shakeX, centerY, 8, TFT_BLACK);
                int dropY = centerY - 26 + (int)((now / 60) % 6);
                canvas.fillCircle(rightX + eyeW + 8, dropY + 5, 4, 0x5FFF);
                canvas.fillTriangle(rightX + eyeW + 8, dropY - 4, rightX + eyeW + 4, dropY + 4, rightX + eyeW + 12, dropY + 4, 0x5FFF);
            } else if (currentEmotion == EMO_DISGUST) {
                canvas.fillTriangle(leftX - 2, centerY - leftH / 2 - 2, leftX + eyeW + 2, centerY - leftH / 2 - 2, leftX + eyeW + 2, centerY - 4, TFT_BLACK);
                canvas.fillTriangle(leftX - 2, centerY + leftH / 2 + 2, leftX + eyeW + 2, centerY + leftH / 2 + 2, leftX + eyeW + 2, centerY + 6, TFT_BLACK);
                canvas.fillTriangle(rightX - 2, centerY - rightH / 2 - 2, rightX + eyeW + 2, centerY - rightH / 2 - 2, rightX - 2, centerY - 6, TFT_BLACK);
                canvas.fillTriangle(rightX - 2, centerY + rightH / 2 + 2, rightX + eyeW + 2, centerY + rightH / 2 + 2, rightX - 2, centerY + 8, TFT_BLACK);
            } else if (currentEmotion == EMO_CONTEMPT) {
                canvas.fillRect(leftX - 2, centerY - eyeH / 2 - 2, eyeW + 4, eyeH / 2 - 2, TFT_BLACK);
                canvas.fillRect(rightX - 2, centerY - eyeH / 2 - 2, eyeW + 4, eyeH / 4, TFT_BLACK);
                canvas.fillTriangle(rightX - 2, centerY - eyeH / 4 - 2, rightX + eyeW + 2, centerY - eyeH / 4 - 2, rightX - 2, centerY - 2, TFT_BLACK);
            } else if (currentEmotion == EMO_SHAME) {
                canvas.fillTriangle(leftX - 2, centerY - eyeH / 2 - 2, leftX + (int)(eyeW * 0.65f), centerY - eyeH / 2 - 2, leftX - 2, centerY - eyeH / 2 + 18, TFT_BLACK);
                canvas.fillTriangle(rightX + eyeW + 2, centerY - eyeH / 2 - 2, rightX + (int)(eyeW * 0.35f), centerY - eyeH / 2 - 2, rightX + eyeW + 2, centerY - eyeH / 2 + 18, TFT_BLACK);
                canvas.fillEllipse(leftX + 10, centerY + 38, 16, 8, 0xF995);
                canvas.fillEllipse(rightX + eyeW - 10, centerY + 38, 16, 8, 0xF995);
            } else if (currentEmotion == EMO_DESPAIR) {
                canvas.fillTriangle(leftX - 2, centerY - eyeH / 2 - 2, leftX + (int)(eyeW * 0.75f), centerY - eyeH / 2 - 2, leftX - 2, centerY - eyeH / 2 + 32, TFT_BLACK);
                canvas.fillTriangle(rightX + eyeW + 2, centerY - eyeH / 2 - 2, rightX + (int)(eyeW * 0.25f), centerY - eyeH / 2 - 2, rightX + eyeW + 2, centerY - eyeH / 2 + 32, TFT_BLACK);
                int lcx = leftX + eyeW / 2, rcx = rightX + eyeW / 2;
                canvas.drawCircle(lcx, centerY + 4, 14, TFT_BLACK);
                canvas.drawCircle(lcx, centerY + 4, 22, TFT_BLACK);
                canvas.drawCircle(rcx, centerY + 4, 14, TFT_BLACK);
                canvas.drawCircle(rcx, centerY + 4, 22, TFT_BLACK);
                int tearOff = (int)((now / 35) % 24);
                canvas.fillCircle(leftX + 14, centerY + 28 + tearOff, 5, 0x5E5F);
                canvas.fillCircle(rightX + eyeW - 14, centerY + 28 + ((tearOff + 10) % 24), 5, 0x5E5F);
            } else if (currentEmotion == EMO_MALICE) {
                canvas.fillTriangle(leftX - 2, centerY - eyeH / 2 - 2, leftX + eyeW + 2, centerY - eyeH / 2 - 2, leftX + eyeW + 2, centerY - 2, TFT_BLACK);
                canvas.fillTriangle(rightX - 2, centerY - eyeH / 2 - 2, rightX + eyeW + 2, centerY - eyeH / 2 - 2, rightX - 2, centerY - 2, TFT_BLACK);
                canvas.fillCircle(leftX + eyeW / 2, centerY + 48, 42, TFT_BLACK);
                canvas.fillCircle(rightX + eyeW / 2, centerY + 48, 42, TFT_BLACK);
            }
        }

        // 2. Boca Expressiva Eilik/BMO + LipSync (y = 166)
        int mouthX = FACE_SCREEN_W / 2;
        int mouthY = 166;
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
        } else if (currentEmotion == EMO_FEAR) {
            int fx = mouthX + shakeX;
            canvas.fillRoundRect(fx - 18, mouthY - 7, 36, 14, 6, eyeColor);
            canvas.fillRoundRect(fx - 15, mouthY - 4, 30, 8, 4, 0x3008);
            canvas.drawFastHLine(fx - 13, mouthY, 26, 0xFFFF);
            canvas.drawFastVLine(fx - 4, mouthY - 3, 6, 0xFFFF);
            canvas.drawFastVLine(fx + 5, mouthY - 3, 6, 0xFFFF);
        } else if (currentEmotion == EMO_DISGUST) {
            canvas.drawLine(mouthX - 18, mouthY + 2, mouthX - 6, mouthY - 5, eyeColor);
            canvas.drawLine(mouthX - 6, mouthY - 5, mouthX + 6, mouthY + 4, eyeColor);
            canvas.drawLine(mouthX + 6, mouthY + 4, mouthX + 18, mouthY - 2, eyeColor);
            canvas.drawLine(mouthX - 18, mouthY + 3, mouthX - 6, mouthY - 4, eyeColor);
            canvas.fillEllipse(mouthX + 10, mouthY + 8, 6, 8, 0xF98C);
        } else if (currentEmotion == EMO_CONTEMPT) {
            canvas.fillRoundRect(mouthX - 16, mouthY + 2, 22, 4, 2, eyeColor);
            canvas.drawLine(mouthX + 5, mouthY + 3, mouthX + 19, mouthY - 5, eyeColor);
            canvas.drawLine(mouthX + 5, mouthY + 4, mouthX + 19, mouthY - 4, eyeColor);
        } else if (currentEmotion == EMO_SHAME) {
            canvas.drawLine(mouthX - 10, mouthY, mouthX - 5, mouthY + 4, eyeColor);
            canvas.drawLine(mouthX - 5, mouthY + 4, mouthX, mouthY, eyeColor);
            canvas.drawLine(mouthX, mouthY, mouthX + 5, mouthY + 4, eyeColor);
            canvas.drawLine(mouthX + 5, mouthY + 4, mouthX + 10, mouthY, eyeColor);
        } else if (currentEmotion == EMO_DESPAIR) {
            canvas.fillCircle(mouthX + shakeX, mouthY + 12, 20, eyeColor);
            canvas.fillCircle(mouthX + shakeX, mouthY + 12, 16, 0x3008);
            canvas.fillRect(mouthX + shakeX - 22, mouthY + 12, 44, 22, TFT_BLACK);
            canvas.fillEllipse(mouthX + shakeX, mouthY + 8, 9, 4, 0xF98C);
        } else if (currentEmotion == EMO_MALICE) {
            canvas.fillCircle(mouthX, mouthY - 6, 22, eyeColor);
            canvas.fillRect(mouthX - 24, mouthY - 28, 48, 24, TFT_BLACK);
            canvas.fillCircle(mouthX, mouthY - 9, 19, TFT_BLACK);
            canvas.fillTriangle(mouthX + 11, mouthY - 1, mouthX + 16, mouthY - 1, mouthX + 14, mouthY + 7, 0xFFFF);
        } else {
            canvas.fillRoundRect(mouthX - 18, mouthY, 36, 5, 2, eyeColor);
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
