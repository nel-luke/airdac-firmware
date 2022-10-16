#ifndef AIRDAC_FIRMWARE_EVENTING_H
#define AIRDAC_FIRMWARE_EVENTING_H

#include <esp_http_server.h>

extern const httpd_uri_t AVTransport_Subscribe;
extern const httpd_uri_t ConnectionManager_Subscribe;
extern const httpd_uri_t RenderingControl_Subscribe;

extern const httpd_uri_t AVTransport_Unsubscribe;
extern const httpd_uri_t ConnectionManager_Unsubscribe;
extern const httpd_uri_t RenderingControl_Unsubscribe;

void start_eventing();

#endif //AIRDAC_FIRMWARE_EVENTING_H
