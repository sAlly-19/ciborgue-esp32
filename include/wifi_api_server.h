#ifndef CIBORGUE_WIFI_API_SERVER_H
#define CIBORGUE_WIFI_API_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include "config.h"
#include "robot_types.h"
#include "nvs_config.h"
#include "led_controller.h"
#include "display_hal.h"
#include "memory_manager.h"
#include "pomodoro_manager.h"
#include "touch_sensor.h"
#include "tools_controller.h"
#include "gemini_brain.h"
#include "sfx_synth.h"
#include "captive_portal.h"
#include "power_manager.h"
#include "personality_manager.h"
#include "alarm_manager.h"

extern void processSerialCommand(String cmd);
extern void setRobotState(RobotState newState);
extern void setRobotEmotion(FaceEmotion newEmotion);
extern void sendTelemetryPacket();
extern void setDeskClockMode(bool active);
extern void cancelActiveTimer();

/**
 * Subclasse de WebServer com timeout curto (300 ms) para sockets ociosos e diagnóstico de TLS/parse.
 */
class PortalAwareWebServer : public WebServer {
public:
    std::function<void(const char*)> onBadReq;

    explicit PortalAwareWebServer(int port = 80) : WebServer(port) {}

    void handleClient() override {
        if (_currentStatus == HC_NONE) {
            _currentClient = _server.available();
            if (!_currentClient) {
                if (_nullDelay) {
                    delay(1);
                }
                return;
            }
            _currentStatus = HC_WAIT_READ;
            _statusChange = millis();
        }

        bool keepCurrentClient = false;
        bool callYield = false;

        if (_currentClient.connected()) {
            switch (_currentStatus) {
            case HC_NONE:
                break;
            case HC_WAIT_READ:
                if (_currentClient.available()) {
                    if (_currentClient.peek() == 0x16) {
                        if (onBadReq) onBadReq("TLS_ON_PORT80");
                    } else if (_parseRequest(_currentClient)) {
                        _currentClient.setTimeout(HTTP_MAX_SEND_WAIT / 1000);
                        _contentLength = CONTENT_LENGTH_NOT_SET;
                        _handleRequest();
                    } else {
                        if (onBadReq) onBadReq("PARSE_FAIL");
                    }
                } else {
                    if ((unsigned long)(millis() - _statusChange) <= 300UL) {
                        keepCurrentClient = true;
                    } else {
                        if (onBadReq) onBadReq("IDLE_SOCK_300MS");
                    }
                    callYield = true;
                }
                break;
            case HC_WAIT_CLOSE:
                if ((unsigned long)(millis() - _statusChange) <= HTTP_MAX_CLOSE_WAIT) {
                    keepCurrentClient = true;
                    callYield = true;
                }
                break;
            }
        }

        if (!keepCurrentClient) {
            _currentClient = WiFiClient();
            _currentStatus = HC_NONE;
            _currentUpload.reset();
        }

        if (callYield) {
            yield();
        }
    }
};

class WifiApiServer {
private:
    PortalAwareWebServer server;
    WiFiUDP udp;
    NvsConfigManager& nvs;
    LedController& led;
    DisplayHAL& display;
    MemoryManager& memory;
    PomodoroManager& pomodoro;
    TouchHeadSensor& touchSensor;
    ToolsController& tools;
    GeminiBrain& brain;
    SfxSynth& sfx;
    CaptivePortalManager& portal;
    PowerManager& power;
    PersonalityManager& personality;
    AlarmManager& alarms;
    volatile RobotState& currentState;
    FaceEmotion& currentEmotion;
    RobotTimer& activeTimer;
    bool routesRegistered = false;
    bool httpStarted = false;
    bool staServicesStarted = false;

    void addCorsHeaders() {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        server.sendHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    }

