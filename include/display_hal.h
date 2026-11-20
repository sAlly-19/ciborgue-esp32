#ifndef CIBORGUE_DISPLAY_HAL_H
#define CIBORGUE_DISPLAY_HAL_H

#include <Arduino.h>
#include <vector>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config.h"
#include "robot_types.h"
#include "bench_face_assets.h"

#if ENABLE_PHYSICAL_DISPLAY
#include <SPI.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
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
    MK_MALICE,
    MK_CURIOUS,
    MK_BORED,
    MK_SILENT,
    MK_ANGEL,
    MK_COOL,
    MK_CRAZY,
    MK_LIAR
};

/**
 * Motor leve de transições suaves (~280-320ms a 50 FPS) para LovyanGFX no Core 0.
 * Interpola cor RGB565, dimensões, pálpebras contínuas, intensidade de detalhes e morph elástico da boca.
 */
struct FaceMorphEngine {
    SpringF colR, colG, colB;
    SpringF eyeW, leftH, rightH, offX, offY, shakeAmp, tilt;
    SpringF lTopL, lTopR, rTopL, rTopR;
    SpringF lBotL, lBotR, rBotL, rBotR;
    SpringF botArc, fearCorner;
    SpringF fearW, shameW, despairW, blushW, blushIntensity, blushY, sweatW, sweatScale, loveW, maliceW;

    MouthKey prevMouth = MK_NEUTRAL;
    MouthKey currMouth = MK_NEUTRAL;
    float mouthT = 1.0f;
    uint32_t lastMouthStep = 0;

