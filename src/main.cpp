#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"
#include "robot_types.h"
#include "nvs_config.h"
#include "led_controller.h"
#include "display_hal.h"
#include "audio_hal.h"
#include "sfx_synth.h"
#include "memory_manager.h"
#include "personality_manager.h"
#include "alarm_manager.h"
#include "pomodoro_manager.h"
#include "touch_sensor.h"
#include "captive_portal.h"
#include "power_manager.h"
#include "ble_settings.h"
#include "tools_controller.h"
#include "offline_engine.h"
#include "gemini_brain.h"
#include "wifi_api_server.h"

// Instâncias globais dos subsistemas do Ciborgue
NvsConfigManager nvsConfig;
LedController    ledController;
DisplayHAL       displayHal;
AudioHAL         audioHal;
SfxSynth         sfxSynth(audioHal, nvsConfig);
MemoryManager    memoryManager;

volatile RobotState currentState   = STATE_BOOT;
FaceEmotion         currentEmotion = EMO_NEUTRAL;
RobotTimer          activeTimer;

volatile bool       animPaused     = false;
volatile bool       animIsParked   = false;

PersonalityManager   personalityManager(displayHal, sfxSynth, currentState, currentEmotion);
AlarmManager         alarmManager(ledController, displayHal, sfxSynth, currentState, currentEmotion);
PomodoroManager      pomodoroManager(ledController, displayHal, sfxSynth, personalityManager, currentState, currentEmotion);
TouchHeadSensor      touchSensor(ledController, displayHal, sfxSynth, personalityManager, currentState, currentEmotion);
CaptivePortalManager captivePortal(nvsConfig, displayHal, sfxSynth);
PowerManager         powerManager(nvsConfig, ledController, displayHal, sfxSynth, touchSensor, pomodoroManager, captivePortal, personalityManager, alarmManager);
ToolsController      toolsController(ledController, displayHal, nvsConfig, memoryManager, pomodoroManager, sfxSynth, powerManager, captivePortal, personalityManager, alarmManager, currentEmotion, activeTimer);
OfflineCommandEngine offlineEngine(nvsConfig, memoryManager, pomodoroManager, sfxSynth, powerManager, captivePortal, toolsController);
GeminiBrain          geminiBrain(nvsConfig, memoryManager, toolsController, offlineEngine, sfxSynth, personalityManager);
WifiApiServer        wifiApiServer(nvsConfig, ledController, displayHal, memoryManager, pomodoroManager, touchSensor, toolsController, geminiBrain, sfxSynth, captivePortal, powerManager, personalityManager, alarmManager, currentState, currentEmotion, activeTimer);

// Buffer Serial assíncrono não-bloqueante
static char serialBuffer[1024];
static size_t serialPos = 0;
static unsigned long lastTelemetryMillis = 0;
static unsigned long alertStartMillis = 0;
static unsigned long lastTimerSyncMillis = 0;

BleSettings bleSettings(nvsConfig, powerManager);

void notifyUserActivity() {
    powerManager.registerActivity();
}

void ensureHttpServerReady() {
    wifiApiServer.ensureHttpStarted();
}

void sendTelemetryPacket();
void setRobotState(RobotState newState);
void setRobotEmotion(FaceEmotion newEmotion);

void setDeskClockMode(bool active) {
    displayHal.setDeskClock(active);
    if (active) {
        displayHal.setNextAlarmHint(alarmManager.getNextAlarmHint());
        Serial.println("EVT:DESK_CLOCK=1");
        Serial.println("EVT:WIDGET={\"type\":\"DESK_CLOCK\"}");
    } else {
        Serial.println("EVT:DESK_CLOCK=0");
        Serial.println("EVT:WIDGET={\"type\":\"NONE\"}");
    }
    sendTelemetryPacket();
}

