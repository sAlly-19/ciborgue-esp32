#ifndef CIBORGUE_SFX_SYNTH_H
#define CIBORGUE_SFX_SYNTH_H

#include <Arduino.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "config.h"
#include "robot_types.h"
#include "audio_hal.h"
#include "nvs_config.h"

struct SfxNote {
    float f0;
    float f1;
    float durSec;
    float vibHz;
    float vibDepth;
};

struct SfxRequest {
    SfxKind kind;
    float gainScale;
};

class SfxSynth {
private:
    AudioHAL& audio;
    NvsConfigManager& nvs;

    QueueHandle_t sfxQueue      = nullptr;
    TaskHandle_t  sfxTaskHandle = nullptr;
    volatile bool busy          = false;

    void renderChirp(float f0, float f1, float durSec, float vibHz, float vibDepth, float gain) {
#if ENABLE_PHYSICAL_AUDIO
        const int sampleRate = SFX_SAMPLE_RATE;
        const int totalSamples = max(1, (int)(sampleRate * durSec));
        int16_t chunk[256];
        float phase = 0.0f;

        float volNorm = ((float)nvs.getSfxVolume() / 100.0f) * gain;
        float amplitude = constrain(volNorm, 0.0f, 1.5f) * 9000.0f;

        int written = 0;
        while (written < totalSamples) {
            int n = min(256, totalSamples - written);
            for (int i = 0; i < n; i++) {
                float t = (float)(written + i) / (float)totalSamples;
                float freq = f0 + (f1 - f0) * powf(t, 0.85f);
                if (vibHz > 0.0f && vibDepth != 0.0f) {
                    freq += sinf(t * (float)M_PI * vibHz) * vibDepth;
                }
                phase += (2.0f * (float)M_PI * freq) / (float)sampleRate;
                if (phase > 2.0f * (float)M_PI) {
                    phase = fmodf(phase, 2.0f * (float)M_PI);
                }

                float env = powf(sinf(t * (float)M_PI), 0.65f);
                float wave = sinf(phase) + 0.25f * sinf(phase * 2.0f);
                float sampleF = wave * env * amplitude;
                if (sampleF > 32000.0f) sampleF = 32000.0f;
                if (sampleF < -32000.0f) sampleF = -32000.0f;
                chunk[i] = (int16_t)sampleF;
            }
            audio.writePcm(chunk, (size_t)n);
            written += n;
        }

        // Pequena pausa limpa entre notas (~15ms)
        memset(chunk, 0, sizeof(chunk));
        int gapSamples = (sampleRate * 15) / 1000;
        while (gapSamples > 0) {
            int n = min(256, gapSamples);
            audio.writePcm(chunk, (size_t)n);
            gapSamples -= n;
        }
#else
        (void)f0; (void)f1; (void)durSec; (void)vibHz; (void)vibDepth; (void)gain;
        delay((uint32_t)(durSec * 1000.0f));
#endif
    }

    void playSequence(const SfxNote* notes, int count, float gain) {
        for (int i = 0; i < count; i++) {
            renderChirp(notes[i].f0, notes[i].f1, notes[i].durSec,
                        notes[i].vibHz, notes[i].vibDepth, gain);
        }
        audio.clearTxDma();
    }

