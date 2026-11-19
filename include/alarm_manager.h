#ifndef CIBORGUE_ALARM_MANAGER_H
#define CIBORGUE_ALARM_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <vector>
#include <time.h>
#include "config.h"
#include "robot_types.h"
#include "led_controller.h"
#include "display_hal.h"
#include "sfx_synth.h"

extern void setRobotState(RobotState newState);
extern void setRobotEmotion(FaceEmotion newEmotion);
extern void notifyUserActivity();
extern void sendTelemetryPacket();

/**
 * Gerencia até 6 despertadores recorrentes persistidos na Flash NVS ("cib_alm") com disparo NTP.
 */
class AlarmManager {
private:
    Preferences prefs;
    LedController& led;
    DisplayHAL& display;
    SfxSynth& sfx;
    volatile RobotState& currentState;
    FaceEmotion& currentEmotion;

    std::vector<AlarmItem> alarms;
    uint8_t nextId = 1;
    bool ringing = false;
    unsigned long ringingStartMs = 0;
    unsigned long lastRingSfxMs = 0;
    unsigned long lastCheckMs = 0;
    long lastTriggeredKey = -1;

    static String formatHHMM(uint8_t h, uint8_t m) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)h, (unsigned)m);
        return String(buf);
    }

    static String maskToPt(uint8_t mask) {
        if (mask == 0)    return "único";
        if (mask == 0x7F) return "todos os dias";
        if (mask == 0x3E) return "seg a sex";
        if (mask == 0x41) return "fim de semana";
        return "recorrente";
    }

    void syncDisplayHint() const {
        display.setNextAlarmHint(getNextAlarmHint());
    }

    void saveToNvs() {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (const auto& a : alarms) {
            JsonObject o = arr.add<JsonObject>();
            o["id"]   = a.id;
            o["h"]    = a.hour;
            o["m"]    = a.minute;
            o["mask"] = a.daysMask;
            o["en"]   = a.enabled;
            o["lbl"]  = a.label;
        }
        String out;
        serializeJson(doc, out);

        prefs.begin("cib_alm", false);
        prefs.putString("alarms", out);
        prefs.putUChar("next_id", nextId);
        prefs.end();

        syncDisplayHint();
        emitAlarmsPacket();
    }

