#include "audio.h"
#include "audio_common.h"
#include "flac_wrapper.h"
#include "mad_wrapper.h"
#include "helix_wrapper.h"

#include <stdbool.h>
#include <memory.h>
#include <sys/param.h>

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
static SemaphoreHandle_t audio_mutex;
static AudioDecoderConfig_t decoder_config;

static const unsigned int max_sample_rate = 48000;

struct {
    const uint8_t* current_buffer;
    size_t buffer_length;
    size_t remaining_bytes;
    size_t offset;
    size_t total_written;
    int32_t* write_buff;
    unsigned int sample_rate;
    bool failed;
} buffer_info = { 0 };

static inline void send_ready(void) {
    decoder_config.decoder_ready_cb();
}

void send_finished(void) {
    decoder_config.decoder_finished_cb();
}

void send_fail(void) {
    decoder_config.decoder_failed_cb();
}

static size_t fill_buffer(uint8_t* encoded_buffer, size_t buffer_length) {
    assert(buffer_length != 0);

    if (buffer_info.failed)
        return 0;

    size_t len = buffer_length;

    uint32_t bits = 0;
    xTaskNotifyWait(0, 0, &bits, 0);
    if (bits & STOP_DECODER)
        goto return_finished;

    if (buffer_info.remaining_bytes == 0) {
        send_ready();
        xTaskNotifyWait(0, ULONG_MAX, &bits, portMAX_DELAY);
//        ESP_LOGI(TAG, "Buffer swapped!");
        if (bits & CONTINUE_DECODER) {
            buffer_info.remaining_bytes = buffer_info.buffer_length;
            buffer_info.offset = 0;
        } else if (bits & STOP_DECODER) {
return_finished:
            send_finished();
            return false;
        } else {
                // Shouldn't happen
                abort();
        }
    }
    
    len = MIN(len, buffer_info.remaining_bytes);

    buffer_info.remaining_bytes -= len;

    memcpy(encoded_buffer, buffer_info.current_buffer + buffer_info.offset, len);
    buffer_info.offset += len;
    buffer_info.total_written += len;

    if (buffer_info.total_written == decoder_config.file_size) {
        send_finished();
    } else if (len < buffer_length){
        size_t tmp = buffer_length - len;
//        ESP_LOGI(TAG, "Have %d, need %d", *len, tmp);
        len += fill_buffer(encoded_buffer + len, tmp);
    }

    return len;
}

static void write(const int32_t* left_samples, const int32_t* right_samples, size_t sample_length, unsigned int sample_rate, unsigned int bit_depth) {
    if (buffer_info.failed) {
        i2s_zero_dma_buffer(I2S_NUM);
    }

    if (buffer_info.sample_rate != sample_rate) {
        buffer_info.sample_rate = sample_rate;
        i2s_set_sample_rates(I2S_NUM, sample_rate);
    }

    if (buffer_info.write_buff == NULL) {
        buffer_info.write_buff = malloc(2*sample_length*sizeof(int32_t));
        assert(buffer_info.write_buff != NULL);
    }

    size_t shift = bit_depth == 24 ? 8 : 0;

    int j = 0;
    for (int i = 0; i < sample_length; i++) {
        buffer_info.write_buff[j++] = left_samples[i] << shift;
        buffer_info.write_buff[j++] = right_samples[i] << shift;
    }

    size_t bytes_written;
    i2s_write(I2S_NUM, buffer_info.write_buff, 2*sample_length*sizeof(int32_t), &bytes_written, portMAX_DELAY);
}

static void decoder_failed(void) {
    buffer_info.failed = true;
    decoder_config.decoder_failed_cb();
}

static size_t bytes_elapsed(void) {
    return buffer_info.total_written;
}

static size_t total_bytes(void) {
    return decoder_config.file_size;
}

void audio_reset(void) {
    xTaskNotify(audio_task, STOP_DECODER, eSetBits);
    xSemaphoreTake(audio_mutex, portMAX_DELAY);
//    delete_flac_decoder();
//    delete_mad_decoder();
    delete_helix_decoder();
    free(buffer_info.write_buff);
    memset(&buffer_info, 0, sizeof(buffer_info));
    xSemaphoreGive(audio_mutex);
}

void audio_decoder_continue(const uint8_t* new_buffer, size_t buffer_length) {
    buffer_info.current_buffer = new_buffer;
    buffer_info.buffer_length = buffer_length;

    xTaskNotify(audio_task, CONTINUE_DECODER, eSetBits);
}

static const AudioContext_t context = {
//        .set_sample_rate = set_sample_rate,
        .fill_buffer = fill_buffer,
        .write = write,
        .decoder_failed = decoder_failed,
        .bytes_elapsed = bytes_elapsed,
        .total_bytes = total_bytes
};

bool audio_init_decoder(const AudioDecoderConfig_t* config) {
    xSemaphoreTake(audio_mutex, portMAX_DELAY);
    ESP_LOGI(TAG, "Initializing audio");
    // Search for content type

    // if (strcmp(content_type, decoder[i])==0)
    // if not found return false
//    bool b = xSemaphoreTake(audio_mutex, 0);
//    assert(b);

    memcpy(&decoder_config, config, sizeof(decoder_config));
    buffer_info.sample_rate = max_sample_rate;
//    xSemaphoreGive(audio_mutex);

//    init_flac_decoder();
//    init_mad_decoder();
    init_helix_decoder();

    xSemaphoreGive(audio_mutex);
    xTaskNotify(audio_task, RUN_DECODER, eSetBits);
    return true;
}

_Noreturn static void audio_loop(void* args) {
    ESP_LOGI(TAG, "Audio loop started");
    while(1) {
        uint32_t bits;
        xTaskNotifyWait(0, ULONG_MAX, &bits, portMAX_DELAY);

        ESP_LOGI(TAG, "Starting player!");
        if (bits & RUN_DECODER) {
            asm volatile("" ::: "memory");
            xSemaphoreTake(audio_mutex, portMAX_DELAY);
//            run_flac_decoder(&context);
//            run_mad_decoder(&context);
            run_helix_decoder(&context);
            i2s_zero_dma_buffer(I2S_NUM);
            xSemaphoreGive(audio_mutex);
            ESP_LOGI(TAG, "Decoder stopped");
        }
    }
}

void audio_start(size_t stack_size, int priority) {
    ESP_LOGI(TAG, "Initializing I2S");

    i2s_config_t i2s_config = {
            .mode = I2S_MODE_MASTER | I2S_MODE_TX,
            .sample_rate = max_sample_rate,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .tx_desc_auto_clear = true,
            .dma_buf_count = 8,
            .dma_buf_len = 511,
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

#ifndef WROVER_KIT
    gpio_set_direction(MUTE, GPIO_MODE_OUTPUT);
    gpio_set_level(MUTE, 1);
#endif

    audio_mutex = xSemaphoreCreateMutex();
    xTaskCreate(audio_loop, "Audio Loop", stack_size, NULL, priority, &audio_task);
}
