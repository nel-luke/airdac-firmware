#include "wifi.h"
#include "connect.h"
#include "provision.h"

#include <sys/param.h>
#include <esp_log.h>
#include <nvs.h>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include <esp_wifi_types.h>
#include <esp_wifi.h>
#include <esp_http_server.h>

#define NVS_WIFI_NS "wifi"
#define NVS_SSID_KEY "ssid"
#define NVS_PASSPHRASE_KEY "passphrase"

static const char TAG[] = "wifi";

void service_wifi(void) {
    if (wifi_poll_disconnected() == true) {
        ESP_LOGE(TAG, "System disconnected to Wi-Fi. Restarting");
        esp_restart();
    }
}

void wifi_start(const char* host_name) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    char ssid[MAX_SSID_LEN] = {0};
    char passphrase[MAX_PASSPHRASE_LEN] = {0};

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_WIFI_NS, NVS_READONLY, &nvs);

    bool poll_connected = true;
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Credentials not set in NVS! Starting Wi-Fi AP immediately.");
        poll_connected = false;
        goto start_provision;
    }

    size_t length = MAX_SSID_LEN;
    ESP_ERROR_CHECK(nvs_get_str(nvs, NVS_SSID_KEY, ssid, &length));

    length = MAX_PASSPHRASE_LEN;
    ESP_ERROR_CHECK(nvs_get_str(nvs, NVS_PASSPHRASE_KEY, passphrase, &length));
    nvs_close(nvs);

    if (wifi_connect(ssid, passphrase) == ESP_OK) {
        ESP_LOGI(TAG, "Wi-Fi connected successfully.");
        return;
    }

    ESP_LOGI(TAG, "Failed to connect. Starting Wi-Fi AP...");
    memset(ssid, 0, MAX_SSID_LEN);
    memset(passphrase, 0, MAX_PASSPHRASE_LEN);
start_provision:
    wifi_get_credentials(poll_connected, host_name, ssid, passphrase);
    ESP_LOGI(TAG, "Storing credentials in NVS");
    ESP_ERROR_CHECK(nvs_open(NVS_WIFI_NS, NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK(nvs_set_str(nvs, NVS_SSID_KEY, ssid));
    ESP_ERROR_CHECK(nvs_set_str(nvs, NVS_PASSPHRASE_KEY, passphrase));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);

    ESP_LOGI(TAG, "Done! Restarting system in");
    for (int i = 3 ; i > 0; i--) {
        ESP_LOGI(TAG, "%d", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    esp_restart();
}