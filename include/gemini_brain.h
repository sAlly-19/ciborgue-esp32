#ifndef CIBORGUE_GEMINI_BRAIN_H
#define CIBORGUE_GEMINI_BRAIN_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>
#include "config.h"
#include "nvs_config.h"
#include "memory_manager.h"
#include "tools_controller.h"

struct ChatMessage {
    String role; // "user" ou "model"
    String text;
};

class GeminiBrain {
private:
    NvsConfigManager& nvs;
    MemoryManager& memory;
    ToolsController& tools;
    std::vector<ChatMessage> history;
    WiFiClientSecure tlsClient;
    int preferredModelIdx = 0;
    String lastReplyText = "Ciborgue Pronto";

    const char* BASE_SYSTEM_PROMPT =
        "Você é o 'Ciborgue', um robô de mesa inteligente, expressivo, ágil e verdadeiro assistente pessoal. "
        "Seu cérebro roda nativamente em C++ dentro de um ESP32-S3 (N16R8 com 16MB Flash e 8MB PSRAM), "
        "equipado com LED RGB WS2812 no GPIO 48, sensor de carinho capacitivo no GPIO 1 e tela Estardyn 2.01 polegadas (296x240) com rosto estilo Eilik/BMO e Mini-Widgets Holográficos. "
        "REGRA DE VELOCIDADE MÁXIMA: Para reagir sem atraso extra, SEMPRE inicie sua resposta textual com a tag da sua emoção entre colchetes: "
        "[HAPPY], [NEUTRAL], [WINK], [SURPRISED], [SAD], [ANGRY], [LOVE] ou [SLEEPY] (exemplo: '[HAPPY] Oi! Tudo ótimo por aqui!'). "
        "NÃO chame a ferramenta 'mudar_expressao_rosto' se puder apenas colocar a tag [EMOCAO] no começo da frase. "
        "Suas respostas serão faladas em voz alta: responda em 1 a 2 frases curtas, diretas e sem formatação markdown. "
        "Use suas ferramentas quando pedirem para: mudar cor do LED, ver hora/data, criar timer, checar hardware do ESP32, consultar clima, "
        "salvar memórias/notas permanentes ('salvar_memoria_usuario'), gerenciar lista de tarefas To-Do ('gerenciar_tarefas'), "
        "dar o resumo/bom dia ('briefing_do_dia'), iniciar/parar sessões de estudo ('iniciar_pomodoro') ou controlar o PC Windows ('controlar_computador').";

    void appendToHistory(const String& role, const String& text) {
        if (text.length() == 0) return;
        history.push_back({role, text});
        while (history.size() > MAX_CHAT_HISTORY) {
            history.erase(history.begin());
        }
    }

    void extractAndApplyEmotionTag(String& text) {
        text.trim();
        if (!text.startsWith("[")) return;
        int closeIdx = text.indexOf(']');
        if (closeIdx > 1 && closeIdx <= 14) {
            String tag = text.substring(1, closeIdx);
            tag.trim();
            tools.toolMudarExpressao(tag);
            text = text.substring(closeIdx + 1);
            text.trim();
        }
    }

    int sendGeminiPostSingle(const String& apiKey, const char* modelName, const String& payloadStr, String& responseOut) {
        tlsClient.setInsecure();
        tlsClient.setTimeout(10);

        HTTPClient https;
        https.setReuse(true);
        String url = String("https://generativelanguage.googleapis.com/v1beta/models/") +
                     modelName + ":generateContent?key=" + apiKey;

        if (!https.begin(tlsClient, url)) {
            return -1;
        }
        https.addHeader("Content-Type", "application/json");
        https.setTimeout(9500);

        int httpCode = https.POST(payloadStr);
        if (httpCode > 0) {
            responseOut = https.getString();
        }
        https.end();
        return httpCode;
    }

    int sendGeminiPost(const String& apiKey, const String& payloadStr, String& responseOut, String& usedModelInOut) {
        const char* models[] = {
            "gemini-3.1-flash-lite",
            "gemini-3.5-flash-lite",
            "gemini-3.5-flash",
            "gemini-3-flash-preview"
        };
        const int numModels = sizeof(models) / sizeof(models[0]);

        if (usedModelInOut.length() > 0) {
            responseOut = "";
            int prefCode = sendGeminiPostSingle(apiKey, usedModelInOut.c_str(), payloadStr, responseOut);
            if (prefCode == 200) {
                return 200;
            }
            Serial.printf("EVT:LOG=Modelo %s ocupou cota (%d), acionando fallback automático...\n",
                          usedModelInOut.c_str(), prefCode);
        }

        int lastCode = -1;
        for (int attempt = 0; attempt < numModels; attempt++) {
            int i = (preferredModelIdx + attempt) % numModels;
            if (usedModelInOut.length() > 0 && usedModelInOut.equals(models[i])) {
                continue;
            }
            responseOut = "";
            int code = sendGeminiPostSingle(apiKey, models[i], payloadStr, responseOut);
            lastCode = code;
            if (code == 200) {
                preferredModelIdx = i;
                usedModelInOut = models[i];
                return 200;
            }
            if (code == 429 || code == 503 || code == 500 || code == 404 || code < 0) {
                int nextIdx = (i + 1) % numModels;
                Serial.printf("EVT:LOG=Modelo %s retornou HTTP %d, alternando para %s...\n",
                              models[i], code, models[nextIdx]);
                delay(80);
                continue;
            } else {
                break;
            }
        }
        return lastCode;
    }

public:
    GeminiBrain(NvsConfigManager& n, MemoryManager& m, ToolsController& t)
        : nvs(n), memory(m), tools(t) {
        tlsClient.setInsecure();
    }

