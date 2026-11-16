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

extern void processSerialCommand(String cmd);
extern void setRobotState(RobotState newState);
extern void setRobotEmotion(FaceEmotion newEmotion);
extern void sendTelemetryPacket();

class WifiApiServer {
private:
    WebServer server;
    WiFiUDP udp;
    NvsConfigManager& nvs;
    LedController& led;
    DisplayHAL& display;
    MemoryManager& memory;
    PomodoroManager& pomodoro;
    TouchHeadSensor& touchSensor;
    ToolsController& tools;
    GeminiBrain& brain;
    volatile RobotState& currentState;
    FaceEmotion& currentEmotion;
    RobotTimer& activeTimer;
    bool started = false;

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
            doc["timer_rem_s"] = (activeTimer.targetMillis - millis()) / 1000;
            doc["timer_label"] = activeTimer.label;
        } else {
            doc["timer_rem_s"] = 0;
            doc["timer_label"] = "";
        }

        doc["pomo_active"] = pomodoro.isActive();
        doc["pomo_rem_s"]  = pomodoro.getRemainingSec();
        doc["pomo_goal"]   = pomodoro.getGoal();

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
            addCorsHeaders();
            server.send(404, "application/json", "{\"ok\":false,\"error\":\"Endpoint nao encontrado\"}");
        });

        server.on("/api/status", HTTP_GET, [this]() {
            addCorsHeaders();
            server.send(200, "application/json", buildStatusJson());
        });

        server.on("/api/chat", HTTP_POST, [this]() {
            addCorsHeaders();
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
            JsonDocument req;
            deserializeJson(req, server.arg("plain"));
            String emo = req["emotion"] | req["emocao"] | "NEUTRAL";
            setRobotEmotion(stringToEmotion(emo));
            sendTelemetryPacket();
            server.send(200, "application/json", buildStatusJson());
        });

        server.on("/api/state", HTTP_POST, [this]() {
            addCorsHeaders();
            JsonDocument req;
            deserializeJson(req, server.arg("plain"));
            String st = req["state"] | "IDLE";
            setRobotState(stringToState(st));
            sendTelemetryPacket();
            server.send(200, "application/json", buildStatusJson());
        });

        server.on("/api/led", HTTP_POST, [this]() {
            addCorsHeaders();
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
            sendTelemetryPacket();
            server.send(200, "application/json", buildStatusJson());
        });

        server.on("/api/command", HTTP_POST, [this]() {
            addCorsHeaders();
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
            brain.clearHistory();
            server.send(200, "application/json", "{\"ok\":true,\"message\":\"Historico limpo\"}");
        });
    }

public:
    WifiApiServer(NvsConfigManager& n, LedController& l, DisplayHAL& d, MemoryManager& mem,
                  PomodoroManager& pomo, TouchHeadSensor& touch, ToolsController& t,
                  GeminiBrain& b, volatile RobotState& st, FaceEmotion& emo, RobotTimer& tmr)
        : server(80), nvs(n), led(l), display(d), memory(mem), pomodoro(pomo),
          touchSensor(touch), tools(t), brain(b),
          currentState(st), currentEmotion(emo), activeTimer(tmr) {}

    void beginIfWifiConnected() {
        if (started || !nvs.isWifiConnected()) return;
        setupRoutes();
        server.begin();
        MDNS.begin("ciborgue");
        MDNS.addService("http", "tcp", 80);
        udp.begin(4210);
        started = true;
        Serial.printf("EVT:LOG=Servidor HTTP Android ativo em http://%s e http://ciborgue.local (UDP 4210)\n",
                      WiFi.localIP().toString().c_str());
    }

    void handleClient() {
        if (!nvs.isWifiConnected()) return;
        if (!started) {
            beginIfWifiConnected();
        }
        server.handleClient();

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
