#ifndef CIBORGUE_AUDIO_HAL_H
#define CIBORGUE_AUDIO_HAL_H

#include <Arduino.h>
#include "config.h"

#if ENABLE_PHYSICAL_AUDIO
#include <driver/i2s.h>
#endif

class AudioHAL {
public:
    void begin() {
#if ENABLE_PHYSICAL_AUDIO
        // 1. Configuração do Microfone I2S (INMP441) em I2S_NUM_0
        i2s_config_t micConfig = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
            .sample_rate = 16000,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
            .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 8,
            .dma_buf_len = 256,
            .use_apll = false,
            .tx_desc_auto_clear = false,
            .fixed_mclk = 0
        };
        i2s_pin_config_t micPins = {
            .bck_io_num = PIN_I2S_MIC_SCK,
            .ws_io_num = PIN_I2S_MIC_WS,
            .data_out_num = I2S_PIN_NO_CHANGE,
            .data_in_num = PIN_I2S_MIC_SD
        };
        i2s_driver_install(I2S_NUM_0, &micConfig, 0, NULL);
        i2s_set_pin(I2S_NUM_0, &micPins);

        // 2. Configuração do Amplificador I2S (MAX98357A) em I2S_NUM_1
        i2s_config_t ampConfig = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
            .sample_rate = 16000,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
            .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 8,
            .dma_buf_len = 256,
            .use_apll = false,
            .tx_desc_auto_clear = true,
            .fixed_mclk = 0
        };
        i2s_pin_config_t ampPins = {
            .bck_io_num = PIN_I2S_AMP_BCLK,
            .ws_io_num = PIN_I2S_AMP_LRC,
            .data_out_num = PIN_I2S_AMP_DIN,
            .data_in_num = I2S_PIN_NO_CHANGE
        };
        i2s_driver_install(I2S_NUM_1, &ampConfig, 0, NULL);
        i2s_set_pin(I2S_NUM_1, &ampPins);

        Serial.println("EVT:LOG=Drivers I2S (INMP441 + MAX98357A) inicializados!");
#else
        Serial.println("EVT:LOG=Audio I2S em Modo Ponte Virtual PC (ENABLE_PHYSICAL_AUDIO=0)");
#endif
    }
};

#endif // CIBORGUE_AUDIO_HAL_H
