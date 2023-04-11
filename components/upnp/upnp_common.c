#include "upnp_common.h"

#include <time.h>

#include <esp_log.h>

//static const char* TAG = "upnp_common";
EventGroupHandle_t upnp_events;

const char* server_STR = SERVER_STR;
const char* useragent_STR = "AirDAC";

static char date_str[50];

char* get_date(void) {
    time_t now;
    time(&now);
    struct tm *time_struct = gmtime(&now);
    assert(strftime(date_str, sizeof(date_str), "%a, %d %b %Y %T GMT", time_struct) != 0);
    return date_str;
}

inline void start_events(void) {
    upnp_events = xEventGroupCreate();
}

inline uint32_t get_events(void) {
    return xEventGroupWaitBits(upnp_events, ALL_EVENT_BITS, pdFALSE, pdFALSE, 0);
}

inline void flag_event(uint32_t event) {
    xEventGroupSetBits(upnp_events, event);
}

inline void unflag_event(uint32_t event) {
    xEventGroupClearBits(upnp_events, event);
}