#ifndef AIRDAC_FIRMWARE_UPNP_COMMON_H
#define AIRDAC_FIRMWARE_UPNP_COMMON_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#define SERVER_PORT 80

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define ESP_VER_STR STR(ESP_IDF_VERSION_MAJOR) "." STR(ESP_IDF_VERSION_MINOR) "." STR(ESP_IDF_VERSION_PATCH)

#define SERVER_STR "esp-idf/" ESP_VER_STR " UPnP/1.0 AirDAC/1.0"
#define USERAGENT_STR "AirDAC DLNADOC/1.50"

#define EVENTING_SEND_INITIAL_NOTIFY_BIT BIT0
#define DISCOVERY_SEND_NOTIFY_BIT BIT1
#define EVENTING_CLEAN_SUBSCRIBERS_BIT BIT2
extern EventGroupHandle_t upnp_events;

char* getDate(void);

#endif //AIRDAC_FIRMWARE_UPNP_COMMON_H
