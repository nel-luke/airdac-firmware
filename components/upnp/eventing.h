#ifndef AIRDAC_FIRMWARE_UPNP_EVENTING_H
#define AIRDAC_FIRMWARE_UPNP_EVENTING_H

#include <esp_http_server.h>

#define EVENTING_URIS 6

void start_eventing(httpd_handle_t server, int port);
void eventing_clean_subscribers(void);

void event_av_transport(const char* message);
void send_protocol_info(void);
void event_rendering_control(const char* message);

#endif //AIRDAC_FIRMWARE_UPNP_EVENTING_H
