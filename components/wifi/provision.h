#ifndef AIRDAC_FIRMWARE_WIFI_PROVISION_H
#define AIRDAC_FIRMWARE_WIFI_PROVISION_H

#include <stdbool.h>

void wifi_get_credentials(bool poll_connected, const char* host_name, char* ssid, char* passphrase);

#endif //AIRDAC_FIRMWARE_WIFI_PROVISION_H
