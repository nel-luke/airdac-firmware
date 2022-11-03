#include "stream.h"

#include <sys/param.h>
#include <math.h>

#include <esp_log.h>
#include <esp_http_client.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define START_STREAM  BIT0
#define DOWNLOAD        BIT1
#define FLUSH_BUFFER    BIT2
#define STOP_STREAM     BIT3
static xTaskHandle stream_task;
static volatile SemaphoreHandle_t stream_mutex;

static const char TAG[] = "streamer";

static struct {
    STREAM_CONFIG_STRUCT

    esp_http_client_handle_t client;

    size_t file_size;

    void** buffers;
    SemaphoreHandle_t* buff_sems;
    void* download_offset;
    unsigned int download_i;
    unsigned int ready_i;

    size_t total_read_len;
    unsigned int bytes_left;
    bool once;
} stream_info = { 0 };

static void download_data(void) {
    if (stream_info.bytes_left == 0) {
        ESP_LOGI(TAG, "Download finished!");
        stream_info.stream_finished();
        return;
    }

    int read_len = esp_http_client_read(stream_info.client, stream_info.download_offset, stream_info.buffer_length);
    if (read_len <= 0) {
        ESP_LOGE(TAG, "Error read data");
        stream_info.stream_failed();
        return;
    }
    stream_info.bytes_left -= read_len;

    if (stream_info.once == false && stream_info.download_i == stream_info.buffer_count-2) {
        for (int i = 0; i < stream_info.download_i; i++)
            xSemaphoreGive(stream_info.buff_sems[i]);

        stream_info.once = true;
    }

    if (stream_info.once == true) {
        xSemaphoreGive(stream_info.buff_sems[stream_info.download_i]);
    }

    stream_info.download_i++;
    if (stream_info.download_i == stream_info.buffer_count) {
        stream_info.download_i = 0;
    }

    stream_info.buffer_ready();
    xSemaphoreTake(stream_info.buff_sems[stream_info.download_i], portMAX_DELAY);

    stream_info.download_offset = stream_info.buffers[stream_info.download_i];
    xTaskNotify(stream_task, DOWNLOAD, eSetBits);
}

inline void stream_take_buffer(const uint8_t** buffer, size_t* buffer_length) {
    xSemaphoreTake(stream_info.buff_sems[stream_info.ready_i], portMAX_DELAY);
    *buffer = stream_info.buffers[stream_info.ready_i];
    *buffer_length = stream_info.buffer_length;
}

inline void stream_release_buffer(void) {
    xSemaphoreGive(stream_info.buff_sems[stream_info.ready_i]);

    stream_info.ready_i++;
    if (stream_info.ready_i == stream_info.buffer_count)
        stream_info.ready_i = 0;
}

inline void seek_stream(size_t seek_position) {
    assert(seek_position <= stream_info.file_size);
    xSemaphoreTake(stream_mutex, portMAX_DELAY);

    stream_info.bytes_left = (stream_info.file_size - seek_position);

    xSemaphoreGive(stream_mutex);
    xTaskNotify(stream_task, FLUSH_BUFFER | DOWNLOAD, eSetBits);
}

void start_stream(const char* url, size_t file_size) {
    xSemaphoreTake(stream_mutex, portMAX_DELAY);

    stream_info.file_size = file_size;
    stream_info.bytes_left = stream_info.file_size;

    stream_info.ready_i = 0;
    stream_info.download_i = 0;

    for (int i = 0; i < stream_info.buffer_count; i++) {
        stream_info.buffers[i] = heap_caps_malloc(stream_info.buffer_length, MALLOC_CAP_SPIRAM);
        assert(stream_info.buffers[i] != NULL);
    }

    esp_http_client_config_t download_config = {
            .url = url,
            .method = HTTP_METHOD_GET,
            .port = stream_info.port,
            .user_agent = stream_info.user_agent
    };
    stream_info.client = esp_http_client_init(&download_config);
    esp_http_client_set_header(stream_info.client, "Range", "0-\n");

    esp_err_t err;
    if ((err = esp_http_client_open(stream_info.client, 0)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        stream_info.stream_failed();
        return;
    }

    esp_http_client_fetch_headers(stream_info.client);

    xSemaphoreGive(stream_mutex);
    xTaskNotify(stream_task, START_STREAM, eSetBits);
}

void stop_stream(void) {
    xTaskNotify(stream_task, STOP_STREAM, eSetBits);
    xSemaphoreTake(stream_mutex, portMAX_DELAY);

    ESP_ERROR_CHECK(esp_http_client_close(stream_info.client));
    ESP_ERROR_CHECK(esp_http_client_cleanup(stream_info.client));

    for (int i = 0; i < stream_info.buffer_count; i++) {
        free(stream_info.buffers[i]);
    }

    xSemaphoreGive(stream_mutex);
}

_Noreturn static void stream_loop(void* args) {
    ESP_LOGI(TAG, "Stream loop started");
    while (1) {
        uint32_t bits;
        xTaskNotifyWait(0, ULONG_MAX, &bits, portMAX_DELAY);
        if (bits & STOP_STREAM) {
            asm volatile("" ::: "memory");

            for (int i = 0; i < stream_info.buffer_count; i++) {
                if (i != stream_info.download_i)
                    xSemaphoreTake(stream_info.buff_sems[i], portMAX_DELAY);
            }

            xSemaphoreGive(stream_mutex);
            ESP_LOGI(TAG, "Streamer stopped");
            continue;
        }

        if (bits & START_STREAM) {
            xSemaphoreTake(stream_mutex, portMAX_DELAY);
            ESP_LOGI(TAG, "Starting stream");
            for (int i = 0; i < stream_info.buffer_count; i++) {
                // Keep the buffer to download in
                if (i != stream_info.download_i)
                    xSemaphoreGive(stream_info.buff_sems[i]);
            }
            stream_info.download_offset = stream_info.buffers[stream_info.download_i];
            stream_info.once = false;
            xTaskNotify(stream_task, DOWNLOAD, eSetBits);
        }

        if (bits & FLUSH_BUFFER) {
            stream_info.download_offset = stream_info.buffers[stream_info.download_i];
        }

        if (bits & DOWNLOAD) {
            download_data();
        }
    }
}

void init_stream(size_t stack_size, int priority, const StreamConfig_t* config) {
    memcpy(&stream_info, config, sizeof(StreamConfig_t));

    stream_info.buff_sems = malloc(sizeof(SemaphoreHandle_t) * stream_info.buffer_count);
    stream_info.buffers = malloc(stream_info.buffer_count);
    for (int i = 0; i < stream_info.buffer_count; i++) {
        stream_info.buff_sems[i] = xSemaphoreCreateBinary();
    }

    stream_mutex = xSemaphoreCreateMutex();
    xTaskCreate(stream_loop, "Stream Loop", stack_size, NULL, priority, &stream_task);
}