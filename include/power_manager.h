#ifndef CIBORGUE_POWER_MANAGER_H
#define CIBORGUE_POWER_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <driver/uart.h>
#include <driver/touch_sensor.h>
#include "config.h"
#include "robot_types.h"
#include "nvs_config.h"
#include "led_controller.h"
#include "display_hal.h"
#include "sfx_synth.h"
#include "touch_sensor.h"
#include "pomodoro_manager.h"
#include "captive_portal.h"
#include "personality_manager.h"
#include "alarm_manager.h"

extern volatile bool animPaused;
extern volatile bool animIsParked;
extern void setRobotState(RobotState newState);
extern void setRobotEmotion(FaceEmotion newEmotion);
extern void sendTelemetryPacket();
extern void setDeskClockMode(bool active);

class PowerManager {
private:
    NvsConfigManager& nvs;
    LedController& led;
    DisplayHAL& display;
    SfxSynth& sfx;
    TouchHeadSensor& touchSensor;
    PomodoroManager& pomodoro;
    CaptivePortalManager& portal;
    PersonalityManager& personality;
    AlarmManager& alarms;

    PowerStage stage = PWR_AWAKE;
    unsigned long lastActivityMillis = 0;
    bool sleepRequestedNow = false;

    // Debounce e detecção de clique curto / pressão longa do botão BOOT (GPIO 0)
    int lastRawBtn = HIGH;
    int stableBtn = HIGH;
    unsigned long lastDebounceAt = 0;
    unsigned long btnPressedAt = 0;
    bool holdTriggered = false;

    void handleBootButton(volatile RobotState& currentState) {
        int raw = digitalRead(PIN_BOOT_BUTTON);
        unsigned long now = millis();
        if (raw != lastRawBtn) {
            lastDebounceAt = now;
            lastRawBtn = raw;
        }
        if ((now - lastDebounceAt) < 40UL) return;

        if (raw != stableBtn) {
            stableBtn = raw;
            if (stableBtn == LOW) {
                btnPressedAt = now;
                holdTriggered = false;
            } else {
                // Botão solto
                if (!holdTriggered) {
                    unsigned long dur = now - btnPressedAt;
                    if (dur < 1000UL) {
                        if (alarms.isRinging()) {
                            registerActivity();
                            alarms.stopRinging();
                        } else if (display.isDeskClockActive()) {
                            registerActivity();
                            setDeskClockMode(false);
                        } else if (stage == PWR_ECO_SLEEPY) {
                            registerActivity();
                            sfx.play(SFX_WAKE);
                        } else if (currentState == STATE_ALERT) {
                            registerActivity();
                            setRobotState(STATE_IDLE);
                            setRobotEmotion(EMO_NEUTRAL);
                        } else {
                            registerActivity();
                            bool nextMute = !nvs.isSfxMuted();
                            nvs.saveSfxMuted(nextMute);
                            if (!nextMute) {
                                sfx.play(SFX_WINK);
                            }
                            Serial.printf("EVT:LOG=Botão BOOT: áudio %s.\n", nextMute ? "mutado" : "reativado");
                            sendTelemetryPacket();
                        }
                    }
                }
            }
        } else if (stableBtn == LOW && !holdTriggered) {
            if ((now - btnPressedAt) >= BOOT_PORTAL_HOLD_MS) {
                holdTriggered = true;
                Serial.println("EVT:LOG=Botão BOOT pressionado por 3s -> Abrindo Portal Cativo Wi-Fi...");
                portal.startPortal(true);
            }
        }
    }

public:
    PowerManager(NvsConfigManager& n, LedController& l, DisplayHAL& d, SfxSynth& s,
                 TouchHeadSensor& t, PomodoroManager& p, CaptivePortalManager& cp,
                 PersonalityManager& pm, AlarmManager& am)
        : nvs(n), led(l), display(d), sfx(s), touchSensor(t), pomodoro(p), portal(cp),
          personality(pm), alarms(am) {}

