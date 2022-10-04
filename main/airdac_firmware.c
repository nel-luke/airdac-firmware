#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>

#include <nvs_flash.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_http_server.h>

#include "wifi_provisioner.h"

static const char *TAG = "app";
#define HOST_NAME "AirDAC"

void app_main(void)
{
    /*
        Turn off warnings from HTTP server as redirecting traffic will yield
        lots of invalid requests
    */
    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_parse", ESP_LOG_ERROR);


    // Initialize networking stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create default event loop needed by the  main app
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize NVS needed by Wi-Fi
    ESP_ERROR_CHECK(nvs_flash_init());

    // Initialize Wi-Fi including netif with default config
    esp_netif_create_default_wifi_ap();

    start_provisioner(HOST_NAME);

    while (1) {
        ESP_LOGI(TAG, "Hello world!");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}