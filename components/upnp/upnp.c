#include "upnp.h"
#include "common.h"
#include "uuid.h"
#include "eventing.h"
#include "description.h"
#include "discovery.h"

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
    config.max_uri_handlers = EVENTING_URIS + DESCRIPTION_URIS;
    config.lru_purge_enable = true;
    config.server_port = SERVER_PORT;

    // Start the httpd server
    ESP_LOGI(TAG, "Using %d URIs", config.max_uri_handlers);
    ESP_LOGI(TAG, "Web server on port %d", config.server_port);
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    return server;
}

void start_upnp(const char* ip_addr, const uint8_t* mac_addr, const char* friendly_name) {
    ESP_LOGI(TAG, "Starting uPnP");
    strcpy(upnp_info.ip_addr, ip_addr);
    strcpy(upnp_info.friendly_name, friendly_name);

    uuid_init(mac_addr);
    get_device_uuid(&upnp_info.uuid);
    ESP_LOGI(TAG, "UUID is %s", upnp_info.uuid.uuid_s);

    httpd_handle_t server = start_webserver();
    start_eventing(server);
    start_description(server, upnp_info.friendly_name, upnp_info.uuid.uuid_s, upnp_info.ip_addr);
    start_discovery(upnp_info.ip_addr, upnp_info.uuid.uuid_s);
}