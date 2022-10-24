#include "audio.h"
#include "audio_common.h"
#include "flac.h"

#include <stdbool.h>
#include <memory.h>

#include <esp_log.h>
#include <driver/i2s.h>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#define I2S_NUM     (0)
#define WROVER_KIT

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

static const char TAG[] = "audio";

static xTaskHandle audio_task;
static SemaphoreHandle_t i2s_sem;

static AudioDecoderConfig_t decoder_config;

static void* local_buff;
static uint32_t local_len;

struct file_data {
    uint32_t blocksize;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bits_per_sample;
    uint64_t total_samples;
    uint32_t buffer_bytes;
};

void init_i2s(int sample_rate, int bit_depth, int channels) {
    ESP_LOGI(TAG, "Initializing I2S");

    i2s_config_t i2s_config = {
            .mode = I2S_MODE_MASTER | I2S_MODE_TX,
            .sample_rate = sample_rate,
            .bits_per_sample = bit_depth,
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .tx_desc_auto_clear = true,
            .dma_buf_count = 8,
            .dma_buf_len = 1024,
            .use_apll = true,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1
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
    //i2s_set_clk(I2S_NUM, sample_rate, bit_depth, channels);

#ifndef WROVER_KIT
    gpio_set_direction(MUTE, GPIO_MODE_OUTPUT);
    gpio_set_level(MUTE, 1);
#endif
}

void audio_reset(void) {
    xTaskNotify(audio_task, STOP, eSetBits);

    xSemaphoreTake(i2s_sem, portMAX_DELAY);
    i2s_driver_uninstall(I2S_NUM);
    // I2S driver is deleted so keep the mutex until it is initialized again
    ESP_LOGI(TAG, "Audio reset successfully");
}

void send_fail(void) {
    decoder_config.decoder_fail_cb();
}

void send_ready(void) {
    decoder_config.decoder_ready_cb();
}

void audio_decoder_continue(void* buff, size_t buff_len) {
    local_buff = buff;
    local_len = buff_len;
    xTaskNotify(audio_task, CONTINUE, eSetBits);
}

void audio_init_buffer(const AudioBufferConfig_t* config) {
//    xSemaphoreTake(i2s_sem, portMAX_DELAY);
    init_i2s(config->sample_rate, config->bit_depth, config->channels);
    xSemaphoreGive(i2s_sem);
}

bool audio_init_decoder(const AudioDecoderConfig_t* config) {
    // Search for content type

    // if (strcmp(content_type, decoder[i])==0)
    // if not found return false
//    bool b = xSemaphoreTake(audio_mutex, 0);
//    assert(b);

    memcpy(&decoder_config, config, sizeof(decoder_config));
//    xSemaphoreGive(audio_mutex);

    xTaskNotify(audio_task, START_FLAC_DECODER, eSetBits);
    return true;
}

_Noreturn static void audio_loop(void* args) {
    ESP_LOGI(TAG, "Audio loop started");
    uint32_t bits;
    while(1) {
        xTaskNotifyWait(0, ULONG_MAX, &bits, portMAX_DELAY);
        if (bits & (STOP | CONTINUE)) {
            continue;
        }

        ESP_LOGI(TAG, "Starting player!");
        xSemaphoreTake(i2s_sem, portMAX_DELAY);
        if (bits & START_FLAC_DECODER) {
            start_flac_decoder(decoder_config.file_size, &local_buff, &local_len);
            decoder_config.metadata_finished_cb();
            continue_flac_decoder();
            stop_flac_decoder();
        }
        i2s_zero_dma_buffer(I2S_NUM);
        xSemaphoreGive(i2s_sem);
    }
}


void audio_start(size_t stack_size, int priority) {
    i2s_sem = xSemaphoreCreateBinary();
    xTaskCreate(audio_loop, "Audio Loop", stack_size, NULL, priority, &audio_task);
}
