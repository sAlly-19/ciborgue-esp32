#ifndef CIBORGUE_CONFIG_H
#define CIBORGUE_CONFIG_H

#include <Arduino.h>

// ============================================================================
// 1. FLAGS DE HARDWARE (Mude para 1 assim que as peças chegarem do AliExpress)
// ============================================================================
#define ENABLE_PHYSICAL_DISPLAY 0   // 1 = Ativa Tela Estardyn 2.01" TFT 240x296 (JD9853 SPI)
#define ENABLE_PHYSICAL_AUDIO   0   // 1 = Ativa Microfone INMP441 + Amplificador MAX98357A (I2S)
#define ENABLE_TOUCH_SENSOR     1   // 1 = Ativa Sensor Touch Capacitivo Nativo no GPIO 1 (Carinho na Cabeça)

// ============================================================================
// 2. PINOUT SEGURO PARA ESP32-S3-WROOM-1 (N16R8 - Evita GPIOs 26-37 da PSRAM/Flash)
// ============================================================================

// LED RGB Onboard (WS2812 Endereçável)
#define PIN_RGB_LED         48

// Sensor de Carinho na Cabeça (Touch Capacitivo Nativo Touch1 do ESP32-S3)
#define PIN_TOUCH_HEAD       1      // GPIO 1 (Basta ligar a um pad/fita metálica no topo da cabeça)

// Tela Estardyn 2.01" TFT LCD 240x296 (Controlador JD9853 - SPI 8 Pinos)
// Serigrafia dos 8 pinos: GND | VCC (3.3V) | SCL | SDA | RES | DC | CS | BLK
#define PIN_TFT_SCK         12      // Pino SCL da tela
#define PIN_TFT_MOSI        11      // Pino SDA da tela
#define PIN_TFT_RES         10      // Pino RES (Reset) da tela
#define PIN_TFT_DC           9      // Pino DC (Data/Command) da tela
#define PIN_TFT_CS          13      // Pino CS (Chip Select) da tela
#define PIN_TFT_BLK         14      // Pino BLK (Backlight) da tela

// Dimensões da Tela (Modo Paisagem Horizontal: 296x240)
#define TFT_NATIVE_WIDTH   240
#define TFT_NATIVE_HEIGHT  296
#define FACE_SCREEN_W      296
#define FACE_SCREEN_H      240

// Microfone Digital I2S (INMP441) - Porta I2S_NUM_0
#define PIN_I2S_MIC_SCK      4      // SCK / BCLK
#define PIN_I2S_MIC_WS       5      // WS / LRCK
#define PIN_I2S_MIC_SD       6      // SD / DOUT (L/R no GND = Canal Esquerdo)

// Amplificador de Áudio I2S (MAX98357A) - Porta I2S_NUM_1
#define PIN_I2S_AMP_BCLK    15      // BCLK
#define PIN_I2S_AMP_LRC     16      // LRC / WS
#define PIN_I2S_AMP_DIN      7      // DIN

// ============================================================================
// 3. PARÂMETROS DE REDE, NTP E IA GEMINI
// ============================================================================
#define SERIAL_BAUD_RATE    115200
#define NTP_SERVER_1        "pool.ntp.org"
#define NTP_SERVER_2        "time.nist.gov"
#define GMT_OFFSET_SEC      (-3 * 3600) // Horário de Brasília (UTC-3)
#define DAYLIGHT_OFFSET_SEC 0

#define GEMINI_MODEL_NAME   "gemini-3.1-flash-lite"
#define MAX_CHAT_HISTORY    8           // Últimas 8 mensagens no contexto

#endif // CIBORGUE_CONFIG_H
