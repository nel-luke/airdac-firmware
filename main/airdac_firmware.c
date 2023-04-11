#include "misc.h"
#include "wifi.h"
#include "upnp.h"
#include "audio.h"

#include <string.h>

#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <driver/gpio.h>
#include <nvs_flash.h>

#include <lwip/err.h>
#include <lwip/sockets.h>

#define HOST_NAME_KEY "host_name"
#define MAX_HOST_NAME_LEN 16
#define DEFAULT_HOST_NAME "AirDAC"

static const char *TAG = "airdac";

static char friendly_name[50];

_Noreturn void app_main(void)
{
    ESP_LOGI(TAG, "========== AirDAC ==========");

    ESP_LOGI(TAG, "Initializing GPIO");
    gpio_config_t gpio = {
            .pin_bit_mask = 0x00000021,
            .intr_type = GPIO_INTR_DISABLE,
            .mode = GPIO_MODE_OUTPUT,
            .pull_down_en = 0,
            .pull_up_en = 0
    };
    gpio_config(&gpio);
    gpio_set_level(GPIO_NUM_2, 1);
    gpio_set_level(GPIO_NUM_4, 1);

    ESP_LOGI(TAG, "Initializing NVS");
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition is truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    char host_name[MAX_HOST_NAME_LEN];
    size_t length = MAX_HOST_NAME_LEN;

    ESP_LOGI(TAG, "Retrieving host name...");
    nvs_handle_t nvs;
    err = nvs_open("storage", NVS_READONLY, &nvs);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Host name not initialized yet!");
        ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &nvs));
        ESP_LOGI(TAG, "Setting host name to %s", DEFAULT_HOST_NAME);
        ESP_ERROR_CHECK(nvs_set_str(nvs, HOST_NAME_KEY, DEFAULT_HOST_NAME));
        ESP_ERROR_CHECK(nvs_commit(nvs));
        nvs_close(nvs);
        strcpy(host_name, DEFAULT_HOST_NAME);
        //ESP_ERROR_CHECK(nvs_get_str(nvs, "host_name", host_name, &length));
    } else {
        ESP_ERROR_CHECK(err);
        ESP_ERROR_CHECK(nvs_get_str(nvs, HOST_NAME_KEY, host_name, &length));
        nvs_close(nvs);
    }
    ESP_LOGI(TAG, "Host name is %s", host_name);

    ESP_LOGI(TAG, "Starting event loop");
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_LOGI(TAG, "Starting Wi-Fi");
    // Initialize networking stack
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_netif_set_hostname(netif, host_name));
    wifi_start(host_name);

    ESP_LOGI(TAG, "Starting SNTP");
    misc_start_sntp();

    ESP_LOGI(TAG, "Retrieving MAC address...");
    uint8_t mac_addr[6];
    ESP_ERROR_CHECK(esp_netif_get_mac(netif, mac_addr));
    ESP_LOGI(TAG, "MAC address is %x:%x:%x:%x:%x:%x",
             mac_addr[0], mac_addr[1], mac_addr[2],
             mac_addr[3], mac_addr[4], mac_addr[5]);

    esp_netif_ip_info_t ip_info = { 0 };
    esp_netif_get_ip_info(netif, &ip_info);
    struct in_addr ip_struct = { 0 };
    inet_addr_from_ip4addr(&ip_struct, &ip_info.ip);

    char ip_addr[INET_ADDRSTRLEN];
    lwip_inet_ntop(AF_INET, &ip_struct, ip_addr, INET_ADDRSTRLEN);

    ESP_LOGI(TAG, "Starting audio driver");
    audio_start(4096, 15);

    ESP_LOGI(TAG, "Starting uPnP");
    strcpy(friendly_name, host_name);
    upnp_start(8192, 19, 80, ip_addr, mac_addr, friendly_name);

    ESP_LOGI(TAG, "Setup finished. Entering infinite loop");
    uint32_t bit = 1;
    while (1) {
        service_wifi();
        gpio_set_level(GPIO_NUM_0, bit);
//        gpio_set_level(GPIO_NUM_2, bit);
//        gpio_set_level(GPIO_NUM_4, bit);
        bit ^= 1;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}