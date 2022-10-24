#ifndef AIRDAC_FIRMWARE_UPNP_H
#define AIRDAC_FIRMWARE_UPNP_H

#include <stddef.h>
#include <stdint.h>

void upnp_start(size_t stack_size, int priority, int port, const char* ip_addr, const uint8_t* mac_addr, const char* friendly_name);

#endif //AIRDAC_FIRMWARE_UPNP_H