    void renderRequest(const SfxRequest& req) {
        Serial.printf("EVT:SFX=%s\n", sfxKindToString(req.kind));

        switch (req.kind) {
            case SFX_BOOT: {
                static const SfxNote seq[] = {
                    {440, 660, 0.07f, 4.0f, 12.0f},
                    {660, 880, 0.07f, 5.0f, 15.0f},
                    {880, 1320, 0.12f, 6.0f, 20.0f}
                };
                playSequence(seq, 3, req.gainScale);
                break;
            }
            case SFX_WAKE: {
                static const SfxNote seq[] = {
                    {520, 780, 0.07f, 6.0f, 18.0f},
                    {820, 1280, 0.11f, 6.0f, 18.0f}
                };
                playSequence(seq, 2, req.gainScale);
                break;
            }
            case SFX_HAPPY: {
                static const SfxNote seq[] = {
                    {587, 659, 0.06f, 6.0f, 15.0f},
                    {784, 987, 0.06f, 6.0f, 18.0f},
                    {1046, 1318, 0.10f, 8.0f, 22.0f}
                };
                playSequence(seq, 3, req.gainScale);
                break;
            }
            case SFX_LOVE: {
                static const SfxNote seq[] = {
                    {523, 659, 0.08f, 5.0f, 14.0f},
                    {659, 784, 0.08f, 5.0f, 16.0f},
                    {784, 1174, 0.14f, 7.0f, 22.0f}
                };
                playSequence(seq, 3, req.gainScale);
                break;
            }
            case SFX_SAD: {
                static const SfxNote seq[] = {
                    {620, 440, 0.14f, 3.0f, 8.0f},
                    {430, 290, 0.20f, 3.0f, 6.0f}
                };
                playSequence(seq, 2, req.gainScale);
                break;
            }
            case SFX_ANGRY: {
                static const SfxNote seq[] = {
                    {220, 160, 0.10f, 12.0f, 25.0f},
                    {190, 140, 0.14f, 14.0f, 30.0f}
                };
                playSequence(seq, 2, req.gainScale);
                break;
            }
            case SFX_SURPRISED: {
                static const SfxNote seq[] = {
                    {440, 1380, 0.14f, 8.0f, 25.0f}
                };
                playSequence(seq, 1, req.gainScale);
                break;
            }
            case SFX_WINK: {
                static const SfxNote seq[] = {
                    {660, 990, 0.06f, 6.0f, 14.0f},
                    {990, 1480, 0.09f, 6.0f, 18.0f}
                };
                playSequence(seq, 2, req.gainScale);
                break;
            }
            case SFX_THINK: {
                static const SfxNote seq[] = {
                    {480, 620, 0.05f, 6.0f, 12.0f},
                    {720, 540, 0.05f, 6.0f, 12.0f},
                    {640, 860, 0.06f, 6.0f, 15.0f}
                };
                playSequence(seq, 3, req.gainScale * 0.85f);
                break;
            }
            case SFX_ALERT: {
                static const SfxNote seq[] = {
                    {880, 1180, 0.09f, 10.0f, 20.0f},
                    {880, 1180, 0.09f, 10.0f, 20.0f},
                    {980, 1420, 0.12f, 10.0f, 25.0f}
                };
                playSequence(seq, 3, req.gainScale);
                break;
            }
            case SFX_POMO_FOCUS: {
                static const SfxNote seq[] = {
                    {523, 784, 0.09f, 4.0f, 10.0f},
                    {784, 1046, 0.12f, 4.0f, 10.0f}
                };
                playSequence(seq, 2, req.gainScale);
                break;
            }
            case SFX_POMO_BREAK: {
                static const SfxNote seq[] = {
                    {659, 880, 0.07f, 6.0f, 15.0f},
                    {880, 1175, 0.08f, 6.0f, 15.0f},
                    {1175, 1568, 0.13f, 7.0f, 20.0f}
                };
                playSequence(seq, 3, req.gainScale);
                break;
            }
            case SFX_SLEEP: {
                static const SfxNote seq[] = {
                    {660, 480, 0.12f, 2.0f, 6.0f},
                    {460, 260, 0.18f, 2.0f, 5.0f}
                };
                playSequence(seq, 2, req.gainScale * 0.8f);
                break;
            }
            case SFX_OFFLINE: {
                static const SfxNote seq[] = {
                    {580, 420, 0.09f, 4.0f, 10.0f},
                    {400, 260, 0.13f, 3.0f, 8.0f}
                };
                playSequence(seq, 2, req.gainScale * 0.85f);
                break;
            }
            case SFX_ONLINE: {
                static const SfxNote seq[] = {
                    {440, 620, 0.06f, 5.0f, 10.0f},
                    {660, 990, 0.10f, 5.0f, 14.0f}
                };
                playSequence(seq, 2, req.gainScale * 0.85f);
                break;
            }
            case SFX_IDLE_CHIRP: {
                uint32_t variant = esp_random() % 4;
                if (variant == 0) {
                    static const SfxNote seq[] = {
                        {680, 920, 0.06f, 8.0f, 18.0f},
                        {880, 740, 0.07f, 6.0f, 14.0f}
                    };
                    playSequence(seq, 2, req.gainScale);
                } else if (variant == 1) {
                    static const SfxNote seq[] = {
                        {540, 780, 0.05f, 6.0f, 12.0f},
                        {820, 1040, 0.08f, 9.0f, 20.0f}
                    };
                    playSequence(seq, 2, req.gainScale);
                } else if (variant == 2) {
                    static const SfxNote seq[] = {
                        {740, 1120, 0.08f, 10.0f, 24.0f}
                    };
                    playSequence(seq, 1, req.gainScale);
                } else {
                    static const SfxNote seq[] = {
                        {620, 500, 0.05f, 5.0f, 10.0f},
                        {520, 840, 0.08f, 7.0f, 16.0f}
                    };
                    playSequence(seq, 2, req.gainScale);
                }
                break;
            }
            case SFX_FEAR: {
                static const SfxNote seq[] = {
                    {620, 740, 0.06f, 22.0f, 35.0f},
                    {700, 860, 0.06f, 24.0f, 40.0f},
                    {780, 660, 0.09f, 26.0f, 42.0f}
                };
                playSequence(seq, 3, req.gainScale);
                break;
            }
            case SFX_DISGUST: {
                static const SfxNote seq[] = {
                    {340, 210, 0.11f, 16.0f, 28.0f},
                    {230, 145, 0.15f, 18.0f, 32.0f}
                };
                playSequence(seq, 2, req.gainScale);
                break;
            }
            case SFX_CONTEMPT: {
                static const SfxNote seq[] = {
                    {420, 380, 0.06f, 3.0f, 6.0f},
                    {320, 250, 0.09f, 2.0f, 5.0f}
                };
                playSequence(seq, 2, req.gainScale);
                break;
            }
            case SFX_SHAME: {
                static const SfxNote seq[] = {
                    {760, 620, 0.07f, 5.0f, 10.0f},
                    {580, 640, 0.09f, 4.0f, 9.0f}
                };
                playSequence(seq, 2, req.gainScale * 0.85f);
                break;
            }
            case SFX_DESPAIR: {
                static const SfxNote seq[] = {
                    {740, 520, 0.12f, 10.0f, 22.0f},
                    {500, 340, 0.14f, 10.0f, 20.0f},
                    {330, 190, 0.20f, 8.0f, 16.0f}
                };
                playSequence(seq, 3, req.gainScale);
                break;
            }
            case SFX_MALICE: {
                static const SfxNote seq[] = {
                    {460, 620, 0.06f, 8.0f, 14.0f},
                    {540, 760, 0.06f, 9.0f, 16.0f},
                    {660, 940, 0.10f, 12.0f, 22.0f}
                };
                playSequence(seq, 3, req.gainScale);
                break;
            }
        }
    }

