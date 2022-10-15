#ifndef AIRDAC_FIRMWARE_UPNP_H
#define AIRDAC_FIRMWARE_UPNP_H

#include <esp_netif.h>

#define UUIDS_LEN 37

struct ssdp_uuid {
    char uuid_s[UUIDS_LEN];
};
typedef struct ssdp_uuid uuid_t;

void get_uuid(const uint8_t* mac_addr, uuid_t* uuid, time_t* saved_time);
void start_upnp(const char* ip_addr, const uint8_t* mac_addr, const char* friendly_name);


#endif //AIRDAC_FIRMWARE_UPNP_H
