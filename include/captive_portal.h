#ifndef CIBORGUE_CAPTIVE_PORTAL_H
#define CIBORGUE_CAPTIVE_PORTAL_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <lwip/sockets.h>
#include <errno.h>
#include <ArduinoJson.h>
#include "config.h"
#include "robot_types.h"
#include "nvs_config.h"
#include "display_hal.h"
#include "sfx_synth.h"

extern void setRobotState(RobotState newState);
extern void notifyUserActivity();
extern void sendTelemetryPacket();
extern void ensureHttpServerReady();

/**
 * Servidor DNS cativo zero-malloc com suporte a EDNS0 (ARCount > 0) e NODATA limpo para AAAA/HTTPS.
 */
class CaptiveDnsServer {
private:
    WiFiUDP udp;
    IPAddress apIp;
    bool running = false;
    uint32_t aReplies = 0;
    uint32_t otherReplies = 0;

public:
    bool start(uint16_t port, const IPAddress& ip) {
        udp.stop();
        apIp = ip;
        running = (udp.begin(port) == 1);
        aReplies = 0;
        otherReplies = 0;
        return running;
    }

    void stop() {
        if (running) {
            udp.stop();
            running = false;
        }
    }

    uint32_t getAReplies() const { return aReplies; }
    uint32_t getOtherReplies() const { return otherReplies; }

    void processNextRequest() {
        if (!running) return;

        for (int iter = 0; iter < 6; iter++) {
            int pktLen = udp.parsePacket();
            if (pktLen <= 0) break;

            uint8_t rx[512];
            if (pktLen < 17 || pktLen > (int)sizeof(rx)) {
                udp.flush();
                continue;
            }

            int n = udp.read(rx, pktLen);
            if (n < 17) continue;

            // QR == 0 (consulta), OPCODE == 0 (padrão), QDCOUNT == 1 (aceita ARCount > 0 / EDNS0)
            bool isQuery = (rx[2] & 0x80) == 0;
            bool isStdOp = (rx[2] & 0x78) == 0;
            bool oneQuestion = (rx[4] == 0 && rx[5] == 1);
            if (!isQuery || !isStdOp || !oneQuestion) {
                continue;
            }

            // Caminha os labels da Question validando limites e rejeitando compressão inesperada na pergunta
            size_t pos = 12;
            bool validName = false;
            while (pos < (size_t)n) {
                uint8_t labelLen = rx[pos];
                if (labelLen == 0) {
                    pos++;
                    validName = true;
                    break;
                }
                if ((labelLen & 0xC0) != 0 || (pos + 1 + labelLen >= (size_t)n)) {
                    validName = false;
                    break;
                }
                pos += 1 + labelLen;
            }
            if (!validName || (pos + 4 > (size_t)n)) {
                continue;
            }

            size_t qEnd = pos + 4;
            uint16_t qType  = ((uint16_t)rx[pos] << 8) | rx[pos + 1];
            uint16_t qClass = ((uint16_t)rx[pos + 2] << 8) | rx[pos + 3];

            uint8_t tx[512];
            if (qEnd > sizeof(tx)) continue;
            memcpy(tx, rx, qEnd);

            tx[2] = (uint8_t)(0x84 | (rx[2] & 0x01)); // QR=1, AA=1, preserva RD
            tx[3] = 0x80;                             // RA=1, RCODE=0 (NOERROR)
            tx[4] = 0x00;
            tx[5] = 0x01;                             // QDCOUNT=1
            tx[8] = tx[9] = tx[10] = tx[11] = 0x00;   // NSCOUNT=0, ARCOUNT=0 (descarta OPT/EDNS0)

            size_t txLen = qEnd;
            if ((qType == 1 || qType == 255) && qClass == 1 && (qEnd + 16 <= sizeof(tx))) {
                tx[6] = 0x00;
                tx[7] = 0x01; // ANCOUNT=1
                const uint8_t ans[16] = {
                    0xC0, 0x0C,             // Ponteiro DNS para QName (offset 12)
                    0x00, 0x01,             // TYPE A
                    0x00, 0x01,             // CLASS IN
                    0x00, 0x00, 0x00, 0x3C, // TTL = 60s
                    0x00, 0x04,             // RDLENGTH = 4 bytes
                    apIp[0], apIp[1], apIp[2], apIp[3]
                };
                memcpy(tx + qEnd, ans, 16);
                txLen = qEnd + 16;
                aReplies++;
            } else {
                tx[6] = 0x00;
                tx[7] = 0x00; // ANCOUNT=0 (NODATA limpo para AAAA/HTTPS)
                otherReplies++;
            }

            udp.beginPacket(udp.remoteIP(), udp.remotePort());
            udp.write(tx, txLen);
            udp.endPacket();
        }
    }
};

