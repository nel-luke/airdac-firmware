#ifndef AIRDAC_FIRMWARE_WIFI_CONNECT_H
#define AIRDAC_FIRMWARE_WIFI_CONNECT_H

#include <esp_err.h>

esp_err_t wifi_connect(const char* ssid, const char* password);

#endif //AIRDAC_FIRMWARE_WIFI_CONNECT_H
