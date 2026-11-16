# 🤖 Ciborgue ESP32-S3 — Cérebro Autônomo Gemini + Simulador 296x240 + API Wi-Fi Android

Robô de mesa inteligente baseado no **ESP32-S3 (`N16R8` - 16MB Flash Quad / 8MB PSRAM Octal)** com cérebro **Google Gemini** rodando nativamente em C++ na placa, 6 ferramentas de hardware integradas, **Servidor HTTP REST + Auto-Discovery UDP para App Android** e bancada virtual em Python/PySide6 para a tela **Estardyn 2.01" TFT (`296x240` Paisagem - JD9853)**.

---

## 🚀 Arquitetura do Projeto

```
ESP-32/
├── platformio.ini              # Configuração PlatformIO (ESP32-S3 N16R8 + 8MB PSRAM qio_opi)
├── include/
│   ├── config.h                # Pinout seguro ESP32-S3, flags físicas e modelo Gemini
│   ├── robot_types.h           # Estados (IDLE, LISTEN, THINK, SPEAK, ALERT) e 8 Emoções
│   ├── nvs_config.h            # Persistência Flash NVS (Wi-Fi, Gemini API Key, Cidade) + NTP
│   ├── memory_manager.h        # Memória de Longo Prazo e Lista de Tarefas (To-Do) na Flash NVS
│   ├── pomodoro_manager.h      # Máquina de estados do Companheiro de Estudo Pomodoro
│   ├── touch_sensor.h          # Sensor de Carinho na Cabeça (Touch Capacitivo Nativo GPIO 1)
│   ├── led_controller.h        # Driver não-bloqueante 50 FPS WS2812 (GPIO 48)
│   ├── display_hal.h           # Driver para tela Estardyn 2.01" JD9853 (296x240)
│   ├── audio_hal.h             # Driver I2S para INMP441 (Mic) e MAX98357A (Amp)
│   ├── tools_controller.h      # 11 Ferramentas C++ acionadas pelo Gemini via Function Calling
│   ├── gemini_brain.h          # Cliente HTTPS Gemini C++ com sessão TLS reutilizável e contexto NVS
│   └── wifi_api_server.h       # Servidor HTTP REST (Porta 80) + mDNS (ciborgue.local) + UDP 4210
├── src/
│   └── main.cpp                # Loop principal + Task FreeRTOS Core 0 (50 FPS) + Protocolo Serial/Wi-Fi
├── app/                        # Pacote Python modular da Bancada Virtual PC (PySide6)
│   ├── config.py               # Vozes, prosódia emocional e constantes da tela 296x240
│   ├── signals.py              # Barramento Qt Signals thread-safe
│   ├── physics.py              # Física de mola Harmônica (Squash & Stretch) e partículas
│   ├── sfx.py                  # Efeitos sonoros robóticos em memória
│   ├── widgets_overlay.py      # Renderizador de Mini-Widgets Holográficos 296x240 e Barra Pomodoro
│   ├── face_canvas.py          # Renderizador 60 FPS estilo Eilik/BMO + Layout Dividido Híbrido
│   ├── pc_controller.py        # Controle Total do PC Windows por Voz (Mídia, Volume, Apps, Sessão, Status)
│   ├── memory_panel.py         # Painel Visual de Tarefas NVS, Memórias, Pomodoro e Carinho
│   ├── audio_tts.py            # Pipeline TTS de baixa latência (~0.8s) com LipSync PCM em tempo real
│   ├── mic_listener.py         # Captura de voz sounddevice 16kHz + Wake Word "Ciborgue"
│   ├── serial_bridge.py        # Ponte USB Serial auto-reconectável com espelho JSON local
│   └── main_window.py          # Janela principal da bancada virtual e telemetria
├── ciborgue.py                 # Ponto de entrada da aplicação PC
└── PROMPT_ANDROID_APP.md       # Prompt completo para gerar o App Android conectado ao ESP32-S3
```

---

## 📡 API Wi-Fi HTTP para App Android (`include/wifi_api_server.h`)

Assim que o ESP32-S3 conecta ao Wi-Fi, ele inicia um servidor HTTP na **porta 80** (`http://<IP_DO_ESP32>` ou `http://ciborgue.local`) e escuta pacotes de descoberta na **porta UDP 4210** (`"CIBORGUE_DISCOVER"`):

| Método | Rota | Corpo JSON (Exemplo) | Descrição |
|---|---|---|---|
| `GET` | `/api/status` | — | Retorna telemetria completa (`state`, `emotion`, `temp_c`, `wifi_rssi`, `ip`, `heap_kb`, `psram_kb`, `led_r/g/b`, `timer_rem_s`, `last_reply`) |
| `POST` | `/api/chat` | `{"message": "Oi Ciborgue!"}` | Envia mensagem ao cérebro Gemini C++ no ESP32-S3 e retorna `{"ok":true, "reply":"...", "emotion":"HAPPY"}` |
| `POST` | `/api/emotion` | `{"emotion": "HAPPY"}` | Muda expressão (`NEUTRAL`, `HAPPY`, `SAD`, `ANGRY`, `SURPRISED`, `WINK`, `LOVE`, `SLEEPY`) |
| `POST` | `/api/state` | `{"state": "LISTEN"}` | Muda estado (`IDLE`, `LISTEN`, `THINK`, `SPEAK`, `SLEEP`, `ALERT`) |
| `POST` | `/api/led` | `{"color": "roxo", "effect": "pulsar"}` ou `{"r":0,"g":255,"b":120,"mode":"SOLID"}` | Controla o LED RGB WS2812 no GPIO 48 |
| `POST` | `/api/timer` | `{"seconds": 60, "label": "Café"}` | Agenda timer/alarme no ESP32-S3 |
| `GET/POST` | `/api/tasks` | `{"action": "adicionar", "title": "Estudar C++"}` | Lista ou gerencia Tarefas (To-Do) na Flash NVS |
| `GET/POST` | `/api/memories` | `{"action": "adicionar", "fact": "Prova sexta"}` | Lista ou grava Memórias de Longo Prazo na Flash NVS |
| `POST` | `/api/pomodoro` | `{"action": "iniciar", "focus_min": 25, "break_min": 5, "goal": "Código"}` | Inicia ou para o Companheiro Pomodoro |
| `POST` | `/api/pet` | `{}` | Dispara reação de carinho na cabeça (`LOVE` + LED rosa + fala) |
| `POST` | `/api/config` | `{"wifi_ssid":"...","wifi_password":"...","gemini_key":"...","city":"Sao Paulo"}` | Salva configurações na memória Flash NVS |
| `POST` | `/api/command` | `{"cmd": "CMD:EMOTION=WINK"}` | Executa qualquer comando do protocolo Serial |
| `POST` | `/api/clear_history` | `{}` | Limpa o histórico de conversa na memória do ESP32-S3 |

Para criar o aplicativo Android que controla o Ciborgue, consulte o arquivo [`PROMPT_ANDROID_APP.md`](PROMPT_ANDROID_APP.md).
