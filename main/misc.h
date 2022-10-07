#ifndef AIRDAC_FIRMWARE_MISC_H
#define AIRDAC_FIRMWARE_MISC_H

#include "time.h"

#define UUIDS_LEN 37

void initialize_sntp();
void get_uuid(uint8_t* mac_addr, char* uuid, time_t* saved_time);

#endif //AIRDAC_FIRMWARE_MISC_H