void cancelActiveTimer() {
    activeTimer.active = false;
    activeTimer.durationSec = 0;
    activeTimer.targetMillis = 0;
    displayHal.syncTimer(false, 0, 60, "");
    displayHal.dismissOverlay();
    if (currentState == STATE_ALERT && !alarmManager.isRinging()) {
        setRobotState(captivePortal.isActive() ? STATE_PORTAL : STATE_IDLE);
        setRobotEmotion(EMO_NEUTRAL);
    }
    Serial.println("EVT:TIMER=0,");
    Serial.println("EVT:WIDGET={\"type\":\"NONE\"}");
    sendTelemetryPacket();
}

bool handlePriorityTapAction() {
    if (alarmManager.isRinging()) {
        alarmManager.stopRinging();
        return true;
    }
    if (currentState == STATE_ALERT) {
        alertStartMillis = 0;
        setRobotState(captivePortal.isActive() ? STATE_PORTAL : STATE_IDLE);
        setRobotEmotion(EMO_NEUTRAL);
        displayHal.dismissOverlay();
        Serial.println("EVT:WIDGET={\"type\":\"NONE\"}");
        return true;
    }
    if (displayHal.isDeskClockActive()) {
        setDeskClockMode(false);
        return true;
    }
    if (displayHal.hasActiveCard()) {
        displayHal.dismissOverlay();
        Serial.println("EVT:WIDGET={\"type\":\"NONE\"}");
        return true;
    }
    return false;
}

void setRobotState(RobotState newState) {
    currentState = newState;
    displayHal.setState(newState);
    if (newState != STATE_CUSTOM && newState != STATE_IDLE) {
        ledController.setStateMode();
    }
    Serial.printf("EVT:STATE=%s\n", stateToString(newState));
}

void setRobotEmotion(FaceEmotion newEmotion) {
    personalityManager.trackReaction(newEmotion, powerManager.getStage() == PWR_AWAKE);
    currentEmotion = newEmotion;
    displayHal.setEmotion(newEmotion);
    Serial.printf("EVT:EMOTION=%s\n", emotionToString(newEmotion));
}

void sendTelemetryPacket() {
    JsonDocument doc;
    doc["state"]      = stateToString(currentState);
    doc["emotion"]    = emotionToString(currentEmotion);
    doc["temp_c"]     = serialized(String(temperatureRead(), 1));
    doc["wifi_ok"]    = nvsConfig.isWifiConnected();
    doc["wifi_ssid"]  = nvsConfig.getWifiSsid();
    doc["wifi_rssi"]  = nvsConfig.isWifiConnected() ? WiFi.RSSI() : 0;
    doc["ip"]         = nvsConfig.isWifiConnected() ? WiFi.localIP().toString() : "0.0.0.0";
    doc["has_key"]    = nvsConfig.hasGeminiKey();
    doc["heap_kb"]    = ESP.getFreeHeap() / 1024;
    doc["psram_kb"]   = ESP.getFreePsram() / 1024;
    doc["uptime_s"]   = millis() / 1000;
    doc["led_r"]      = ledController.getR();
    doc["led_g"]      = ledController.getG();
    doc["led_b"]      = ledController.getB();

    if (activeTimer.active && activeTimer.targetMillis > millis()) {
        doc["timer_rem_s"]   = (activeTimer.targetMillis - millis()) / 1000;
        doc["timer_total_s"] = activeTimer.durationSec;
        doc["timer_label"]   = activeTimer.label;
    } else {
        doc["timer_rem_s"]   = 0;
        doc["timer_total_s"] = 0;
    }

    doc["pomo_active"]   = pomodoroManager.isActive();
    doc["pomo_rem_s"]    = pomodoroManager.getRemainingSec();
    doc["pomo_goal"]     = pomodoroManager.getGoal();

    doc["net_mode"]      = netModeToString(nvsConfig.getNetMode());
    doc["portal_active"] = captivePortal.isActive();
    doc["ap_ip"]         = captivePortal.isActive() ? WiFi.softAPIP().toString() : "";
    doc["ap_clients"]    = captivePortal.isActive() ? WiFi.softAPgetStationNum() : 0;
    doc["power_stage"]   = powerStageToString(powerManager.getStage());
    doc["sfx_vol"]       = nvsConfig.getSfxVolume();
    doc["sfx_mute"]      = nvsConfig.isSfxMuted();
    doc["sfx_idle"]      = nvsConfig.isSfxIdleEnabled();
    doc["fast_path"]     = nvsConfig.isFastPathEnabled();
    doc["tft_brightness"] = nvsConfig.getTftBrightness();
    doc["ble_enabled"] = nvsConfig.isBleEnabled();
    doc["ble_connected"] = bleSettings.isConnected();

    doc["desk_clock"]    = displayHal.isDeskClockActive();
    doc["alarm_ringing"] = alarmManager.isRinging();
    doc["alarms_count"]  = alarmManager.getTotalCount();
    const PetState& pet  = personalityManager.getState();
    doc["affection"]     = pet.affection;
    doc["energy"]        = pet.energy;
    doc["mood"]          = pet.mood;
    doc["mood_label"]    = personalityManager.getMoodLabel();

    String out;
    serializeJson(doc, out);
    Serial.printf("EVT:TELEMETRY=%s\n", out.c_str());
}

