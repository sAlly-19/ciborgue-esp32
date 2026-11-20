#ifndef CIBORGUE_OFFLINE_ENGINE_H
#define CIBORGUE_OFFLINE_ENGINE_H

#include <Arduino.h>
#include "config.h"
#include "robot_types.h"
#include "nvs_config.h"
#include "memory_manager.h"
#include "pomodoro_manager.h"
#include "sfx_synth.h"
#include "power_manager.h"
#include "captive_portal.h"
#include "tools_controller.h"

class OfflineCommandEngine {
private:
    NvsConfigManager& nvs;
    MemoryManager& memory;
    PomodoroManager& pomodoro;
    SfxSynth& sfx;
    PowerManager& power;
    CaptivePortalManager& portal;
    ToolsController& tools;

    static int extractFirstInt(const String& s, int fallback) {
        int val = 0;
        bool found = false;
        for (size_t i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            if (c >= '0' && c <= '9') {
                val = val * 10 + (c - '0');
                found = true;
            } else if (found) {
                break;
            }
        }
        return found ? val : fallback;
    }

    static bool extractHourMinute(const String& s, int& hourOut, int& minOut) {
        int nums[2] = {-1, -1};
        int count = 0;
        int cur = 0;
        bool inNum = false;
        for (size_t i = 0; i < s.length() && count < 2; i++) {
            char c = s.charAt(i);
            if (c >= '0' && c <= '9') {
                cur = cur * 10 + (c - '0');
                inNum = true;
            } else if (inNum) {
                nums[count++] = cur;
                cur = 0;
                inNum = false;
            }
        }
        if (inNum && count < 2) {
            nums[count++] = cur;
        }
        if (count == 0 || nums[0] < 0 || nums[0] > 23) return false;
        hourOut = nums[0];
        minOut  = (count >= 2 && nums[1] >= 0 && nums[1] <= 59) ? nums[1] : 0;
        return true;
    }

    static bool containsWord(const String& padded, const char* word) {
        String needle = String(" ") + word + " ";
        return padded.indexOf(needle) >= 0;
    }

    static bool detectColorAndEffect(const String& norm, String& colorOut, String& effectOut) {
        effectOut = "solido";
        if (norm.indexOf("arco iris") >= 0 || norm.indexOf("arco-iris") >= 0 || norm.indexOf("rainbow") >= 0) {
            colorOut = "arco iris";
            effectOut = "arco_iris";
            return true;
        }
        if (norm.indexOf("puls") >= 0 || norm.indexOf("respir") >= 0) {
            effectOut = "pulsar";
        } else if (norm.indexOf("pisc") >= 0 || norm.indexOf("alert") >= 0) {
            effectOut = "alerta";
        }

        const char* colors[] = {
            "vermelho", "vermelha", "verde", "azul", "amarelo", "amarela",
            "roxo", "roxa", "violeta", "ciano", "turquesa", "laranja",
            "rosa", "pink", "branco", "branca", "apagar", "desligar"
        };
        for (size_t i = 0; i < sizeof(colors) / sizeof(colors[0]); i++) {
            if (norm.indexOf(colors[i]) >= 0) {
                colorOut = colors[i];
                return true;
            }
        }
        if (norm.indexOf("desliga") >= 0 || norm.indexOf("apaga") >= 0) {
            colorOut = "desligar";
            return true;
        }
        return false;
    }