    FaceMorphEngine() {
        colR.init(0.0f); colG.init(238.0f); colB.init(255.0f);
        eyeW.init(76.0f); leftH.init(81.0f); rightH.init(81.0f);
        offX.init(0.0f); offY.init(0.0f); shakeAmp.init(0.0f); tilt.init(0.0f);
        lTopL.init(0.0f); lTopR.init(0.0f); rTopL.init(0.0f); rTopR.init(0.0f);
        lBotL.init(0.0f); lBotR.init(0.0f); rBotL.init(0.0f); rBotR.init(0.0f);
        botArc.init(0.0f); fearCorner.init(0.0f);
        fearW.init(0.0f); shameW.init(0.0f); despairW.init(0.0f); blushW.init(0.0f);
        blushIntensity.init(1.0f); blushY.init(44.0f);
        sweatW.init(0.0f); sweatScale.init(1.0f);
        loveW.init(0.0f); maliceW.init(0.0f);
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
            case EMO_CURIOUS: return MK_CURIOUS;
            case EMO_BORED: return MK_BORED;
            case EMO_SILENT: return MK_SILENT;
            case EMO_ANGEL: return MK_ANGEL;
            case EMO_COOL: return MK_COOL;
            case EMO_CRAZY: return MK_CRAZY;
            case EMO_LIAR: return MK_LIAR;
            default:            return MK_NEUTRAL;
        }
    }

    void step(RobotState st, FaceEmotion emo, unsigned long now) {
        // 1. Alvo de cor RGB888 para fusão suave
        float tr = 0.0f, tg = 190.0f, tb = 255.0f;
        if (st == STATE_LISTEN) {
            tr = 0.0f; tg = 250.0f; tb = 145.0f;
        } else if (st == STATE_THINK) {
            tr = 185.0f + 60.0f * sinf((float)now * 0.0055f);
            tg = 95.0f;
            tb = 255.0f;
        } else if (st == STATE_ALERT) {
            tr = 255.0f;
            tg = ((now / 333) & 1) ? 220.0f : 125.0f;
            tb = 0.0f;
        } else {
            switch (emo) {
                case EMO_ANGRY:    tr = 255.0f; tg = 55.0f;  tb = 75.0f;   break;
                case EMO_LOVE:     tr = 255.0f; tg = 90.0f;  tb = 190.0f; break;
                case EMO_SAD:      tr = 85.0f;  tg = 165.0f; tb = 255.0f; break;
                case EMO_FEAR:     tr = 148.0f; tg = 212.0f; tb = 255.0f; break;
                case EMO_DISGUST:  tr = 148.0f; tg = 232.0f; tb = 64.0f;  break;
                case EMO_CONTEMPT: tr = 154.0f; tg = 174.0f; tb = 228.0f; break;
                case EMO_SHAME:    tr = 255.0f; tg = 154.0f; tb = 186.0f; break;
                case EMO_DESPAIR:  tr = 114.0f; tg = 114.0f; tb = 255.0f; break;
                case EMO_MALICE:   tr = 194.0f; tg = 70.0f;  tb = 255.0f; break;
                case EMO_CURIOUS: tr = 65.0f; tg = 214.0f; tb = 255.0f; break;
                case EMO_BORED: tr = 146.0f; tg = 158.0f; tb = 220.0f; break;
                case EMO_SILENT: tr = 132.0f; tg = 222.0f; tb = 211.0f; break;
                case EMO_ANGEL: tr = 255.0f; tg = 219.0f; tb = 115.0f; break;
                case EMO_COOL: tr = 64.0f; tg = 200.0f; tb = 255.0f; break;
                case EMO_CRAZY: tr = 255.0f; tg = 128.0f; tb = 92.0f; break;
                case EMO_LIAR: tr = 255.0f; tg = 170.0f; tb = 90.0f; break;
                default:           tr = 0.0f;   tg = 238.0f; tb = 255.0f; break;
            }
        }
        colR.target = tr; colG.target = tg; colB.target = tb;
        colR.step(0.14f, 0.78f);
        colG.step(0.14f, 0.78f);
        colB.step(0.14f, 0.78f);

        static_assert(sizeof(bench_face::poses) / sizeof(bench_face::poses[0]) == EMO_LIAR + 1,
                      "Bench poses must match FaceEmotion");
        const auto& pose = bench_face::poses[static_cast<unsigned>(emo)];
        eyeW.target = 76.0f * pose.w_scale;
        leftH.target = 81.2f * pose.lh_scale;
        rightH.target = 81.2f * pose.rh_scale;
        offX.target = 1.16f * pose.gaze_x;
        offY.target = 1.16f * pose.gaze_y;
        if (st == STATE_THINK) {
            offX.target = 13.92f * sinf((now % 1676) * 0.00375f);
            offY.target = -12.76f;
        }
        shakeAmp.target = 1.16f * pose.shake_amp;
        tilt.target = pose.tilt;
        eyeW.step(0.16f, 0.68f); leftH.step(0.16f, 0.68f); rightH.step(0.16f, 0.68f);
        offX.step(0.16f, 0.74f); offY.step(0.16f, 0.74f); shakeAmp.step(0.18f, 0.76f);
        tilt.step(0.14f, 0.76f);

        lTopL.target = pose.l_top_l; lTopR.target = pose.l_top_r;
        rTopL.target = pose.r_top_l; rTopR.target = pose.r_top_r;
        lBotL.target = pose.l_bot_l; lBotR.target = pose.l_bot_r;
        rBotL.target = pose.r_bot_l; rBotR.target = pose.r_bot_r;
        botArc.target = pose.bot_arc; fearCorner.target = pose.fear_corner;
        lTopL.step(0.19f, 0.68f); lTopR.step(0.19f, 0.68f);
        rTopL.step(0.19f, 0.68f); rTopR.step(0.19f, 0.68f);
        lBotL.step(0.19f, 0.68f); lBotR.step(0.19f, 0.68f);
        rBotL.step(0.19f, 0.68f); rBotR.step(0.19f, 0.68f);
        botArc.step(0.19f, 0.68f); fearCorner.step(0.19f, 0.68f);

        blushW.target = pose.blush_alpha;
        blushIntensity.target = pose.blush_intensity;
        blushY.target = pose.blush_y_off;
        sweatW.target = pose.sweat_alpha;
        sweatScale.target = pose.sweat_scale;
        fearW.target = emo == EMO_FEAR;
        shameW.target = emo == EMO_SHAME;
        despairW.target = emo == EMO_DESPAIR;
        loveW.target = emo == EMO_LOVE;
        maliceW.target = emo == EMO_MALICE;
        blushW.step(0.20f, 0.78f); blushIntensity.step(0.20f, 0.78f); blushY.step(0.20f, 0.78f);
        sweatW.step(0.20f, 0.78f); sweatScale.step(0.20f, 0.78f);
        fearW.step(0.20f, 0.78f); shameW.step(0.20f, 0.78f); despairW.step(0.20f, 0.78f);
        loveW.step(0.20f, 0.78f); maliceW.step(0.20f, 0.78f);

        // 3. Progresso do morph elástico da boca (~260ms em 50 FPS)
        MouthKey nextMk = resolveMouthKey(st, emo);
        if (nextMk != currMouth) {
            prevMouth = currMouth;
            currMouth = nextMk;
            mouthT = 0.0f;
        }
        const uint32_t elapsed = now - lastMouthStep;
        lastMouthStep = now;
        if (mouthT < 1.0f) {
            const float duration = currMouth == MK_SPEAK ? 112.0f : 215.0f;
            mouthT = min(1.0f, mouthT + min(elapsed, uint32_t{64}) / duration);
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

    float lipAmp = 0.0f;
    float lipZcr = 0.0f;
    uint32_t lipSampleMs = 0;
    bool lipSampleValid = false;

    void lockState() const {
        if (stateMux) xSemaphoreTake(stateMux, portMAX_DELAY);
    }

    void unlockState() const {
        if (stateMux) xSemaphoreGive(stateMux);
    }

#if ENABLE_PHYSICAL_DISPLAY
    LGFX_Sprite canvas;
    bool spriteReady = false;
    bool canvasNeedsByteSwap = false;
    bool backlightPwmAttached = false;
    bool previousFrameValid = false;
    uint16_t* previousFrame = nullptr;
    uint8_t timedEyeFrames = 0;
    uint32_t eyeTimingStartUs = 0;
    uint32_t eyeTransferUs = 0;
    uint32_t eyeDrawUs = 0;
    float speechWidth = 32.0f;
    float speechHeight = 4.5f;
    enum class FaceParticleKind : uint8_t { None, Heart, Star, Sweat, Tear, Zzz, Spark, Dot, Bubble };
    struct FaceParticle {
        FaceParticleKind kind = FaceParticleKind::None;
        uint32_t born = 0;
        uint16_t life = 0;
        int16_t x = 0, y = 0;
        int8_t vx = 0, vy = 0;
        uint8_t size = 0;
    };
    FaceParticle faceParticles[8];
    FaceEmotion animatedEmotion = EMO_NEUTRAL;
    RobotState animatedState = STATE_IDLE;
    uint32_t expressionStarted = 0, gestureStarted = 0, nextGesture = 0;
    uint32_t motionUpdated = 0, nextParticle = 0;
    float gazeX = 0, gazeY = 0, gazeTilt = 0, gazeSquint = 0;
    float targetGazeX = 0, targetGazeY = 0, targetTilt = 0, targetSquint = 0;
    SemaphoreHandle_t tftMux = nullptr;
    uint8_t currentRotation = 0;
    uint8_t currentBrightness = TFT_AWAKE_BRIGHTNESS;
    bool currentInversion = false;
    static constexpr uint8_t backlightChannel = 7;
    static constexpr uint8_t bgrBit = 0x08;
    static constexpr uint32_t commandSpiHz = 4000000;
    static constexpr uint32_t framebufferSpiHz = 40000000;
    static constexpr size_t chunkBytes = 4096;
    alignas(4) uint8_t transferChunk[chunkBytes];

    void beginCommand(int dcPin, int csPin, uint8_t cmd, uint8_t spiMode,
                      uint32_t clockHz = commandSpiHz) {
        SPI.beginTransaction(SPISettings(clockHz, MSBFIRST, spiMode));
        digitalWrite(csPin, LOW);
        digitalWrite(dcPin, LOW);
        SPI.transfer(cmd);
    }

    void endCommand(int csPin) {
        digitalWrite(csPin, HIGH);
        SPI.endTransaction();
    }

    void rawWriteCmdData(int dcPin, int csPin, uint8_t cmd,
                         const uint8_t* data, size_t len, uint8_t spiMode) {
        beginCommand(dcPin, csPin, cmd, spiMode);
        if (len) {
            digitalWrite(dcPin, HIGH);
            SPI.writeBytes(data, len);
        }
        endCommand(csPin);
    }

    void rawWriteCmd(uint8_t cmd) {
        rawWriteCmdData(PIN_TFT_DC, PIN_TFT_CS, cmd, nullptr, 0, SPI_MODE0);
    }

    void writeRotation(int dcPin, int csPin, uint8_t spiMode) {
        static constexpr uint8_t madctl[] = {0x00, 0x60, 0xC0, 0xA0};
        const uint8_t value = madctl[currentRotation] | bgrBit;
        rawWriteCmdData(dcPin, csPin, 0x36, &value, 1, spiMode);
    }

    void resetPanel(int dcPin, int csPin) {
        pinMode(csPin, OUTPUT);
        digitalWrite(csPin, HIGH);
        pinMode(dcPin, OUTPUT);
        digitalWrite(dcPin, HIGH);
        pinMode(PIN_TFT_RES, OUTPUT);
        digitalWrite(PIN_TFT_RES, HIGH);
        delay(10);
        digitalWrite(PIN_TFT_RES, LOW);
        delay(30);
        digitalWrite(PIN_TFT_RES, HIGH);
        delay(150);
    }

    void rawInitOtp() {
        resetPanel(PIN_TFT_DC, PIN_TFT_CS);
        rawWriteCmd(0x11);
        delay(120);
        alignas(4) const uint8_t pixelMode[4] = {0x55};
        rawWriteCmdData(PIN_TFT_DC, PIN_TFT_CS, 0x3A, pixelMode, 1, SPI_MODE0);
        writeRotation(PIN_TFT_DC, PIN_TFT_CS, SPI_MODE0);
        rawWriteCmd(0x21);
        rawWriteCmd(0x13);
        rawWriteCmd(0x29);
        delay(50);
    }

    void setAddressRange(uint8_t cmd, uint16_t start, uint16_t end) {
        const uint8_t data[] = {uint8_t(start >> 8), uint8_t(start),
                                uint8_t(end >> 8), uint8_t(end)};
        rawWriteCmdData(PIN_TFT_DC, PIN_TFT_CS, cmd, data, sizeof(data), SPI_MODE0);
    }

    void pushCanvasRegion(const uint16_t* pixels, int left, int top, int right, int bottom) {
        // Compensate for the panel's horizontal mirror in canvas coordinates.
        const int outputLeft = FACE_SCREEN_W - 1 - right;
        const int outputRight = FACE_SCREEN_W - 1 - left;
        const bool landscape = currentRotation & 1;
        const int x0 = landscape ? outputLeft : FACE_SCREEN_H - 1 - bottom;
        const int x1 = landscape ? outputRight : FACE_SCREEN_H - 1 - top;
        const int y0 = landscape ? top : outputLeft;
        const int y1 = landscape ? bottom : outputRight;
        setAddressRange(0x2A, x0, x1);
        setAddressRange(0x2B, y0, y1);
        beginCommand(PIN_TFT_DC, PIN_TFT_CS, 0x2C, SPI_MODE0, framebufferSpiHz);
        digitalWrite(PIN_TFT_DC, HIGH);
        size_t used = 0;
        for (int y = y0; y <= y1; ++y) {
            const uint16_t* pixel = landscape
                ? pixels + y * FACE_SCREEN_W + right
                : pixels + (FACE_SCREEN_H - 1 - x0) * FACE_SCREEN_W + (FACE_SCREEN_W - 1 - y);
            const int step = landscape ? -1 : -FACE_SCREEN_W;
            for (int x = x0; x <= x1; ++x) {
                const uint16_t value = pixel[(x - x0) * step];
                const uint16_t wire = canvasNeedsByteSwap ? __builtin_bswap16(value) : value;
                transferChunk[used++] = uint8_t(wire);
                transferChunk[used++] = uint8_t(wire >> 8);
                if (used == chunkBytes) {
                    SPI.writeBytes(transferChunk, used);
                    used = 0;
                }
            }
        }
        if (used) SPI.writeBytes(transferChunk, used);
        endCommand(PIN_TFT_CS);

    }

    // Caller holds tftMux. Split face updates so an animated mouth and two
    // animated eyes do not force a transfer of the empty space between them.
    void pushCanvasViaSpi(bool face = false) {
        const auto* pixels = static_cast<const uint16_t*>(canvas.getBuffer());
        struct Region { int left, top, right, bottom; };
        constexpr Region whole = {0, 0, FACE_SCREEN_W - 1, FACE_SCREEN_H - 1};
        constexpr Region faceRegions[] = {
            {0, 0, FACE_SCREEN_W - 1, 51},
            {0, 52, FACE_SCREEN_W / 2 - 1, 147},
            {FACE_SCREEN_W / 2, 52, FACE_SCREEN_W - 1, 147},
            {0, 148, FACE_SCREEN_W - 1, FACE_SCREEN_H - 1},
        };
        if (!previousFrameValid) {
            pushCanvasRegion(pixels, whole.left, whole.top, whole.right, whole.bottom);
            if (previousFrame) {
                memcpy(previousFrame, pixels, FACE_SCREEN_W * FACE_SCREEN_H * sizeof(uint16_t));
                previousFrameValid = true;
            }
            return;
        }

        const Region* regions = face ? faceRegions : &whole;
        const size_t count = face ? sizeof(faceRegions) / sizeof(faceRegions[0]) : 1;
        for (size_t i = 0; i < count; ++i) {
            const auto& region = regions[i];
            int left = region.right;
            int right = -1;
            int top = region.bottom;
            int bottom = -1;
            const size_t rowBytes = (region.right - region.left + 1) * sizeof(uint16_t);
            for (int y = region.top; y <= region.bottom; ++y) {
                const auto* row = pixels + y * FACE_SCREEN_W;
                auto* prior = previousFrame + y * FACE_SCREEN_W;
                if (memcmp(row + region.left, prior + region.left, rowBytes) == 0) continue;
                int x0 = region.left;
                int x1 = region.right;
                while (row[x0] == prior[x0]) ++x0;
                while (row[x1] == prior[x1]) --x1;
                left = min(left, x0);
                right = max(right, x1);
                top = min(top, y);
                bottom = y;
                memcpy(prior + x0, row + x0, (x1 - x0 + 1) * sizeof(uint16_t));
            }
            if (right >= 0) pushCanvasRegion(pixels, left, top, right, bottom);
        }
    }

    void drawColorTest() {
        canvas.fillRect(0, 0, 99, FACE_SCREEN_H, TFT_RED);
        canvas.fillRect(99, 0, 98, FACE_SCREEN_H, TFT_GREEN);
        canvas.fillRect(197, 0, 99, FACE_SCREEN_H, TFT_BLUE);
        canvas.setTextColor(TFT_WHITE, TFT_BLACK);
        canvas.setTextSize(2);
        canvas.setCursor(52, 110);
        canvas.print("CIBORGUE 296x240");
    }

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
                            int shakeX, unsigned long now) {
        int ey = centerY - eh / 2;
        const int rad = min(23, min(ew, eh) / 2);
        const float red = faceMorph.colR.val;
        const float green = faceMorph.colG.val;
        const float blue = faceMorph.colB.val;
        canvas.fillRoundRect(ex - 2, ey - 2, ew + 4, eh + 4, rad + 2,
                             packRgb565(red * 0.25f, green * 0.25f, blue * 0.25f));
        for (int row = 0; row < eh; ++row) {
            const int cornerY = max(0, rad - min(row, eh - 1 - row));
            const int inset = rad - (int)sqrtf(rad * rad - cornerY * cornerY);
            const float t = (float)row / (eh - 1);
            const float shade = t < 0.55f ? 1.35f - 0.35f * t / 0.55f
                                          : 1.0f - (t - 0.55f) * 0.37f;
            canvas.drawFastHLine(ex + inset, ey + row, ew - 2 * inset,
                                 packRgb565(red * shade, green * shade, blue * shade));
        }
        if (eh > 23) {
            canvas.fillEllipse(ex + ew * 30 / 100, centerY - eh * 22 / 100,
                               ew * 13 / 100, eh * 11 / 100, 0xEFFF);
            canvas.fillEllipse(ex + ew * 68 / 100, centerY + eh * 20 / 100,
                               ew * 6 / 100, eh * 6 / 100,
                               packRgb565((red + 255) * 0.5f, (green + 255) * 0.5f,
                                          (blue + 255) * 0.5f));
        }

        if (eh > 22) {
            float fw = clampf01(faceMorph.fearW.val);
            if (fw > 0.10f) {
                int pr = max(2, (int)(8.0f * fw));
                canvas.fillCircle(ex + ew / 2 + shakeX, centerY, pr, TFT_BLACK);
                canvas.fillCircle(ex + ew / 2 + shakeX - 2, centerY - 2, 2, 0xEFFF);
            }

            float dw = clampf01(faceMorph.despairW.val);
            if (dw > 0.10f) {
                int rcx = ex + ew / 2;
                int r1 = max(2, (int)(14.0f * dw));
                int r2 = max(3, (int)(22.0f * dw));
                canvas.drawCircle(rcx, centerY + 4, r1, TFT_BLACK);
                canvas.drawCircle(rcx, centerY + 4, r2, TFT_BLACK);
            }

            if (faceMorph.loveW.val > 0.10f) {
                const int cx = ex + ew / 2;
                const int size = max(2, (int)roundf((7.0f + sinf((now % 700) * 0.009f)) * clampf01(faceMorph.loveW.val)));
                const int cy = centerY + 2;
                canvas.fillCircle(cx - size / 2, cy - size / 2, size, 0xFF9E);
                canvas.fillCircle(cx + size / 2, cy - size / 2, size, 0xFF9E);
                canvas.fillTriangle(cx - size * 3 / 2, cy - size / 4,
                                    cx + size * 3 / 2, cy - size / 4,
                                    cx, cy + size * 2, 0xFF9E);
            }
        }

        if (eh > 22 && faceMorph.shameW.val > 0.10f) {
            const int rx = max(1, (int)(ew * 0.17f * clampf01(faceMorph.shameW.val)));
            canvas.fillEllipse(ex + ew * 38 / 100, centerY + eh * 18 / 100,
                               rx, max(1, eh / 10), 0xFF9E);
        }
        if (eh > 20 && faceMorph.maliceW.val > 0.10f) {
            const int ry = max(1, (int)(eh * 0.22f * clampf01(faceMorph.maliceW.val)));
            canvas.fillEllipse(ex + ew / 2 + (isLeft ? 5 : -5), centerY + 2, 5, ry, 0xFEDF);
        }

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
                const float arcHeight = eh * (0.88f + 0.08f * min(1.0f, ba));
                const int arcY = centerY + (int)(eh * (0.56f - 0.42f * ba) + arcHeight / 2);
                canvas.fillEllipse(ex + ew / 2, arcY, (int)(ew * 0.68f),
                                   (int)(arcHeight / 2), TFT_BLACK);
            }

            float bl = isLeft ? faceMorph.lBotL.val : faceMorph.rBotL.val;
            float br = isLeft ? faceMorph.lBotR.val : faceMorph.rBotR.val;
            if (max(bl, br) > 0.01f) {
                drawBotCutQuad(ex, ey, ew, eh, bl, br);
            }
        }

        const float blush = clampf01(faceMorph.blushW.val);
        if (blush > 0.02f) {
            const int cx = ex + ew / 2 + (isLeft ? -13 : 13);
            const int cy = centerY + (int)roundf(faceMorph.blushY.val * 1.16f);
            canvas.fillEllipse(cx, cy, (int)(19 * faceMorph.blushIntensity.val), 8, packRgb565(88 * blush, 17 * blush, 47 * blush));
            canvas.fillEllipse(cx, cy, 15, 5, packRgb565(200 * blush, 50 * blush, 105 * blush));
            for (int dx = -7; dx <= 7; dx += 7) {
                canvas.drawLine(cx + dx - 1, cy + 2, cx + dx + 1, cy - 2,
                                packRgb565(255 * blush, 140 * blush, 174 * blush));
            }
        }

        const float sweat = clampf01(faceMorph.sweatW.val) * constrain(faceMorph.sweatScale.val, 0.3f, 1.0f);
        if (!isLeft && sweat > 0.10f) {
            const int x = ex + ew + 4;
            const int y = centerY - 16 + (int)roundf(sinf((now % 1000) * 0.006283f) * 2);
            const int r = max(2, (int)(5 * sweat));
            canvas.fillTriangle(x, y - r * 2, x - r, y, x + r, y, 0x5EFF);
            canvas.fillEllipse(x, y, r, r + 1, 0x5EFF);
            canvas.fillCircle(x - 1, y - 1, 1, 0xEFFF);
        }
    }

    void updateSpeechMouth(RobotState state, uint32_t now, bool live, float amp, float zcr) {
        float width = 32.0f;
        float height = 4.5f;
        if (state == STATE_SPEAK && !live) {
            // Short overlapping syllable pulses for speech without PCM samples.
            amp = 0.15f + 0.62f * powf(0.5f + 0.5f * sinf(now * 0.039f), 2.0f);
            zcr = 0.5f + 0.4f * sinf(now * 0.023f);
        }
        if (state == STATE_SPEAK && amp > 0.04f) {
            width = min(54.0f, 26.0f + zcr * 28.0f + amp * 14.0f);
            height = min(30.0f, 6.5f + amp * 28.0f * (1.15f - 0.35f * zcr));
        }
        speechWidth += (width - speechWidth) * 0.65f;
        speechHeight += (height - speechHeight) * (height > speechHeight ? 0.82f : 0.62f);
    }

    FaceParticleKind particleKind(RobotState state, FaceEmotion emotion) const {
        if (state == STATE_THINK) return FaceParticleKind::Dot;
        switch (emotion) {
            case EMO_LOVE: return FaceParticleKind::Heart;
            case EMO_HAPPY: case EMO_SURPRISED: case EMO_ANGEL: case EMO_CRAZY:
                return FaceParticleKind::Star;
            case EMO_SAD: case EMO_DESPAIR: return FaceParticleKind::Tear;
            case EMO_SLEEPY: return FaceParticleKind::Zzz;
            case EMO_FEAR: case EMO_SHAME: return FaceParticleKind::Sweat;
            case EMO_DISGUST: return FaceParticleKind::Bubble;
            case EMO_MALICE: return FaceParticleKind::Spark;
            default: return FaceParticleKind::None;
        }
    }

    void spawnFaceParticle(FaceParticleKind kind, uint32_t now) {
        if (kind == FaceParticleKind::None) return;
        for (auto& particle : faceParticles) {
            if (particle.kind != FaceParticleKind::None && now - particle.born < particle.life) continue;
            const bool falling = kind == FaceParticleKind::Tear || kind == FaceParticleKind::Sweat;
            particle.kind = kind;
            particle.born = now;
            particle.life = random(420, 840);
            particle.x = random(0, 2) ? random(39, 57) : random(238, 256);
            particle.y = random(70, 104);
            particle.vx = random(-7, 8);
            particle.vy = falling ? random(25, 58) : random(-48, -22);
            particle.size = random(3, 7);
            return;
        }
    }

    void updateFaceMotion(RobotState state, FaceEmotion emotion, uint32_t now, bool visible) {
        const auto& pose = bench_face::poses[static_cast<unsigned>(emotion)];
        const bool emotionChanged = emotion != animatedEmotion;
        if (emotionChanged || state != animatedState || nextGesture == 0) {
            if (emotionChanged) expressionStarted = now;
            animatedEmotion = emotion;
            animatedState = state;
            timedEyeFrames = 0;
            eyeTransferUs = eyeDrawUs = 0;
            nextGesture = now + 1200;
            gestureStarted = now - 1000;
            targetGazeX = targetGazeY = targetTilt = targetSquint = 0;
            for (auto& particle : faceParticles) particle.kind = FaceParticleKind::None;
            nextParticle = now;
        }
        if (state == STATE_IDLE && (int32_t)(now - nextGesture) >= 0) {
            nextGesture = now + random(1600, 3401);
            if (pose.allow_idle_gaze || emotion == EMO_COOL || emotion == EMO_LIAR) {
                gestureStarted = now;
                const long choice = random(0, 100);
                const bool glance = choice < 45;
                targetGazeX = glance ? random(-14, 15) : 0;
                targetGazeY = glance ? random(-7, 7) : 0;
                if (emotion == EMO_BORED) {
                    targetGazeX = glance ? 9.0f : 0.0f;
                    targetGazeY = glance ? -11.0f : 0.0f;
                }
                targetTilt = glance ? targetGazeX * 0.38f : (choice >= 75 ? random(-4, 5) : 0);
                targetSquint = glance && random(0, 3) == 0 ? 0.22f : 0.0f;
            }
        }
        const float dt = min(64UL, (unsigned long)(now - motionUpdated));
        const float blend = dt / (70.0f + dt);
        motionUpdated = now;
        gazeX += (targetGazeX - gazeX) * blend;
        gazeY += (targetGazeY - gazeY) * blend;
        gazeTilt += (targetTilt - gazeTilt) * blend;
        gazeSquint += (targetSquint - gazeSquint) * blend;
        if (!visible) {
            for (auto& particle : faceParticles) particle.kind = FaceParticleKind::None;
            return;
        }
        const auto kind = particleKind(state, emotion);
        if (emotionChanged || (int32_t)(now - nextParticle) >= 0) {
            const int count = emotionChanged && state == STATE_IDLE ? 3 : 1;
            for (int i = 0; i < count; ++i) spawnFaceParticle(kind, now);
            nextParticle = now + random(490, 850);
        }
    }

    void drawFaceParticles(uint32_t now) {
        for (const auto& p : faceParticles) {
            const uint32_t age = now - p.born;
            if (p.kind == FaceParticleKind::None || age >= p.life) continue;
            const float fade = 1.0f - (float)age / p.life;
            const int x = p.x + (int)(p.vx * (int32_t)age / 1000);
            const int y = p.y + (int)(p.vy * (int32_t)age / 1000);
            const int r = p.size;
            switch (p.kind) {
                case FaceParticleKind::Heart: {
                    const auto c = packRgb565(255 * fade, 90 * fade, 175 * fade);
                    canvas.fillCircle(x - r / 2, y, r / 2, c);
                    canvas.fillCircle(x + r / 2, y, r / 2, c);
                    canvas.fillTriangle(x - r, y, x + r, y, x, y + r, c);
                    break;
                }
                case FaceParticleKind::Star:
                case FaceParticleKind::Spark: {
                    const bool star = p.kind == FaceParticleKind::Star;
                    const auto c = packRgb565(255 * fade, (star ? 230 : 150) * fade,
                                               (star ? 90 : 255) * fade);
                    const float angle = age * 0.006f;
                    const int dx = (int)roundf(r * cosf(angle)), dy = (int)roundf(r * sinf(angle));
                    canvas.drawLine(x - dx, y - dy, x + dx, y + dy, c);
                    canvas.drawLine(x - dy, y + dx, x + dy, y - dx, c);
                    break;
                }
                case FaceParticleKind::Zzz: {
                    const auto c = packRgb565(150 * fade, 210 * fade, 255 * fade);
                    canvas.drawFastHLine(x - r, y - r, 2 * r, c);
                    canvas.drawLine(x + r - 1, y - r, x - r, y + r, c);
                    canvas.drawFastHLine(x - r, y + r, 2 * r, c);
                    break;
                }
                case FaceParticleKind::Sweat: case FaceParticleKind::Tear:
                    canvas.fillEllipse(x, y, max(1, r / 2), r, packRgb565(85 * fade, 195 * fade, 255 * fade));
                    break;
                case FaceParticleKind::Bubble:
                    canvas.drawCircle(x, y, r, packRgb565(160 * fade, 245 * fade, 80 * fade));
                    break;
                case FaceParticleKind::Dot:
                    canvas.fillCircle(x, y, max(1, r / 2), packRgb565(200 * fade, 120 * fade, 255 * fade));
                    break;
                default: break;
            }
        }
    }

    void drawExpressionDetails(FaceEmotion emotion, int lx, int rx, int ly, int ry, uint32_t now) {
        const int cx = (lx + rx) / 2, cy = (ly + ry) / 2;
        switch (emotion) {
            case EMO_ANGEL:
                canvas.drawEllipse(cx, cy - 58, 31, 8, 0xFEAA);
                canvas.drawEllipse(cx, cy - 58, 30, 7, 0xFEAA);
                break;
            case EMO_COOL:
                canvas.drawLine(lx, ly - 5, rx, ry - 5, 0x4414);
                for (int i = 0; i < 2; ++i) {
                    const int x = i ? rx : lx, y = i ? ry : ly;
                    canvas.fillRoundRect(x - 39, y - 21, 78, 42, 9, 0x0884);
                    canvas.drawRoundRect(x - 39, y - 21, 78, 42, 9, 0x4414);
                    canvas.drawLine(x - 27, y + 5, x - 8, y - 12, 0x5476);
                }
                break;
            case EMO_CRAZY:
                for (int eye = 0; eye < 2; ++eye) {
                    const int x = eye ? rx : lx, y = eye ? ry : ly;
                    int px = x, py = y;
                    const float phase = (now % 1050) * 0.005984f;
                    for (int i = 1; i <= 24; ++i) {
                        const float angle = (eye ? -1.0f : 1.0f) * (i * 0.46f + phase);
                        const float radius = i * (eye ? 0.55f : 0.78f);
                        const int nx = x + (int)roundf(radius * cosf(angle));
                        const int ny = y + (int)roundf(radius * sinf(angle));
                        canvas.drawLine(px, py, nx, ny, 0x40C7);
                        px = nx; py = ny;
                    }
                }
                break;
            case EMO_LIAR: {
                const float t = min(1.0f, (now - expressionStarted) / 650.0f);
                const int length = 12 + (int)(36 * (2 * t - t * t));
                canvas.drawLine(cx, cy + 45, cx + length, cy + 50, 0xFD4B);
                canvas.drawLine(cx, cy + 46, cx + length, cy + 51, 0xFD4B);
                canvas.drawLine(cx + length, cy + 50, cx + length - 9, cy + 55, 0xFD4B);
                break;
            }
            default: break;
        }
    }

    void shiftFaceVertically(float shift) {
        auto* pixels = static_cast<uint16_t*>(canvas.getBuffer());
        const int offset = (int)floorf(shift);
        const unsigned weight = (unsigned)roundf((shift - offset) * 16.0f);
        const bool down = shift >= 0.0f;
        constexpr uint32_t mask = 0x07E0F81F;
        static const uint16_t emptyRow[FACE_SCREEN_W] = {};
        constexpr size_t rowBytes = sizeof(emptyRow);
        for (int row = 0; row < FACE_SCREEN_H; ++row) {
            const int y = down ? FACE_SCREEN_H - 1 - row : row;
            const int a = y - offset;
            const int b = a - 1;
            auto* dst = pixels + y * FACE_SCREEN_W;
            const auto* rowA = a >= 0 && a < FACE_SCREEN_H ? pixels + a * FACE_SCREEN_W : emptyRow;
            const auto* rowB = b >= 0 && b < FACE_SCREEN_H ? pixels + b * FACE_SCREEN_W : emptyRow;
            if (weight == 0 || weight == 16 || memcmp(rowA, rowB, rowBytes) == 0) {
                const auto* source = weight == 16 ? rowB : rowA;
                if (source != dst) memcpy(dst, source, rowBytes);
                continue;
            }
            for (int x = 0; x < FACE_SCREEN_W; ++x) {
                const uint16_t p = rowA[x];
                const uint16_t q = rowB[x];
                if (p == q) {
                    dst[x] = p;
                    continue;
                }
                const uint32_t c = canvasNeedsByteSwap ? p : __builtin_bswap16(p);
                const uint32_t d = canvasNeedsByteSwap ? q : __builtin_bswap16(q);
                const uint32_t cSplit = (c | (c << 16)) & mask;
                const uint32_t dSplit = (d | (d << 16)) & mask;
                const uint32_t mixed = ((cSplit * (16 - weight) + dSplit * weight) >> 4) & mask;
                const uint16_t rgb = mixed | (mixed >> 16);
                dst[x] = canvasNeedsByteSwap ? rgb : __builtin_bswap16(rgb);
            }
        }
    }

    void drawMouthShapeMorph(MouthKey key, int cx, int cy, float scale,
                             uint16_t color, uint32_t now) {
        if (scale < 0.15f || key == MK_SILENT) return;
        if (key == MK_SPEAK) {
            const int w = max(1, (int)roundf(speechWidth * 1.16f * scale));
            const int h = max(1, (int)roundf(speechHeight * 1.16f * scale));
            const int r = max(1, min(w, h) * 46 / 100);
            canvas.fillRoundRect(cx - w / 2, cy - h / 2, w, h, r, color);
            if (w > 8 && h > 8) {
                canvas.fillRoundRect(cx - w / 2 + 3, cy - h / 2 + 3, w - 6, h - 6, max(1, r - 2), 0x2864);
                canvas.fillEllipse(cx, cy + h / 6, max(2, w / 3), max(1, h / 5), 0xFB52);
                if (h >= 13 && w >= 35) {
                    canvas.fillRoundRect(cx - w / 4, cy - h / 2 + 3, w / 2, max(2, h / 5), 2, 0xF7DF);
                }
            }
            return;
        }

        static_assert(sizeof(bench_face::mouths) / sizeof(bench_face::mouths[0]) == MK_LIAR + 1,
                      "Bench mouths must match MouthKey");
        const auto& asset = bench_face::mouths[static_cast<unsigned>(key)];
        float scaleX = scale;
        float scaleY = scale;
        if (key == MK_LISTEN) {
            scaleY *= 1.0f + 0.5f * fabsf(sinf((now % 305) * 0.0103f));
        } else if (key == MK_SLEEPY) {
            const float pulse = 1.0f + 0.30f * sinf((now % 1676) * 0.00375f);
            scaleX *= pulse;
            scaleY *= pulse;
        } else if (key == MK_FEAR || key == MK_DESPAIR) {
            cy += (int)roundf(sinf((now % 240) * 0.02618f));
        }
        const int left = (int)roundf(asset.x * scaleX);
        const int top = (int)roundf(asset.y * scaleY);
        const int width = max(1, (int)roundf(asset.width * scaleX));
        const int height = max(1, (int)roundf(asset.height * scaleY));
        const unsigned red = ((color >> 11) & 31) * 255 / 31;
        const unsigned green = ((color >> 5) & 63) * 255 / 63;
        const unsigned blue = (color & 31) * 255 / 31;
        auto* buffer = static_cast<uint16_t*>(canvas.getBuffer());
        for (int y = max(0, cy + top); y < min(FACE_SCREEN_H, cy + top + height); ++y) {
            const auto* row = asset.pixels + ((y - cy - top) * asset.height / height) * asset.width;
            for (int x = max(0, cx + left); x < min(FACE_SCREEN_W, cx + left + width); ++x) {
                const uint32_t pixel = row[(x - cx - left) * asset.width / width];
                if (pixel == 0) continue;
                const unsigned tint = pixel >> 24;
                const unsigned r = ((pixel >> 16) & 255) + (red * tint + 127) / 255;
                const unsigned g = ((pixel >> 8) & 255) + (green * tint + 127) / 255;
                const unsigned b = (pixel & 255) + (blue * tint + 127) / 255;
                const uint16_t rgb = ((min(r, 255U) & 248) << 8) |
                                     ((min(g, 255U) & 252) << 3) | (min(b, 255U) >> 3);
                buffer[y * FACE_SCREEN_W + x] = canvasNeedsByteSwap ? rgb : __builtin_bswap16(rgb);
            }
        }
    }

