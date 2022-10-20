#ifndef AIRDAC_FIRMWARE_WIFI_CONNECT_H
#define AIRDAC_FIRMWARE_WIFI_CONNECT_H

#include <stdbool.h>
#include <esp_err.h>

esp_err_t wifi_connect(const char* ssid, const char* password);
bool wifi_poll_connected(void);
bool wifi_poll_disconnected(void);

#endif //AIRDAC_FIRMWARE_WIFI_CONNECT_H