static const char PORTAL_HTML_HEAD[] PROGMEM = R"HTML(<!DOCTYPE html><html lang="pt-BR"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Ciborgue Setup</title><style>*{box-sizing:border-box;font-family:sans-serif}body{margin:0;background:#090d16;color:#e5e7eb;padding:12px}.c{max-width:440px;margin:0 auto;background:#111827;border:1px solid #1f2937;border-radius:12px;padding:14px}h1{margin:0 0 4px;font-size:18px;color:#06b6d4}p.s{margin:0 0 10px;font-size:12px;color:#9ca3af}.b{display:inline-block;padding:3px 8px;border-radius:9px;font-size:11px;background:#0f172a;border:1px solid #334155;margin:0 5px 5px 0}.ok{border-color:#10b981;color:#6ee7b7}label{display:block;font-size:12px;color:#cbd5e1;margin:8px 0 3px}input[type=text],input[type=password]{width:100%;padding:9px;border-radius:7px;border:1px solid #334155;background:#0b1120;color:#fff;font-size:14px}input[type=range]{width:100%}.r{display:flex;align-items:center;justify-content:space-between;padding:7px 0;border-bottom:1px solid #1e293b;font-size:13px}button,.btn{display:block;width:100%;padding:10px;border:0;border-radius:8px;font-size:14px;font-weight:700;cursor:pointer;text-align:center;text-decoration:none}.p{background:#06b6d4;color:#041018;margin-top:12px}.sec{background:#1e293b;color:#e2e8f0;border:1px solid #334155;margin:8px 0}.nl{max-height:150px;overflow:auto;border:1px solid #1e293b;border-radius:7px;margin-bottom:6px}.ni{display:flex;justify-content:space-between;padding:8px 10px;cursor:pointer;border-bottom:1px solid #1e293b;font-size:13px}</style></head><body><div class="c"><h1>Ciborgue ESP32-S3</h1><p class="s">Configuração Wi-Fi e IA</p>)HTML";

static const char PORTAL_HTML_TAIL[] PROGMEM = R"HTML(</div><script>async function sw(){const b=document.getElementById('sb'),x=document.getElementById('nl');b.disabled=1;b.textContent='Escaneando...';x.style.display='block';x.innerHTML='<div class="ni">Buscando redes...</div>';try{const r=await fetch('/api/wifi_scan');const ns=await r.json();x.innerHTML=ns.length?'':'<div class="ni">Nenhuma rede.</div>';ns.forEach(n=>{const d=document.createElement('div');d.className='ni';d.innerHTML='<span>'+n.ssid.replace(/</g,'&lt;')+(n.enc?' *':'')+'</span><span>'+n.rssi+' dBm</span>';d.onclick=()=>{document.getElementById('ssid').value=n.ssid;document.getElementById('pass').focus();};x.appendChild(d);});}catch(e){x.innerHTML='<div class="ni">Erro no scan.</div>';}b.disabled=0;b.textContent='Escanear Redes Wi-Fi';}</script></body></html>)HTML";

class CaptivePortalManager {
private:
    NvsConfigManager& nvs;
    DisplayHAL& display;
    SfxSynth& sfx;

