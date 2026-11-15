#ifndef CIBORGUE_NVS_CONFIG_H
#define CIBORGUE_NVS_CONFIG_H

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <time.h>
#include "config.h"

class NvsConfigManager {
private:
    Preferences prefs;
    String wifiSsid;
    String wifiPass;
    String geminiKey;
    String defaultCity;
    bool ntpConfigured = false;
    unsigned long lastWifiAttempt = 0;

public:
    void begin() {
        prefs.begin("ciborgue", false);
        wifiSsid    = prefs.getString("wifi_ssid", "");
        wifiPass    = prefs.getString("wifi_pass", "");
        geminiKey   = prefs.getString("gemini_key", "");
        defaultCity = prefs.getString("city", "Sao Paulo");
        prefs.end();
    }

    void saveWifi(const String& ssid, const String& pass) {
        wifiSsid = ssid;
        wifiPass = pass;
        wifiSsid.trim();
        wifiPass.trim();
        prefs.begin("ciborgue", false);
        prefs.putString("wifi_ssid", wifiSsid);
        prefs.putString("wifi_pass", wifiPass);
        prefs.end();
    }

    void saveGeminiKey(const String& key) {
        geminiKey = key;
        geminiKey.trim();
        prefs.begin("ciborgue", false);
        prefs.putString("gemini_key", geminiKey);
        prefs.end();
    }

    void saveDefaultCity(const String& city) {
        defaultCity = city;
        defaultCity.trim();
        prefs.begin("ciborgue", false);
        prefs.putString("city", defaultCity);
        prefs.end();
    }

    String getWifiSsid() const { return wifiSsid; }
    String getGeminiKey() const { return geminiKey; }
    String getDefaultCity() const { return defaultCity; }
    bool hasWifiCredentials() const { return wifiSsid.length() > 0; }
    bool hasGeminiKey() const { return geminiKey.length() > 10; }
    bool isWifiConnected() const { return WiFi.status() == WL_CONNECTED; }

    bool connectWifi(unsigned long timeoutMs = 8000) {
        if (!hasWifiCredentials()) {
            Serial.println("EVT:LOG=Sem credenciais Wi-Fi salvas na NVS.");
            return false;
        }
        WiFi.mode(WIFI_STA);
        WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
        Serial.printf("EVT:LOG=Conectando ao Wi-Fi '%s'...\n", wifiSsid.c_str());

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
            delay(150);
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("EVT:LOG=Wi-Fi conectado! IP: %s (RSSI: %d dBm)\n",
                          WiFi.localIP().toString().c_str(), WiFi.RSSI());
            syncNtpTime();
            return true;
        } else {
            Serial.println("EVT:LOG=Falha ao conectar ao Wi-Fi (verifique SSID/Senha).");
            return false;
        }
    }

    void syncNtpTime() {
        if (WiFi.status() == WL_CONNECTED) {
            configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER_1, NTP_SERVER_2);
            ntpConfigured = true;
        }
    }

    void loopMaintainWifi() {
        if (hasWifiCredentials() && WiFi.status() != WL_CONNECTED) {
            unsigned long now = millis();
            if (now - lastWifiAttempt > 20000) {
                lastWifiAttempt = now;
                WiFi.disconnect();
                WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
            }
        } else if (WiFi.status() == WL_CONNECTED && !ntpConfigured) {
            syncNtpTime();
        }
    }
};

#endif // CIBORGUE_NVS_CONFIG_H