    String buildStatusJson() {
        JsonDocument doc;
        doc["ok"]          = true;
        doc["device"]      = "Ciborgue ESP32-S3 (N16R8)";
        doc["hostname"]    = "ciborgue.local";
        doc["state"]       = stateToString(currentState);
        doc["emotion"]     = emotionToString(currentEmotion);
        doc["last_reply"]  = brain.getLastReply();
        doc["temp_c"]      = serialized(String(temperatureRead(), 1));
        doc["wifi_ok"]     = nvs.isWifiConnected();
        doc["wifi_ssid"]   = nvs.getWifiSsid();
        doc["wifi_rssi"]   = nvs.isWifiConnected() ? WiFi.RSSI() : 0;
        doc["ip"]          = nvs.isWifiConnected() ? WiFi.localIP().toString() : "0.0.0.0";
        doc["has_key"]     = nvs.hasGeminiKey();
        doc["city"]        = nvs.getDefaultCity();
        doc["heap_kb"]     = ESP.getFreeHeap() / 1024;
        doc["psram_kb"]    = ESP.getFreePsram() / 1024;
        doc["uptime_s"]    = millis() / 1000;
        doc["led_r"]       = led.getR();
        doc["led_g"]       = led.getG();
        doc["led_b"]       = led.getB();

        if (activeTimer.active && activeTimer.targetMillis > millis()) {
            doc["timer_rem_s"]   = (activeTimer.targetMillis - millis()) / 1000;
            doc["timer_total_s"] = activeTimer.durationSec;
            doc["timer_label"]   = activeTimer.label;
        } else {
            doc["timer_rem_s"]   = 0;
            doc["timer_total_s"] = 0;
            doc["timer_label"]   = "";
        }

        doc["pomo_active"] = pomodoro.isActive();
        doc["pomo_rem_s"]  = pomodoro.getRemainingSec();
        doc["pomo_goal"]   = pomodoro.getGoal();

        doc["net_mode"]      = netModeToString(nvs.getNetMode());
        doc["portal_active"] = portal.isActive();
        doc["ap_ip"]         = portal.isActive() ? WiFi.softAPIP().toString() : "";
        doc["ap_clients"]    = portal.isActive() ? WiFi.softAPgetStationNum() : 0;
        doc["power_stage"]   = powerStageToString(power.getStage());
        doc["sfx_vol"]       = nvs.getSfxVolume();
        doc["sfx_mute"]      = nvs.isSfxMuted();
        doc["sfx_idle"]      = nvs.isSfxIdleEnabled();
        doc["fast_path"]     = nvs.isFastPathEnabled();

        const PetState& ps = personality.getState();
        doc["desk_clock"]    = display.isDeskClockActive();
        doc["alarm_ringing"] = alarms.isRinging();
        doc["alarms_count"]  = alarms.getTotalCount();
        doc["affection"]     = ps.affection;
        doc["energy"]        = ps.energy;
        doc["mood"]          = ps.mood;
        doc["mood_label"]    = personality.getMoodLabel();

        String out;
        serializeJson(doc, out);
        return out;
    }

