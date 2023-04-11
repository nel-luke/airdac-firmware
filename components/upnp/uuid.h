#ifndef AIRDAC_FIRMWARE_UUID_H
#define AIRDAC_FIRMWARE_UUID_H

#include <time.h>

#define UUIDS_LEN 42

struct ssdp_uuid {
    char uuid_s[UUIDS_LEN];
};
typedef struct ssdp_uuid uuid_t;

void uuid_init(const uint8_t* mac_addr);
void get_device_uuid(uuid_t* uuid);
void generate_uuid(uuid_t* uuid);

#endif //AIRDAC_FIRMWARE_UUID_H
