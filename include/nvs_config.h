#ifndef CIBORGUE_NVS_CONFIG_H
#define CIBORGUE_NVS_CONFIG_H

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <time.h>
#include "config.h"
#include "robot_types.h"

class NvsConfigManager {
private:
    Preferences prefs;
    String wifiSsid;
    String wifiPass;
    String geminiKey;
    String defaultCity;

    uint8_t tftBrightness = TFT_AWAKE_BRIGHTNESS;
    bool bleEnabled = true;
    uint8_t sfxVolume       = 75;
    bool    sfxMuted        = false;
    bool    sfxIdleEnabled  = true;
    bool    fastPathEnabled = true;

    bool ntpConfigured          = false;
    bool portalActiveFlag       = false;
    bool portalManualFlag       = false;
    bool scanInProgress         = false;
    bool apiReachable           = true;
    bool lastWifiConnected      = false;
    bool pendingTransition      = false;
    bool transitionNowConnected = false;

    unsigned long lastWifiAttempt = 0;
    unsigned long backoffMs       = 10000UL;
    unsigned long staProbeStopAt  = 0;

public:
    void begin() {
        prefs.begin("ciborgue", false);
        wifiSsid        = prefs.getString("wifi_ssid", "");
        wifiPass        = prefs.getString("wifi_pass", "");
        geminiKey       = prefs.getString("gemini_key", "");
        defaultCity     = prefs.getString("city", "Sao Paulo");
        tftBrightness   = prefs.getUChar("tft_bri", TFT_AWAKE_BRIGHTNESS);
        bleEnabled      = prefs.getBool("ble_enabled", true);
        sfxVolume       = (uint8_t)constrain((int)prefs.getUChar("sfx_vol", 75), 0, 100);
        sfxMuted        = prefs.getBool("sfx_mute", false);
        sfxIdleEnabled  = prefs.getBool("sfx_idle", true);
        fastPathEnabled = prefs.getBool("fast_path", true);
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

    void saveTftBrightness(uint8_t level) {
        if (tftBrightness == level) return;
        tftBrightness = level;
        if (!prefs.begin("ciborgue", false)) return;
        prefs.putUChar("tft_bri", level);
        prefs.end();
    }

    void saveBleEnabled(bool enabled) {
        if (bleEnabled == enabled) return;
        bleEnabled = enabled;
        if (!prefs.begin("ciborgue", false)) return;
        prefs.putBool("ble_enabled", enabled);
        prefs.end();
    }

    uint8_t getTftBrightness() const { return tftBrightness; }
    bool isBleEnabled() const { return bleEnabled; }

    void saveSfxVolume(int vol) {
        sfxVolume = (uint8_t)constrain(vol, 0, 100);
        prefs.begin("ciborgue", false);
        prefs.putUChar("sfx_vol", sfxVolume);
        prefs.end();
    }

    void saveSfxMuted(bool muted) {
        sfxMuted = muted;
        prefs.begin("ciborgue", false);
        prefs.putBool("sfx_mute", sfxMuted);
        prefs.end();
    }

    void saveSfxIdleEnabled(bool enabled) {
        sfxIdleEnabled = enabled;
        prefs.begin("ciborgue", false);
        prefs.putBool("sfx_idle", sfxIdleEnabled);
        prefs.end();
    }

    void saveFastPathEnabled(bool enabled) {
        fastPathEnabled = enabled;
        prefs.begin("ciborgue", false);
        prefs.putBool("fast_path", fastPathEnabled);
        prefs.end();
    }

    String getWifiSsid() const      { return wifiSsid; }
    String getGeminiKey() const     { return geminiKey; }
    String getDefaultCity() const   { return defaultCity; }
    uint8_t getSfxVolume() const    { return sfxVolume; }
    bool isSfxMuted() const         { return sfxMuted; }
    bool isSfxIdleEnabled() const   { return sfxIdleEnabled; }
    bool isFastPathEnabled() const  { return fastPathEnabled; }

    bool hasWifiCredentials() const { return wifiSsid.length() > 0; }
    bool hasGeminiKey() const       { return geminiKey.length() > 10; }
    bool isWifiConnected() const    { return WiFi.status() == WL_CONNECTED; }
    bool isPortalActive() const     { return portalActiveFlag; }
    bool isApiReachable() const     { return apiReachable; }

    void setPortalActive(bool v)    { portalActiveFlag = v; }
    void setPortalManual(bool v)    { portalManualFlag = v; }
    void setScanInProgress(bool v)  { scanInProgress = v; }
    void setApiReachable(bool v)    { apiReachable = v; }

    void stopStaScanning() {
        staProbeStopAt = 0;
        WiFi.setAutoReconnect(false);
        WiFi.disconnect(false, false);
    }

    NetMode getNetMode() const {
        if (portalActiveFlag) return NET_PORTAL_AP;
        if (!isWifiConnected()) return NET_OFFLINE;
        return apiReachable ? NET_ONLINE : NET_WIFI_ONLY;
    }

    void triggerAsyncReconnect() {
        if (!hasWifiCredentials()) return;
        backoffMs = 10000UL;
        staProbeStopAt = 0;
        lastWifiAttempt = millis();
        WiFi.mode(portalActiveFlag ? WIFI_AP_STA : WIFI_STA);
        WiFi.setAutoReconnect(true);
        WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
    }

    bool connectWifi(unsigned long timeoutMs = WIFI_BOOT_CONNECT_TIMEOUT_MS) {
        if (!hasWifiCredentials()) {
            Serial.println("EVT:LOG=Sem credenciais Wi-Fi salvas na NVS.");
            return false;
        }
        WiFi.mode(portalActiveFlag ? WIFI_AP_STA : WIFI_STA);
        WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
        Serial.printf("EVT:LOG=Conectando ao Wi-Fi '%s'...\n", wifiSsid.c_str());

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
            delay(150);
        }

        if (WiFi.status() == WL_CONNECTED) {
            backoffMs = 10000UL;
            lastWifiConnected = true;
            apiReachable = true;
            Serial.printf("EVT:LOG=Wi-Fi conectado! IP: %s (RSSI: %d dBm)\n",
                          WiFi.localIP().toString().c_str(), WiFi.RSSI());
            syncNtpTime();
            return true;
        } else {
            lastWifiConnected = false;
            lastWifiAttempt = millis();
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

    bool consumeWifiTransition(bool& nowConnected) {
        if (!pendingTransition) return false;
        pendingTransition = false;
        nowConnected = transitionNowConnected;
        return true;
    }

    void loopMaintainWifi() {
        bool connectedNow = (WiFi.status() == WL_CONNECTED);
        if (connectedNow != lastWifiConnected) {
            lastWifiConnected = connectedNow;
            pendingTransition = true;
            transitionNowConnected = connectedNow;
            if (connectedNow) {
                backoffMs = 10000UL;
                apiReachable = true;
                syncNtpTime();
            }
        }

        if (connectedNow) {
            staProbeStopAt = 0;
            if (!ntpConfigured) syncNtpTime();
            return;
        }

        if (staProbeStopAt > 0 && (long)(millis() - staProbeStopAt) >= 0) {
            stopStaScanning();
            if (portalActiveFlag && !isWifiConnected()) {
                WiFi.mode(WIFI_AP);
            }
        }

        if (hasWifiCredentials() && !scanInProgress) {
            unsigned long now = millis();
            if (portalActiveFlag) {
                // Nunca varre canais STA enquanto houver cliente no SoftAP (e dá cooldown de 2 min) ou em portal manual
                if (WiFi.softAPgetStationNum() > 0) {
                    lastWifiAttempt = now;
                    return;
                }
                if (portalManualFlag) {
                    return;
                }
                if (now - lastWifiAttempt >= 120000UL) {
                    lastWifiAttempt = now;
                    if (WiFi.getMode() != WIFI_AP_STA) WiFi.mode(WIFI_AP_STA);
                    WiFi.setAutoReconnect(false);
                    WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
                    staProbeStopAt = now + 6000UL;
                }
            } else if (now - lastWifiAttempt >= backoffMs) {
                lastWifiAttempt = now;
                if (WiFi.getMode() != WIFI_STA) {
                    WiFi.mode(WIFI_STA);
                }
                WiFi.setAutoReconnect(true);
                WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
                backoffMs = min(60000UL, backoffMs * 2UL);
            }
        }
    }
};

#endif // CIBORGUE_NVS_CONFIG_H