    bool tryDirectCommands(const String& norm, String& replyOut) {
        String padded = String(" ") + norm + " ";

        // 1. LED / Luz (exige palavra-chave explícita de LED/luz para não interceptar conversa comum)
        bool mentionsLed = norm.indexOf("led") >= 0 || norm.indexOf("luz") >= 0 ||
                           norm.indexOf("acend") >= 0 || norm.indexOf("apag") >= 0 ||
                           norm.indexOf("muda a cor") >= 0 || norm.indexOf("mude a cor") >= 0 ||
                           norm.indexOf("arco iris") >= 0 || norm.indexOf("arco-iris") >= 0;
        if (mentionsLed) {
            String color, effect;
            if (detectColorAndEffect(norm, color, effect)) {
                sfx.play(SFX_HAPPY, 0.85f);
                replyOut = tools.toolMudarCorLed(color, effect);
                return true;
            }
        }

        // 2. Volume e Mudo do ESP32
        if (norm.indexOf("desmutar") >= 0 || norm.indexOf("tirar do mudo") >= 0 ||
            norm.indexOf("ativar som") >= 0 || norm.indexOf("reativar som") >= 0 ||
            norm.indexOf("ligar som") >= 0) {
            replyOut = tools.toolConfigurarAudio("desmutar");
            return true;
        }
        if (norm.indexOf("mutar") >= 0 || norm.indexOf("silenciar") >= 0 ||
            norm.indexOf("ficar mudo") >= 0 || norm.indexOf("modo mudo") >= 0 ||
            norm.indexOf("desligar som") >= 0) {
            replyOut = tools.toolConfigurarAudio("mutar");
            return true;
        }
        if (norm.indexOf("aumentar volume") >= 0 || norm.indexOf("subir volume") >= 0 || norm.indexOf("mais alto") >= 0) {
            replyOut = tools.toolConfigurarAudio("subir");
            return true;
        }
        if (norm.indexOf("diminuir volume") >= 0 || norm.indexOf("baixar volume") >= 0 || norm.indexOf("mais baixo") >= 0) {
            replyOut = tools.toolConfigurarAudio("descer");
            return true;
        }
        if (norm.indexOf("volume") >= 0) {
            int vol = extractFirstInt(norm, -1);
            if (vol >= 0) {
                replyOut = tools.toolConfigurarAudio("volume", vol);
                return true;
            }
        }
        if (norm.indexOf("desligar chirps") >= 0 || norm.indexOf("parar chirps") >= 0) {
            replyOut = tools.toolConfigurarAudio("chirps_off");
            return true;
        }
        if (norm.indexOf("ativar chirps") >= 0 || norm.indexOf("ligar chirps") >= 0) {
            replyOut = tools.toolConfigurarAudio("chirps_on");
            return true;
        }

        // 3. Sono Inteligente
        if (containsWord(padded, "dormir") || norm.indexOf("vai dormir") >= 0 ||
            norm.indexOf("modo sono") >= 0 || norm.indexOf("descansar agora") >= 0 ||
            norm.indexOf("boa noite ciborgue") >= 0) {
            replyOut = tools.toolModoSono("dormir");
            return true;
        }
        if (norm.indexOf("modo eco") >= 0 || norm.indexOf("ficar sonolento") >= 0) {
            replyOut = tools.toolModoSono("eco");
            return true;
        }

        // 4. Relógio / Data
        if (norm.indexOf("que horas") >= 0 || norm.indexOf("qual a hora") >= 0 ||
            norm.indexOf("horário") >= 0 || norm.indexOf("horario") >= 0 ||
            norm.indexOf("que dia e hoje") >= 0 || norm.indexOf("data de hoje") >= 0) {
            sfx.play(SFX_WINK, 0.8f);
            replyOut = tools.toolObterHoraEData();
            return true;
        }

        // 5. Parar Pomodoro
        if (norm.indexOf("parar pomodoro") >= 0 || norm.indexOf("cancelar pomodoro") >= 0 ||
            norm.indexOf("encerrar pomodoro") >= 0 || norm.indexOf("parar foco") >= 0) {
            sfx.play(SFX_WINK, 0.8f);
            replyOut = tools.toolIniciarPomodoro("parar", 25, 5, "");
            return true;
        }

        // 6. Portal Cativo / Configurar Wi-Fi
        if (norm.indexOf("abrir portal") >= 0 || norm.indexOf("portal wifi") >= 0 ||
            norm.indexOf("configurar wifi") >= 0 || norm.indexOf("modo ap") >= 0) {
            replyOut = tools.toolAbrirPortalWifi("abrir");
            return true;
        }
        if (norm.indexOf("fechar portal") >= 0) {
            replyOut = tools.toolAbrirPortalWifi("fechar");
            return true;
        }

        // 7. Diagnóstico do ESP32
        if (norm.indexOf("status do hardware") >= 0 || norm.indexOf("status do esp") >= 0 ||
            norm.indexOf("temperatura do chip") >= 0 || norm.indexOf("memoria do esp") >= 0) {
            sfx.play(SFX_THINK, 0.8f);
            replyOut = tools.toolStatusHardware();
            return true;
        }

        // 8. Despertador tocando / Timer cancelar / Desk Clock / Humor
        if (norm.indexOf("parar alarme") >= 0 || norm.indexOf("desligar alarme") >= 0 ||
            norm.indexOf("silenciar alarme") >= 0 || norm.indexOf("parar despertador") >= 0) {
            replyOut = tools.toolGerenciarAlarmes("parar");
            return true;
        }
        if (norm.indexOf("parar timer") >= 0 || norm.indexOf("cancelar timer") >= 0) {
            cancelActiveTimer();
            replyOut = "Timer cancelado.";
            return true;
        }
        if (norm.indexOf("sair do relogio") >= 0 || norm.indexOf("fechar relogio") >= 0 ||
            norm.indexOf("desativar modo relogio") >= 0 || norm.indexOf("desligar modo relogio") >= 0) {
            replyOut = tools.toolModoRelogio("desativar");
            return true;
        }
        if (norm.indexOf("modo relogio") >= 0 || norm.indexOf("relogio de mesa") >= 0 ||
            norm.indexOf("desk clock") >= 0) {
            replyOut = tools.toolModoRelogio("ativar");
            return true;
        }
        if (norm.indexOf("como voce esta se sentindo") >= 0 || norm.indexOf("como vc esta se sentindo") >= 0 ||
            norm.indexOf("seu humor") >= 0 || norm.indexOf("nivel de afeto") >= 0 ||
            norm.indexOf("ver humor") >= 0 || norm.indexOf("mostre seu humor") >= 0) {
            replyOut = tools.toolVerHumor();
            return true;
        }

        return false;
    }

public:
    OfflineCommandEngine(NvsConfigManager& n, MemoryManager& m, PomodoroManager& p,
                         SfxSynth& s, PowerManager& pw, CaptivePortalManager& cp,
                         ToolsController& t)
        : nvs(n), memory(m), pomodoro(p), sfx(s), power(pw), portal(cp), tools(t) {}

