#ifndef CIBORGUE_AUDIO_HAL_H
#define CIBORGUE_AUDIO_HAL_H

#include <Arduino.h>
#include "config.h"

#if ENABLE_PHYSICAL_AUDIO
#include <driver/i2s.h>
#endif

class AudioHAL {
private:
    bool txReady = false;

public:
    void begin() {
#if ENABLE_PHYSICAL_AUDIO
        // 1. Configuração do Microfone I2S (INMP441) em I2S_NUM_0
        i2s_config_t micConfig = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
            .sample_rate = SFX_SAMPLE_RATE,
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
            .sample_rate = SFX_SAMPLE_RATE,
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
        if (i2s_driver_install(I2S_NUM_1, &ampConfig, 0, NULL) == ESP_OK) {
            i2s_set_pin(I2S_NUM_1, &ampPins);
            i2s_zero_dma_buffer(I2S_NUM_1);
            txReady = true;
        }

        Serial.println("EVT:LOG=Drivers I2S (INMP441 + MAX98357A) inicializados!");
#else
        txReady = false;
        Serial.println("EVT:LOG=Audio I2S em Modo Ponte Virtual PC (ENABLE_PHYSICAL_AUDIO=0)");
#endif
    }

    bool writePcm(const int16_t* samples, size_t sampleCount) {
#if ENABLE_PHYSICAL_AUDIO
        if (!txReady || samples == nullptr || sampleCount == 0) return false;
        size_t bytesWritten = 0;
        esp_err_t err = i2s_write(
            I2S_NUM_1,
            samples,
            sampleCount * sizeof(int16_t),
            &bytesWritten,
            pdMS_TO_TICKS(150)
        );
        return err == ESP_OK;
#else
        (void)samples;
        (void)sampleCount;
        return true;
#endif
    }

    void clearTxDma() {
#if ENABLE_PHYSICAL_AUDIO
        if (txReady) {
            i2s_zero_dma_buffer(I2S_NUM_1);
        }
#endif
    }
};

#endif // CIBORGUE_AUDIO_HAL_H