public:
    AlarmManager(LedController& l, DisplayHAL& d, SfxSynth& s,
                 volatile RobotState& st, FaceEmotion& emo)
        : led(l), display(d), sfx(s), currentState(st), currentEmotion(emo) {}

    void begin() {
        prefs.begin("cib_alm", true);
        String raw = prefs.getString("alarms", "[]");
        nextId = prefs.getUChar("next_id", 1);
        prefs.end();

        alarms.clear();
        JsonDocument doc;
        if (!deserializeJson(doc, raw) && doc.is<JsonArray>()) {
            for (JsonObject o : doc.as<JsonArray>()) {
                if ((int)alarms.size() >= ALARM_MAX_SLOTS) break;
                AlarmItem item;
                item.id       = o["id"] | nextId++;
                item.hour     = (uint8_t)constrain((int)(o["h"] | 7), 0, 23);
                item.minute   = (uint8_t)constrain((int)(o["m"] | 0), 0, 59);
                item.daysMask = o["mask"] | 0x7F;
                item.enabled  = o["en"] | true;
                item.label    = o["lbl"] | "Alarme";
                alarms.push_back(item);
                if (item.id >= nextId) nextId = item.id + 1;
            }
        }
        syncDisplayHint();
    }

    bool isRinging() const { return ringing; }
    int getTotalCount() const { return (int)alarms.size(); }

    String getNextAlarmHint() const {
        for (const auto& a : alarms) {
            if (a.enabled) {
                return formatHHMM(a.hour, a.minute);
            }
        }
        return "";
    }

    String addAlarm(int hour, int minute, uint8_t daysMask, const String& label) {
        if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
            return "Horário inválido para o alarme (use hora 0-23 e minuto 0-59).";
        }
        if ((int)alarms.size() >= ALARM_MAX_SLOTS) {
            return String("Limite de ") + String(ALARM_MAX_SLOTS) + " alarmes atingido. Remova um antes de criar outro.";
        }
        AlarmItem item;
        item.id = nextId++;
        if (nextId == 0) nextId = 1;
        item.hour = (uint8_t)hour;
        item.minute = (uint8_t)minute;
        item.daysMask = daysMask;
        item.enabled = true;
        item.label = label;
        item.label.trim();
        if (item.label.length() == 0) item.label = "Despertador";
        alarms.push_back(item);
        saveToNvs();
        sfx.play(SFX_WAKE);
        sendTelemetryPacket();

        return String("Alarme #") + String(item.id) + " salvo na Flash NVS para " +
               formatHHMM(item.hour, item.minute) + " (" + maskToPt(daysMask) + "): " + item.label + ".";
    }

    String removeAlarm(int id) {
        for (auto it = alarms.begin(); it != alarms.end(); ++it) {
            if ((int)it->id == id) {
                String t = formatHHMM(it->hour, it->minute);
                alarms.erase(it);
                saveToNvs();
                sendTelemetryPacket();
                return "Alarme das " + t + " removido.";
            }
        }
        int idx = id - 1;
        if (idx >= 0 && idx < (int)alarms.size()) {
            String t = formatHHMM(alarms[idx].hour, alarms[idx].minute);
            alarms.erase(alarms.begin() + idx);
            saveToNvs();
            sendTelemetryPacket();
            return "Alarme das " + t + " removido.";
        }
        return "Alarme não encontrado para remoção.";
    }

    String toggleAlarm(int id, bool enabled) {
        for (auto& a : alarms) {
            if ((int)a.id == id) {
                a.enabled = enabled;
                saveToNvs();
                sendTelemetryPacket();
                return String("Alarme #") + String(id) + " (" + formatHHMM(a.hour, a.minute) + ") " +
                       (enabled ? "ativado." : "desativado.");
            }
        }
        return "Alarme não localizado.";
    }

    String clearAlarms() {
        alarms.clear();
        if (ringing) stopRinging();
        saveToNvs();
        sendTelemetryPacket();
        return "Todos os alarmes foram apagados da Flash NVS.";
    }

    String listAlarmsSummary() const {
        if (alarms.empty()) {
            return "Você não tem nenhum alarme salvo na NVS.";
        }
        String res = String("Alarmes configurados (") + String(alarms.size()) + "): ";
        for (size_t i = 0; i < alarms.size(); i++) {
            const auto& a = alarms[i];
            if (i > 0) res += "; ";
            res += String("#") + String(a.id) + " às " + formatHHMM(a.hour, a.minute) +
                   " (" + (a.enabled ? "ativo" : "pausado") + " - " + a.label + ")";
        }
        res += ".";
        return res;
    }

    String getAlarmsJson() const {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (const auto& a : alarms) {
            JsonObject o = arr.add<JsonObject>();
            o["id"]       = a.id;
            o["hour"]     = a.hour;
            o["minute"]   = a.minute;
            o["time"]     = formatHHMM(a.hour, a.minute);
            o["daysMask"] = a.daysMask;
            o["enabled"]  = a.enabled;
            o["label"]    = a.label;
        }
        String out;
        serializeJson(doc, out);
        return out;
    }

    void emitAlarmsPacket() const {
        Serial.printf("EVT:ALARMS=%s\n", getAlarmsJson().c_str());
    }

    void stopRinging() {
        if (!ringing) return;
        ringing = false;
        display.clearAlarmRingWidget();
        Serial.println("EVT:WIDGET={\"type\":\"NONE\"}");
        if (currentState == STATE_ALERT) {
            setRobotState(STATE_IDLE);
        }
        setRobotEmotion(EMO_NEUTRAL);
        led.setStateMode();
        emitAlarmsPacket();
        sendTelemetryPacket();
        Serial.println("EVT:LOG=Alarme silenciado.");
    }

    void update() {
        unsigned long now = millis();

        if (ringing) {
            if ((now - lastRingSfxMs) >= ALARM_RING_REPEAT_MS) {
                lastRingSfxMs = now;
                sfx.play(SFX_ALERT);
            }
            if ((now - ringingStartMs) >= ALARM_AUTO_STOP_MS) {
                stopRinging();
            }
            return;
        }

        if (alarms.empty() || (now - lastCheckMs) < 1000UL) return;
        lastCheckMs = now;

        struct tm ti;
        if (!getLocalTime(&ti, 0) || ti.tm_year < 100) return;

        long curKey = ((long)ti.tm_yday * 1440L) + ((long)ti.tm_hour * 60L) + (long)ti.tm_min;
        if (curKey == lastTriggeredKey) return;

        bool modified = false;
        for (auto& a : alarms) {
            if (!a.enabled) continue;
            if ((int)a.hour != ti.tm_hour || (int)a.minute != ti.tm_min) continue;
            bool dayMatch = (a.daysMask == 0) || ((a.daysMask & (1 << ti.tm_wday)) != 0);
            if (!dayMatch) continue;

            lastTriggeredKey = curKey;
            ringing = true;
            ringingStartMs = now;
            lastRingSfxMs = now;

            if (a.daysMask == 0) {
                a.enabled = false;
                modified = true;
            }

            notifyUserActivity();
            String hhmm = formatHHMM(a.hour, a.minute);
            setRobotState(STATE_ALERT);
            setRobotEmotion(EMO_SURPRISED);
            display.showAlarmRingWidget(hhmm, a.label);
            sfx.play(SFX_ALERT);

            JsonDocument wDoc;
            wDoc["type"]  = "ALARM_RING";
            wDoc["id"]    = a.id;
            wDoc["time"]  = hhmm;
            wDoc["label"] = a.label;
            String wOut;
            serializeJson(wDoc, wOut);
            Serial.printf("EVT:WIDGET=%s\n", wOut.c_str());
            Serial.printf("EVT:SAY=Atenção! Alarme das %s: %s!\n", hhmm.c_str(), a.label.c_str());
            sendTelemetryPacket();
            break;
        }

        if (modified) {
            saveToNvs();
        }
    }
};

#endif // CIBORGUE_ALARM_MANAGER_H
