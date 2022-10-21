#ifndef AIRDAC_FIRMWARE_UPNP_CONTROL_AV_TRANSPORT_H
#define AIRDAC_FIRMWARE_UPNP_CONTROL_AV_TRANSPORT_H

#include "control_common.h"

#include <stdbool.h>

void init_av_transport(void);
action_err_t av_transport_execute(const char* action_name, char* arguments, char** response);
char* get_av_transport_changes(void);
char* get_av_transport_all(void);

#endif //AIRDAC_FIRMWARE_UPNP_CONTROL_AV_TRANSPORT_H
