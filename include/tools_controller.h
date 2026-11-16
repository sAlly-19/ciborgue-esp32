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
#include "memory_manager.h"
#include "pomodoro_manager.h"

class ToolsController {
private:
    LedController& led;
    DisplayHAL& display;
    NvsConfigManager& nvs;
    MemoryManager& memory;
    PomodoroManager& pomodoro;
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

    void emitWidgetEvent(const JsonDocument& doc) {
        String out;
        serializeJson(doc, out);
        Serial.printf("EVT:WIDGET=%s\n", out.c_str());
    }

public:
    ToolsController(LedController& l, DisplayHAL& d, NvsConfigManager& n, MemoryManager& mem,
                    PomodoroManager& pomo, FaceEmotion& emo, RobotTimer& tmr)
        : led(l), display(d), nvs(n), memory(mem), pomodoro(pomo),
          currentEmotion(emo), activeTimer(tmr) {}

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
        char timeBuf[16];
        char dateBuf[20];
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        snprintf(dateBuf, sizeof(dateBuf), "%02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);

        // Dispara Mini-Widget Holográfico de Relógio na tela 296x240
        JsonDocument wDoc;
        wDoc["type"] = "CLOCK";
        wDoc["time"] = timeBuf;
        wDoc["date"] = dateBuf;
        wDoc["weekday"] = diasSemana[timeinfo.tm_wday];
        emitWidgetEvent(wDoc);

        char buffer[140];
        snprintf(buffer, sizeof(buffer),
                 "Agora são %s (%s, dia %s).",
                 timeBuf, diasSemana[timeinfo.tm_wday], dateBuf);
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
        const char* descPt = weatherCodeToPt(wCode);

        // Dispara Mini-Widget Holográfico de Clima na tela 296x240
        JsonDocument card;
        card["type"]  = "WEATHER";
        card["city"]  = nomeCidade;
        card["temp"]  = serialized(String(temp, 1));
        card["feels"] = serialized(String(sens, 1));
        card["hum"]   = umid;
        card["code"]  = wCode;
        card["desc"]  = descPt;
        emitWidgetEvent(card);

        char res[220];
        snprintf(res, sizeof(res),
                 "Clima em %s: %.1f°C (sensação de %.1f°C), umidade de %d%% e %s.",
                 nomeCidade.c_str(), temp, sens, umid, descPt);
        return String(res);
    }

    String toolSalvarMemoria(const String& acao, const String& fato) {
        String act = acao;
        act.toLowerCase();
        if (act == "limpar" || act == "apagar_tudo") {
            return memory.clearMemories();
        }
        return memory.addMemory(fato);
    }

    String toolGerenciarTarefas(const String& acao, const String& titulo, int id) {
        String act = acao;
        act.toLowerCase();
        String result;
        if (act == "adicionar" || act == "criar" || act == "add") {
            result = memory.addTask(titulo);
        } else if (act == "concluir" || act == "completar" || act == "done") {
            result = memory.completeTask(id, titulo);
        } else if (act == "remover" || act == "excluir" || act == "del") {
            result = memory.removeTask(id);
        } else if (act == "limpar") {
            result = memory.clearTasks(false);
        } else {
            result = memory.getTasksSummaryText();
        }

        // Dispara Mini-Widget Holográfico de Tarefas na tela 296x240
        JsonDocument card;
        card["type"] = "TASKS";
        emitWidgetEvent(card);
        return result;
    }

    String toolBriefingDoDia() {
        String hora = toolObterHoraEData();
        String clima = toolObterClima(nvs.getDefaultCity());
        String tarefas = memory.getTasksSummaryText();
        return hora + " " + clima + " " + tarefas;
    }

    String toolIniciarPomodoro(const String& acao, int minFoco, int minPausa, const String& meta) {
        String act = acao;
        act.toLowerCase();
        if (act == "parar" || act == "cancelar" || act == "stop") {
            return pomodoro.stop();
        }
        return pomodoro.start(minFoco, minPausa, meta);
    }

    String toolControlarComputador(const String& acao, const String& parametro) {
        JsonDocument pcDoc;
        pcDoc["action"] = acao;
        pcDoc["param"]  = parametro;
        String out;
        serializeJson(pcDoc, out);
        Serial.printf("EVT:PC=%s\n", out.c_str());

        JsonDocument card;
        card["type"]   = "PC_CONTROL";
        card["action"] = acao;
        card["param"]  = parametro;
        emitWidgetEvent(card);

        return String("Comando enviado ao PC Windows: ") + acao +
               (parametro.length() > 0 ? (" (" + parametro + ")") : "");
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
        } else if (fnName == "salvar_memoria_usuario") {
            String acao = args["acao"] | "adicionar";
            String fato = args["fato"] | "";
            return toolSalvarMemoria(acao, fato);
        } else if (fnName == "gerenciar_tarefas") {
            String acao = args["acao"] | "listar";
            String titulo = args["titulo"] | "";
            int id = args["id"] | 1;
            return toolGerenciarTarefas(acao, titulo, id);
        } else if (fnName == "briefing_do_dia") {
            return toolBriefingDoDia();
        } else if (fnName == "iniciar_pomodoro") {
            String acao = args["acao"] | "iniciar";
            int foco = args["minutos_foco"] | 25;
            int pausa = args["minutos_pausa"] | 5;
            String meta = args["meta"] | "Foco e Estudo";
            return toolIniciarPomodoro(acao, foco, pausa, meta);
        } else if (fnName == "controlar_computador") {
            String acao = args["acao"] | "pc_status";
            String param = args["parametro"] | "";
            return toolControlarComputador(acao, param);
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
            fn["description"] = "Obtém o horário exato, dia da semana e data atual do relógio NTP do ESP32 e exibe o widget de relógio.";
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
            fn["description"] = "Consulta a temperatura e previsão do tempo atual de qualquer cidade via Wi-Fi e exibe o widget de clima.";
            JsonObject params = fn["parameters"].to<JsonObject>();
            params["type"] = "OBJECT";
            JsonObject props = params["properties"].to<JsonObject>();
            props["cidade"]["type"] = "STRING";
            props["cidade"]["description"] = "Nome da cidade (ex: Sao Paulo, Curitiba, Rio de Janeiro).";
            JsonArray req = params["required"].to<JsonArray>();
            req.add("cidade");
        }

