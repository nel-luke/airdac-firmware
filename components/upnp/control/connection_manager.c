#include "connection_manager.h"
#include "control_common.h"

#include <stdio.h>
#include <string.h>

static const char get_protocol_info_response[] =
        "<Source></Source><Sink>"
        "http-get:*:*:*,"
        "http-get:*:audio/mp3:*,"
        "http-get:*:audio/basic:*,"
        "http-get:*:audio/ogg:*,"
        "http-get:*:audio/ac3:*,"
        "http-get:*:audio/aac:*,"
        "http-get:*:audio/vorbis:*,"
        "http-get:*:audio/flac:*,"
        "http-get:*:audio/x-flac:*,"
        "http-get:*:audio/x-wav:*"
        "</Sink>"
;

static const char get_connection_ids_response[] = "<ConnectionIDs>0</ConnectionIDs>";

static const char get_current_connection_info[] =
        "<RcsID>0</RcsID>"
        "<AVTransportID>0</AVTransportID>"
        "<ProtocolInfo></ProtocolInfo>"
        "<PeerConnectionManager></PeerConnectionManager"
        "<PeerConnectionID>-1</PeerConnectionID>"
        "<Direction>Input</Direction>"
        "<Status>OK</Status>"
        ;

void init_connection_manager(void) {

}

void GetProtocolInfo(char* arguments, char** response) {
    *response = (char*)get_protocol_info_response;
}

void GetConnectionIDs(char* arguments, char** response) {
    *response = (char*)get_connection_ids_response;
}

void GetCurrentConnectionInfo(char* arguments, char** response) {
    *response = (char*)get_current_connection_info;
}

#define NUM_ACTIONS 3
static const struct action action_list[NUM_ACTIONS] = {
        ACTION(GetProtocolInfo),
        ACTION(GetConnectionIDs),
        ACTION(GetCurrentConnectionInfo)
};

bool connection_manager_execute(const char* action_name, char* arguments, char** response) {
    int i = 0;
    while (i < NUM_ACTIONS) {
        if (strcmp(action_name, action_list[i].name) == 0) {
            (*action_list[i].handle)(arguments, response);
            break;
        }
        i++;
    }

    return i < NUM_ACTIONS;
}