    CaptiveDnsServer dnsServer;
    bool active = false;
    bool openedManually = false;
    bool awaitingNewConnection = false;
    bool wifiEventsHooked = false;
    unsigned long pendingConnectAt = 0;
    unsigned long connectedSinceAt = 0;

    static constexpr uint8_t DEBUG_RING_SIZE = 20;
    char debugEvents[DEBUG_RING_SIZE][128] = {{0}};
    uint8_t debugHead = 0;
    uint8_t debugCount = 0;
    portMUX_TYPE debugMux = portMUX_INITIALIZER_UNLOCKED;

    static String uaShort(WebServer& server) {
        String ua = server.header("User-Agent");
        ua.trim();
        if (ua.length() == 0) return "-";
        if (ua.length() > 20) ua = ua.substring(0, 20);
        ua.replace(' ', '_');
        return ua;
    }

    static size_t sendHtmlPage(WebServer& server, const String& html, int& finRes) {
        finRes = -1;
        server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        server.sendHeader("Pragma", "no-cache");
        server.sendHeader("Expires", "-1");
        size_t total = html.length();
        server.setContentLength(total);
        server.send(200, "text/html; charset=utf-8", "");
        WiFiClient cli = server.client();
        if (server.method() == HTTP_HEAD || total == 0) {
            finRes = finishClientResponse(cli, 200);
            return 0;
        }
        cli.setNoDelay(true);
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(html.c_str());
        size_t sent = 0;
        while (sent < total && cli.connected()) {
            size_t chunk = min((size_t)1024, total - sent);
            size_t w = cli.write(ptr + sent, chunk);
            if (w == 0) break;
            sent += w;
            delay(2);
        }
        finRes = finishClientResponse(cli, 250);
        return sent;
    }

    void ensureWifiEventHooks() {
        if (wifiEventsHooked) return;
        wifiEventsHooked = true;
        WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
            if (event == ARDUINO_EVENT_WIFI_AP_START) {
                logDebug("AP_START ip=" + WiFi.softAPIP().toString());
            } else if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
                char buf[64];
                const uint8_t* m = info.wifi_ap_staconnected.mac;
                snprintf(buf, sizeof(buf), "AP_STA_CONN %02X:%02X:%02X:%02X:%02X:%02X",
                         m[0], m[1], m[2], m[3], m[4], m[5]);
                logDebug(buf);
            } else if (event == ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED) {
                IPAddress ip(info.wifi_ap_staipassigned.ip.addr);
                logDebug("AP_STA_IP " + ip.toString());
            } else if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
                char buf[64];
                const uint8_t* m = info.wifi_ap_stadisconnected.mac;
                snprintf(buf, sizeof(buf), "AP_STA_DISC %02X:%02X:%02X:%02X:%02X:%02X",
                         m[0], m[1], m[2], m[3], m[4], m[5]);
                logDebug(buf);
            }
        });
    }

    static String htmlEscape(String s) {
        s.replace("&", "&amp;");
        s.replace("<", "&lt;");
        s.replace(">", "&gt;");
        s.replace("\"", "&quot;");
        return s;
    }

    String buildSetupPage() const {
        String html = FPSTR(PORTAL_HTML_HEAD);

        html += "<div>";
        if (nvs.isWifiConnected()) {
            html += "<span class=\"b ok\">Wi-Fi: " + htmlEscape(nvs.getWifiSsid()) + " (" + WiFi.localIP().toString() + ")</span>";
        } else {
            html += "<span class=\"b\">Wi-Fi: Offline</span>";
        }
        html += nvs.hasGeminiKey()
            ? "<span class=\"b ok\">Key: Salva</span>"
            : "<span class=\"b\">Key: Pendente</span>";
        html += "</div>";

        html += "<button type=\"button\" id=\"sb\" class=\"btn sec\" onclick=\"sw()\">Escanear Redes Wi-Fi</button>";
        html += "<div id=\"nl\" class=\"nl\" style=\"display:none\"></div>";

        html += "<form method=\"POST\" action=\"/save\">";
        html += "<label for=\"ssid\">Rede Wi-Fi (2.4 GHz)</label>";
        html += "<input id=\"ssid\" name=\"ssid\" type=\"text\" value=\"" + htmlEscape(nvs.getWifiSsid()) + "\" placeholder=\"SSID\">";

        html += "<label for=\"pass\">Senha Wi-Fi</label>";
        html += "<input id=\"pass\" name=\"pass\" type=\"password\" placeholder=\"Senha\">";

        html += "<label for=\"key\">Chave Gemini API</label>";
        html += "<input id=\"key\" name=\"key\" type=\"password\" placeholder=\"";
        html += nvs.hasGeminiKey() ? "Já salva (digite só para trocar)" : "AIzaSy...";
        html += "\">";

        html += "<label for=\"city\">Cidade (Clima)</label>";
        html += "<input id=\"city\" name=\"city\" type=\"text\" value=\"" + htmlEscape(nvs.getDefaultCity()) + "\">";

        html += "<label for=\"vol\">Volume SFX (<span id=\"vv\">" + String(nvs.getSfxVolume()) + "</span>%)</label>";
        html += "<input id=\"vol\" name=\"vol\" type=\"range\" min=\"0\" max=\"100\" value=\"" + String(nvs.getSfxVolume()) + "\" oninput=\"document.getElementById('vv').textContent=this.value\">";

        html += "<div class=\"r\"><span>Mudo (SFX)</span><input type=\"checkbox\" name=\"mute\" value=\"1\"";
        if (nvs.isSfxMuted()) html += " checked";
        html += "></div>";

        html += "<div class=\"r\"><span>Chirps Ociosos</span><input type=\"checkbox\" name=\"idle\" value=\"1\"";
        if (nvs.isSfxIdleEnabled()) html += " checked";
        html += "></div>";

        html += "<div class=\"r\"><span>Fast-Path Local</span><input type=\"checkbox\" name=\"fastpath\" value=\"1\"";
        if (nvs.isFastPathEnabled()) html += " checked";
        html += "></div>";

        html += "<button type=\"submit\" class=\"btn p\">Salvar e Conectar</button>";
        html += "</form>";
        html += FPSTR(PORTAL_HTML_TAIL);
        return html;
    }

