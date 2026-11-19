# 🤖 Ciborgue ESP32-S3 — Cérebro Autônomo Gemini + Simulador 296x240 + API Wi-Fi

Robô de mesa inteligente baseado no **ESP32-S3 (`N16R8` — 16MB Flash / 8MB PSRAM `qio_opi`)** com cérebro **Google Gemini** rodando nativamente em C++ na placa, **16 ferramentas** de hardware integradas via *Function Calling*, **Portal Cativo Wi-Fi**, **síntese sonora nativa (I2S/SFX)**, **Sono Inteligente em 2 estágios**, **Engine Offline PT-BR**, **14 emoções com transições suaves (Morph Paramétrico + Bounce Elástico)**, **Servidor HTTP REST + Auto-Discovery UDP (`4210`)** e bancada virtual em Python/PySide6 para a tela **Estardyn 2.01" TFT (`296x240` Paisagem — JD9853)**.

---

## 🚀 Arquitetura do Projeto

```text
ESP-32/
├── platformio.ini              # Configuração PlatformIO (ESP32-S3 N16R8 + 8MB PSRAM qio_opi)
├── gravar_firmware.bat         # Script de compilação e gravação via USB no ESP32-S3
├── iniciar_ciborgue.bat        # Script de inicialização da Bancada Virtual PC
├── include/
│   ├── config.h                # Pinout ESP32-S3, flags físicas e constantes do firmware
│   ├── robot_types.h           # Estados (IDLE, LISTEN, THINK, SPEAK, ALERT, PORTAL, SLEEP) e 14 Emoções
│   ├── nvs_config.h            # Persistência Flash NVS (Wi-Fi, Gemini API Key, Cidade, Áudio) + Backoff Wi-Fi + NTP
│   ├── captive_portal.h        # Portal Cativo Wi-Fi ("Ciborgue-Setup") com DNS wildcard e scanner HTTP
│   ├── memory_manager.h        # Memória de Longo Prazo e Lista de Tarefas (To-Do) na Flash NVS
│   ├── personality_manager.h   # Personalidade, Afeto/Energia/Humor em Flash NVS ("cib_pet") + micro-reações
│   ├── alarm_manager.h         # Até 6 despertadores recorrentes NTP persistidos na Flash NVS ("cib_alm")
│   ├── pomodoro_manager.h      # Máquina de estados do Companheiro de Estudo Pomodoro
│   ├── touch_sensor.h          # Sensor de Carinho na Cabeça (Touch Capacitivo Nativo GPIO 1)
│   ├── led_controller.h        # Driver não-bloqueante 50 FPS WS2812 (GPIO 48)
│   ├── display_hal.h           # Driver 296x240 JD9853 (LovyanGFX): Morph 50 FPS, Cards Híbridos v2 e Pílula Topo
│   ├── audio_hal.h             # Driver I2S para INMP441 (Mic) e MAX98357A (Amp)
│   ├── sfx_synth.h             # Sintetizador sonoro nativo em FreeRTOS (Wall-E / R2-D2 + chirps ociosos)
│   ├── power_manager.h         # Sono Inteligente em 2 estágios (Eco + Light Sleep) e botão BOOT (GPIO 0)
│   ├── offline_engine.h        # Engine local PT-BR (Fallback offline + Fast-Path <5ms opcional)
│   ├── tools_controller.h      # 16 Ferramentas C++ acionadas pelo Gemini via Function Calling
│   ├── gemini_brain.h          # Cliente HTTPS Gemini C++ com TLS reutilizável, Circuit Breaker e contexto NVS
│   └── wifi_api_server.h       # Servidor HTTP REST (Porta 80) + mDNS (ciborgue.local) + UDP 4210
├── src/
│   └── main.cpp                # Loop principal + Task FreeRTOS Core 0 (50 FPS) + Protocolo Serial/Wi-Fi
├── app/                        # Pacote Python modular da Bancada Virtual PC (PySide6)
│   ├── config.py               # Vozes, prosódia emocional e constantes da tela 296x240
│   ├── signals.py              # Barramento Qt Signals thread-safe
│   ├── physics.py              # Física de mola harmônica e funções de easing (ease_out_quad/cubic/back)
│   ├── sfx.py                  # Efeitos sonoros robóticos em memória
│   ├── widgets_overlay.py      # Renderizador Híbrido v2 vetorial (Cards de Tela Cheia + Pílula Superior)
│   ├── face_canvas.py          # Renderizador 60 FPS OLED puro (14 emoções com morph suave e sem bordas)
│   ├── pc_controller.py        # Controle do PC Windows por Voz (Mídia, Volume, Apps, Sessão, Status)
│   ├── memory_panel.py         # Painel Visual NVS (Tarefas, Memórias, Pomodoro, Alarmes, Humor & Relógio)
│   ├── audio_tts.py            # Pipeline TTS de baixa latência com LipSync PCM em tempo real
│   ├── mic_listener.py         # Captura de voz sounddevice 16kHz + Wake Word "Ciborgue"
│   ├── serial_bridge.py        # Ponte USB Serial + Wi-Fi auto-reconectável com espelho JSON local
│   └── main_window.py          # Janela principal da bancada virtual e telemetria
└── ciborgue.py                 # Ponto de entrada da aplicação PC
```

