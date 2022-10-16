#include "upnp.h"
#include "eventing.h"
#include "discovery.h"
#include "description.h"

#include <esp_log.h>

#include <lwip/sockets.h>

static const char *TAG = "upnp";

static struct {
    char ip_addr[IPADDR_STRLEN_MAX];
    uuid_t uuid;
    char friendly_name[50];
} upnp_info;

void start_upnp(const char* ip_addr, const uint8_t* mac_addr, const char* friendly_name) {
    strcpy(upnp_info.ip_addr, ip_addr);
    strcpy(upnp_info.friendly_name, friendly_name);

    uuid_init(mac_addr);
    get_device_uuid(&upnp_info.uuid);
    ESP_LOGI(TAG, "UUID is %s", upnp_info.uuid.uuid_s);

    start_eventing();
    start_discovery(upnp_info.ip_addr, upnp_info.uuid.uuid_s);
    start_description(upnp_info.friendly_name, upnp_info.uuid.uuid_s, upnp_info.ip_addr);
}