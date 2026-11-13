# 🤖 Ciborgue - Robôzinho de Mesa com ESP32-S3 & Gemini AI

Robô de mesa inteligente, conversacional e com interação por voz em tempo real (estilo *Gemini Live*), ativado por comando de voz (**Wake Word: "Ciborgue"**) e com controle de hardware em baixíssima latência via **ESP32-S3**.

> Repositório oficial: [https://github.com/sAlly-19/ciborgue-esp32](https://github.com/sAlly-19/ciborgue-esp32)

---

## ⚡ Visão Geral da Arquitetura

O projeto combina a inteligência multimodal do **Google Gemini (2.5 Flash)** com o poder de processamento do **ESP32-S3**.

```
[ Usuário ]
    │
    ▼ (Voz: "Ciborgue...")
[ Microfone ] ──> [ Detecção de Wake Word ]
                          │
                          ▼
             [ ESP32-S3: LED Verde Instantâneo ] (COM4 / Serial)
                          │
                          ▼
            [ Cérebro Gemini 2.5 Flash + Tools ]
                          │
            ┌─────────────┴─────────────┐
            ▼                           ▼
  [ Síntese de Voz ]           [ Execução de Tarefas ]
  (ESP32: LED Ciano)           (ESP32: Troca cor LED, Servos, GPIOs)
```

---

## 🛠️ Hardware Utilizado

* **Microcontrolador:** ESP32-S3-WROOM-1 (`N16R8`)
  * **Processador:** Dual-Core Xtensa LX7 até 240 MHz
  * **Memória Flash:** 16 MB
  * **Memória PSRAM:** 8 MB (Octal SPI)
  * **Portas USB:** 2x USB-C (1x Nativa OTG / 1x UART CH343)
* **Feedback Visual Onboard:** LED RGB endereçável WS2812 no pino **`GPIO 48`**
* **Expansões previstas (em andamento):**
  * Microfone Digital I2S (`INMP441`)
  * Amplificador de Áudio I2S (`MAX98357A`) + Mini Alto-falante
  * Servo-motores para movimentação de cabeça/braços

---

## 💡 Máquina de Estados Visuais (LED RGB)

O firmware no ESP32 gerencia estados visuais instantâneos para indicar o que o robô está fazendo:

| Estado | Cor do LED | Significado |
| :--- | :--- | :--- |
| **`STATE:IDLE`** | 🔵 Azul (respiração suave) | Dormindo / Esperando a palavra de ativação |
| **`STATE:LISTEN`** | 🟢 Verde vivo | Acordou! Escutando o comando do usuário |
| **`STATE:THINK`** | 🟣 Roxo pulsante | Processando o pensamento / Chamada de IA |
| **`STATE:SPEAK`** | 💠 Ciano oscilante | Falando a resposta em voz alta |
| **`STATE:ALERT`** | 🟠 Laranja / Amarelo | Alerta ou aviso do sistema |

---

## 📂 Estrutura do Projeto

```
ciborgue-esp32/
├── .env.example          # Exemplo de configuração de variáveis de ambiente
├── .gitignore            # Filtro para ignorar builds (.pio/) e segredos (.env)
├── ciborgue.py           # Orquestrador Python (Wake Word, Áudio, Gemini & Serial)
├── platformio.ini        # Configuração de build e upload da placa ESP32-S3
├── requirements.txt      # Dependências Python necessárias
├── src/
│   └── main.cpp          # Firmware C++ (Arduino Core) com controle do WS2812 e Serial
└── include/
```

---

## 🚀 Como Executar

### 1. Clonar o Repositório e Instalar Dependências

```bash
git clone https://github.com/sAlly-19/ciborgue-esp32.git
cd ciborgue-esp32

# Instalar as bibliotecas Python
pip install -r requirements.txt
```

### 2. Configurar a Chave de API

Copie o arquivo de exemplo e insira sua chave gratuita do [Google AI Studio](https://aistudio.google.com/):

```bash
cp .env.example .env
```

Edite o arquivo `.env`:
```ini
GEMINI_API_KEY=sua_chave_aqui
SERIAL_PORT=COM4
BAUD_RATE=115200
WAKE_WORD=ciborgue
```

### 3. Gravar o Firmware no ESP32-S3

Com a placa conectada na porta USB:

```bash
# Compilar e fazer upload do firmware
pio run --target upload
```

### 4. Iniciar o Ciborgue

```bash
python ciborgue.py
```

Assim que a mensagem de prontidão aparecer, basta falar no microfone:
* *"Ciborgue, acenda a luz vermelha"*
* *"Ciborgue, que horas são?"*
* *"Ciborgue, conte uma curiosidade sobre robôs"*

---

## 🔧 Ferramentas Integradas (Function Calling)

O Ciborgue possui suporte a **Tool Calling nativo** do Gemini. Atualmente ele pode:
1. `tool_mudar_cor_led(cor)`: Altera em tempo real a cor do LED RGB do corpo.
2. `tool_obter_hora()`: Informa as horas com precisão.
3. `tool_mover_robo(direcao, angulo)`: Prepara o posicionamento angular dos servos.

---

## 📄 Licença

Distribuído sob a licença MIT. Sinta-se à vontade para modificar e expandir!
