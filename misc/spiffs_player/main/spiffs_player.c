#include <string.h>
#include <stdio.h>
#include <sys/unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "driver/gpio.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#define TAG         "app"
#define MOUNT_POINT "/spiffs"
#define FILE_NAME   MOUNT_POINT"/small_wave.wav"
#define I2S_NUM     (0)
#define I2S_WS      (GPIO_NUM_21)
#define I2S_BCK     (GPIO_NUM_23)
#define I2S_MCK     (GPIO_NUM_0)
#define I2S_DO      (GPIO_NUM_19)
#define MUTE        (GPIO_NUM_18)

#define TASK_STACK  (2048)
#define BUF_CNT    (6)
#define BUF_LEN     (60)

struct wav_header_reduced {
    uint32_t file_size;
    uint32_t format_length;
    uint8_t format;
    uint8_t channels;
    uint32_t sample_rate;
    uint32_t frame_count;
    uint16_t bytes_per_frame; // 1 frame = left + right sample
    uint16_t bits_per_sample;
    uint32_t data_size;
};

struct music_task_parameters {
    struct wav_header_reduced* header;
    FILE* f;
};

#define ROT2(_buff) (((_buff)[1] << 8) | (_buff)[0])
#define ROT4(_buff) (((_buff)[3] << 24) | ((_buff)[2] << 16) | ((_buff)[1] << 8) | (_buff)[0])
#define dump(_buff) printf("%x %x %x %x\n", (_buff)[0], (_buff)[1], (_buff)[2], (_buff)[3])

void init_spiffs() {
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
            .base_path = MOUNT_POINT,
            .partition_label = NULL,
            .max_files = 5,
            .format_if_mount_failed = false
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        esp_spiffs_format(conf.partition_label);
        return;
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    // Check consistency of reported partiton size info.
//    if (used > total) {
//        ESP_LOGW(TAG, "Number of used bytes cannot be larger than total. Performing SPIFFS_check().");
//        ret = esp_spiffs_check(conf.partition_label);
//        // Could be also used to mend broken files, to clean unreferenced pages, etc.
//        // More info at https://github.com/pellepl/spiffs/wiki/FAQ#powerlosses-contd-when-should-i-run-spiffs_check
//        if (ret != ESP_OK) {
//            ESP_LOGE(TAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
//            return;
//        } else {
//            ESP_LOGI(TAG, "SPIFFS_check() successful");
//        }
//    }
}

void read_wav_header(FILE* f, struct wav_header_reduced** header) {
    *header = malloc(sizeof(struct wav_header_reduced));

    ESP_LOGI(TAG, "Reading WAV header.");
    uint8_t buff[4];
    fread(buff, 1, 4, f); // RIFF mark

    fread(buff, 1, 4, f);
    (*header)->file_size = ROT4(buff);

    fread(buff, 1, 4, f); // WAVE mark
    fread(buff, 1, 4, f); // fmt mark

    fread(buff, 1, 4, f);
    (*header)->format_length = ROT4(buff);

    fread(buff, 1, 2, f);
    (*header)->format = buff[0];

    fread(buff, 1, 2, f);
    (*header)->channels = buff[0];

    fread(buff, 1, 4, f);
    (*header)->sample_rate = ROT4(buff);

    fread(buff, 1, 4, f);
    (*header)->frame_count = ROT4(buff);

    fread(buff, 1, 2, f);
    (*header)->bytes_per_frame = ROT2(buff);

    fread(buff, 1, 2, f);
    (*header)->bits_per_sample = ROT2(buff);

    char ch = 0;
    do {
        fread(&ch, 1, 1, f);
    } while (ch != 'd');
    fread(buff, 1, 3, f); // data mark

    fread(buff, 1, 4, f);
    (*header)->data_size = ROT4(buff);

    ESP_LOGI(TAG, "Header read successfully!");
}

void print_wav_header(FILE* stream, const char* path, const struct wav_header_reduced* header) {
    const char header_info[] = " \
        \n%s Header Info: \
        \nFile size: %d \
        \nFormat length: %d \
        \nFormat: %d \
        \nChannels: %d \
        \nSample rate: %d \
        \nFrame count: %d \
        \nBytes per frame: %d \
        \nBits per sample: %d \
        \nData size: %d\n\n "
    ;
    fprintf(stream, header_info, path,
            header->file_size,
            header->format_length,
            header->format,
            header->channels,
            header->sample_rate,
            header->frame_count,
            header->bytes_per_frame,
            header->bits_per_sample,
            header->data_size
    );
}

void init_i2s(uint32_t sample_rate, uint16_t bits_per_sample, uint8_t num_channels) {
    ESP_LOGI(TAG, "Initializing I2S peripheral.");
    i2s_config_t i2s_config = {
            .mode = I2S_MODE_MASTER | I2S_MODE_TX,
            .sample_rate = sample_rate,
            .bits_per_sample = bits_per_sample,
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .tx_desc_auto_clear = false,
            .dma_buf_count = BUF_CNT,
            .dma_buf_len = BUF_LEN,
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
    i2s_set_clk(I2S_NUM, sample_rate, bits_per_sample, num_channels);

    gpio_set_direction(MUTE, GPIO_MODE_OUTPUT);
    gpio_set_level(MUTE, 1);
    ESP_LOGI(TAG, "I2S initialized!");
}

static uint32_t i = 0;
void transmit_music_task(void* args) {
    //vTaskDelay(3000 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "Music task %d started.", i++);
    struct music_task_parameters* parameters = args;
    uint16_t* buff = malloc(sizeof(uint16_t)*BUF_LEN);
    size_t read = 0;
    while (1) {
        read = fread(buff, sizeof(uint16_t), BUF_LEN, parameters->f);
        if (read == 0) {
            ESP_LOGI(TAG, "Restarting");
            rewind(parameters->f);
            memset(buff, 0, sizeof(uint16_t)*BUF_LEN);
        }
        size_t i2s_bytes_write = 0;
        i2s_write(I2S_NUM, buff, sizeof(uint16_t)*BUF_LEN, &i2s_bytes_write, portMAX_DELAY);
    }
    ESP_LOGI(TAG, "Done!");
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "\nApplication Start.");
    esp_chip_info_t info;
    esp_chip_info(&info);

    ESP_LOGI(TAG, "\nFree heap size: %d", esp_get_free_heap_size());
    ESP_LOGI(TAG, "\nFree internal heap size: %d", esp_get_free_internal_heap_size());
    ESP_LOGI(TAG, "\nModel: %d\nFeatures: %d\nRevision: %d\nCores: %d\n", info.model, info.features, info.revision, info.cores);

    init_spiffs();

    const char path[] = FILE_NAME;
    ESP_LOGI(TAG, "Reading %s.", path);
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        // Not printing path since it is printed in the previous line
        ESP_LOGE(TAG, "Failed to open file.");
        return;
    }
    ESP_LOGI(TAG, "File opened!");

    struct wav_header_reduced* header;
    read_wav_header(f, &header);
    print_wav_header(stdout, path, header);
    ESP_LOGI(TAG, "Test");

    init_i2s(header->sample_rate, header->bits_per_sample, header->channels);

    ESP_LOGI(TAG, "Creating parameters.");
    struct music_task_parameters* parameters = malloc(sizeof(struct music_task_parameters));
    parameters->header = header;
    parameters->f = f;
    ESP_LOGI(TAG, "Parameters created!");

    ESP_LOGI(TAG, "Creating music task.");
    xTaskCreate(&transmit_music_task, "Transmit Music", TASK_STACK, (void*) parameters, 5, NULL);
    ESP_LOGI(TAG, "Main task finished.");
}