public:
    CaptivePortalManager(NvsConfigManager& n, DisplayHAL& d, SfxSynth& s)
        : nvs(n), display(d), sfx(s) {}

    bool isActive() const { return active; }

    // Half-close limpo (TCP FIN após drenar TX) evitando TCP RST no teardown do WebServer
    static int finishClientResponse(WiFiClient& cli, uint16_t waitMs = 220) {
        int fd = cli.fd();
        if (fd < 0) return -1;
        lwip_shutdown(fd, SHUT_WR);
        unsigned long start = millis();
        uint8_t sink[64];
        while ((unsigned long)(millis() - start) < waitMs) {
            ssize_t r = lwip_recv(fd, sink, sizeof(sink), MSG_DONTWAIT);
            if (r == 0) {
                return 1;
            }
            if (r < 0 && errno != EWOULDBLOCK && errno != EAGAIN) {
                return -errno;
            }
            delay(2);
        }
        return 0;
    }

    int finishAndLogClient(WiFiClient& cli, const char* tag = nullptr, uint16_t waitMs = 200) {
        int finRes = finishClientResponse(cli, waitMs);
        if (tag && tag[0]) {
            logDebug(String(tag) + " fin=" + String(finRes));
        }
        return finRes;
    }

    void logDebug(const char* msg) {
        char line[128];
        snprintf(line, sizeof(line), "[%lus] %s", millis() / 1000UL, msg ? msg : "");
        portENTER_CRITICAL(&debugMux);
        strncpy(debugEvents[debugHead], line, sizeof(debugEvents[0]) - 1);
        debugEvents[debugHead][sizeof(debugEvents[0]) - 1] = '\0';
        debugHead = (debugHead + 1) % DEBUG_RING_SIZE;
        if (debugCount < DEBUG_RING_SIZE) debugCount++;
        portEXIT_CRITICAL(&debugMux);
        Serial.printf("EVT:LOG=[PORTAL] %s\n", line);
    }

    void logDebug(const String& msg) {
        logDebug(msg.c_str());
    }

    void dumpDebugLog() {
        Serial.printf("EVT:AP_DEBUG=active:%d mode:%d ap_ip:%s sta_n:%d dns_a:%lu dns_o:%lu\n",
                      active ? 1 : 0, (int)WiFi.getMode(),
                      WiFi.softAPIP().toString().c_str(), WiFi.softAPgetStationNum(),
                      (unsigned long)dnsServer.getAReplies(), (unsigned long)dnsServer.getOtherReplies());
        char snapshot[DEBUG_RING_SIZE][128];
        uint8_t count = 0;
        uint8_t head = 0;
        portENTER_CRITICAL(&debugMux);
        count = debugCount;
        head = debugHead;
        memcpy(snapshot, debugEvents, sizeof(snapshot));
        portEXIT_CRITICAL(&debugMux);

        uint8_t start = (count < DEBUG_RING_SIZE) ? 0 : head;
        for (uint8_t i = 0; i < count; i++) {
            uint8_t idx = (start + i) % DEBUG_RING_SIZE;
            Serial.printf("EVT:AP_DEBUG_LINE=%s\n", snapshot[idx]);
        }
    }

    void serveSetupPage(WebServer& server) {
        notifyUserActivity();
        String html = buildSetupPage();
        int finRes = -1;
        size_t sent = sendHtmlPage(server, html, finRes);
        logDebug("HTTP " + String(server.method() == HTTP_HEAD ? "HEAD" : "GET") +
                 " host=" + server.hostHeader() + " uri=" + server.uri() +
                 " sent=" + String(sent) + "/" + String(html.length()) +
                 " fin=" + String(finRes) + " ua=" + uaShort(server));
    }

    void startPortal(bool manual = false) {
        notifyUserActivity();
        openedManually = manual;
        awaitingNewConnection = false;
        connectedSinceAt = 0;

        if (active) {
            nvs.setPortalManual(manual);
            setRobotState(STATE_PORTAL);
            ensureHttpServerReady();
            return;
        }

        ensureWifiEventHooks();
        active = true;
        nvs.setPortalActive(true);
        nvs.setPortalManual(manual);

        WiFi.setSleep(false);
        bool keepStaOnline = (manual && nvs.isWifiConnected());
        if (!keepStaOnline) {
            nvs.stopStaScanning();
            WiFi.mode(WIFI_OFF);
            delay(100);
            WiFi.mode(WIFI_AP);
            delay(100);
        } else {
            WiFi.mode(WIFI_AP_STA);
            delay(100);
        }

        bool apOk = WiFi.softAP(CAPTIVE_AP_SSID, nullptr, 6, 0, 4);
        delay(250);
        WiFi.setTxPower(WIFI_POWER_15dBm);

        IPAddress apIp(192, 168, 4, 1);
        IPAddress apMask(255, 255, 255, 0);
        bool cfgOk = true;
        if (WiFi.softAPIP() != apIp) {
            cfgOk = WiFi.softAPConfig(apIp, apIp, apMask);
            delay(100);
        }

        IPAddress realIp = WiFi.softAPIP();
        bool dnsOk = dnsServer.start(53, realIp);

        ensureHttpServerReady();
        setRobotState(STATE_PORTAL);
        display.setNetBadge("AP");
        display.setSubtitle("Wi-Fi: Ciborgue-Setup (192.168.4.1)");
        sfx.play(SFX_WAKE);

        char initMsg[96];
        snprintf(initMsg, sizeof(initMsg), "start mode=%d apOk=%d cfgOk=%d dnsOk=%d ip=%s",
                 (int)WiFi.getMode(), apOk ? 1 : 0, cfgOk ? 1 : 0, dnsOk ? 1 : 0, realIp.toString().c_str());
        logDebug(initMsg);

        Serial.println("EVT:PORTAL=1");
        Serial.printf("EVT:LOG=Portal Cativo aberto! Conecte no Wi-Fi '%s' -> http://%s\n",
                      CAPTIVE_AP_SSID, realIp.toString().c_str());
        sendTelemetryPacket();
    }

    void stopPortal() {
        if (!active) return;
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        WiFi.setTxPower(WIFI_POWER_19_5dBm);
        WiFi.setAutoReconnect(true);

        active = false;
        openedManually = false;
        awaitingNewConnection = false;
        connectedSinceAt = 0;
        nvs.setPortalActive(false);
        nvs.setPortalManual(false);

        display.setNetBadge(nvs.isWifiConnected() ? "" : "OFFLINE");
        display.setSubtitle("Ciborgue Pronto");
        setRobotState(STATE_IDLE);

        Serial.println("EVT:PORTAL=0");
        Serial.println("EVT:LOG=Portal Cativo encerrado.");
        sendTelemetryPacket();
    }

    void update() {
        if (!active) return;

        dnsServer.processNextRequest();

        if (pendingConnectAt > 0 && (long)(millis() - pendingConnectAt) >= 0) {
            pendingConnectAt = 0;
            awaitingNewConnection = true;
            nvs.triggerAsyncReconnect();
        }

        // Auto-fechamento seguro quando o Wi-Fi STA conectar (mantém portal manual aberto até salvar nova rede)
        if ((!openedManually || awaitingNewConnection) && WiFi.status() == WL_CONNECTED) {
            if (connectedSinceAt == 0) {
                connectedSinceAt = millis();
            } else if (millis() - connectedSinceAt >= 2500UL) {
                Serial.println("EVT:LOG=Wi-Fi STA conectado! Fechando AP Ciborgue-Setup automaticamente...");
                stopPortal();
            }
        } else {
            connectedSinceAt = 0;
        }
    }

    void registerRoutes(WebServer& server) {
        server.on("/", HTTP_ANY, [this, &server]() {
            serveSetupPage(server);
        });

        server.on("/index.html", HTTP_ANY, [this, &server]() {
            serveSetupPage(server);
        });

        server.on("/favicon.ico", HTTP_ANY, [&server]() {
            server.send(204, "text/plain", "");
            WiFiClient cli = server.client();
            finishClientResponse(cli, 120);
        });

        server.on("/ping", HTTP_ANY, [this, &server]() {
            server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
            server.send(200, "text/plain", "CIBORGUE_OK");
            WiFiClient cli = server.client();
            int finRes = finishClientResponse(cli, 200);
            logDebug("PING_HTTP fin=" + String(finRes) + " ua=" + uaShort(server));
        });

        server.on("/api/wifi_scan", HTTP_GET, [this, &server]() {
            notifyUserActivity();
            nvs.setScanInProgress(true);
            bool staWasOff = ((WiFi.getMode() & WIFI_STA) == 0);
            if (staWasOff) {
                WiFi.enableSTA(true);
                delay(120);
            }
            int n = WiFi.scanNetworks(false, false);

            // Ordena índices por RSSI decrescente e remove duplicados
            int indices[24];
            int count = min(n, 24);
            for (int i = 0; i < count; i++) indices[i] = i;
            for (int i = 0; i < count; i++) {
                for (int j = i + 1; j < count; j++) {
                    if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
                        int tmp = indices[i];
                        indices[i] = indices[j];
                        indices[j] = tmp;
                    }
                }
            }

            JsonDocument doc;
            JsonArray arr = doc.to<JsonArray>();
            for (int i = 0; i < count; i++) {
                String ssid = WiFi.SSID(indices[i]);
                if (ssid.length() == 0) continue;
                bool dup = false;
                for (JsonObject existing : arr) {
                    if (existing["ssid"] == ssid) {
                        dup = true;
                        break;
                    }
                }
                if (dup) continue;
                JsonObject obj = arr.add<JsonObject>();
                obj["ssid"] = ssid;
                obj["rssi"] = WiFi.RSSI(indices[i]);
                obj["enc"]  = (WiFi.encryptionType(indices[i]) != WIFI_AUTH_OPEN);
            }
            WiFi.scanDelete();
            if (staWasOff && !awaitingNewConnection && !nvs.isWifiConnected()) {
                WiFi.enableSTA(false);
            }
            nvs.setScanInProgress(false);

            String out;
            serializeJson(doc, out);
            server.send(200, "application/json", out);
            WiFiClient cli = server.client();
            finishClientResponse(cli, 150);
        });

        server.on("/save", HTTP_POST, [this, &server]() {
            notifyUserActivity();
            String ssid = server.arg("ssid");
            String pass = server.arg("pass");
            String key  = server.arg("key");
            String city = server.arg("city");
            ssid.trim();
            pass.trim();
            key.trim();
            city.trim();

            if (ssid.length() > 0) {
                nvs.saveWifi(ssid, pass);
            }
            if (key.length() > 10) {
                nvs.saveGeminiKey(key);
            }
            if (city.length() > 0) {
                nvs.saveDefaultCity(city);
            }
            if (server.hasArg("vol")) {
                nvs.saveSfxVolume(server.arg("vol").toInt());
            }
            nvs.saveSfxMuted(server.hasArg("mute"));
            nvs.saveSfxIdleEnabled(server.hasArg("idle"));
            nvs.saveFastPathEnabled(server.hasArg("fastpath"));

            sfx.play(SFX_HAPPY);
            if (ssid.length() > 0) {
                pendingConnectAt = millis() + 1000UL;
            }
            sendTelemetryPacket();

            String resp = FPSTR(PORTAL_HTML_HEAD);
            resp += "<h2 style=\"color:#10b981;margin-top:0\">Configuração Salva!</h2>";
            if (ssid.length() > 0) {
                resp += "<p class=\"s\">Tentando conectar à rede <b>" + htmlEscape(ssid) +
                        "</b>. Assim que conectar, o ponto de acesso <b>Ciborgue-Setup</b> será fechado automaticamente.</p>";
            } else {
                resp += "<p class=\"s\">Preferências gravadas com sucesso na memória Flash NVS.</p>";
            }
            resp += "<a class=\"btn sec\" href=\"/\">Voltar</a>";
            resp += FPSTR(PORTAL_HTML_TAIL);
            int finRes = -1;
            size_t sent = sendHtmlPage(server, resp, finRes);
            logDebug("SAVE ssid=" + (ssid.length() > 0 ? ssid : String("-")) +
                     " sent=" + String(sent) + " fin=" + String(finRes));
        });

        auto sendProbeRedirect = [this, &server]() {
            if (active) {
                String realIp = WiFi.softAPIP().toString();
                if (server.hostHeader().startsWith(realIp)) {
                    serveSetupPage(server);
                    return;
                }
                server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
                String target = String("http://") + realIp + "/";
                server.sendHeader("Location", target, true);
                server.send(302, "text/html",
                            String("<html><body><a href='") + target + "'>Setup</a></body></html>");
                WiFiClient cli = server.client();
                int finRes = finishClientResponse(cli, 200);
                logDebug("PROBE uri=" + server.uri() + " host=" + server.hostHeader() +
                         " fin=" + String(finRes) + " ua=" + uaShort(server));
            } else {
                server.send(204, "text/plain", "");
            }
        };

        server.on("/generate_204", HTTP_ANY, sendProbeRedirect);
        server.on("/gen_204", HTTP_ANY, sendProbeRedirect);
        server.on("/hotspot-detect.html", HTTP_ANY, sendProbeRedirect);
        server.on("/ncsi.txt", HTTP_ANY, sendProbeRedirect);
        server.on("/connecttest.txt", HTTP_ANY, sendProbeRedirect);
        server.on("/fwlink", HTTP_ANY, sendProbeRedirect);
        server.on("/canonical.html", HTTP_ANY, sendProbeRedirect);
        server.on("/redirect", HTTP_ANY, sendProbeRedirect);
        server.on("/success.txt", HTTP_ANY, sendProbeRedirect);
    }
};

#endif // CIBORGUE_CAPTIVE_PORTAL_H