    static String normalizePtBr(const String& raw) {
        String out;
        out.reserve(raw.length());
        bool lastSpace = true;

        for (size_t i = 0; i < raw.length(); i++) {
            uint8_t b = (uint8_t)raw.charAt(i);
            char mapped = 0;

            if (b == 0xC3 && (i + 1) < raw.length()) {
                uint8_t b2 = (uint8_t)raw.charAt(++i);
                if ((b2 >= 0x80 && b2 <= 0x85) || (b2 >= 0xA0 && b2 <= 0xA5)) mapped = 'a';
                else if ((b2 >= 0x88 && b2 <= 0x8B) || (b2 >= 0xA8 && b2 <= 0xAB)) mapped = 'e';
                else if ((b2 >= 0x8C && b2 <= 0x8F) || (b2 >= 0xAC && b2 <= 0xAF)) mapped = 'i';
                else if ((b2 >= 0x92 && b2 <= 0x96) || (b2 >= 0xB2 && b2 <= 0xB6)) mapped = 'o';
                else if ((b2 >= 0x99 && b2 <= 0x9C) || (b2 >= 0xB9 && b2 <= 0xBC)) mapped = 'u';
                else if (b2 == 0x87 || b2 == 0xA7) mapped = 'c';
                else if (b2 == 0x91 || b2 == 0xB1) mapped = 'n';
            } else if (b >= 'A' && b <= 'Z') {
                mapped = (char)(b + 32);
            } else if ((b >= 'a' && b <= 'z') || (b >= '0' && b <= '9') || b == '-') {
                mapped = (char)b;
            } else if (b == ' ' || b == '\t' || b == '\n' || b == '\r' ||
                       b == '?' || b == '!' || b == '.' || b == ',' ||
                       b == ';' || b == ':' || b == '"' || b == '\'') {
                mapped = ' ';
            }

            if (mapped == ' ') {
                if (!lastSpace) {
                    out += ' ';
                    lastSpace = true;
                }
            } else if (mapped != 0) {
                out += mapped;
                lastSpace = false;
            }
        }
        out.trim();
        return out;
    }

