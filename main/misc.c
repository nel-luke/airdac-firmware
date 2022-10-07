#include "misc.h"
#include "build_time.h"

#include <string.h>
#include <time.h>

//#include "esp_attr.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define UUID_BASE 122193846E9
#define UUIDI_LEN 16

#define NVS_UUID_NS "uuid"
#define NVS_UUID_KEY "uuid_i"
#define NVS_TIME_KEY "uuid_time"

static const char *TAG = "common";

void get_uuid(uint8_t* mac_addr, char* uuid, time_t* saved_time) {
    time_t build_time = BUILD_TIME;
    long long uuid_time = (long long)(build_time)*10E6 + UUID_BASE;

    uint8_t clock_sequence[2];
    esp_fill_random(clock_sequence, 2);

    uint8_t uuid_i[UUIDI_LEN];
    time_t nvs_time;

    nvs_handle_t nvs;
    esp_err_t err;
    err = nvs_open(NVS_UUID_NS, NVS_READONLY, &nvs);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "NVS namespace not found. Generating UUID.");
        goto generate_uuid;
    }
    ESP_ERROR_CHECK(err);

    size_t length = UUIDI_LEN;
    ESP_ERROR_CHECK(nvs_get_blob(nvs, NVS_UUID_KEY, uuid_i, &length));

    length = sizeof(nvs_time);
    ESP_ERROR_CHECK(nvs_get_blob(nvs, NVS_TIME_KEY, &nvs_time, &length));
    *saved_time = nvs_time;
    nvs_close(nvs);

    return_uuid:
    snprintf(uuid, 37, "%x%x%x%x-%x%x-%x%x-%x%x-%x%x%x%x%x%x",
             uuid_i[0], uuid_i[1], uuid_i[2], uuid_i[3],
             uuid_i[4], uuid_i[5], uuid_i[6], uuid_i[7],
             uuid_i[8], uuid_i[9], uuid_i[10], uuid_i[11],
             uuid_i[12], uuid_i[13], uuid_i[14], uuid_i[15]);
    return;


    generate_uuid:
    // {
    uuid_i[0] = (uint8_t)(uuid_time >> 52);
    uuid_i[1] = (uint8_t)(uuid_time >> 44);
    uuid_i[2] = (uint8_t)(uuid_time >> 36);
    uuid_i[3] = (uint8_t)(uuid_time >> 28);
    // -
    uuid_i[4] = (uint8_t)(uuid_time >> 20);
    uuid_i[5] = (uint8_t)(uuid_time >> 12);
    // -
    uuid_i[6] = (uint8_t)(0x10 | (uuid_time >> 8));
    uuid_i[7] = (uint8_t)(uuid_time & 0xFF);
    // -
    uuid_i[8] = (uint8_t)(0x40 | (clock_sequence[0]&0x0F));
    uuid_i[9] = clock_sequence[1];
    memcpy(uuid_i+10, mac_addr, 6);
    // }
    ESP_LOGI(TAG, "Storing UUID in NVS");
    ESP_ERROR_CHECK(nvs_open(NVS_UUID_NS, NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK(nvs_set_blob(nvs, NVS_UUID_KEY, uuid_i, UUIDI_LEN));
    ESP_ERROR_CHECK(nvs_set_blob(nvs, NVS_TIME_KEY, &build_time, sizeof(build_time)));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);
    *saved_time = build_time;
    goto return_uuid;
}

static void sntp_synced_cb(struct timeval *tv)
{
    time_t now = 0;
    time(&now);

    struct tm timeinfo = { 0 };
    localtime_r(&now, &timeinfo);

    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

    ESP_LOGI(TAG, "Time has been synchronised to %s UTC", strftime_buf);
}

void initialize_sntp()
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
#if LWIP_DHCP_GET_NTP_SRV && SNTP_MAX_SERVERS > 1
    sntp_servermode_dhcp(1);
    sntp_setservername(1, "pool.ntp.org");
#endif
    sntp_set_time_sync_notification_cb(sntp_synced_cb);
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);

    sntp_init();
}