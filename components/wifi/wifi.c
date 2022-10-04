#include "wifi.h"
#include "connect.h"
#include "provision.h"

#include <sys/param.h>

#include "esp_log.h"
#include "nvs.h"

#include <esp_wifi_types.h>
#include <esp_http_server.h>

#define NVS_SSID_NAME "ssid"
#define NVS_PASSPHRASE_NAME "passphrase"

static const char *TAG = "airdac_wifi";

void start_wifi(const char* host_name) {
    ESP_LOGI(TAG, "Starting Wi-Fi...");

    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open("wifi", NVS_READWRITE, &nvs));
    char ssid[MAX_SSID_LEN] = {0};
    char passphrase[MAX_PASSPHRASE_LEN] = {0};
    esp_err_t err;
    size_t length = MAX_SSID_LEN;
    err = nvs_get_str(nvs, NVS_SSID_NAME, ssid, &length);

    length = MAX_PASSPHRASE_LEN;
    err |= nvs_get_str(nvs, NVS_PASSPHRASE_NAME, passphrase, &length);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Credentials not set in NVS! Starting Wi-Fi AP immediately.");
        goto start_provision;
    }
    ESP_ERROR_CHECK(err);

    if (wifi_connect(ssid, passphrase) == ESP_OK) {
        ESP_LOGI(TAG, "Wi-Fi connected successfully.");
        return;
    }

    ESP_LOGI(TAG, "Failed to connect. Starting Wi-Fi AP...");
    memset(ssid, 0, MAX_SSID_LEN);
    memset(passphrase, 0, MAX_PASSPHRASE_LEN);
start_provision:
    wifi_get_credentials(host_name, ssid, passphrase);
    ESP_LOGI(TAG, "Storing credentials in NVS");
    ESP_ERROR_CHECK(nvs_set_str(nvs, NVS_SSID_NAME, ssid));
    ESP_ERROR_CHECK(nvs_set_str(nvs, NVS_PASSPHRASE_NAME, passphrase));
    ESP_LOGI(TAG, "Done! Restarting system in");
    for (int i = 3 ; i > 0; i--) {
        ESP_LOGI(TAG, "%d", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    esp_restart();
}