#endif

public:
    void begin(uint8_t brightness = TFT_AWAKE_BRIGHTNESS) {
        if (!stateMux) {
            stateMux = xSemaphoreCreateMutex();
        }
#if ENABLE_PHYSICAL_DISPLAY
        tftMux = xSemaphoreCreateMutex();
        if (!tftMux) {
            Serial.println("EVT:LOG=TFT falha ao criar mutex");
            return;
        }

        pinMode(PIN_TFT_BLK, OUTPUT);
        digitalWrite(PIN_TFT_BLK, HIGH);
        SPI.begin(PIN_TFT_SCK, -1, PIN_TFT_MOSI, -1);
        rawInitOtp();
        rawWriteCmd(0x20);

        ledcSetup(backlightChannel, 12000, 8);
        setBrightness(brightness);
        canvas.setColorDepth(16);
        canvas.setPsram(true);
        canvas.setSwapBytes(true);
        if (!canvas.createSprite(FACE_SCREEN_W, FACE_SCREEN_H)) {
            Serial.println("EVT:LOG=TFT falha ao alocar canvas RGB565");
            return;
        }
        drawColorTest();
        const auto* rgb = static_cast<const uint8_t*>(canvas.getBuffer());
        const uint8_t* green = rgb + 99 * 2;
        const uint8_t* blue = rgb + 197 * 2;
        Serial.printf("EVT:LOG=TFT JD9853 OTP SPI comandos=%uMHz framebuffer=%uMHz canvas=296x240 RGB565 R=%02X%02X G=%02X%02X B=%02X%02X\n",
                      unsigned(commandSpiHz / 1000000), unsigned(framebufferSpiHz / 1000000),
                      rgb[0], rgb[1], green[0], green[1], blue[0], blue[1]);
        canvasNeedsByteSwap = rgb[0] == 0x00 && rgb[1] == 0xF8;
        previousFrame = static_cast<uint16_t*>(ps_malloc(FACE_SCREEN_W * FACE_SCREEN_H * sizeof(uint16_t)));
        Serial.printf("EVT:LOG=TFT atualizacao parcial=%d\n", previousFrame != nullptr);
        spriteReady = true;
#else
        Serial.println("EVT:LOG=Display em Modo Simulador Virtual 296x240 no PC (ENABLE_PHYSICAL_DISPLAY=0)");
#endif
    }

    void setState(RobotState s) {
        lockState();
        currentState = s;
        if (s != STATE_SPEAK) lipSampleValid = false;
        unlockState();
    }

    void setLipSync(float amp, float zcr) {
        lockState();
        if (currentState == STATE_SPEAK) {
            lipAmp = amp;
            lipZcr = zcr;
            lipSampleMs = millis();
            lipSampleValid = true;
        }
        unlockState();
    }
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

    bool isPhysicalDisplayReady() const {
#if ENABLE_PHYSICAL_DISPLAY
        return spriteReady;
#else
        return false;
#endif
    }
    uint8_t getRotation() const { return currentRotation; }
    uint8_t getBrightness() const { return currentBrightness; }
    bool isInverted() const { return currentInversion; }

    void setBrightness(uint8_t level) {
#if ENABLE_PHYSICAL_DISPLAY
        currentBrightness = level;
        if (level == 255) {
            if (backlightPwmAttached) {
                ledcDetachPin(PIN_TFT_BLK);
                backlightPwmAttached = false;
            }
            pinMode(PIN_TFT_BLK, OUTPUT);
            digitalWrite(PIN_TFT_BLK, HIGH);
        } else {
            ledcWrite(backlightChannel, level);
            if (!backlightPwmAttached) {
                ledcAttachPin(PIN_TFT_BLK, backlightChannel);
                backlightPwmAttached = true;
            }
        }
#else
        (void)level;
#endif
    }

    void setRotation(uint8_t rot) {
#if ENABLE_PHYSICAL_DISPLAY
        if (!spriteReady) return;
        xSemaphoreTake(tftMux, portMAX_DELAY);
        currentRotation = rot & 3;
        previousFrameValid = false;
        writeRotation(PIN_TFT_DC, PIN_TFT_CS, SPI_MODE0);
        xSemaphoreGive(tftMux);
#else
        (void)rot;
#endif
    }

    void setInvert(bool inv) {
#if ENABLE_PHYSICAL_DISPLAY
        if (!spriteReady) return;
        xSemaphoreTake(tftMux, portMAX_DELAY);
        rawWriteCmd(inv ? 0x21 : 0x20);
        currentInversion = inv;
        xSemaphoreGive(tftMux);
#else
        (void)inv;
#endif
    }

    void runColorTest() {
#if ENABLE_PHYSICAL_DISPLAY
        if (!spriteReady) return;
        xSemaphoreTake(tftMux, portMAX_DELAY);
        drawColorTest();
        pushCanvasViaSpi();
        delay(1000);
        xSemaphoreGive(tftMux);
#endif
    }

    void update() {
        unsigned long now = millis();

        if (!isBlinking && (now - lastBlink > 3800)) {
            isBlinking = true;
            lastBlink = now;
        } else if (isBlinking && (now - lastBlink >= 112)) {
            isBlinking = false;
            lastBlink = now;
        }

        lockState();
        const RobotState state = currentState;
        const bool liveLipSync = lipSampleValid && (uint32_t)(now - lipSampleMs) < 140;
        const float amp = liveLipSync ? lipAmp : 0.0f;
        const float zcr = liveLipSync ? lipZcr : 0.0f;
        unlockState();
        const FaceEmotion emotion = currentEmotion;
        faceMorph.step(state, emotion, now);

#if ENABLE_PHYSICAL_DISPLAY
        if (!spriteReady) return;
        updateSpeechMouth(state, now, liveLipSync, amp, zcr);
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

        updateFaceMotion(state, emotion, now, curW == WIDGET_NONE && !dClock);

        xSemaphoreTake(tftMux, portMAX_DELAY);
        const uint32_t drawStartUs = micros();
        canvas.startWrite();
        canvas.fillScreen(TFT_BLACK);

        if (curW != WIDGET_NONE || dClock) {
            DisplayWidgetType effective = (curW != WIDGET_NONE) ? curW : WIDGET_DESK_CLOCK;
            float remainRatio = (wUntil > 0 && wUntil > now && effective != WIDGET_DESK_CLOCK && effective != WIDGET_ALARM_RING)
                                ? (float)(wUntil - now) / (float)wDur : -1.0f;
            drawCardContent(effective, now, remainRatio,
                            cTime, cDate, cWd, wc, tTitles, tDone, dCnt, tCnt,
                            isFocus, pRem, pTot, pGoal, tRem, tTot, tLbl,
                            ps, mLbl, aTime, aLbl, aHint, pcAct, pcPrm);
            canvas.endWrite();
            pushCanvasViaSpi();
            xSemaphoreGive(tftMux);
            return;
        }

        uint16_t eyeColor = faceMorph.packColor();
        drawFaceParticles(now);
        const float blink = isBlinking && emotion != EMO_SLEEPY
            ? sinf((now - lastBlink) * (PI / 112.0f)) : 0.0f;
        const float openness = 1.0f - blink * 0.94f;
        int eyeW = max(20, (int)roundf(faceMorph.eyeW.val * (1.0f + 0.16f * blink)));
        int leftH = max(4, (int)roundf(faceMorph.leftH.val * openness));
        int rightH = max(4, (int)roundf(faceMorph.rightH.val * openness * (1.0f - gazeSquint)));

        int shakeAmp = (int)roundf(faceMorph.shakeAmp.val);
        int shakeX = (shakeAmp > 0) ? (((int)((now / 42) % 3) - 1) * shakeAmp) : 0;

        int offX = (int)roundf(faceMorph.offX.val + gazeX);
        int offY = (int)roundf(faceMorph.offY.val + gazeY);
        const float breathY = sinf((now % 2618) * 0.0024f) * 2.55f;
        const int leftW = max(20, eyeW - eyeW * offX / 151);
        const int rightW = max(20, eyeW + eyeW * offX / 151);
        const int leftX = 85 - leftW / 2 + offX + shakeX;
        const int rightX = 211 - rightW / 2 + offX + shakeX;
        const int centerY = 96 + offY;
        const int tiltY = (int)roundf(63.0f * sinf((faceMorph.tilt.val + gazeTilt) * (PI / 180.0f)));

        // 1. Olhos Cozmo/Eilik com Morph Contínuo
        drawSingleEyeMorph(leftX, centerY - tiltY, leftW, leftH, true, shakeX, now);
        drawSingleEyeMorph(rightX, centerY + tiltY, rightW, rightH, false, shakeX, now);

        // 2. Boca Expressiva Eilik/BMO com Morph Elástico + LipSync (y = 166)
        int mouthX = FACE_SCREEN_W / 2 + (int)roundf(offX * 0.45f);
        int mouthY = 170 + (int)roundf(offY * 0.35f);
        const uint32_t gestureAge = now - gestureStarted;
        const float mouthPulse = state == STATE_IDLE && gestureAge < 480
            ? sinf(gestureAge * (PI / 480.0f)) : 0.0f;
        const float mouthScale = 1.0f + (emotion == EMO_BORED ? 0.22f : 0.10f) * mouthPulse;
        if (faceMorph.mouthT >= 0.999f || faceMorph.prevMouth == faceMorph.currMouth) {
            drawMouthShapeMorph(faceMorph.currMouth, mouthX, mouthY, mouthScale, eyeColor, now);
        } else if (faceMorph.mouthT < 0.40f) {
            float sc = 1.0f - (faceMorph.mouthT / 0.40f) * 0.72f;
            drawMouthShapeMorph(faceMorph.prevMouth, mouthX, mouthY, sc, eyeColor, now);
        } else {
            float u = (faceMorph.mouthT - 0.40f) / 0.60f;
            float sc = max(0.25f, min(1.15f, 0.28f + 0.72f * easeOutBackF(u)));
            drawMouthShapeMorph(faceMorph.currMouth, mouthX, mouthY, sc, eyeColor, now);
        }

        // 3. Pílula discreta no topo (apenas se Pomodoro, Timer ou AP/OFFLINE ativos)
        drawExpressionDetails(emotion, leftX + leftW / 2, rightX + rightW / 2,
                              centerY - tiltY, centerY + tiltY, now);
        shiftFaceVertically(breathY);
        drawTopPill(now, hasPomo, isFocus, pRem, pTot, hasTmr, tRem, tTot, badge);

        canvas.endWrite();
        const uint32_t transferStartUs = micros();
        pushCanvasViaSpi(true);
        xSemaphoreGive(tftMux);
        constexpr uint8_t sampleFrames = 16;
        if (timedEyeFrames < sampleFrames) {
            const uint32_t transferEndUs = micros();
            if (timedEyeFrames == 0) {
                eyeTimingStartUs = transferEndUs;
                Serial.println("EVT:LOG=TFT primeiro frame dos olhos enviado");
            } else {
                eyeTransferUs += transferEndUs - transferStartUs;
                eyeDrawUs += transferStartUs - drawStartUs;
            }
            if (++timedEyeFrames == sampleFrames) {
                const uint32_t elapsedUs = transferEndUs - eyeTimingStartUs;
                Serial.printf("EVT:LOG=TFT olhos FPS=%.1f flush_medio_ms=%.1f desenho_medio_ms=%.1f\n",
                              (sampleFrames - 1) * 1000000.0 / elapsedUs,
                              eyeTransferUs / ((sampleFrames - 1) * 1000.0),
                              eyeDrawUs / ((sampleFrames - 1) * 1000.0));
            }
        }
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