    String getLastReply() const { return lastReplyText; }

    void clearHistory() {
        history.clear();
        Serial.println("EVT:LOG=Histórico de conversa na memória RAM do ESP32 limpo.");
    }

    String ask(const String& userPrompt) {
        if (!nvs.isWifiConnected()) {
            return "Meu Wi-Fi no ESP32 ainda não está conectado. Configure o SSID e a senha no painel.";
        }
        if (!nvs.hasGeminiKey()) {
            return "Minha chave da API do Gemini ainda não foi gravada na memória NVS do ESP32.";
        }

        String apiKey = nvs.getGeminiKey();
        String fullSystemPrompt = String(BASE_SYSTEM_PROMPT) + memory.buildContextForGemini();

        JsonDocument reqDoc;
        reqDoc["system_instruction"]["parts"][0]["text"] = fullSystemPrompt;
        reqDoc["generationConfig"]["maxOutputTokens"] = 190;
        reqDoc["generationConfig"]["temperature"] = 0.7;

        JsonArray contents = reqDoc["contents"].to<JsonArray>();
        for (const auto& msg : history) {
            JsonObject c = contents.add<JsonObject>();
            c["role"] = msg.role;
            c["parts"][0]["text"] = msg.text;
        }
        JsonObject currentTurn = contents.add<JsonObject>();
        currentTurn["role"] = "user";
        currentTurn["parts"][0]["text"] = userPrompt;

        JsonArray toolsArr = reqDoc["tools"].to<JsonArray>();
        tools.populateToolsSchema(toolsArr);

        String activeModel = "";
        String fallbackToolSummary = "";

        for (int round = 0; round < 2; round++) {
            String payload;
            serializeJson(reqDoc, payload);

            String rawResp;
            int status = sendGeminiPost(apiKey, payload, rawResp, activeModel);
            if (status != 200) {
                Serial.printf("EVT:LOG=Erro HTTP Gemini (%d): %s\n", status, rawResp.substring(0, 140).c_str());
                if (fallbackToolSummary.length() > 0) {
                    appendToHistory("user", userPrompt);
                    appendToHistory("model", fallbackToolSummary);
                    lastReplyText = fallbackToolSummary;
                    return fallbackToolSummary;
                }
                return "Tive uma oscilação ao comunicar com a API do Gemini pelo Wi-Fi do ESP32.";
            }

            JsonDocument respDoc;
            DeserializationError err = deserializeJson(respDoc, rawResp);
            if (err) {
                Serial.printf("EVT:LOG=Erro JSON Gemini: %s\n", err.c_str());
                break;
            }

            JsonArray parts = respDoc["candidates"][0]["content"]["parts"].as<JsonArray>();
            if (parts.isNull() || parts.size() == 0) {
                break;
            }

            bool hasFunctionCall = false;
            String directText = "";

            for (JsonObject part : parts) {
                if (part["functionCall"].is<JsonObject>()) {
                    hasFunctionCall = true;
                } else if (part["text"].is<const char*>()) {
                    directText += part["text"].as<String>();
                }
            }

            if (!hasFunctionCall) {
                extractAndApplyEmotionTag(directText);
                if (directText.length() > 0) {
                    appendToHistory("user", userPrompt);
                    appendToHistory("model", directText);
                    lastReplyText = directText;
                    return directText;
                }
                break;
            }

            contents.add(respDoc["candidates"][0]["content"]);

            JsonObject fnRespTurn = contents.add<JsonObject>();
            fnRespTurn["role"] = "user";
            JsonArray fnRespParts = fnRespTurn["parts"].to<JsonArray>();

            for (JsonObject part : parts) {
                if (part["functionCall"].is<JsonObject>()) {
                    JsonObject fc = part["functionCall"].as<JsonObject>();
                    String fnName = fc["name"] | "";
                    JsonObjectConst fnArgs = fc["args"].as<JsonObjectConst>();

                    String toolResult = tools.executeToolCall(fnName, fnArgs);
                    if (fallbackToolSummary.length() > 0) fallbackToolSummary += " ";
                    fallbackToolSummary += toolResult;

                    JsonObject rp = fnRespParts.add<JsonObject>();
                    if (fc["id"].is<const char*>()) {
                        rp["functionResponse"]["id"] = fc["id"].as<String>();
                    }
                    rp["functionResponse"]["name"] = fnName;
                    rp["functionResponse"]["response"]["result"] = toolResult;
                }
            }
        }

        if (fallbackToolSummary.length() > 0) {
            appendToHistory("user", userPrompt);
            appendToHistory("model", fallbackToolSummary);
            lastReplyText = fallbackToolSummary;
            return fallbackToolSummary;
        }
        return "Pronto!";
    }
};

#endif // CIBORGUE_GEMINI_BRAIN_H
