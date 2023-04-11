#include <stdio.h>
#include <math.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"

#define SAMPLE_RATE     (44100)
#define VOLUME          (0.001f) //(0.001f) // -60dB FS
#define LEVELS          (32766) // 2^(16-1)-1
#define WAVE_FREQ_HZ    (997.0f)
#define TWOPI           (6.28318531f)
#define PHASE_INC       (TWOPI * WAVE_FREQ_HZ / SAMPLE_RATE)

#define I2S_NUM     (0)
//#define WROVER_KIT

#ifdef WROVER_KIT
#define I2S_WS      (GPIO_NUM_27)
#define I2S_BCK     (GPIO_NUM_26)
#define I2S_MCK     (I2S_PIN_NO_CHANGE)
#define I2S_DO      (GPIO_NUM_25)
#else
#define I2S_WS      (GPIO_NUM_21)
#define I2S_BCK     (GPIO_NUM_23)
#define I2S_MCK     (GPIO_NUM_0)
#define I2S_DO      (GPIO_NUM_19)
#define MUTE        (GPIO_NUM_18)
#endif

#define TASK_STACK  (2048)
#define BUF_CNT    (2)
#define BUF_LEN     (441)

static const char* TAG = "app";

// Accumulated phase
static float p = 0.0f;

// Output buffer (2ch interleaved)
static int16_t out_buf[BUF_LEN];

// Fill the output buffer and write to I2S DMA
static void write_buffer()
{
    float fsamp = 0.0f;
    uint16_t isamp = 0;
    size_t bytes_written;

    for (uint32_t i = 0; i < BUF_LEN; i+=2) {
        double dither = floor((double)(rand()+rand()-RAND_MAX/2)/RAND_MAX);
        fsamp = sinf(p) * VOLUME;
        isamp = (int16_t) (fsamp*LEVELS + dither);

        // Increment and wrap phase
        p += PHASE_INC;
        if (p >= TWOPI)
            p -= TWOPI;

        out_buf[i] = out_buf[i+1] = isamp;
    }

    // Write with max delay. We want to push buffers as fast as we
    // can into DMA memory. If DMA memory isn't transmitted yet this
    // will yield the task until the interrupt fires when DMA buffer has
    // space again. If we aren't keeping up with the real-time deadline,
    // audio will glitch and the task will completely consume the CPU,
    // not allowing any task switching interrupts to be processed.
    i2s_write(I2S_NUM, out_buf, sizeof(out_buf), &bytes_written, portMAX_DELAY);

    // You could put a taskYIELD() here to ensure other tasks always have a chance to run.
    // taskYIELD();
}

static void audio_task(void *userData)
{
    while(1) {
        //ESP_LOGI(TAG, "Looping");
        write_buffer();
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting application.");
    i2s_config_t i2s_config = {
            .mode = I2S_MODE_MASTER | I2S_MODE_TX,
            .sample_rate = SAMPLE_RATE,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .dma_buf_count = BUF_CNT,
            .dma_buf_len = BUF_LEN,
            .use_apll = true,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2
    };

    i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);

    i2s_pin_config_t pin_config = {
            .mck_io_num = I2S_MCK,
            .bck_io_num = I2S_BCK,
            .ws_io_num = I2S_WS,
            .data_out_num = I2S_DO,
            .data_in_num = I2S_PIN_NO_CHANGE
    };
    i2s_set_pin(I2S_NUM, &pin_config);
    //i2s_set_clk(I2S_NUM, sample_rate, bits_per_sample, num_channels);

#ifndef WROVER_KIT
    gpio_set_direction(MUTE, GPIO_MODE_OUTPUT);
    gpio_set_level(MUTE, 1);
#endif

    ESP_LOGI(TAG, "Configuration successful.");
    // Highest possible priority for realtime audio task
    xTaskCreate(audio_task, "audio", 2048, NULL, configMAX_PRIORITIES - 1, NULL);
}