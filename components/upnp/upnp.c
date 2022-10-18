#include "upnp.h"
#include "upnp_common.h"
#include "uuid.h"
#include "control.h"
#include "eventing.h"
#include "description.h"
#include "discovery.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_log.h>

#include <esp_netif.h>
#include <esp_http_server.h>

static const char *TAG = "upnp";

static struct {
    char ip_addr[IPADDR_STRLEN_MAX];
    uuid_t uuid;
    char friendly_name[50];
} upnp_info;



static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 5;
    config.max_uri_handlers = CONTROL_URIS + EVENTING_URIS + DESCRIPTION_URIS;
    config.lru_purge_enable = true;
    config.server_port = SERVER_PORT;

    // Start the httpd server
    ESP_LOGI(TAG, "Using %d URIs", config.max_uri_handlers);
    ESP_LOGI(TAG, "Web server on port %d", config.server_port);
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    return server;
}

_Noreturn void upnp_loop(void* args) {
    while (1) {
        if (xEventGroupWaitBits(upnp_events, EVENTING_SEND_INITIAL_NOTIFY_BIT, pdTRUE, pdFALSE, 10) & EVENTING_SEND_INITIAL_NOTIFY_BIT)
            eventing_send_initial_notify();

        if (xEventGroupWaitBits(upnp_events, DISCOVERY_SEND_NOTIFY_BIT, pdTRUE, pdFALSE, 10) & DISCOVERY_SEND_NOTIFY_BIT)
            discovery_send_notify();

        if (xEventGroupWaitBits(upnp_events, EVENTING_CLEAN_SUBSCRIBERS_BIT, pdTRUE, pdFALSE, 10) & EVENTING_CLEAN_SUBSCRIBERS_BIT)
            eventing_clean_subscribers();

        service_discovery();
    }
}

void start_upnp(const char* ip_addr, const uint8_t* mac_addr, const char* friendly_name) {
    ESP_LOGI(TAG, "Starting uPnP");
    upnp_events = xEventGroupCreate();
    strcpy(upnp_info.ip_addr, ip_addr);
    strcpy(upnp_info.friendly_name, friendly_name);

    uuid_init(mac_addr);
    get_device_uuid(&upnp_info.uuid);
    ESP_LOGI(TAG, "UUID is %s", upnp_info.uuid.uuid_s);

    httpd_handle_t server = start_webserver();
    start_control(server);
    start_eventing(server);
    start_description(server, upnp_info.friendly_name, upnp_info.uuid.uuid_s, upnp_info.ip_addr);
    start_discovery(upnp_info.ip_addr, upnp_info.uuid.uuid_s);

    xTaskCreate(upnp_loop, "uPnP Loop", 2048, NULL, 10, NULL);
}