    void setupRoutes() {
        server.onNotFound([this]() {
            if (server.method() == HTTP_OPTIONS) {
                addCorsHeaders();
                server.send(204);
                return;
            }
            if (portal.isActive() && !server.uri().startsWith("/api/")) {
                String apIpStr = WiFi.softAPIP().toString();
                if (server.hostHeader().startsWith(apIpStr)) {
                    portal.logDebug("NOT_FOUND host=" + server.hostHeader() + " uri=" + server.uri());
                    portal.serveSetupPage(server);
                } else {
                    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
                    String target = String("http://") + apIpStr + "/";
                    server.sendHeader("Location", target, true);
                    server.send(302, "text/html",
                                String("<html><body><a href='") + target + "'>Setup</a></body></html>");
                    WiFiClient cli = server.client();
                    portal.finishAndLogClient(cli, ("NOT_FOUND host=" + server.hostHeader() + " uri=" + server.uri()).c_str());
                }
                return;
            }
            addCorsHeaders();
            server.send(404, "application/json", "{\"ok\":false,\"error\":\"Endpoint nao encontrado\"}");
        });

        server.on("/api/status", HTTP_GET, [this]() {
            addCorsHeaders();
            server.send(200, "application/json", buildStatusJson());
        });

        server.on("/api/chat", HTTP_POST, [this]() {
            addCorsHeaders();
            power.registerActivity();
            JsonDocument req;
            if (deserializeJson(req, server.arg("plain"))) {
                server.send(400, "application/json", "{\"ok\":false,\"error\":\"JSON invalido\"}");
                return;
            }
            String message = req["message"] | req["prompt"] | req["text"] | "";
            message.trim();
            if (message.length() == 0) {
                server.send(400, "application/json", "{\"ok\":false,\"error\":\"Campo 'message' vazio\"}");
                return;
            }

            Serial.printf("EVT:LOG=[Android Wi-Fi] Pergunta: %s\n", message.c_str());
            setRobotState(STATE_THINK);
            display.setSubtitle("Pensando...");

            String reply = brain.ask(message);
            reply.replace("\n", " ");
            reply.trim();

            setRobotState(STATE_IDLE);
            display.setSubtitle(reply);
            Serial.printf("EVT:SAY=%s\n", reply.c_str());
            sendTelemetryPacket();

            JsonDocument res;
            res["ok"]      = true;
            res["reply"]   = reply;
            res["state"]   = stateToString(currentState);
            res["emotion"] = emotionToString(currentEmotion);
            res["led_r"]   = led.getR();
            res["led_g"]   = led.getG();
            res["led_b"]   = led.getB();

            String out;
            serializeJson(res, out);
            server.send(200, "application/json", out);
        });

        server.on("/api/emotion", HTTP_POST, [this]() {
            addCorsHeaders();
            power.registerActivity();
            JsonDocument req;
            deserializeJson(req, server.arg("plain"));
            String emo = req["emotion"] | req["emocao"] | "NEUTRAL";
            setRobotEmotion(stringToEmotion(emo));
            sendTelemetryPacket();
            server.send(200, "application/json", buildStatusJson());
        });

        server.on("/api/state", HTTP_POST, [this]() {
            addCorsHeaders();
            power.registerActivity();
            JsonDocument req;
            deserializeJson(req, server.arg("plain"));
            String st = req["state"] | "IDLE";
            setRobotState(stringToState(st));
            sendTelemetryPacket();
            server.send(200, "application/json", buildStatusJson());
        });

        server.on("/api/led", HTTP_POST, [this]() {
            addCorsHeaders();
            power.registerActivity();
            JsonDocument req;
            deserializeJson(req, server.arg("plain"));

            if (req["color"].is<const char*>() || req["cor"].is<const char*>()) {
                String cor = req["color"] | req["cor"] | "azul";
                String efeito = req["effect"] | req["efeito"] | "solido";
                led.setNamedColor(cor, efeito);
            } else {
                int r = req["r"] | 0;
                int g = req["g"] | 255;
                int b = req["b"] | 120;
                String modeStr = req["mode"] | "SOLID";
                modeStr.toUpperCase();
                LedEffectMode mode = LED_MODE_SOLID;
                if (modeStr == "PULSE" || modeStr == "PULSAR") mode = LED_MODE_PULSE;
                else if (modeStr == "RAINBOW" || modeStr == "ARCO_IRIS") mode = LED_MODE_RAINBOW;
                else if (modeStr == "ALERT" || modeStr == "ALERTA") mode = LED_MODE_ALERT;
                led.setCustomColor((uint8_t)r, (uint8_t)g, (uint8_t)b, mode);
            }
            sendTelemetryPacket();
            server.send(200, "application/json", buildStatusJson());
        });

        server.on("/api/timer", HTTP_POST, [this]() {
            addCorsHeaders();
            JsonDocument req;
            deserializeJson(req, server.arg("plain"));
            int seconds = req["seconds"] | req["segundos"] | 60;
            String label = req["label"] | req["lembrete"] | "Alarme do App Android!";
            String msg = tools.toolDefinirTimer(seconds, label);
            sendTelemetryPacket();

            JsonDocument res;
            res["ok"] = true;
            res["message"] = msg;
            res["timer_rem_s"] = seconds;
            res["timer_label"] = label;
            String out;
            serializeJson(res, out);
            server.send(200, "application/json", out);
        });

        // Endpoint de Tarefas NVS (/api/tasks)
        server.on("/api/tasks", HTTP_GET, [this]() {
            addCorsHeaders();
            server.send(200, "application/json", memory.getTasksJson());
        });

        server.on("/api/tasks", HTTP_POST, [this]() {
            addCorsHeaders();
            JsonDocument req;
            deserializeJson(req, server.arg("plain"));
            String action = req["action"] | "add";
            String title = req["title"] | "";
            int id = req["id"] | 1;
            tools.toolGerenciarTarefas(action, title, id);
            server.send(200, "application/json", memory.getTasksJson());
        });

        // Endpoint de Memórias de Longo Prazo NVS (/api/memories)
        server.on("/api/memories", HTTP_GET, [this]() {
            addCorsHeaders();
            server.send(200, "application/json", memory.getMemoriesJson());
        });

        server.on("/api/memories", HTTP_POST, [this]() {
            addCorsHeaders();
            JsonDocument req;
            deserializeJson(req, server.arg("plain"));
            String action = req["action"] | "adicionar";
            String fact = req["fact"] | req["fato"] | "";
            tools.toolSalvarMemoria(action, fact);
            server.send(200, "application/json", memory.getMemoriesJson());
        });

        // Endpoint de Pomodoro (/api/pomodoro)
        server.on("/api/pomodoro", HTTP_POST, [this]() {
            addCorsHeaders();
            JsonDocument req;
            deserializeJson(req, server.arg("plain"));
            String action = req["action"] | "iniciar";
            int focus = req["focus_min"] | 25;
            int brk = req["break_min"] | 5;
            String goal = req["goal"] | "Foco";
            String msg = tools.toolIniciarPomodoro(action, focus, brk, goal);
            JsonDocument res;
            res["ok"] = true;
            res["message"] = msg;
            String out;
            serializeJson(res, out);
            server.send(200, "application/json", out);
        });

        // Endpoint de Carinho (/api/pet)
        server.on("/api/pet", HTTP_POST, [this]() {
            addCorsHeaders();
            touchSensor.triggerPetReaction(true);
            server.send(200, "application/json", buildStatusJson());
        });

        server.on("/api/config", HTTP_POST, [this]() {
            addCorsHeaders();
            JsonDocument req;
            deserializeJson(req, server.arg("plain"));

            if (req["gemini_key"].is<const char*>()) {
                String k = req["gemini_key"].as<String>();
                k.trim();
                if (k.length() > 10) nvs.saveGeminiKey(k);
            }
            if (req["city"].is<const char*>()) {
                String c = req["city"].as<String>();
                c.trim();
                if (c.length() > 0) nvs.saveDefaultCity(c);
            }
            if (req["wifi_ssid"].is<const char*>() && req["wifi_password"].is<const char*>()) {
                String ssid = req["wifi_ssid"].as<String>();
                String pass = req["wifi_password"].as<String>();
                ssid.trim();
                if (ssid.length() > 0) {
                    nvs.saveWifi(ssid, pass);
                }
            }
            if (req["sfx_vol"].is<int>()) {
                nvs.saveSfxVolume(req["sfx_vol"].as<int>());
            }
            if (req["sfx_mute"].is<bool>()) {
                nvs.saveSfxMuted(req["sfx_mute"].as<bool>());
            }
            if (req["sfx_idle"].is<bool>()) {
                nvs.saveSfxIdleEnabled(req["sfx_idle"].as<bool>());
            }
            if (req["fast_path"].is<bool>()) {
                nvs.saveFastPathEnabled(req["fast_path"].as<bool>());
            }
            power.registerActivity();
            sendTelemetryPacket();
            server.send(200, "application/json", buildStatusJson());
        });

        server.on("/api/audio", HTTP_POST, [this]() {
            addCorsHeaders();
            power.registerActivity();
            JsonDocument req;
            deserializeJson(req, server.arg("plain"));
            if (req["volume"].is<int>()) {
                nvs.saveSfxVolume(req["volume"].as<int>());
            }
            if (req["mute"].is<bool>()) {
                nvs.saveSfxMuted(req["mute"].as<bool>());
            }
            if (req["idle_chirps"].is<bool>()) {
                nvs.saveSfxIdleEnabled(req["idle_chirps"].as<bool>());
            }
            if (req["play"].is<const char*>()) {
                sfx.play(stringToSfxKind(req["play"].as<String>()));
            }
            sendTelemetryPacket();
            server.send(200, "application/json", buildStatusJson());
        });

        server.on("/api/sleep", HTTP_POST, [this]() {
            addCorsHeaders();
            JsonDocument req;
            deserializeJson(req, server.arg("plain"));
            String mode = req["mode"] | "eco";
            mode.toLowerCase();
            if (mode == "light" || mode == "dormir") {
                server.send(200, "application/json", "{\"ok\":true,\"sleep\":\"LIGHT\"}");
                power.requestLightSleep();
                return;
            } else if (mode == "wake" || mode == "acordar") {
                power.registerActivity();
                sfx.play(SFX_WAKE);
            } else {
                power.enterEcoMode();
            }
            sendTelemetryPacket();
            server.send(200, "application/json", buildStatusJson());
        });

        server.on("/api/portal", HTTP_POST, [this]() {
            addCorsHeaders();
            power.registerActivity();
            JsonDocument req;
            deserializeJson(req, server.arg("plain"));
            String action = req["action"] | "start";
            action.toLowerCase();
            if (action == "stop") {
                portal.stopPortal();
            } else {
                portal.startPortal(true);
            }
            server.send(200, "application/json", buildStatusJson());
        });

        server.on("/api/alarms", HTTP_GET, [this]() {
            addCorsHeaders();
            server.send(200, "application/json", alarms.getAlarmsJson());
        });

        server.on("/api/alarms", HTTP_POST, [this]() {
            addCorsHeaders();
            power.registerActivity();
            JsonDocument req;
            deserializeJson(req, server.arg("plain"));
            String action = req["action"] | "list";
            action.toLowerCase();
            int id = req["id"] | 1;
            int hour = req["hour"] | 7;
            int minute = req["minute"] | 0;
            uint8_t mask = req["daysMask"] | 0x7F;
            String label = req["label"] | "Despertador";
            String msg = "";
            if (action == "add") {
                msg = alarms.addAlarm(hour, minute, mask, label);
            } else if (action == "del" || action == "remove") {
                msg = alarms.removeAlarm(id);
            } else if (action == "toggle") {
                bool en = req["enabled"] | true;
                msg = alarms.toggleAlarm(id, en);
            } else if (action == "stop") {
                alarms.stopRinging();
                msg = "Alarme silenciado";
            } else if (action == "clear") {
                msg = alarms.clearAlarms();
            } else {
                msg = alarms.listAlarmsSummary();
            }
            display.setNextAlarmHint(alarms.getNextAlarmHint());
            JsonDocument res;
            res["ok"] = true;
            res["message"] = msg;
            res["alarms_count"] = alarms.getTotalCount();
            String out;
            serializeJson(res, out);
            server.send(200, "application/json", out);
        });

        server.on("/api/personality", HTTP_GET, [this]() {
            addCorsHeaders();
            if (server.hasArg("card") && server.arg("card") == "1") {
                personality.showMoodCard();
            }
            server.send(200, "application/json", personality.getStateJson());
        });

        server.on("/api/personality", HTTP_POST, [this]() {
            addCorsHeaders();
            power.registerActivity();
            personality.showMoodCard();
            server.send(200, "application/json", personality.getStateJson());
        });

        server.on("/api/desk_clock", HTTP_POST, [this]() {
            addCorsHeaders();
            power.registerActivity();
            JsonDocument req;
            deserializeJson(req, server.arg("plain"));
            bool active = req["active"] | true;
            setDeskClockMode(active);
            server.send(200, "application/json", buildStatusJson());
        });

        server.on("/api/command", HTTP_POST, [this]() {
            addCorsHeaders();
            power.registerActivity();
            JsonDocument req;
            deserializeJson(req, server.arg("plain"));
            String cmd = req["cmd"] | req["command"] | "";
            cmd.trim();
            if (cmd.length() > 0) {
                processSerialCommand(cmd);
            }
            server.send(200, "application/json", buildStatusJson());
        });

        server.on("/api/clear_history", HTTP_POST, [this]() {
            addCorsHeaders();
            power.registerActivity();
            brain.clearHistory();
            server.send(200, "application/json", "{\"ok\":true,\"message\":\"Historico limpo\"}");
        });
    }

public:
    WifiApiServer(NvsConfigManager& n, LedController& l, DisplayHAL& d, MemoryManager& mem,
                  PomodoroManager& pomo, TouchHeadSensor& touch, ToolsController& t,
                  GeminiBrain& b, SfxSynth& s, CaptivePortalManager& cp, PowerManager& pw,
                  PersonalityManager& pm, AlarmManager& am,
                  volatile RobotState& st, FaceEmotion& emo, RobotTimer& tmr)
        : server(80), nvs(n), led(l), display(d), memory(mem), pomodoro(pomo),
          touchSensor(touch), tools(t), brain(b), sfx(s), portal(cp), power(pw),
          personality(pm), alarms(am), currentState(st), currentEmotion(emo), activeTimer(tmr) {}

