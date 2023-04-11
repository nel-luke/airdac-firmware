// ESP32 music player
// Tsting codecs

#include <string.h>
#include <stdio.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"

#include "FLAC/format.h"
#include "FLAC/stream_decoder.h"

#define TAG         "app"
#define MOUNT_POINT "/sdcard"
#define FILE_NAME   MOUNT_POINT"/rgoff.flac"

#define I2S_NUM     (0)
#define I2S_WS      (GPIO_NUM_27) // Blue wire
#define I2S_BCK     (GPIO_NUM_26) // Orange wire
#define I2S_DO      (GPIO_NUM_25) // Green wire

#define TRIGGER     (GPIO_NUM_22)
#define T_ON()      gpio_set_level(TRIGGER, 1)
#define T_OFF()     gpio_set_level(TRIGGER, 0)

struct file_data {
    uint32_t blocksize;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bits_per_sample;
    uint64_t total_samples;
    uint32_t buffer_bytes;
    uint32_t* bigbuf;
    uint8_t numDMAbufs;
    uint16_t DMAbufsize;
};

struct music_task_parameters {
    struct wav_header_reduced* header;
    FILE* f;
};

void init_sdmmc(sdmmc_card_t** card) {
    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing SD card.");

    const char mount_point[] = MOUNT_POINT;
    sdmmc_host_t host_config = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 10,
        .allocation_unit_size = 0 // Not necessary since card will not be formatted
    };

    ESP_LOGI(TAG, "Mounting filesystem.");
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host_config, &slot_config, &mount_config, card);
    if (ret == ESP_FAIL) {
        ESP_LOGE(TAG, "Failed to mount filesystem.");
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted!");
}

void init_i2s(struct file_data* data) {
    ESP_LOGI(TAG, "Initializing I2S peripheral.");

    // The size shouldn't be an odd number.
    assert((data->buffer_bytes % 2) == 0);

    data->numDMAbufs = 1;
    data->DMAbufsize = data->buffer_bytes;
    while (data->DMAbufsize > 511) {
        data->DMAbufsize = data->DMAbufsize >> 1;
        data->numDMAbufs = 24; //data->numDMAbufs << 1;
    }
    ESP_LOGI(TAG, "Using %d buffers of size %d", data->numDMAbufs, data->DMAbufsize);

    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = data->sample_rate,
        .bits_per_sample = data->bits_per_sample,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .tx_desc_auto_clear = false,
        .dma_buf_count = data->numDMAbufs,
        .dma_buf_len = data->DMAbufsize,
        .use_apll = false,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1
    };
    i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);

    i2s_pin_config_t pin_config = {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = I2S_BCK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_DO,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    i2s_set_pin(I2S_NUM, &pin_config);
    i2s_set_clk(I2S_NUM, data->sample_rate, data->bits_per_sample, data->channels);
    gpio_set_direction(TRIGGER, GPIO_MODE_OUTPUT);
    ESP_LOGI(TAG, "I2S initialized!");
}

//static uint32_t i = 0;
//void transmit_music_task(void* args) {
//    //vTaskDelay(3000 / portTICK_RATE_MS);
//    ESP_LOGI(TAG, "Music task %d started.", i++);
//    struct music_task_parameters* parameters = args;
//    uint16_t* buff = malloc(sizeof(uint16_t)*BUF_LEN);
//    size_t read = 0;
//    //T_ON();
//    while (1) {
//        read = fread(buff, sizeof(uint16_t), BUF_LEN, parameters->f);
//        if (read == 0) {
//            ESP_LOGI(TAG, "Restarting");
//            rewind(parameters->f);
//            memset(buff, 0, sizeof(uint16_t)*BUF_LEN);
//        }
//        size_t i2s_bytes_write = 0;
//        i2s_write(I2S_NUM, buff, sizeof(uint16_t)*BUF_LEN, &i2s_bytes_write, portMAX_DELAY);
//        //vTaskDelay(10/portTICK_RATE_MS);
//    }
//    //T_OFF();
//    ESP_LOGI(TAG, "Done!");
//    vTaskDelete(NULL);
//}

FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data) {
    struct file_data* data = client_data;
    int byte_index = 0;
//    for (int i = 0; i < data->blocksize; i++) {
//        //printf("%x : ", buffer[0][i]);
//        for (int bits = 0; bits < data->bits_per_sample; bits+=8) {
//            data->bigbuf[byte_index++] = (uint8_t)((buffer[0][i]>>bits)&0xff);
//            //printf("%x ", data->bigbuf[byte_index-1]);
//        }
//        //printf(": ");
//        for (int bits = 0; bits < data->bits_per_sample; bits+=8) {
//            data->bigbuf[byte_index++] = (uint8_t)((buffer[1][i]>>bits)&0xff);
//            //printf("%x ", data->bigbuf[byte_index-1]);
//        }
//        //printf("\n");
//    }
    int j = 0;
    for (int i = 0; i < data->blocksize; i++) {
        data->bigbuf[j++] = buffer[0][i]<<8;
        data->bigbuf[j++] = buffer[1][i]<<8;
    }

    size_t i2s_bytes_write;
    T_ON();
    i2s_write(I2S_NUM, data->bigbuf, data->buffer_bytes, &i2s_bytes_write, portMAX_DELAY);
    T_OFF();
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data) {
    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        const FLAC__StreamMetadata_StreamInfo *md = &(metadata->data.stream_info);
        ESP_LOGI(TAG, "min_blocksize: %d", md->min_blocksize);
        ESP_LOGI(TAG, "max_blocksize: %d", md->max_blocksize);
        ESP_LOGI(TAG, "min_framesize: %d", md->min_framesize);
        ESP_LOGI(TAG, "max_framesize: %d", md->max_framesize);
        ESP_LOGI(TAG, "sample_rate: %d", md->sample_rate);
        ESP_LOGI(TAG, "channels: %d", md->channels);
        ESP_LOGI(TAG, "bits_per_sample: %d", md->bits_per_sample);
        ESP_LOGI(TAG, "total_samples: %lld", md->total_samples);

        struct file_data* data = client_data;
        data->blocksize = md->max_blocksize;
        data->sample_rate = md->sample_rate;
        data->channels = md->channels;
        data->bits_per_sample = md->bits_per_sample;
        data->total_samples = data->total_samples;
    }
}

void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data) {

}

void app_main(void) {
    ESP_LOGI(TAG, "\nApplication Start.");

    sdmmc_card_t* card;
    init_sdmmc(&card);
    sdmmc_card_print_info(stdout, card);

    const char path[] = FILE_NAME;
    ESP_LOGI(TAG, "Reading %s.", path);
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        // Not printing path since it is printed in the previous line
        ESP_LOGE(TAG, "Failed to open file.");
        return;
    }
    ESP_LOGI(TAG, "File opened!");

    struct file_data* data = malloc(sizeof(struct file_data));

    FLAC__StreamDecoder* decoder = FLAC__stream_decoder_new();
    FLAC__StreamDecoderInitStatus status = FLAC__stream_decoder_init_FILE(decoder, f, write_callback,
          metadata_callback, error_callback, data);
    assert(status==FLAC__STREAM_DECODER_INIT_STATUS_OK);

    FLAC__bool b = FLAC__stream_decoder_process_until_end_of_metadata(decoder);
    assert(b);

    if (data->bits_per_sample == 16) {
        data->buffer_bytes = 2 * data->blocksize * 2;
    } else {
        data->buffer_bytes = 2 * data->blocksize * 4;
    }
    data->bigbuf = malloc(data->buffer_bytes);
    init_i2s(data);

    b = FLAC__stream_decoder_process_until_end_of_stream(decoder);
    assert(b);
    i2s_stop(I2S_NUM);

    b = FLAC__stream_decoder_finish(decoder);
    free(data->bigbuf);
    free(data);

    //ESP_LOGI(TAG, "Creating music task.");
    //xTaskCreate(&transmit_music_task, "Transmit Music", TASK_STACK, (void*) parameters, 5, NULL);
    ESP_LOGI(TAG, "Main task finished.");
}