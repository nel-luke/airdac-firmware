#ifndef AIRDAC_FIRMWARE_DLNA_SSDP_H
#define AIRDAC_FIRMWARE_DLNA_SSDP_H

#include <esp_netif.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define ESP_VER_STR STR(ESP_IDF_VERSION_MAJOR) "." STR(ESP_IDF_VERSION_MINOR) "." STR(ESP_IDF_VERSION_PATCH)

#define SERVER_STR "esp-idf/" ESP_VER_STR " UPnP/1.0 AirDAC/1.0"

void start_discovery(const char* ip_addr, const char* uuid);

#endif //AIRDAC_FIRMWARE_DLNA_SSDP_H
