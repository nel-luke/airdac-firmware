#ifndef AIRDAC_FIRMWARE_DLNA_SSDP_H
#define AIRDAC_FIRMWARE_DLNA_SSDP_H

#include <esp_netif.h>

void start_discovery(const char* ip_addr, const char* uuid);

#endif //AIRDAC_FIRMWARE_DLNA_SSDP_H
