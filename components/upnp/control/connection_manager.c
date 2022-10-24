#include "connection_manager.h"

#include <stdio.h>
#include <string.h>

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

action_err_t GetProtocolInfo(char* arguments, char** response) {
    *response = (char*)protocol_info;
    return Action_OK;
}

UNIMPLEMENTED(PrepareForConnection)
UNIMPLEMENTED(ConnectionComplete)

action_err_t GetConnectionIDs(char* arguments, char** response) {
    *response = (char*)get_connection_ids_response;
    return Action_OK;
}

action_err_t GetCurrentConnectionInfo(char* arguments, char** response) {
    *response = (char*)get_current_connection_info;
    return Action_OK;
}

#define NUM_ACTIONS 5
static const struct action action_list[NUM_ACTIONS] = {
        ACTION(GetProtocolInfo),
        ACTION(PrepareForConnection),
        ACTION(ConnectionComplete),
        ACTION(GetConnectionIDs),
        ACTION(GetCurrentConnectionInfo)
};

action_err_t connection_manager_execute(const char* action_name, char* arguments, char** response) {
    action_err_t err = Invalid_Action;
    int i = 0;
    while (i < NUM_ACTIONS) {
        if (strcmp(action_name, action_list[i].name) == 0) {
            err = action_list[i].handle(arguments, response);
            break;
        }
        i++;
    }

    return err;
}