    void begin() {
        pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);
        lastActivityMillis = millis();
    }

    PowerStage getStage() const { return stage; }

    void setDisplayBrightness(uint8_t level) {
        nvs.saveTftBrightness(level);
        if (stage == PWR_AWAKE) display.setBrightness(level);
        else if (stage == PWR_ECO_SLEEPY) display.setBrightness(min(uint8_t{35}, level));
    }

    void registerActivity() {
        lastActivityMillis = millis();
        if (stage == PWR_ECO_SLEEPY) {
            stage = PWR_AWAKE;
            display.setBrightness(nvs.getTftBrightness());
            led.setBrightnessScale(1.0f);
            WiFi.setSleep(false);
            setRobotEmotion(EMO_NEUTRAL);
            personality.onRested();
            Serial.println("EVT:SLEEP=AWAKE");
            Serial.println("EVT:LOG=Saiu do Modo Eco (atividade detectada).");
        }
    }

    void enterEcoMode() {
        if (stage != PWR_AWAKE) return;
        stage = PWR_ECO_SLEEPY;
        setRobotEmotion(EMO_SLEEPY);
        display.setSubtitle("Modo Eco (Sonolento)...");
        display.setBrightness(min(uint8_t{35}, nvs.getTftBrightness()));
        led.setBrightnessScale(0.22f);
        if (nvs.isWifiConnected()) {
            WiFi.setSleep(true);
        }
        Serial.println("EVT:SLEEP=ECO");
        Serial.println("EVT:LOG=Estágio 1 do Sono: Modo Eco / Sonolento ativado (Wi-Fi ativo em Modem Sleep).");
        sendTelemetryPacket();
    }

    void requestLightSleep() {
        sleepRequestedNow = true;
    }

    void enterLightSleep(volatile RobotState& currentState) {
        if (portal.isActive() || display.isDeskClockActive() || alarms.isRinging()) {
            Serial.println("EVT:LOG=Light Sleep ignorado porque Portal, Desk Clock ou Alarme está ativo.");
            return;
        }

        sfx.play(SFX_SLEEP);
        sfx.waitIdle(350);

        Serial.println("EVT:SLEEP=LIGHT");
        Serial.println("EVT:LOG=Estágio 2 do Sono: entrando em Light Sleep (acorda por Touch GPIO 1, BOOT GPIO 0 ou Serial).");
        Serial.flush();
        delay(30);

        // Pausa a task de animação de forma sincronizada e apaga LED/Display
        animPaused = true;
        unsigned long waitStart = millis();
        while (!animIsParked && (millis() - waitStart < 80UL)) {
            delay(2);
        }
        led.turnOff();
        display.setBrightness(0);

        // Desliga o rádio Wi-Fi antes do Light Sleep
        bool hadWifi = nvs.hasWifiCredentials();
        WiFi.disconnect(false);
        WiFi.mode(WIFI_OFF);
        delay(20);

        // Configura os 3 despertadores sem reiniciar
        if (digitalRead(PIN_BOOT_BUTTON) == HIGH) {
            gpio_wakeup_enable((gpio_num_t)PIN_BOOT_BUTTON, GPIO_INTR_LOW_LEVEL);
            esp_sleep_enable_gpio_wakeup();
        }
        uart_set_wakeup_threshold(UART_NUM_0, 3);
        esp_sleep_enable_uart_wakeup(UART_NUM_0);
#if ENABLE_TOUCH_SENSOR
        touchSleepWakeUpEnable(PIN_TOUCH_HEAD, TOUCH_SLEEP_WAKE_DELTA);
#endif

        stage = PWR_LIGHT_SLEEP;
        esp_light_sleep_start();

        // === RETORNO INSTANTÂNEO DO LIGHT SLEEP (SEM REBOOT) ===
        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
        gpio_wakeup_disable((gpio_num_t)PIN_BOOT_BUTTON);

#if ENABLE_TOUCH_SENSOR
        int8_t pad = digitalPinToTouchChannel(PIN_TOUCH_HEAD);
        if (pad >= 0) {
            touch_pad_sleep_channel_enable((touch_pad_t)pad, false);
        }
        touchSensor.resetAfterWake();
#endif

        stage = PWR_AWAKE;
        lastActivityMillis = millis();
        display.setBrightness(nvs.getTftBrightness());
        led.setBrightnessScale(1.0f);
        animPaused = false;
        personality.onRested();

        if (hadWifi) {
            nvs.triggerAsyncReconnect();
        }

        const char* causeStr = "OUTRO";
        if (cause == ESP_SLEEP_WAKEUP_TOUCHPAD) causeStr = "TOUCH_GPIO1";
        else if (cause == ESP_SLEEP_WAKEUP_GPIO) causeStr = "BOTAO_BOOT";
        else if (cause == ESP_SLEEP_WAKEUP_UART) causeStr = "SERIAL_UART";

        Serial.println("EVT:SLEEP=AWAKE");
        Serial.printf("EVT:LOG=Acordou do Light Sleep sem reboot (causa=%s)!\n", causeStr);

        if (cause == ESP_SLEEP_WAKEUP_TOUCHPAD) {
            setRobotState(STATE_IDLE);
            touchSensor.triggerPetReaction(true, true);
        } else {
            setRobotState(STATE_IDLE);
            setRobotEmotion(EMO_NEUTRAL);
            sfx.play(SFX_WAKE);
        }
        sendTelemetryPacket();
    }

    void update(volatile RobotState& currentState, const RobotTimer& activeTimer) {
        handleBootButton(currentState);

        if (sleepRequestedNow) {
            sleepRequestedNow = false;
            enterLightSleep(currentState);
            return;
        }

        // Nunca dorme durante atividades ativas, Desk Clock, Alarme tocando ou Portal Cativo
        if (pomodoro.isActive() || activeTimer.active || portal.isActive() ||
            display.isDeskClockActive() || alarms.isRinging() ||
            (currentState != STATE_IDLE && currentState != STATE_CUSTOM)) {
            lastActivityMillis = millis();
            return;
        }

        unsigned long elapsed = millis() - lastActivityMillis;
        if (stage == PWR_AWAKE && elapsed >= SLEEP_ECO_TIMEOUT_MS) {
            enterEcoMode();
        } else if (stage == PWR_ECO_SLEEPY && elapsed >= SLEEP_LIGHT_TIMEOUT_MS) {
            enterLightSleep(currentState);
        }
    }
};

#endif // CIBORGUE_POWER_MANAGER_H