// Task FreeRTOS dedicada à animação com intervalo alvo de 16 ms
void animTask(void* pvParameters) {
    while (true) {
        if (animPaused) {
            animIsParked = true;
            vTaskDelay(pdMS_TO_TICKS(15));
            continue;
        }
        animIsParked = false;
        const TickType_t frameStart = xTaskGetTickCount();
        ledController.update(currentState);
        displayHal.update();
        const TickType_t frameTicks = pdMS_TO_TICKS(16);
        const TickType_t elapsed = xTaskGetTickCount() - frameStart;
        vTaskDelay(elapsed < frameTicks ? frameTicks - elapsed : 1);
    }
}

void processSerialCommand(String cmd) {
    cmd.trim();
    if (cmd.length() == 0) return;

    // 1. Ping / Healthcheck + Sincronia Completa (não reseta timer de sono)
    if (cmd == "PING") {
        Serial.println("PONG");
        Serial.println("EVT:PONG");
        sendTelemetryPacket();
        memoryManager.emitAllSyncPackets();
        pomodoroManager.emitStatusPacket(false);
        alarmManager.emitAlarmsPacket();
        personalityManager.emitPersonalityPacket();
        return;
    }

    if (cmd == "CMD:GET_STATUS" || cmd == "GET:STATUS") {
        sendTelemetryPacket();
        memoryManager.emitAllSyncPackets();
        pomodoroManager.emitStatusPacket(false);
        alarmManager.emitAlarmsPacket();
        personalityManager.emitPersonalityPacket();
        return;
    }

    // Comandos de Sono explícitos antes de registrar atividade
    if (cmd == "CMD:SLEEP") {
        powerManager.requestLightSleep();
        return;
    }
    if (cmd == "CMD:ECO") {
        powerManager.enterEcoMode();
        return;
    }
    if (cmd == "CMD:WAKE") {
        powerManager.registerActivity();
        sfxSynth.play(SFX_WAKE);
        return;
    }

    constexpr char lipSyncPrefix[] = "CMD:LIPSYNC=";
    if (cmd.startsWith(lipSyncPrefix)) {
        const char* payload = cmd.c_str() + sizeof(lipSyncPrefix) - 1;
        char* separator;
        const float amp = strtof(payload, &separator);
        if (separator != payload && *separator == ',') {
            char* end;
            const float zcr = strtof(separator + 1, &end);
            if (end != separator + 1 && *end == '\0' &&
                amp >= 0.0f && amp <= 1.0f && zcr >= 0.0f && zcr <= 1.0f) {
                displayHal.setLipSync(amp, zcr);
            }
        }
        return;
    }

    powerManager.registerActivity();

    // Calibração rápida da tela física via Serial (orientação, inversão de cores e brilho)
    if (cmd.startsWith("CMD:TFT_ROT=")) {
        uint8_t rot = (uint8_t)(cmd.substring(12).toInt() & 3);
        displayHal.setRotation(rot);
        Serial.printf("EVT:LOG=Display rotacao=%u\n", (unsigned)rot);
        return;
    }
    if (cmd.startsWith("CMD:TFT_INV=")) {
        bool inv = cmd.substring(12).toInt() != 0;
        displayHal.setInvert(inv);
        Serial.printf("EVT:LOG=Display invert=%d\n", inv ? 1 : 0);
        return;
    }
    if (cmd == "CMD:BLE_PIN") {
        if (nvsConfig.isBleEnabled() && bleSettings.getPin() != 0) Serial.printf("EVT:BLE_PIN=%06lu\n", (unsigned long)bleSettings.getPin());
        else Serial.println("ERR:BLE_DISABLED");
        return;
    }
    if (cmd == "CMD:BLE=0" || cmd == "CMD:BLE=1") {
        const bool enabled = cmd.endsWith("1");
        bleSettings.setEnabled(enabled);
        Serial.printf("EVT:BLE_ENABLED=%d\n", enabled);
        return;
    }
    if (cmd == "CMD:TFT_BRI?") {
        Serial.printf("EVT:TFT_BRI=%u\n", (unsigned)nvsConfig.getTftBrightness());
        return;
    }
    if (cmd.startsWith("CMD:TFT_BRI=")) {
        const char* value = cmd.c_str() + 12;
        char* end;
        const long parsed = strtol(value, &end, 10);
        if (end == value || *end != '\0' || parsed < 0 || parsed > 255) {
            Serial.println("ERR:TFT_BRI_RANGE");
            return;
        }
        const uint8_t bri = parsed;
        powerManager.setDisplayBrightness(bri);
        Serial.printf("EVT:LOG=Display brilho=%u\n", (unsigned)bri);
        sendTelemetryPacket();
        return;
    }
    if (cmd == "CMD:TFT_TEST") {
        displayHal.runColorTest();
        Serial.println("EVT:LOG=Display teste RGB direto concluido");
        return;
    }
    // 2. Portal Cativo Wi-Fi
    if (cmd == "CMD:PORTAL") {
        captivePortal.startPortal(true);
        return;
    }
    if (cmd == "CMD:PORTAL_STOP") {
        captivePortal.stopPortal();
        return;
    }
    if (cmd == "CMD:AP_DEBUG") {
        captivePortal.dumpDebugLog();
        return;
    }

    // 3. Controle de Efeitos Sonoros Nativos, Volume, Mudo e Fast-Path
    if (cmd.startsWith("CMD:SFX=")) {
        sfxSynth.play(stringToSfxKind(cmd.substring(8)));
        return;
    }
    if (cmd.startsWith("CMD:VOL=")) {
        int vol = cmd.substring(8).toInt();
        nvsConfig.saveSfxVolume(vol);
        nvsConfig.saveSfxMuted(vol <= 0);
        if (vol > 0) sfxSynth.play(SFX_WAKE);
        sendTelemetryPacket();
        return;
    }
    if (cmd.startsWith("CMD:MUTE=")) {
        bool muted = cmd.substring(9).toInt() != 0;
        nvsConfig.saveSfxMuted(muted);
        if (!muted) sfxSynth.play(SFX_WINK);
        sendTelemetryPacket();
        return;
    }
    if (cmd.startsWith("CMD:IDLE_SFX=")) {
        bool on = cmd.substring(13).toInt() != 0;
        nvsConfig.saveSfxIdleEnabled(on);
        sendTelemetryPacket();
        return;
    }
    if (cmd.startsWith("CMD:FASTPATH=")) {
        bool on = cmd.substring(13).toInt() != 0;
        nvsConfig.saveFastPathEnabled(on);
        sendTelemetryPacket();
        return;
    }

    // 4. Provisionamento NVS (Wi-Fi, Gemini API Key, Cidade)
    if (cmd.startsWith("CFG:WIFI=")) {
        String payload = cmd.substring(9);
        int sep = payload.indexOf('|');
        if (sep == -1) sep = payload.indexOf(',');
        if (sep != -1) {
            String ssid = payload.substring(0, sep);
            String pass = payload.substring(sep + 1);
            nvsConfig.saveWifi(ssid, pass);
            Serial.printf("EVT:LOG=Credenciais Wi-Fi ('%s') salvas na Flash NVS!\n", ssid.c_str());
            if (nvsConfig.connectWifi(WIFI_BOOT_CONNECT_TIMEOUT_MS)) {
                if (captivePortal.isActive()) {
                    captivePortal.stopPortal();
                } else {
                    wifiApiServer.ensureStaServicesStarted();
                }
            }
            sendTelemetryPacket();
        }
        return;
    }

    if (cmd.startsWith("CFG:KEY=")) {
        String key = cmd.substring(8);
        nvsConfig.saveGeminiKey(key);
        geminiBrain.resetCircuit();
        Serial.println("EVT:LOG=Chave Gemini API salva na Flash NVS com sucesso!");
        sendTelemetryPacket();
        return;
    }

    if (cmd.startsWith("CFG:CITY=")) {
        String city = cmd.substring(9);
        nvsConfig.saveDefaultCity(city);
        Serial.printf("EVT:LOG=Cidade padrão salva na NVS: %s\n", city.c_str());
        return;
    }

    // 5. Entrada de Voz / Texto do Usuário para o Cérebro (Fast-Path -> Gemini -> Fallback Offline)
    if (cmd.startsWith("MIC:")) {
        String pergunta = cmd.substring(4);
        pergunta.trim();
        if (pergunta.length() == 0) return;

        if (displayHal.isDeskClockActive()) {
            setDeskClockMode(false);
        }
        setRobotState(STATE_THINK);

        String resposta = geminiBrain.ask(pergunta);
        resposta.replace("\n", " ");
        resposta.trim();

        if (!captivePortal.isActive() && currentState == STATE_THINK) {
            setRobotState(STATE_IDLE);
        }
        Serial.printf("EVT:SAY=%s\n", resposta.c_str());
        sendTelemetryPacket();
        return;
    }

    // 6. Notificação de término da fala (TTS) no PC
    if (cmd == "TTS:DONE") {
        setRobotState(captivePortal.isActive() ? STATE_PORTAL : STATE_IDLE);
        return;
    }

    // 7. Carinho na Cabeça (Touch Físico ou Virtual pelo Mouse/Botão)
    if (cmd == "CMD:PET") {
        touchSensor.triggerPetReaction(true);
        return;
    }

    // 8. Comandos Diretos de Tarefas e Memórias NVS pelo Painel Visual PC
    if (cmd.startsWith("CMD:TASK_ADD=")) {
        memoryManager.addTask(cmd.substring(13));
        toolsController.syncTasksToDisplay();
        return;
    }
    if (cmd.startsWith("CMD:TASK_DONE=")) {
        int id = cmd.substring(14).toInt();
        memoryManager.completeTask(id);
        toolsController.syncTasksToDisplay();
        return;
    }
    if (cmd.startsWith("CMD:TASK_DEL=")) {
        int id = cmd.substring(13).toInt();
        memoryManager.removeTask(id);
        toolsController.syncTasksToDisplay();
        return;
    }
    if (cmd == "CMD:TASK_CLEAR") {
        memoryManager.clearTasks(false);
        toolsController.syncTasksToDisplay();
        return;
    }
    if (cmd.startsWith("CMD:MEM_ADD=")) {
        memoryManager.addMemory(cmd.substring(12));
        return;
    }
    if (cmd.startsWith("CMD:MEM_DEL=")) {
        int idx = cmd.substring(12).toInt();
        memoryManager.removeMemory(idx);
        return;
    }
    if (cmd == "CMD:MEM_CLEAR") {
        memoryManager.clearMemories();
        return;
    }

    // 9. Comandos Diretos de Pomodoro pelo Painel PC
    if (cmd.startsWith("CMD:POMO_START=")) {
        String payload = cmd.substring(15);
        int s1 = payload.indexOf('|');
        int s2 = payload.lastIndexOf('|');
        int fMin = 25, bMin = 5;
        String goal = "Foco Total";
        if (s1 != -1 && s2 != -1 && s2 > s1) {
            fMin = payload.substring(0, s1).toInt();
            bMin = payload.substring(s1 + 1, s2).toInt();
            goal = payload.substring(s2 + 1);
        }
        String msg = pomodoroManager.start(fMin, bMin, goal);
        Serial.printf("EVT:SAY=%s\n", msg.c_str());
        return;
    }
    if (cmd == "CMD:POMO_STOP") {
        String msg = pomodoroManager.stop();
        Serial.printf("EVT:LOG=%s\n", msg.c_str());
        return;
    }

    // 9B. Comandos de Modo Relógio, Alarmes Recorrentes, Humor, Timer e Overlay
    if (cmd.startsWith("CMD:DESK_CLOCK=")) {
        bool on = cmd.substring(15).toInt() != 0;
        setDeskClockMode(on);
        return;
    }
    if (cmd.startsWith("CMD:ALARM_ADD=")) {
        String payload = cmd.substring(14);
        int p1 = payload.indexOf('|');
        int p2 = (p1 != -1) ? payload.indexOf('|', p1 + 1) : -1;
        int p3 = (p2 != -1) ? payload.indexOf('|', p2 + 1) : -1;
        if (p1 != -1) {
            int hh = payload.substring(0, p1).toInt();
            int mm = (p2 != -1) ? payload.substring(p1 + 1, p2).toInt() : payload.substring(p1 + 1).toInt();
            uint8_t mask = (p2 != -1 && p3 != -1) ? (uint8_t)payload.substring(p2 + 1, p3).toInt()
                         : (p2 != -1 ? (uint8_t)payload.substring(p2 + 1).toInt() : 0x7F);
            String lbl = (p3 != -1) ? payload.substring(p3 + 1) : "Despertador";
            String msg = alarmManager.addAlarm(hh, mm, mask, lbl);
            Serial.printf("EVT:LOG=%s\n", msg.c_str());
        }
        return;
    }
    if (cmd.startsWith("CMD:ALARM_DEL=")) {
        int id = cmd.substring(14).toInt();
        String msg = alarmManager.removeAlarm(id);
        Serial.printf("EVT:LOG=%s\n", msg.c_str());
        return;
    }
    if (cmd.startsWith("CMD:ALARM_TOGGLE=")) {
        String payload = cmd.substring(17);
        int sep = payload.indexOf('|');
        if (sep != -1) {
            int id = payload.substring(0, sep).toInt();
            bool en = payload.substring(sep + 1).toInt() != 0;
            String msg = alarmManager.toggleAlarm(id, en);
            Serial.printf("EVT:LOG=%s\n", msg.c_str());
        }
        return;
    }
    if (cmd == "CMD:ALARM_STOP") {
        alarmManager.stopRinging();
        return;
    }
    if (cmd == "CMD:ALARM_CLEAR") {
        String msg = alarmManager.clearAlarms();
        Serial.printf("EVT:LOG=%s\n", msg.c_str());
        return;
    }
    if (cmd == "CMD:MOOD_CARD") {
        personalityManager.showMoodCard();
        return;
    }
    if (cmd.startsWith("CMD:TIMER_START=")) {
        String payload = cmd.substring(16);
        int sep = payload.indexOf('|');
        int seg = (sep != -1) ? payload.substring(0, sep).toInt() : payload.toInt();
        String label = (sep != -1) ? payload.substring(sep + 1) : "Timer";
        label.trim();
        if (label.length() == 0) label = "Timer";
        seg = constrain(seg, 1, 86400);
        activeTimer.active = true;
        activeTimer.durationSec = seg;
        activeTimer.targetMillis = millis() + (unsigned long)seg * 1000UL;
        activeTimer.label = label;
        displayHal.syncTimer(true, seg, seg, label);
        displayHal.showTimerWidget();
        Serial.printf("EVT:TIMER=%d,%s\n", seg, label.c_str());
        JsonDocument wDoc;
        wDoc["type"]    = "TIMER";
        wDoc["active"]  = true;
        wDoc["rem_s"]   = seg;
        wDoc["total_s"] = seg;
        wDoc["label"]   = label;
        String wOut;
        serializeJson(wDoc, wOut);
        Serial.printf("EVT:WIDGET=%s\n", wOut.c_str());
        sendTelemetryPacket();
        return;
    }
    if (cmd == "CMD:TIMER_STOP") {
        cancelActiveTimer();
        return;
    }
    if (cmd == "CMD:WIDGET_CLOSE") {
        if (!handlePriorityTapAction()) {
            displayHal.dismissOverlay();
            Serial.println("EVT:WIDGET={\"type\":\"NONE\"}");
        }
        return;
    }

    // 10. Controle de Estado
    if (cmd.startsWith("STATE:") || cmd.startsWith("CMD:STATE=")) {
        int idx = cmd.indexOf(':') != -1 && !cmd.startsWith("CMD:") ? cmd.indexOf(':') + 1 : cmd.indexOf('=') + 1;
        String st = cmd.substring(idx);
        RobotState next = stringToState(st);
        setRobotState(next);
        if (next == STATE_LISTEN) {
            sfxSynth.play(SFX_WAKE, 0.8f);
        }
        Serial.printf("OK:STATE=%s\n", stateToString(next));
        return;
    }

    // 11. Controle de Emoção / Expressão do Rosto 296x240
    if (cmd.startsWith("CMD:EMOTION=") || cmd.startsWith("EMOTION:")) {
        int idx = cmd.indexOf('=') != -1 ? cmd.indexOf('=') + 1 : cmd.indexOf(':') + 1;
        String emo = cmd.substring(idx);
        setRobotEmotion(stringToEmotion(emo));
        return;
    }

    // 12. Controle Direto de Cor RGB ou Nome
    if (cmd.startsWith("RGB:")) {
        String vals = cmd.substring(4);
        int c1 = vals.indexOf(',');
        int c2 = vals.lastIndexOf(',');
        if (c1 != -1 && c2 != -1 && c2 > c1) {
            int r = vals.substring(0, c1).toInt();
            int g = vals.substring(c1 + 1, c2).toInt();
            int b = vals.substring(c2 + 1).toInt();
            ledController.setCustomColor((uint8_t)r, (uint8_t)g, (uint8_t)b, LED_MODE_SOLID);
            Serial.printf("OK:RGB=%d,%d,%d\n", r, g, b);
        }
        return;
    }

    if (cmd.startsWith("CMD:COLOR=")) {
        String payload = cmd.substring(10);
        int sep = payload.indexOf(',');
        if (sep != -1) {
            ledController.setNamedColor(payload.substring(0, sep), payload.substring(sep + 1));
        } else {
            ledController.setNamedColor(payload, "solido");
        }
        return;
    }

    // 13. Limpeza de Histórico Gemini
    if (cmd == "CMD:CLEAR_HISTORY") {
        geminiBrain.clearHistory();
        return;
    }

    Serial.printf("UNKNOWN_CMD:%s\n", cmd.c_str());
}

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(300);

    Serial.println("\n==============================================");
    Serial.println("   ROBO CIBORGUE ESP32-S3 (CEREBRO AUTONOMO)  ");
    Serial.println("==============================================");

    nvsConfig.begin();
    ledController.begin();
    displayHal.begin(nvsConfig.getTftBrightness());
    audioHal.begin();
    sfxSynth.begin();
    memoryManager.begin();
    touchSensor.begin();
    personalityManager.begin();
    alarmManager.begin();
    powerManager.begin();
    bleSettings.begin();

    sfxSynth.play(SFX_BOOT);

    // Inicia task de animação em segundo plano no Core 0
    xTaskCreatePinnedToCore(animTask, "AnimTask", 8192, NULL, 1, NULL, 0);

    if (nvsConfig.hasWifiCredentials() && nvsConfig.connectWifi(WIFI_BOOT_CONNECT_TIMEOUT_MS)) {
        displayHal.setNetBadge("");
        wifiApiServer.ensureHttpStarted();
        wifiApiServer.ensureStaServicesStarted();
    } else {
        Serial.println("EVT:LOG=Sem Wi-Fi ativo no boot -> Abrindo Portal Cativo 'Ciborgue-Setup'...");
        captivePortal.startPortal(false);
    }

    if (!captivePortal.isActive()) {
        setRobotState(STATE_IDLE);
    }
    setRobotEmotion(EMO_NEUTRAL);
    toolsController.syncTasksToDisplay();
    sendTelemetryPacket();
    memoryManager.emitAllSyncPackets();
    alarmManager.emitAlarmsPacket();
    personalityManager.emitPersonalityPacket();
}

