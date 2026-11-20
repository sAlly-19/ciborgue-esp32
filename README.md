# Ciborgue ESP32-S3

O Ciborgue é um robô de mesa com rosto animado, sensor de carinho e funções de assistente. O firmware roda no **ESP32-S3**, desenha as expressões numa tela **JD9853 de 296 × 240** e usa a **Bancada Virtual** no Windows para exibir o mesmo personagem, conversar por voz e controlar o robô.

![Galeria das 21 expressões do Ciborgue no app de bancada](docs/ciborgue-faces.png)

## O que ele faz

- Mostra 21 expressões com piscadas, olhares em repouso, partículas e boca sincronizada com a voz reproduzida pela bancada.
- Reage a carinho e conversas, ajusta a frequência de chirps conforme o horário das interações e reduz o brilho no modo Eco.
- Mantém tarefas, memórias confirmadas, alarmes e configurações na memória não volátil do ESP32. Também oferece timer, Pomodoro, clima e relógio de mesa.
- Recebe comandos por USB ou Wi-Fi. O serviço Bluetooth LE expõe brilho e configurações de som para clientes pareados.

## Como funciona

1. A bancada captura a voz, envia a pergunta e reproduz a resposta. Com USB, ela também envia amostras da fala para mover a boca no visor físico.
2. O firmware resolve comandos locais primeiro. Quando há Wi-Fi e uma chave configurada, o Gemini responde a perguntas gerais e pode chamar as ferramentas do robô. Sem acesso ao Gemini, as funções locais continuam disponíveis.
3. O ESP32 desenha o rosto num canvas RGB565 e envia ao JD9853 por SPI apenas as regiões que mudaram. A bancada usa PySide6 e serve de referência para as poses e bocas do firmware.
4. A NVS guarda as configurações e os dados que precisam sobreviver a um reinício. Preferências inferidas pelo Gemini ficam pendentes até a confirmação do usuário.

## Começar no Windows

Você precisa de Python 3, um ESP32-S3 DevKitC-1 N16R8 e um cabo USB de dados para usar o hardware físico. O projeto configura a **COM4** para gravação e monitoramento. Se a sua placa usar outra porta, ajuste `platformio.ini`.

1. Clone o repositório e instale as dependências.

   ```powershell
   git clone https://github.com/sAlly-19/ciborgue-esp32.git
   cd ciborgue-esp32
   python -m pip install platformio -r requirements.txt
   ```

2. Copie `.env.example` para `.env`. Preencha `WIFI_SSID` e `WIFI_PASSWORD` para configurar a rede e `GEMINI_API_KEY` se quiser usar o Gemini. O arquivo `.env` fica local.
3. Feche o monitor serial e a bancada antes de gravar. Depois, abra a bancada.

   ```powershell
   python -m platformio run -t upload
   python ciborgue.py
   ```

   Também é possível abrir a bancada com [iniciar_ciborgue.bat](iniciar_ciborgue.bat). Para acompanhar o firmware sem ela, use `python -m platformio device monitor -b 115200`.

Se o ESP32 iniciar sem uma rede configurada, conecte-se ao ponto de acesso **Ciborgue-Setup** para fazer a configuração. Quando estiver na rede local, a API costuma ficar disponível em `http://ciborgue.local` ou pelo IP exibido na Serial.

### Hardware

| Componente | Ligação |
| --- | --- |
| JD9853, SPI | SCK 12, MOSI 11, RES 10, DC 9, CS 13, BLK 14 |
| Sensor capacitivo de carinho | GPIO 1 |
| LED RGB da placa | GPIO 48 |
| Microfone INMP441 e amplificador MAX98357A | Opcionais. O áudio físico vem desativado na configuração padrão. |

O pinout e as opções do firmware ficam em [include/config.h](include/config.h). Desative `ENABLE_PHYSICAL_DISPLAY` para compilar sem a tela física.

## Controle e integração

| Interface | Operações |
| --- | --- |
| USB Serial, 115200 baud | `PING`, `MIC:<pergunta>`, `CMD:EMOTION=CURIOUS`, `CMD:TFT_BRI?` e `CMD:TFT_BRI=128` |
| HTTP, rede local | `GET /api/status`, `POST /api/chat` com `{"message":"..."}`, `GET /api/display` e `POST /api/display` com `{"brightness":128}` |
| Bluetooth LE | Consulta e ajuste de brilho, volume, mudo e chirps em conexão autenticada |

O brilho configurado vai de **0 a 255**, persiste após reiniciar e fica limitado a **35** durante o Eco. A bancada também ajusta esse valor na interface.

### Contrato BLE para Android

- Nome anunciado `Ciborgue`. Serviço GATT `6c730001-18af-4d4f-9bdc-c034e99e1400`.
- Característica de leitura e escrita com resposta `6c730002-18af-4d4f-9bdc-c034e99e1400`.
- O pacote tem **5 bytes sem sinal**, na ordem `[versão, brilho, volume, mudo, chirps]`. Use versão 1, brilho de 0 a 255, volume de 0 a 100 e 0 ou 1 para mudo e chirps.
- Leia o pacote atual, altere o campo desejado e escreva o pacote completo. Releia para confirmar a configuração que foi aplicada.
- Consulte o PIN de pareamento na Serial com `CMD:BLE_PIN`. Para fechar ou reabrir o acesso BLE, use `CMD:BLE=0` ou `CMD:BLE=1`.

O [app Android](ciborgue-app/README.md) usa Expo e React Native. O guia do app descreve os pré-requisitos, a instalação e a conexão ao robô.

## Estrutura do projeto

| Caminho | Função |
| --- | --- |
| [src/main.cpp](src/main.cpp) | Inicialização, loop e comandos seriais |
| [include/](include/) | Display, áudio, rede, memória e comportamento do robô |
| [app/](app/) | Bancada Virtual em Python e PySide6 |
| [ciborgue-app/](ciborgue-app/) | Código-fonte do app Android |
| [tools/generate_bench_faces.py](tools/generate_bench_faces.py) | Gera os assets do firmware a partir do renderizador Qt |

## Licença

O código do projeto é distribuído sob a [Licença MIT](LICENSE). Os avisos de licença de terceiros acompanham os respectivos componentes.