    void ensureHttpStarted() {
        if (!routesRegistered) {
            static const char* hdrs[] = {"User-Agent"};
            server.collectHeaders(hdrs, 1);
            server.onBadReq = [this](const char* reason) {
                if (portal.isActive()) {
                    portal.logDebug(String("SOCK_") + reason);
                }
            };
            setupRoutes();
            portal.registerRoutes(server);
            routesRegistered = true;
        }
        server.enableDelay(false);
        server.begin();
        httpStarted = true;
        Serial.println("EVT:LOG=Servidor Web HTTP (Porta 80) ativo e vinculado!");
    }

    void ensureStaServicesStarted() {
        if (staServicesStarted || !nvs.isWifiConnected()) return;
        ensureHttpStarted();
        MDNS.begin("ciborgue");
        MDNS.addService("http", "tcp", 80);
        udp.begin(4210);
        staServicesStarted = true;
        Serial.printf("EVT:LOG=Servidor HTTP Android ativo em http://%s e http://ciborgue.local (UDP 4210)\n",
                      WiFi.localIP().toString().c_str());
    }

    void beginIfWifiConnected() {
        if (!nvs.isWifiConnected()) return;
        ensureStaServicesStarted();
    }

    void handleClient() {
        if (nvs.isWifiConnected() && !staServicesStarted) {
            ensureStaServicesStarted();
        }
        if (httpStarted && (portal.isActive() || nvs.isWifiConnected())) {
            server.handleClient();
        }
        if (!nvs.isWifiConnected() || !staServicesStarted) return;

        int packetSize = udp.parsePacket();
        if (packetSize > 0) {
            char buf[128];
            int len = udp.read(buf, sizeof(buf) - 1);
            if (len > 0) {
                buf[len] = '\0';
                String msg(buf);
                msg.trim();
                if (msg.startsWith("CIBORGUE_DISCOVER")) {
                    String resp = String("{\"device\":\"Ciborgue ESP32-S3\",\"ip\":\"") +
                                  WiFi.localIP().toString() + "\",\"port\":80,\"hostname\":\"ciborgue.local\"}";
                    udp.beginPacket(udp.remoteIP(), udp.remotePort());
                    udp.print(resp);
                    udp.endPacket();
                }
            }
        }
    }
};

#endif // CIBORGUE_WIFI_API_SERVER_H
