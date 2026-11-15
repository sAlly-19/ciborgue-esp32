#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"
#include "robot_types.h"
#include "nvs_config.h"
#include "led_controller.h"
#include "display_hal.h"
#include "audio_hal.h"
#include "tools_controller.h"
#include "gemini_brain.h"
#include "wifi_api_server.h"

// Instâncias globais dos subsistemas do Ciborgue
NvsConfigManager nvsConfig;
LedController    ledController;
DisplayHAL       displayHal;
AudioHAL         audioHal;

volatile RobotState currentState   = STATE_BOOT;
FaceEmotion         currentEmotion = EMO_NEUTRAL;
RobotTimer          activeTimer;

ToolsController toolsController(ledController, displayHal, nvsConfig, currentEmotion, activeTimer);
GeminiBrain     geminiBrain(nvsConfig, toolsController);
WifiApiServer   wifiApiServer(nvsConfig, ledController, displayHal, toolsController, geminiBrain, currentState, currentEmotion, activeTimer);

// Buffer Serial assíncrono não-bloqueante
static char serialBuffer[1024];
static size_t serialPos = 0;
static unsigned long lastTelemetryMillis = 0;
static unsigned long alertStartMillis = 0;

void setRobotState(RobotState newState) {
    currentState = newState;
    displayHal.setState(newState);
    if (newState != STATE_CUSTOM && newState != STATE_IDLE) {
        ledController.setStateMode();
    }
    Serial.printf("EVT:STATE=%s\n", stateToString(newState));
}

void setRobotEmotion(FaceEmotion newEmotion) {
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
        doc["timer_rem_s"] = (activeTimer.targetMillis - millis()) / 1000;
        doc["timer_label"] = activeTimer.label;
    } else {
        doc["timer_rem_s"] = 0;
    }

    String out;
    serializeJson(doc, out);
    Serial.printf("EVT:TELEMETRY=%s\n", out.c_str());
}

// Task FreeRTOS dedicada à animação 50 FPS (continua fluida mesmo durante chamadas HTTPS Gemini)
void animTask(void* pvParameters) {
    while (true) {
        ledController.update(currentState);
        displayHal.update();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void processSerialCommand(String cmd) {
    cmd.trim();
    if (cmd.length() == 0) return;

    // 1. Ping / Healthcheck
    if (cmd == "PING") {
        Serial.println("PONG");
        Serial.println("EVT:PONG");
        sendTelemetryPacket();
        return;
    }

    // 2. Provisionamento NVS (Wi-Fi, Gemini API Key, Cidade)
    if (cmd.startsWith("CFG:WIFI=")) {
        String payload = cmd.substring(9);
        int sep = payload.indexOf('|');
        if (sep == -1) sep = payload.indexOf(',');
        if (sep != -1) {
            String ssid = payload.substring(0, sep);
            String pass = payload.substring(sep + 1);
            nvsConfig.saveWifi(ssid, pass);
            Serial.printf("EVT:LOG=Credenciais Wi-Fi ('%s') salvas na Flash NVS!\n", ssid.c_str());
            nvsConfig.connectWifi(8000);
            sendTelemetryPacket();
        }
        return;
    }

    if (cmd.startsWith("CFG:KEY=")) {
        String key = cmd.substring(8);
        nvsConfig.saveGeminiKey(key);
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

    // 3. Entrada de Voz / Texto do Usuário para o Cérebro Gemini no ESP32
    if (cmd.startsWith("MIC:")) {
        String pergunta = cmd.substring(4);
        pergunta.trim();
        if (pergunta.length() == 0) return;

        setRobotState(STATE_THINK);
        displayHal.setSubtitle("Pensando...");

        String resposta = geminiBrain.ask(pergunta);
        resposta.replace("\n", " ");
        resposta.trim();

        // Permanece em THINK até o PC avisar CMD:STATE=SPEAK no início exato do áudio
        displayHal.setSubtitle(resposta);
        Serial.printf("EVT:SAY=%s\n", resposta.c_str());
        sendTelemetryPacket();
        return;
    }

    // 4. Notificação de término da fala (TTS) no PC
    if (cmd == "TTS:DONE") {
        setRobotState(STATE_IDLE);
        displayHal.setSubtitle("Ciborgue Pronto");
        return;
    }

    // 5. Controle de Estado (Compatível com protocolo antigo e novo)
    if (cmd.startsWith("STATE:") || cmd.startsWith("CMD:STATE=")) {
        int idx = cmd.indexOf(':') != -1 && !cmd.startsWith("CMD:") ? cmd.indexOf(':') + 1 : cmd.indexOf('=') + 1;
        String st = cmd.substring(idx);
        RobotState next = stringToState(st);
        setRobotState(next);
        Serial.printf("OK:STATE=%s\n", stateToString(next));
        return;
    }

    // 6. Controle de Emoção / Expressão do Rosto 296x240
    if (cmd.startsWith("CMD:EMOTION=") || cmd.startsWith("EMOTION:")) {
        int idx = cmd.indexOf('=') != -1 ? cmd.indexOf('=') + 1 : cmd.indexOf(':') + 1;
        String emo = cmd.substring(idx);
        setRobotEmotion(stringToEmotion(emo));
        return;
    }

    // 7. Controle Direto de Cor RGB ou Nome
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

    // 8. Telemetria e Limpeza de Memória
    if (cmd == "CMD:GET_STATUS" || cmd == "GET:STATUS") {
        sendTelemetryPacket();
        return;
    }

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

    ledController.begin();
    displayHal.begin();
    audioHal.begin();
    nvsConfig.begin();

    // Inicia task de animação em segundo plano no Core 0 (50 FPS garantidos)
    xTaskCreatePinnedToCore(animTask, "AnimTask", 4096, NULL, 1, NULL, 0);

    if (nvsConfig.hasWifiCredentials()) {
        nvsConfig.connectWifi(6000);
        wifiApiServer.beginIfWifiConnected();
    } else {
        Serial.println("EVT:LOG=Aguardando provisionamento Wi-Fi/Gemini via .env ou Painel PC.");
    }

    setRobotState(STATE_IDLE);
    setRobotEmotion(EMO_NEUTRAL);
    sendTelemetryPacket();
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

    // 2. Verificação de Timers / Despertadores agendados pelo usuário
    if (activeTimer.active && millis() >= activeTimer.targetMillis) {
        activeTimer.active = false;
        setRobotState(STATE_ALERT);
        setRobotEmotion(EMO_SURPRISED);
        alertStartMillis = millis();
        String msg = "Atenção! O seu timer terminou: " + activeTimer.label;
        displayHal.setSubtitle(msg);
        Serial.printf("EVT:SAY=%s\n", msg.c_str());
        sendTelemetryPacket();
    }

    // Retorna de STATE_ALERT para IDLE após 6 segundos se não estiver falando
    if (currentState == STATE_ALERT && alertStartMillis > 0 && (millis() - alertStartMillis > 6000)) {
        alertStartMillis = 0;
        setRobotState(STATE_IDLE);
        setRobotEmotion(EMO_NEUTRAL);
    }

    // 3. Servidor HTTP REST + UDP Auto-Discovery para App Android e Manutenção de Wi-Fi
    nvsConfig.loopMaintainWifi();
    wifiApiServer.handleClient();
    if (millis() - lastTelemetryMillis > 5000) {
        lastTelemetryMillis = millis();
        sendTelemetryPacket();
    }

    delay(5);
}
