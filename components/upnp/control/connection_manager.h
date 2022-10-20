#ifndef AIRDAC_FIRMWARE_CONNECTION_MANAGER_H
#define AIRDAC_FIRMWARE_CONNECTION_MANAGER_H

#include <stdbool.h>

void init_connection_manager(void);
bool connection_manager_execute(const char* action_name, char* arguments, char** response);

#endif //AIRDAC_FIRMWARE_CONNECTION_MANAGER_H
