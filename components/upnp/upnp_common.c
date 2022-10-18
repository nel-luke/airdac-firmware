#include "upnp_common.h"

#include <time.h>

#include <esp_log.h>

//static const char* TAG = "upnp_common";
EventGroupHandle_t upnp_events;

static char date_str[50];

char* getDate(void) {
    time_t now;
    time(&now);
    struct tm *time_struct = gmtime(&now);
    assert(strftime(date_str, sizeof(date_str), "%a, %d %b %Y %T GMT", time_struct) != 0);
    return date_str;
}