    static void sfxTaskEntry(void* arg) {
        SfxSynth* self = static_cast<SfxSynth*>(arg);
        self->sfxTaskLoop();
    }

    void sfxTaskLoop() {
        SfxRequest req{};
        while (true) {
            if (xQueueReceive(sfxQueue, &req, portMAX_DELAY) == pdTRUE) {
                busy = true;
                if (!nvs.isSfxMuted() && nvs.getSfxVolume() > 0) {
                    renderRequest(req);
                }
                busy = false;
            }
        }
    }

public:
    SfxSynth(AudioHAL& a, NvsConfigManager& n) : audio(a), nvs(n) {}

    void begin() {
        sfxQueue = xQueueCreate(6, sizeof(SfxRequest));
        xTaskCreatePinnedToCore(sfxTaskEntry, "SfxTask", 4096, this, 2, &sfxTaskHandle, 0);
    }

    bool play(SfxKind kind, float gainScale = 1.0f) {
        if (!sfxQueue || nvs.isSfxMuted() || nvs.getSfxVolume() == 0) return false;
        SfxRequest req{kind, gainScale};
        return xQueueSend(sfxQueue, &req, 0) == pdPASS;
    }

    void waitIdle(uint32_t maxMs) {
        if (!sfxQueue) return;
        unsigned long start = millis();
        while ((busy || uxQueueMessagesWaiting(sfxQueue) > 0) && (millis() - start < maxMs)) {
            delay(10);
        }
    }

    bool isBusy() const {
        return busy || (sfxQueue && uxQueueMessagesWaiting(sfxQueue) > 0);
    }

    bool idleChirpEnabled() const {
        return nvs.isSfxIdleEnabled() && !nvs.isSfxMuted() && nvs.getSfxVolume() > 0;
    }
};

#endif // CIBORGUE_SFX_SYNTH_H