---

## 📡 API Wi-Fi HTTP REST (`include/wifi_api_server.h`)

Assim que o ESP32-S3 conecta ao Wi-Fi, ele inicia um servidor HTTP na **porta 80** (`http://<IP_DO_ESP32>` ou `http://ciborgue.local`) e escuta pacotes de descoberta na **porta UDP 4210** (`"CIBORGUE_DISCOVER"`):

| Método | Rota | Corpo JSON (Exemplo) | Descrição |
|---|---|---|---|
| `GET` | `/api/status` | — | Retorna telemetria completa (`state`, `emotion`, `temp_c`, `wifi_rssi`, `ip`, `heap_kb`, `psram_kb`, `led_r/g/b`, `timer_rem_s`, `desk_clock`, `alarm_ringing`, `affection`, `energy`, `mood`, `last_reply`) |
| `POST` | `/api/chat` | `{"message": "Oi Ciborgue!"}` | Envia mensagem ao cérebro Gemini C++ (com Fast-Path / Fallback Offline) e retorna `reply` + `emotion` |
| `POST` | `/api/emotion` | `{"emotion": "HAPPY"}` | Muda expressão entre as 14 emoções (`NEUTRAL`, `HAPPY`, `SAD`, `ANGRY`, `SURPRISED`, `WINK`, `LOVE`, `SLEEPY`, `FEAR`, `DISGUST`, `CONTEMPT`, `SHAME`, `DESPAIR`, `MALICE`) |
| `POST` | `/api/state` | `{"state": "LISTEN"}` | Muda estado (`IDLE`, `LISTEN`, `THINK`, `SPEAK`, `ALERT`, `PORTAL`, `SLEEP`) |
| `POST` | `/api/led` | `{"color": "roxo", "effect": "pulsar"}` ou `{"r":0,"g":255,"b":120,"mode":"SOLID"}` | Controla o LED RGB WS2812 no GPIO 48 |
| `POST` | `/api/timer` | `{"seconds": 60, "label": "Café"}` | Agenda timer no ESP32-S3 com card e pílula superior |
| `GET/POST` | `/api/tasks` | `{"action": "adicionar", "title": "Estudar C++"}` | Lista ou gerencia Tarefas (To-Do) na Flash NVS |
| `GET/POST` | `/api/memories` | `{"action": "adicionar", "fact": "Prova sexta"}` | Lista ou grava Memórias de Longo Prazo na Flash NVS |
| `POST` | `/api/pomodoro` | `{"action": "iniciar", "focus_min": 25, "break_min": 5, "goal": "Código"}` | Inicia ou para o Companheiro Pomodoro |
| `GET/POST` | `/api/alarms` | `{"action": "add", "hour": 7, "minute": 30, "daysMask": 127, "label": "Acordar"}` | Lista, cria, remove, alterna ou silencia até 6 despertadores recorrentes NVS |
| `GET/POST` | `/api/personality` | `{}` | Consulta o estado de Afeto/Energia/Humor (com `POST` ou `?card=1` exibe o card `MOOD` na tela) |
| `POST` | `/api/desk_clock` | `{"active": true}` | Ativa ou desativa o Modo Relógio de Mesa persistente |
| `POST` | `/api/pet` | `{}` | Dispara reação de carinho na cabeça (`LOVE` + LED rosa + evolução de afeto) |
| `POST` | `/api/audio` | `{"volume": 75, "mute": false, "idle_chirps": true}` | Ajusta volume/mudo/chirps ociosos do sintetizador SFX ou toca som (`play`) |
| `POST` | `/api/sleep` | `{"mode": "eco"}` | Alterna Eco Mode, Light Sleep (`"light"`) ou despertar (`"wake"`) |
| `POST` | `/api/portal` | `{"action": "start"}` | Abre ou fecha o Portal Cativo `Ciborgue-Setup` |
| `POST` | `/api/config` | `{"wifi_ssid":"...","wifi_password":"...","gemini_key":"...","city":"Sao Paulo"}` | Salva configurações na memória Flash NVS |
| `POST` | `/api/command` | `{"cmd": "CMD:EMOTION=WINK"}` | Executa qualquer comando do protocolo Serial |
| `POST` | `/api/clear_history` | `{}` | Limpa o histórico de conversa na memória do ESP32-S3 |
