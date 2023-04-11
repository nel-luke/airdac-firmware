#ifndef AIRDAC_FIRMWARE_CONNECTION_MANAGER_H
#define AIRDAC_FIRMWARE_CONNECTION_MANAGER_H

#include "control_common.h"

#include <stdbool.h>

void init_connection_manager(void);
action_err_t connection_manager_execute(const char* action_name, char* arguments, char** response);

#endif //AIRDAC_FIRMWARE_CONNECTION_MANAGER_H
