#include <esp_event.h>

#include <esp_log.h>
static const char *TAG = "app";

void app_main(void)
{
    while (1) {
        ESP_LOGI(TAG, "Hello world!");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}