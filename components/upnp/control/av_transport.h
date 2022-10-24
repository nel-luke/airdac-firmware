#ifndef AIRDAC_FIRMWARE_UPNP_CONTROL_AV_TRANSPORT_H
#define AIRDAC_FIRMWARE_UPNP_CONTROL_AV_TRANSPORT_H

#include "control_common.h"
#include "../upnp_common.h"

#include <stdbool.h>

void init_av_transport(void);
action_err_t av_transport_execute(const char* action_name, char* arguments, char** response);
char* get_av_transport_changes(void);
char* get_av_transport_all(void);
char* get_track_url(void);
void get_stream_info(FileInfo_t* info);

#endif //AIRDAC_FIRMWARE_UPNP_CONTROL_AV_TRANSPORT_H
