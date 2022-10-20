#ifndef AIRDAC_FIRMWARE_UPNP_COMMON_H
#define AIRDAC_FIRMWARE_UPNP_COMMON_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#define SERVER_PORT 80

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define ESP_VER_STR STR(ESP_IDF_VERSION_MAJOR) "." STR(ESP_IDF_VERSION_MINOR) "." STR(ESP_IDF_VERSION_PATCH)

#define SERVER_STR "esp-idf/" ESP_VER_STR " UPnP/1.0 AirDAC/1.0"
#define USERAGENT_STR "AirDAC"

#define AV_TRANSPORT_CHANGED        BIT0
#define CONNECTION_MANAGER_CHANGED  BIT1
#define RENDERING_CONTROL_CHANGED   BIT2

#define AV_TRANSPORT_SEND_ALL BIT3
#define SEND_PROTOCOL_INFO BIT4
#define RENDERING_CONTROL_SEND_ALL BIT5

#define EVENTING_CLEAN_SUBSCRIBERS  BIT6
#define DISCOVERY_SEND_NOTIFY       BIT7

#define ALL_EVENT_BITS     0x00FFFFFF

char* get_date(void);

void start_events(void);
uint32_t get_events(void);
void send_event(uint32_t event);

#endif //AIRDAC_FIRMWARE_UPNP_COMMON_H
