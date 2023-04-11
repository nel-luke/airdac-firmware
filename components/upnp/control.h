#ifndef AIRDAC_FIRMWARE_UPNP_CONTROL_H
#define AIRDAC_FIRMWARE_UPNP_CONTROL_H

#include <esp_http_server.h>

#define CONTROL_URIS 3

void start_control(httpd_handle_t server);

#endif //AIRDAC_FIRMWARE_UPNP_CONTROL_H
