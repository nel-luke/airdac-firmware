#include "ssdp.h"

#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#define UDP_PORT 1900
#define MULTICAST_TTL 2
#define MULTICAST_IPV4_ADDR "239.255.255.250"

static const char *TAG = "multicast";
static const char *V4TAG = "mcast-ipv4";



void start_discovery(void) {
}