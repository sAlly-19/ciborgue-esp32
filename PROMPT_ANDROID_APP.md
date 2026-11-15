# 📱 Prompt Oficial para IA de Desenvolvimento Android — App Companion "Ciborgue ESP32-S3"

Copie todo o bloco abaixo e cole na sua IA de desenvolvimento Android (junto com o link do repositório `https://github.com/sAlly-19/ciborgue-esp32`):

---

```markdown
Você é um Engenheiro Android Sênior especialista em Kotlin, Jetpack Compose, Material Design 3, Coroutines/Flow e comunicação IoT (HTTP REST / UDP Discovery / USB OTG Serial).

Sua missão é criar o aplicativo Android oficial de controle, telemetria em tempo real, voz e rosto virtual do robô de mesa **"Ciborgue ESP32-S3"**, seguindo rigorosamente a arquitetura, rotas de API, estados, expressões faciais e protocolo já implementados no repositório oficial do projeto:
🔗 Repositório de referência: https://github.com/sAlly-19/ciborgue-esp32

Consulte especialmente os arquivos do repositório:
- `include/wifi_api_server.h` (Servidor HTTP REST na porta 80 + Auto-Discovery UDP na porta 4210 + mDNS `ciborgue.local`)
- `include/robot_types.h` (Estados operacionais, as 8 emoções faciais e os modos de efeito do LED WS2812)
- `include/tools_controller.h` (As 6 ferramentas nativas em C++ do cérebro autônomo Gemini no ESP32-S3)
- `src/main.cpp` (Protocolo Serial USB `115200 8N1` e ciclo de vida de estados/timers)
- `app/face_canvas.py` e `app/physics.py` (Referência visual da tela 296x240 paisagem estilo Eilik/Cozmo/BMO com física de mola Squash & Stretch, olhos expressivos e boca sincronizada)

---

## 1. Formas de Conexão com o ESP32-S3 (Modo Duplo: Wi-Fi HTTP + USB OTG Opcional)

O firmware do ESP32-S3 (`include/wifi_api_server.h` e `src/main.cpp`) já expõe nativamente:

### A) Conexão Principal: Wi-Fi Local (HTTP REST na Porta 80 + Auto-Discovery UDP 4210)
1. **Auto-Descoberta na Rede Local (1 clique):**
   - O app deve enviar um broadcast UDP na porta `4210` com a string `"CIBORGUE_DISCOVER"`.
   - O ESP32-S3 responde imediatamente um JSON:
     `{"device":"Ciborgue ESP32-S3","ip":"10.204.85.188","port":80,"hostname":"ciborgue.local"}`
   - Caso a rede bloqueie broadcast UDP, permita também tentar `http://ciborgue.local` ou digitar o IP manualmente (salvo em `DataStore`/`SharedPreferences`).
2. **Polling de Telemetria em Tempo Real (`GET /api/status` a cada 1.5s–2s):**
   - Retorna JSON:
     ```json
     {
       "ok": true,
       "device": "Ciborgue ESP32-S3 (N16R8)",
       "hostname": "ciborgue.local",
       "state": "IDLE",
       "emotion": "HAPPY",
       "last_reply": "Agora são 20:30!",
       "temp_c": 36.5,
       "wifi_ok": true,
       "wifi_ssid": "Café",
       "wifi_rssi": -54,
       "ip": "10.204.85.188",
       "has_key": true,
       "city": "Sao Paulo",
       "heap_kb": 276,
       "psram_kb": 8177,
       "uptime_s": 420,
       "led_r": 0,
       "led_g": 255,
       "led_b": 120,
       "timer_rem_s": 0,
       "timer_label": ""
     }
     ```

### B) Endpoints REST do Firmware (`include/wifi_api_server.h`)
Implemente no `CiborgueApiService` (Retrofit / OkHttp ou Ktor Client com timeout de 15s para `/api/chat`):

1. **`GET /api/status`**
   - Obtém telemetria completa do hardware e estado atual do robô.
2. **`POST /api/chat`**
   - Body: `{"message": "Oi Ciborgue, que horas são e mude o LED para roxo!"}`
   - O próprio ESP32-S3 processa a conversa com o Gemini em C++, executa as Tools nativas (hora NTP, clima Open-Meteo, timer, LED RGB, expressão facial) e retorna:
     ```json
     {
       "ok": true,
       "reply": "Agora são 20:30 e já deixei meu LED roxo pra você!",
       "state": "IDLE",
       "emotion": "HAPPY",
       "led_r": 180,
       "led_g": 0,
       "led_b": 255
     }
     ```
3. **`POST /api/emotion`**
   - Body: `{"emotion": "HAPPY"}`
   - Valores aceitos pelo firmware: `"NEUTRAL"`, `"HAPPY"`, `"SAD"`, `"ANGRY"`, `"SURPRISED"`, `"WINK"`, `"LOVE"`, `"SLEEPY"`.
4. **`POST /api/state`**
   - Body: `{"state": "IDLE"}`
   - Valores aceitos: `"IDLE"`, `"LISTEN"`, `"THINK"`, `"SPEAK"`, `"SLEEP"`, `"ALERT"`, `"CUSTOM"`.
