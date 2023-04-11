#ifndef AIRDAC_FIRMWARE_RENDERING_CONTROL_H
#define AIRDAC_FIRMWARE_RENDERING_CONTROL_H

#include "control_common.h"

#include <stdbool.h>

void init_rendering_control(void);
action_err_t rendering_control_execute(const char* action_name, char* arguments, char** response);
char* get_rendering_control_changes(void);
char* get_rendering_control_all(void);

#endif //AIRDAC_FIRMWARE_RENDERING_CONTROL_H