void loop() {
    // 1. Leitura Serial assíncrona caractere a caractere (zero bloqueio)
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (serialPos > 0) {
                serialBuffer[serialPos] = '\0';
                String line(serialBuffer);
                serialPos = 0;
                processSerialCommand(line);
            }
        } else if (serialPos < sizeof(serialBuffer) - 1) {
            serialBuffer[serialPos++] = c;
        }
    }

    // 2. Verificação e sincronia de Timers agendados pelo usuário
    if (activeTimer.active) {
        unsigned long now = millis();
        if (now >= activeTimer.targetMillis) {
            String finishedLabel = activeTimer.label;
            activeTimer.active = false;
            activeTimer.durationSec = 0;
            activeTimer.targetMillis = 0;
            displayHal.syncTimer(false, 0, 60, "");
            powerManager.registerActivity();
            setRobotState(STATE_ALERT);
            setRobotEmotion(EMO_SURPRISED);
            sfxSynth.play(SFX_ALERT);
            alertStartMillis = now;
            String msg = "Atenção! O seu timer terminou: " + finishedLabel;
            Serial.println("EVT:TIMER=0,");
            Serial.printf("EVT:SAY=%s\n", msg.c_str());
            sendTelemetryPacket();
        } else if ((now - lastTimerSyncMillis) >= 1000UL) {
            lastTimerSyncMillis = now;
            int remS = (int)((activeTimer.targetMillis - now) / 1000UL);
            int totalS = max(1, (int)activeTimer.durationSec);
            displayHal.syncTimer(true, remS, totalS, activeTimer.label);
        }
    }

    if (currentState == STATE_ALERT && !alarmManager.isRinging() && alertStartMillis > 0 && (millis() - alertStartMillis > 6000)) {
        alertStartMillis = 0;
        setRobotState(captivePortal.isActive() ? STATE_PORTAL : STATE_IDLE);
        setRobotEmotion(EMO_NEUTRAL);
    }

    // 3. Sensores, Alarmes, Personalidade, Pomodoro, Sono Inteligente e Portal Cativo
    touchSensor.update();
    alarmManager.update();
    pomodoroManager.update();
    powerManager.update(currentState, activeTimer);
    personalityManager.update(powerManager.getStage(), captivePortal.isActive() || alarmManager.isRinging() || displayHal.hasActiveCard() || displayHal.isDeskClockActive());
    captivePortal.update();

    // 4. Manutenção Wi-Fi resiliente (backoff exponencial), servidor HTTP/UDP e chirps ociosos
    nvsConfig.loopMaintainWifi();
    bool wifiNowConnected = false;
    if (nvsConfig.consumeWifiTransition(wifiNowConnected)) {
        if (wifiNowConnected) {
            Serial.println("EVT:NET=ONLINE");
            geminiBrain.resetCircuit();
            if (!captivePortal.isActive()) {
                displayHal.setNetBadge("");
            }
            wifiApiServer.ensureStaServicesStarted();
            sfxSynth.play(SFX_ONLINE);
        } else {
            Serial.println("EVT:NET=OFFLINE");
            if (!captivePortal.isActive()) {
                displayHal.setNetBadge("OFFLINE");
            }
            sfxSynth.play(SFX_OFFLINE);
        }
        sendTelemetryPacket();
    }

    wifiApiServer.handleClient();
    bleSettings.update();

    if (millis() - lastTelemetryMillis > 5000) {
        lastTelemetryMillis = millis();
        sendTelemetryPacket();
    }

    delay(5);
}
