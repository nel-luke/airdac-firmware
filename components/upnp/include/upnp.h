#ifndef AIRDAC_FIRMWARE_UPNP_H
#define AIRDAC_FIRMWARE_UPNP_H

#include <stdint.h>

void start_upnp(const char* ip_addr, const uint8_t* mac_addr, const char* friendly_name);

#endif //AIRDAC_FIRMWARE_UPNP_H
