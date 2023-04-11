#include "uuid.h"
#include "build_time.h"

#include <memory.h>

#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_random.h>

#define UUID_BASE 122193846E9
#define UUIDI_LEN 16

#define NVS_UUID_NS "uuid"
#define NVS_UUID_KEY "uuid_i"

static const char* TAG = "upnp_uuid";
static uint8_t static_mac[6];

static void make_uuid(uint8_t* uuid_i) {
    time_t now;
    time(&now);
    now = now > BUILD_TIME ? now : BUILD_TIME;

    long long uuid_time = (long long)(now)*10E6 + UUID_BASE;

    uint8_t clock_sequence[2];
    esp_fill_random(clock_sequence, 2);

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
    memcpy(uuid_i+10, static_mac, 6);
    // }
}

static inline void print_uuid(const uint8_t* uuid_i, uuid_t* uuid) {
    snprintf(uuid->uuid_s, UUIDS_LEN, "uuid:%x%x%x%x-%x%x-%x%x-%x%x-%x%x%x%x%x%x",
             uuid_i[0], uuid_i[1], uuid_i[2], uuid_i[3],
             uuid_i[4], uuid_i[5], uuid_i[6], uuid_i[7],
             uuid_i[8], uuid_i[9], uuid_i[10], uuid_i[11],
             uuid_i[12], uuid_i[13], uuid_i[14], uuid_i[15]);
}

void uuid_init(const uint8_t* mac_addr) {
    memcpy(static_mac, mac_addr, 6);
}

void get_device_uuid(uuid_t* uuid) {
    uint8_t uuid_i[UUIDI_LEN];

    nvs_handle_t nvs;
    esp_err_t err;
    err = nvs_open(NVS_UUID_NS, NVS_READONLY, &nvs);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "NVS namespace not found. Generating UUID.");
        make_uuid(uuid_i);
        goto store_uuid;
    }
    ESP_ERROR_CHECK(err);

    size_t length = UUIDI_LEN;
    ESP_ERROR_CHECK(nvs_get_blob(nvs, NVS_UUID_KEY, uuid_i, &length));
    nvs_close(nvs);

    return_uuid:
    print_uuid(uuid_i, uuid);
    return;

    store_uuid:
    ESP_LOGI(TAG, "Storing UUID in NVS");
    ESP_ERROR_CHECK(nvs_open(NVS_UUID_NS, NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK(nvs_set_blob(nvs, NVS_UUID_KEY, uuid_i, UUIDI_LEN));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);
    goto return_uuid;
}

void generate_uuid(uuid_t* uuid) {
    uint8_t uuid_i[UUIDI_LEN];
    make_uuid(uuid_i);
    print_uuid(uuid_i, uuid);
}