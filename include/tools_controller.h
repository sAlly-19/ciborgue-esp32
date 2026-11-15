#ifndef CIBORGUE_TOOLS_CONTROLLER_H
#define CIBORGUE_TOOLS_CONTROLLER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "config.h"
#include "robot_types.h"
#include "led_controller.h"
#include "display_hal.h"
#include "nvs_config.h"

class ToolsController {
private:
    LedController& led;
    DisplayHAL& display;
    NvsConfigManager& nvs;
    FaceEmotion& currentEmotion;
    RobotTimer& activeTimer;

    String urlEncode(const String& str) {
        String encoded = "";
        char c;
        for (size_t i = 0; i < str.length(); i++) {
            c = str.charAt(i);
            if (isalnum(c) || c == '-' || c == '_' || c == '.') {
                encoded += c;
            } else if (c == ' ') {
                encoded += "%20";
            } else {
                char buf[4];
                snprintf(buf, sizeof(buf), "%%%02X", (uint8_t)c);
                encoded += buf;
            }
        }
        return encoded;
    }

    const char* weatherCodeToPt(int code) {
        if (code == 0) return "céu limpo";
        if (code == 1 || code == 2) return "parcialmente nublado";
        if (code == 3) return "encoberto";
        if (code >= 45 && code <= 48) return "neblina";
        if (code >= 51 && code <= 57) return "garoa leve";
        if (code >= 61 && code <= 67) return "chuva";
        if (code >= 71 && code <= 77) return "neve";
        if (code >= 80 && code <= 82) return "pancadas de chuva";
        if (code >= 95) return "tempestade com trovoadas";
        return "condição variável";
    }

public:
    ToolsController(LedController& l, DisplayHAL& d, NvsConfigManager& n, FaceEmotion& emo, RobotTimer& tmr)
        : led(l), display(d), nvs(n), currentEmotion(emo), activeTimer(tmr) {}

    String toolMudarExpressao(const String& emocaoStr) {
        currentEmotion = stringToEmotion(emocaoStr);
        display.setEmotion(currentEmotion);
        Serial.printf("EVT:EMOTION=%s\n", emotionToString(currentEmotion));
        return String("Expressão do rosto alterada para ") + emotionToString(currentEmotion) + ".";
    }

    String toolMudarCorLed(const String& cor, const String& efeito = "solido") {
        return led.setNamedColor(cor, efeito);
    }

