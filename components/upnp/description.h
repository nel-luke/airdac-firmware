#ifndef AIRDAC_FIRMWARE_UPNP_DESCRIPTION_H
#define AIRDAC_FIRMWARE_UPNP_DESCRIPTION_H

#include <esp_http_server.h>

#define DESCRIPTION_URIS 6

void start_description(httpd_handle_t server, const char* friendly_name, const char* uuid, const char* ip_addr);

#endif //AIRDAC_FIRMWARE_UPNP_DESCRIPTION_H