5. **`POST /api/led`**
   - Aceita controle por nome/efeito: `{"color": "roxo", "effect": "pulsar"}` (`solido`, `pulsar`, `arco_iris`, `alerta`, `desligar`)
   - Ou controle direto RGB: `{"r": 0, "g": 255, "b": 120, "mode": "SOLID"}` (`SOLID`, `PULSE`, `RAINBOW`, `ALERT`).
6. **`POST /api/timer`**
   - Body: `{"seconds": 60, "label": "Tirar o café"}`
7. **`POST /api/config`**
   - Body: `{"wifi_ssid": "...", "wifi_password": "...", "gemini_key": "...", "city": "Sao Paulo"}`
   - Grava permanentemente na memória Flash NVS do ESP32-S3.
8. **`POST /api/command`**
   - Body: `{"cmd": "CMD:EMOTION=WINK"}`
   - Ponte direta compatível com qualquer comando do protocolo Serial (`PING`, `MIC:<texto>`, `CMD:STATE=<st>`, `CMD:EMOTION=<emo>`, `CMD:COLOR=<cor>,<efeito>`, `RGB:<r>,<g>,<b>`, `CMD:CLEAR_HISTORY`, `TTS:DONE`).
9. **`POST /api/clear_history`**
   - Limpa o histórico multi-turn da memória RAM/PSRAM do ESP32-S3.

---

## 2. Funcionalidades e Telas que o App Android Deve Ter

Construa uma interface moderna em **Jetpack Compose (Tema Dark Cyberpunk/Sci-Fi `#0A0E17` com acentos Ciano `#38BDF8` e Verde `#22C55E`)**:

1. **Rosto Virtual Interativo 296x240 em Tempo Real (Canvas Compose):**
   - Renderize no topo do app (e permita modo Tela Cheia Horizontal/Landscape para usar o próprio celular como cabeça do robô caso queira!) o rosto do Ciborgue seguindo `app/face_canvas.py`:
     - Olhos 3D estilo Eilik/Cozmo com brilho especular, pálpebras animadas para as 8 emoções (`NEUTRAL`, `HAPPY`, `SAD`, `ANGRY`, `SURPRISED`, `WINK`, `LOVE`, `SLEEPY`), piscada orgânica e partículas flutuantes (corações em `LOVE`, `Zzz` em `SLEEPY`, faíscas em `HAPPY`).
     - Boca vetorial estilo BMO com dentinhos, língua e animação de LipSync sincronizada quando o celular estiver falando a resposta (`reply`) via TTS!
     - Física de mola (*Squash & Stretch*) ao tocar no rosto ou mudar de emoção.
2. **Voz Bidirecional no Android (STT + TTS Sincronizado):**
   - Botão **🎤 Falar com o Ciborgue** (usando `SpeechRecognizer` nativo do Android em `pt-BR` + opção de escuta contínua).
   - Ao iniciar a escuta no celular, envie `POST /api/state` com `{"state": "LISTEN"}`.
   - Ao obter o texto falado, envie para `POST /api/chat` com `{"message": texto}`.
   - Ao receber `reply`, envie `POST /api/state` com `{"state": "SPEAK"}`, fale a resposta em voz alta no celular (com `TextToSpeech` em `pt-BR` ou Gemini TTS) animando a boca no Canvas, e no `onDone` envie `POST /api/command` com `{"cmd": "TTS:DONE"}` para o ESP32-S3 voltar para `IDLE`.
3. **Painel de Controle Rápido (Expressões & LED RGB GPIO 48):**
   - Grade de botões com ícones para testar instantaneamente as 8 expressões (`POST /api/emotion`).
   - Seletor de cores RGB (Color Wheel + Presets: Vermelho, Verde, Azul, Roxo, Ciano, Arco-Íris, Pulsar, Alerta, Apagar) enviando para `POST /api/led`.
4. **Card de Telemetria em Tempo Real do ESP32-S3:**
   - Exiba ao vivo os dados de `GET /api/status`: Temperatura interna do chip (`°C`), RSSI do Wi-Fi (`dBm`), IP local, RAM Livre (`KB`), PSRAM Octal Livre (`8177 KB`), Uptime, cor atual do LED e contagem regressiva de Timer ativo (`timer_rem_s`).
5. **Aba/Modal de Provisionamento NVS e Conexão:**
   - Botão **"🔍 Procurar Ciborgue na Rede Wi-Fi (UDP 4210)"** + campo de IP manual.
   - Formulário para gravar remotamente na Flash NVS do ESP32-S3 (`POST /api/config`): Wi-Fi SSID, Senha, Gemini API Key e Cidade Padrão do Clima.

---

## 3. Entregáveis Esperados
- Estrutura completa do projeto Android (Kotlin + Jetpack Compose + MVVM `CiborgueViewModel` + `CiborgueRepository`).
- `AndroidManifest.xml` configurado com permissões `INTERNET`, `ACCESS_NETWORK_STATE`, `ACCESS_WIFI_STATE`, `RECORD_AUDIO` e `usesCleartextTraffic="true"` (indispensável no Android 9+ para acessar o HTTP local do ESP32-S3 em `http://10.204.85.188`).
- Código completo, compilável e sem placeholders.
```
