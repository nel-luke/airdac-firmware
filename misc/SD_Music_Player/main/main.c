// ESP32 WAV music player
// Plays a song named song.wav from an SD card

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
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"

#define TAG         "app"
#define MOUNT_POINT "/sdcard"
#define FILE_NAME   MOUNT_POINT"/16b.wav"
#define I2S_NUM     (0)
#define I2S_WS      (GPIO_NUM_27) // Blue wire
#define I2S_BCK     (GPIO_NUM_26) // Orange wire
#define I2S_DO      (GPIO_NUM_25) // Green wire

#define TASK_STACK  (2048)
//#define BUF_CNT    (12)
//#define BUF_LEN     (1024)

#define TRIGGER     (GPIO_NUM_22)
#define T_ON()      gpio_set_level(TRIGGER, 1)
#define T_OFF()     gpio_set_level(TRIGGER, 0)

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
    uint16_t buflen;
    uint16_t numbufs;
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

#define ROT2(_buff) (((_buff)[1] << 8) | (_buff)[0]) 
#define ROT4(_buff) (((_buff)[3] << 24) | ((_buff)[2] << 16) | ((_buff)[1] << 8) | (_buff)[0])
#define dump(_buff) printf("%x %x %x %x\n", (_buff)[0], (_buff)[1], (_buff)[2], (_buff)[3])

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

    char data[] = "0000";
    do {
        data[0] = data[1];
        data[1] = data[2];
        data[2] = data[3];
        fread(data+3, 1, 1, f);
    } while (strcmp(data, "data") != 0);
    //fread(buff, 1, 3, f); // data mark
    
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

void init_i2s(struct wav_header_reduced* header) {
    header->numbufs = 2;
    header->buflen = 682;

    ESP_LOGI(TAG, "Initializing I2S peripheral.");
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = header->sample_rate,
        .bits_per_sample = header->bits_per_sample,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .tx_desc_auto_clear = false,
        .dma_buf_count = header->numbufs,
        .dma_buf_len = header->buflen,
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
    //i2s_set_clk(I2S_NUM, sample_rate, bits_per_sample, num_channels);
    ESP_LOGI(TAG, "I2S initialized!");
}

static uint32_t i = 0;
void transmit_music_task(void* args) {
    //vTaskDelay(3000 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "Music task %d started.", i++);
    struct music_task_parameters* parameters = args;
    int bps = parameters->header->bits_per_sample;
    int size = 2*(parameters->header->buflen)*bps/8;
    uint8_t* buff = malloc(size);
    size_t read = 0;
    //fread(buff, 1, 1, parameters->f);
    //T_ON();
    while (1) {
        read = fread(buff, 1, size, parameters->f);
        if (read == 0) {
            ESP_LOGI(TAG, "Restarting");
            rewind(parameters->f);
            memset(buff, 0, size);
        }
//        for (int i = 0; i < 20; i++) {
//            printf("%x ", buff[i]);
//        }
//        printf("\n");
        size_t i2s_bytes_write = 0;
        T_ON();
        i2s_write(I2S_NUM, buff, size, &i2s_bytes_write, portMAX_DELAY);
        T_OFF();
        //vTaskDelay(10/portTICK_RATE_MS);
    }
    //T_OFF();
    ESP_LOGI(TAG, "Done!");
    vTaskDelete(NULL);
}

void app_main(void) {
    ESP_LOGI(TAG, "\nApplication Start.");
    esp_chip_info_t info;
    esp_chip_info(&info);

    ESP_LOGI(TAG, "\nFree heap size: %d", esp_get_free_heap_size());
    ESP_LOGI(TAG, "\nFree internal heap size: %d", esp_get_free_internal_heap_size());
    ESP_LOGI(TAG, "\nModel: %d\nFeatures: %d\nRevision: %d\nCores: %d\n", info.model, info.features, info.revision, info.cores);
    gpio_set_direction(TRIGGER, GPIO_MODE_OUTPUT);

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

    struct wav_header_reduced* header;
    read_wav_header(f, &header);
    print_wav_header(stdout, path, header);
    ESP_LOGI(TAG, "Test");

    init_i2s(header);

    ESP_LOGI(TAG, "Creating parameters.");
    struct music_task_parameters* parameters = malloc(sizeof(struct music_task_parameters));
    parameters->header = header;
    parameters->f = f;
    ESP_LOGI(TAG, "Parameters created!");

    ESP_LOGI(TAG, "Creating music task.");
    xTaskCreate(&transmit_music_task, "Transmit Music", TASK_STACK, (void*) parameters, 5, NULL);
    ESP_LOGI(TAG, "Main task finished.");
}