    String toolObterHoraEData() {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo, 500)) {
            unsigned long upSec = millis() / 1000;
            return String("Relógio NTP ainda não sincronizado. O robô está ligado há ") + String(upSec) + " segundos.";
        }
        const char* diasSemana[] = {"domingo", "segunda-feira", "terça-feira", "quarta-feira", "quinta-feira", "sexta-feira", "sábado"};
        char buffer[140];
        snprintf(buffer, sizeof(buffer),
                 "Agora são %02d:%02d (%s, dia %02d/%02d/%04d).",
                 timeinfo.tm_hour, timeinfo.tm_min,
                 diasSemana[timeinfo.tm_wday],
                 timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
        return String(buffer);
    }

    String toolDefinirTimer(int segundos, const String& lembrete) {
        if (segundos <= 0) segundos = 5;
        if (segundos > 86400) segundos = 86400;
        activeTimer.active = true;
        activeTimer.durationSec = segundos;
        activeTimer.targetMillis = millis() + ((unsigned long)segundos * 1000UL);
        activeTimer.label = lembrete.length() > 0 ? lembrete : "Tempo esgotado!";
        Serial.printf("EVT:TIMER=%d,%s\n", segundos, activeTimer.label.c_str());

        if (segundos >= 60) {
            int min = segundos / 60;
            int rem = segundos % 60;
            return String("Timer configurado para ") + String(min) + " minuto(s) e " + String(rem) + " segundo(s): " + activeTimer.label;
        }
        return String("Timer configurado para ") + String(segundos) + " segundos: " + activeTimer.label;
    }

    String toolStatusHardware() {
        float tempC = temperatureRead();
        uint32_t freeHeapKB = ESP.getFreeHeap() / 1024;
        uint32_t freePsramKB = ESP.getFreePsram() / 1024;
        unsigned long uptimeSec = millis() / 1000;
        int rssi = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
        String ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "desconectado";

        char buf[260];
        snprintf(buf, sizeof(buf),
                 "Chip ESP32-S3 a %.1f graus Celsius, Wi-Fi RSSI %d dBm (IP %s), Memória RAM livre %u KB, PSRAM livre %u KB, ligado há %lu segundos.",
                 tempC, rssi, ip.c_str(), freeHeapKB, freePsramKB, uptimeSec);
        return String(buf);
    }

    String toolObterClima(String cidade) {
        cidade.trim();
        if (cidade.length() == 0) cidade = nvs.getDefaultCity();
        if (WiFi.status() != WL_CONNECTED) {
            return "Não foi possível consultar o clima porque o Wi-Fi do ESP32 está desconectado.";
        }

        HTTPClient http;
        String geoUrl = "http://geocoding-api.open-meteo.com/v1/search?name=" + urlEncode(cidade) + "&count=1&language=pt&format=json";
        http.begin(geoUrl);
        http.setTimeout(4000);
        int code = http.GET();
        if (code != 200) {
            http.end();
            return "Não consegui localizar as coordenadas da cidade " + cidade + ".";
        }

        JsonDocument geoDoc;
        DeserializationError err = deserializeJson(geoDoc, http.getString());
        http.end();

        if (err || !geoDoc["results"].is<JsonArray>() || geoDoc["results"].size() == 0) {
            return "Cidade '" + cidade + "' não encontrada no serviço meteorológico.";
        }

        float lat = geoDoc["results"][0]["latitude"] | 0.0f;
        float lon = geoDoc["results"][0]["longitude"] | 0.0f;
        String nomeCidade = geoDoc["results"][0]["name"] | cidade;

        String weatherUrl = String("http://api.open-meteo.com/v1/forecast?latitude=") + String(lat, 4) +
                            "&longitude=" + String(lon, 4) +
                            "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code&timezone=auto";
        http.begin(weatherUrl);
        http.setTimeout(4000);
        code = http.GET();
        if (code != 200) {
            http.end();
            return "Erro ao consultar o clima atual para " + nomeCidade + ".";
        }

        JsonDocument wDoc;
        err = deserializeJson(wDoc, http.getString());
        http.end();
        if (err) {
            return "Falha ao processar os dados meteorológicos.";
        }

        float temp = wDoc["current"]["temperature_2m"] | 0.0f;
        float sens = wDoc["current"]["apparent_temperature"] | 0.0f;
        int umid   = wDoc["current"]["relative_humidity_2m"] | 0;
        int wCode  = wDoc["current"]["weather_code"] | 0;

        char res[220];
        snprintf(res, sizeof(res),
                 "Clima em %s: %.1f°C (sensação de %.1f°C), umidade de %d%% e %s.",
                 nomeCidade.c_str(), temp, sens, umid, weatherCodeToPt(wCode));
        return String(res);
    }

    String executeToolCall(const String& fnName, JsonObjectConst args) {
        Serial.printf("EVT:LOG=Executando Tool C++ no ESP32: %s\n", fnName.c_str());

        if (fnName == "mudar_expressao_rosto") {
            String emo = args["emocao"] | "happy";
            return toolMudarExpressao(emo);
        } else if (fnName == "mudar_cor_led") {
            String cor = args["cor"] | "azul";
            String efeito = args["efeito"] | "solido";
            return toolMudarCorLed(cor, efeito);
        } else if (fnName == "obter_hora_e_data") {
            return toolObterHoraEData();
        } else if (fnName == "definir_timer") {
            int seg = args["segundos"] | 10;
            String lemb = args["lembrete"] | "Tempo finalizado!";
            return toolDefinirTimer(seg, lemb);
        } else if (fnName == "status_hardware") {
            return toolStatusHardware();
        } else if (fnName == "obter_clima") {
            String cid = args["cidade"] | nvs.getDefaultCity();
            return toolObterClima(cid);
        }
        return "Ferramenta desconhecida: " + fnName;
    }

    void populateToolsSchema(JsonArray toolsArray) {
        JsonObject toolObj = toolsArray.add<JsonObject>();
        JsonArray decls = toolObj["function_declarations"].to<JsonArray>();

        // 1. mudar_expressao_rosto
        {
            JsonObject fn = decls.add<JsonObject>();
            fn["name"] = "mudar_expressao_rosto";
            fn["description"] = "Muda a expressão dos olhos na tela 296x240 do robô.";
            JsonObject params = fn["parameters"].to<JsonObject>();
            params["type"] = "OBJECT";
            JsonObject props = params["properties"].to<JsonObject>();
            props["emocao"]["type"] = "STRING";
            props["emocao"]["description"] = "Emoção: feliz, triste, bravo, surpreso, piscadinha, apaixonado, sonolento ou neutro.";
            JsonArray req = params["required"].to<JsonArray>();
            req.add("emocao");
        }

        // 2. mudar_cor_led
        {
            JsonObject fn = decls.add<JsonObject>();
            fn["name"] = "mudar_cor_led";
            fn["description"] = "Altera a cor e o efeito de iluminação do LED RGB WS2812 no corpo do ESP32-S3.";
            JsonObject params = fn["parameters"].to<JsonObject>();
            params["type"] = "OBJECT";
            JsonObject props = params["properties"].to<JsonObject>();
            props["cor"]["type"] = "STRING";
            props["cor"]["description"] = "Cor: vermelho, verde, azul, amarelo, roxo, ciano, laranja, rosa, branco ou desligar.";
            props["efeito"]["type"] = "STRING";
            props["efeito"]["description"] = "Efeito opcional: solido, pulsar, arco_iris ou alerta.";
            JsonArray req = params["required"].to<JsonArray>();
            req.add("cor");
        }

        // 3. obter_hora_e_data
        {
            JsonObject fn = decls.add<JsonObject>();
            fn["name"] = "obter_hora_e_data";
            fn["description"] = "Obtém o horário exato, dia da semana e data atual do relógio interno NTP do ESP32.";
            JsonObject params = fn["parameters"].to<JsonObject>();
            params["type"] = "OBJECT";
            params["properties"].to<JsonObject>();
        }

        // 4. definir_timer
        {
            JsonObject fn = decls.add<JsonObject>();
            fn["name"] = "definir_timer";
            fn["description"] = "Agenda um timer ou despertador em segundos no ESP32 para avisar o usuário.";
            JsonObject params = fn["parameters"].to<JsonObject>();
            params["type"] = "OBJECT";
            JsonObject props = params["properties"].to<JsonObject>();
            props["segundos"]["type"] = "INTEGER";
            props["segundos"]["description"] = "Tempo em segundos (ex: 60 para 1 minuto, 300 para 5 minutos).";
            props["lembrete"]["type"] = "STRING";
            props["lembrete"]["description"] = "Motivo do lembrete ou alarme.";
            JsonArray req = params["required"].to<JsonArray>();
            req.add("segundos");
        }

        // 5. status_hardware
        {
            JsonObject fn = decls.add<JsonObject>();
            fn["name"] = "status_hardware";
            fn["description"] = "Lê diagnósticos reais do ESP32-S3: temperatura do chip em °C, sinal Wi-Fi, memória PSRAM/RAM e uptime.";
            JsonObject params = fn["parameters"].to<JsonObject>();
            params["type"] = "OBJECT";
            params["properties"].to<JsonObject>();
        }

        // 6. obter_clima
        {
            JsonObject fn = decls.add<JsonObject>();
            fn["name"] = "obter_clima";
            fn["description"] = "Consulta a temperatura e previsão do tempo atual de qualquer cidade via Wi-Fi.";
            JsonObject params = fn["parameters"].to<JsonObject>();
            params["type"] = "OBJECT";
            JsonObject props = params["properties"].to<JsonObject>();
            props["cidade"]["type"] = "STRING";
            props["cidade"]["description"] = "Nome da cidade (ex: Sao Paulo, Curitiba, Rio de Janeiro).";
            JsonArray req = params["required"].to<JsonArray>();
            req.add("cidade");
        }
    }
};

#endif // CIBORGUE_TOOLS_CONTROLLER_H
