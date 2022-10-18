#ifndef AIRDAC_FIRMWARE_UPNP_EVENTING_H
#define AIRDAC_FIRMWARE_UPNP_EVENTING_H

#include <esp_http_server.h>

#define EVENTING_URIS 6

void start_eventing(httpd_handle_t server);
void eventing_send_initial_notify(void);
void eventing_clean_subscribers(void);

#endif //AIRDAC_FIRMWARE_UPNP_EVENTING_H