    bool tryFastPath(const String& userPrompt, String& replyOut) {
        if (!nvs.isFastPathEnabled()) return false;
        String norm = normalizePtBr(userPrompt);
        if (norm.length() == 0 || norm.length() > 64) return false;
        return tryDirectCommands(norm, replyOut);
    }

    String handleOffline(const String& userPrompt) {
        String norm = normalizePtBr(userPrompt);
        String directReply;
        if (tryDirectCommands(norm, directReply)) {
            return directReply;
        }

        // 1. Despertador recorrente por horário fixo vs Timer / Cronômetro
        if (norm.indexOf("alarme") >= 0 || norm.indexOf("despertador") >= 0) {
            if (norm.indexOf("listar") >= 0 || norm.indexOf("meus") >= 0 || norm.indexOf("quais") >= 0) {
                return tools.toolGerenciarAlarmes("listar");
            }
            if (norm.indexOf("remover") >= 0 || norm.indexOf("apagar") >= 0 ||
                norm.indexOf("excluir") >= 0 || norm.indexOf("deletar") >= 0) {
                return tools.toolGerenciarAlarmes("remover", 0, 0, "", "", extractFirstInt(norm, 1));
            }
            if (norm.indexOf("minuto") < 0 && norm.indexOf("segundo") < 0 && norm.indexOf("daqui") < 0) {
                int hh = 7, mm = 0;
                if (extractHourMinute(norm, hh, mm)) {
                    const char* rep = (norm.indexOf("semana") >= 0 || norm.indexOf("util") >= 0 || norm.indexOf("uteis") >= 0) ? "semana"
                                    : (norm.indexOf("fds") >= 0 || norm.indexOf("fim de semana") >= 0) ? "fds"
                                    : (norm.indexOf("unico") >= 0 || norm.indexOf("uma vez") >= 0) ? "unico" : "diario";
                    return tools.toolGerenciarAlarmes("adicionar", hh, mm, rep, "Despertador");
                }
            }
        }
        if (norm.indexOf("timer") >= 0 || norm.indexOf("alarme") >= 0 ||
            norm.indexOf("despertador") >= 0 || norm.indexOf("cronometro") >= 0) {
            int val = extractFirstInt(norm, 10);
            int seg = val;
            if (norm.indexOf("minuto") >= 0 || norm.indexOf(" min") >= 0) seg = val * 60;
            else if (norm.indexOf("hora") >= 0) seg = val * 3600;
            return tools.toolDefinirTimer(seg, "Timer Offline");
        }

        // 2. Pomodoro
        if (norm.indexOf("pomodoro") >= 0 || norm.indexOf("modo foco") >= 0 || norm.indexOf("iniciar foco") >= 0) {
            int mins = extractFirstInt(norm, 25);
            return tools.toolIniciarPomodoro("iniciar", mins, 5, "Foco Offline");
        }

        // 3. Emoções do Rosto
        if (norm.indexOf("feliz") >= 0 || norm.indexOf("alegre") >= 0) return tools.toolMudarExpressao("happy");
        if (norm.indexOf("triste") >= 0) return tools.toolMudarExpressao("sad");
        if (norm.indexOf("bravo") >= 0 || norm.indexOf("irritado") >= 0) return tools.toolMudarExpressao("angry");
        if (norm.indexOf("surpreso") >= 0 || norm.indexOf("espantado") >= 0) return tools.toolMudarExpressao("surprised");
        if (norm.indexOf("piscadinha") >= 0 || norm.indexOf("pisca") >= 0) return tools.toolMudarExpressao("wink");
        if (norm.indexOf("apaixonado") >= 0 || norm.indexOf("carinho") >= 0) return tools.toolMudarExpressao("love");
        if (norm.indexOf("sonolento") >= 0 || norm.indexOf("sono") >= 0) return tools.toolMudarExpressao("sleepy");
        if (norm.indexOf("medo") >= 0 || norm.indexOf("apavorado") >= 0 || norm.indexOf("pavor") >= 0) return tools.toolMudarExpressao("fear");
        if (norm.indexOf("nojo") >= 0 || norm.indexOf("eca") >= 0 || norm.indexOf("repulsa") >= 0) return tools.toolMudarExpressao("disgust");
        if (norm.indexOf("desprezo") >= 0 || norm.indexOf("desdem") >= 0 || norm.indexOf("esnobe") >= 0) return tools.toolMudarExpressao("contempt");
        if (norm.indexOf("vergonha") >= 0 || norm.indexOf("envergonhad") >= 0 || norm.indexOf("timido") >= 0) return tools.toolMudarExpressao("shame");
        if (norm.indexOf("desespero") >= 0 || norm.indexOf("desesperad") >= 0 || norm.indexOf("panico") >= 0) return tools.toolMudarExpressao("despair");
        if (norm.indexOf("malicia") >= 0 || norm.indexOf("malicios") >= 0 || norm.indexOf("travess") >= 0) return tools.toolMudarExpressao("malice");
        if (norm.indexOf("neutro") >= 0 || norm.indexOf("normal") >= 0) return tools.toolMudarExpressao("neutral");

        // 4. Tarefas NVS
        if (norm.indexOf("tarefa") >= 0) {
            if (norm.indexOf("limpar") >= 0) {
                return tools.toolGerenciarTarefas("limpar", "", 0);
            }
            if (norm.indexOf("concluir") >= 0 || norm.indexOf("completar") >= 0 || norm.indexOf("finalizar") >= 0) {
                int id = extractFirstInt(norm, 1);
                return tools.toolGerenciarTarefas("concluir", "", id);
            }
            if (norm.indexOf("adicionar") >= 0 || norm.indexOf("criar") >= 0 || norm.indexOf("nova tarefa") >= 0) {
                int pos = userPrompt.indexOf(':');
                String title = (pos >= 0) ? userPrompt.substring(pos + 1) : userPrompt;
                if (pos < 0) {
                    int tIdx = norm.indexOf("tarefa");
                    if (tIdx >= 0 && (int)userPrompt.length() > tIdx + 6) {
                        title = userPrompt.substring(tIdx + 6);
                    }
                }
                title.trim();
                if (title.length() == 0) title = "Nova tarefa offline";
                return tools.toolGerenciarTarefas("adicionar", title, 0);
            }
            return tools.toolGerenciarTarefas("listar", "", 0);
        }

        // 5. Memórias NVS
        String memoryReply;
        if (memory.handleConversationMemory(userPrompt, memoryReply)) return memoryReply;
        if (norm.indexOf("esquecer memorias") >= 0 || norm.indexOf("limpar memorias") >= 0) {
            return tools.toolSalvarMemoria("limpar", "");
        }

        // 6. Saudações offline amigáveis
        String padded = String(" ") + norm + " ";
        if (containsWord(padded, "oi") || containsWord(padded, "ola") ||
            norm.indexOf("bom dia") >= 0 || norm.indexOf("boa tarde") >= 0 ||
            norm.indexOf("tudo bem") >= 0) {
            tools.toolMudarExpressao("happy");
            return "Oi! Estou em modo offline no momento, mas continuo aqui com você! Posso controlar meu LED, timers, Pomodoro, tarefas e hora local.";
        }
        if (norm.indexOf("obrigado") >= 0 || norm.indexOf("valeu") >= 0) {
            tools.toolMudarExpressao("wink");
            return "Por nada! Mesmo offline, estou sempre pronto pra ajudar!";
        }

        sfx.play(SFX_SAD, 0.7f);
        return "Estou sem acesso à nuvem do Gemini no momento. No modo offline você ainda pode pedir LED, hora, timers, Pomodoro, tarefas, emoções, volume ou dormir!";
    }
};

#endif // CIBORGUE_OFFLINE_ENGINE_H
