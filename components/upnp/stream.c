#include "stream.h"

#include <sys/param.h>
#include <math.h>

#include <esp_log.h>
#include <esp_http_client.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define NUM_BUFFS       2

#define START_STREAM  BIT0
#define DOWNLOAD        BIT1
#define FLUSH_BUFFER    BIT2
#define STOP_STREAM     BIT3
static xTaskHandle stream_task;
static SemaphoreHandle_t stream_mutex;

static const char TAG[] = "streamer";

static struct {
    const char* user_agent;
    int port;
    buffer_ready_cbt buffer_ready_cb;
    stream_disconnected_cbt stream_failed_cb;

    char* url;
    size_t file_size;
    size_t buffer_size;

    void* buffers[NUM_BUFFS];
    SemaphoreHandle_t buff_sems[NUM_BUFFS];
    void* download_offset;
    int download_i;
    int ready_i;

    esp_http_client_handle_t client;
    int bytes_left;
    bool fail;
} stream_info;

static void clean_buffers(void) {
    for (int i = 0 ; i < NUM_BUFFS; i++) {
        xSemaphoreTake(stream_info.buff_sems[i], portMAX_DELAY);
        free(stream_info.buffers[i]);
        // Keep the semaphores until new buffers are allocated
    }
}

static void prepare_buffers(void) {
    stream_info.ready_i = 0;
    stream_info.download_i = 0;
    for (int i = 0; i < NUM_BUFFS; i++) {
        stream_info.buffers[i] = heap_caps_malloc(stream_info.buffer_size, MALLOC_CAP_SPIRAM);
        assert(stream_info.buffers[i] != NULL);
        // Keep the buffer to download in
        if (i != stream_info.download_i)
            xSemaphoreGive(stream_info.buff_sems[i]);
    }
    stream_info.download_offset = stream_info.buffers[stream_info.download_i];
}

static esp_err_t download_cb(esp_http_client_event_t *evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        memcpy(stream_info.download_offset, evt->data, evt->data_len);
        stream_info.download_offset += evt->data_len;
    } else if (evt->event_id == HTTP_EVENT_DISCONNECTED || evt->event_id == HTTP_EVENT_ERROR) {
        stream_info.fail = true;
    }

    return ESP_OK;
}

static void prepare_client(void) {
    esp_http_client_config_t download_config = {
            .url = stream_info.url,
            .method = HTTP_METHOD_GET,
            .port = stream_info.port,
            .event_handler = download_cb,
            .user_agent = stream_info.user_agent
    };

    stream_info.client = esp_http_client_init(&download_config);
    stream_info.bytes_left = (int)stream_info.file_size;
}

static void download_data(void) {
    xSemaphoreTake(stream_mutex, portMAX_DELAY);
    uint32_t bytes_start = stream_info.file_size - stream_info.bytes_left;
    uint32_t bytes_end = MIN(bytes_start + stream_info.buffer_size - 1, stream_info.file_size);
    stream_info.bytes_left = MAX(0, stream_info.bytes_left - stream_info.buffer_size);
    xSemaphoreGive(stream_mutex);

    int range_size = snprintf(NULL, 0, "bytes=%d-%d\n", bytes_start, bytes_end);
    char* range = malloc(range_size+1);
    snprintf(range, range_size, "bytes=%d-%d\n", bytes_start, bytes_end);

    esp_http_client_set_header(stream_info.client, "Range", range);
    esp_http_client_perform(stream_info.client);
    free(range);

    if (stream_info.fail) {
        stream_info.stream_failed_cb();
        xSemaphoreGive(stream_info.buff_sems[stream_info.download_i]);
        return;
    }

    if (stream_info.bytes_left != 0) {
        // Download buffer finished

        xSemaphoreGive(stream_info.buff_sems[stream_info.download_i]);

        stream_info.download_i++;
        if (stream_info.download_i == NUM_BUFFS)
            stream_info.download_i = 0;

        stream_info.buffer_ready_cb();
        xSemaphoreTake(stream_info.buff_sems[stream_info.download_i], portMAX_DELAY);
        stream_info.download_offset = stream_info.buffers[stream_info.download_i];
    } else {
        // File finished. Wait for cleanup
        ESP_LOGI(TAG, "Download finished!");

    }
}

_Noreturn static void stream_loop(void* args) {
    ESP_LOGI(TAG, "Stream loop started");
    clean_buffers();
    uint32_t bits;
    while (1) {
        xTaskNotifyWait(0, ULONG_MAX, &bits, portMAX_DELAY);

        if (bits & STOP_STREAM) {
            clean_buffers();
            esp_http_client_cleanup(stream_info.client);
            ESP_LOGI(TAG, "Streamer stopped");
        }

        if (bits & FLUSH_BUFFER) {
            stream_info.download_offset = stream_info.buffers[stream_info.download_i];
        }

        if (bits & START_STREAM) {
            xSemaphoreTake(stream_mutex, portMAX_DELAY);

            ESP_LOGI(TAG, "Streaming started");
            prepare_buffers();
            prepare_client();
            stream_info.fail = false;
            xSemaphoreGive(stream_mutex);
        }

        if (bits & DOWNLOAD) {
            download_data();
        }
    }
}

inline void* take_ready_buffer(void) {
    xSemaphoreTake(stream_info.buff_sems[stream_info.ready_i], portMAX_DELAY);
    return stream_info.buffers[stream_info.ready_i];
}

inline void release_ready_buffer(void) {
    xSemaphoreGive(stream_info.buff_sems[stream_info.ready_i]);

    stream_info.ready_i++;
    if (stream_info.ready_i == NUM_BUFFS)
        stream_info.ready_i = 0;
}

inline void go(void) {
    xTaskNotify(stream_task, DOWNLOAD, eSetBits);
}

inline void stop_stream(void) {
    xTaskNotify(stream_task, STOP_STREAM, eSetBits);
}

inline void seek_stream(int seek_position) {
    xSemaphoreTake(stream_mutex, portMAX_DELAY);

    if (seek_position >= 0)
        stream_info.bytes_left = (int)(stream_info.file_size - seek_position);

    xTaskNotify(stream_task, FLUSH_BUFFER | DOWNLOAD, eSetBits);
    xSemaphoreGive(stream_mutex);
}

void start_stream(char* url, size_t file_size, size_t buffer_size) {
    bool b = xSemaphoreTake(stream_mutex, 0);
    assert(b);

    stream_info.url = url;
    stream_info.file_size = file_size;
    stream_info.buffer_size = buffer_size;

    xSemaphoreGive(stream_mutex);
    xTaskNotify(stream_task, START_STREAM, eSetBits);
}

void init_stream(size_t stack_size, int priority, const char* user_agent, int port,
         buffer_ready_cbt buffer_ready_cb, stream_disconnected_cbt stream_disconnected_cb) {
    stream_info.user_agent = user_agent;
    stream_info.port = port;
    stream_info.buffer_ready_cb = buffer_ready_cb;
    stream_info.stream_failed_cb = stream_disconnected_cb;

    stream_mutex = xSemaphoreCreateMutex();
    for (int i = 0; i < NUM_BUFFS; i++) {
        stream_info.buff_sems[i] = xSemaphoreCreateBinary();
        xSemaphoreGive(stream_info.buff_sems[i]);
    }

    xTaskCreate(stream_loop, "Stream Loop", stack_size, NULL, priority, &stream_task);
}