        // 7. salvar_memoria_usuario
        {
            JsonObject fn = decls.add<JsonObject>();
            fn["name"] = "salvar_memoria_usuario";
            fn["description"] = "Grava permanentemente na Flash NVS um fato importante sobre o usuário (nome, preferências, datas de provas, notas) para lembrar no futuro.";
            JsonObject params = fn["parameters"].to<JsonObject>();
            params["type"] = "OBJECT";
            JsonObject props = params["properties"].to<JsonObject>();
            props["acao"]["type"] = "STRING";
            props["acao"]["description"] = "Ação: 'adicionar' ou 'limpar'.";
            props["fato"]["type"] = "STRING";
            props["fato"]["description"] = "O fato ou nota a ser guardado (ex: 'Prova de Redes na sexta-feira').";
            JsonArray req = params["required"].to<JsonArray>();
            req.add("fato");
        }

        // 8. gerenciar_tarefas
        {
            JsonObject fn = decls.add<JsonObject>();
            fn["name"] = "gerenciar_tarefas";
            fn["description"] = "Adiciona, conclui, remove ou lista as tarefas (To-Do List) salvas na Flash NVS do ESP32 e abre o widget visual de tarefas.";
            JsonObject params = fn["parameters"].to<JsonObject>();
            params["type"] = "OBJECT";
            JsonObject props = params["properties"].to<JsonObject>();
            props["acao"]["type"] = "STRING";
            props["acao"]["description"] = "Ação: 'adicionar', 'concluir', 'remover', 'listar' ou 'limpar'.";
            props["titulo"]["type"] = "STRING";
            props["titulo"]["description"] = "Título ou descrição da tarefa.";
            props["id"]["type"] = "INTEGER";
            props["id"]["description"] = "Número/ID da tarefa (para concluir ou remover).";
            JsonArray req = params["required"].to<JsonArray>();
            req.add("acao");
        }

        // 9. briefing_do_dia
        {
            JsonObject fn = decls.add<JsonObject>();
            fn["name"] = "briefing_do_dia";
            fn["description"] = "Gera um resumo matinal/diário completo combinando horário atual, clima da cidade e lista de tarefas pendentes.";
            JsonObject params = fn["parameters"].to<JsonObject>();
            params["type"] = "OBJECT";
            params["properties"].to<JsonObject>();
        }

        // 10. iniciar_pomodoro
        {
            JsonObject fn = decls.add<JsonObject>();
            fn["name"] = "iniciar_pomodoro";
            fn["description"] = "Inicia ou encerra o Modo Companheiro de Estudo Pomodoro com barra visual na tela 296x240.";
            JsonObject params = fn["parameters"].to<JsonObject>();
            params["type"] = "OBJECT";
            JsonObject props = params["properties"].to<JsonObject>();
            props["acao"]["type"] = "STRING";
            props["acao"]["description"] = "Ação: 'iniciar' ou 'parar'.";
            props["minutos_foco"]["type"] = "INTEGER";
            props["minutos_foco"]["description"] = "Minutos de foco (padrão 25).";
            props["minutos_pausa"]["type"] = "INTEGER";
            props["minutos_pausa"]["description"] = "Minutos de descanso (padrão 5).";
            props["meta"]["type"] = "STRING";
            props["meta"]["description"] = "O que o usuário vai estudar ou programar.";
            JsonArray req = params["required"].to<JsonArray>();
            req.add("acao");
        }

        // 11. controlar_computador
        {
            JsonObject fn = decls.add<JsonObject>();
            fn["name"] = "controlar_computador";
            fn["description"] = "Executa comandos e automações no PC Windows conectado: controlar mídia/música, ajustar volume, abrir programas/sites, bloquear tela, desligar/reiniciar/suspender o PC, fechar processos ou ler uso de CPU/RAM/Disco/Bateria do PC.";
            JsonObject params = fn["parameters"].to<JsonObject>();
            params["type"] = "OBJECT";
            JsonObject props = params["properties"].to<JsonObject>();
            props["acao"]["type"] = "STRING";
            props["acao"]["description"] = "Ação exata: 'media_play_pause', 'media_next', 'media_prev', 'volume_up', 'volume_down', 'volume_mute', 'volume_set', 'open_app', 'open_url', 'youtube_search', 'google_search', 'lock_pc', 'sleep_pc', 'shutdown_pc', 'restart_pc', 'cancel_shutdown', 'kill_process' ou 'pc_status'.";
            props["parametro"]["type"] = "STRING";
            props["parametro"]["description"] = "Parâmetro da ação: nível de volume (0-100), nome do app (vscode, chrome, spotify, calc, notepad, explorer, etc.), URL/termo de busca ou nome do processo.";
            JsonArray req = params["required"].to<JsonArray>();
            req.add("acao");
        }
    }
};

#endif // CIBORGUE_TOOLS_CONTROLLER_H
