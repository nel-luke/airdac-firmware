#include <string.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>

#include <nvs_flash.h>
#include <esp_netif.h>

#include "wifi.h"

static const char *TAG = "app";
#define MAX_HOST_NAME_LEN 16
#define HOST_NAME "AirDAC"

void app_main(void)
{
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    ESP_LOGI(TAG, "Retrieving host name...");
    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &nvs));
    size_t length = MAX_HOST_NAME_LEN;
    char host_name[length-1];
    err = nvs_get_str(nvs, "host_name", host_name, &length);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Host name not initialized yet!");
        ESP_LOGI(TAG, "Setting host name to %s", HOST_NAME);
        ESP_ERROR_CHECK(nvs_set_str(nvs, "host_name", HOST_NAME));
        ESP_ERROR_CHECK(nvs_commit(nvs));
        ESP_ERROR_CHECK(nvs_get_str(nvs, "host_name", host_name, &length));
    }
    ESP_ERROR_CHECK(err);

    ESP_LOGI(TAG, "Host name is %s", host_name);

    // Initialize networking stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create default event loop needed by the main app
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    start_wifi(host_name);

    while (1) {
        ESP_LOGI(TAG, "Hello world!");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    // Placed here to remember
    //free(host_name);
}