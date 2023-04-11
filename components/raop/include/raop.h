#ifndef AIRDAC_FIRMWARE_RAOP_H
#define AIRDAC_FIRMWARE_RAOP_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

void raop_start(size_t stack_size, int priority, const char* host_name);

#endif //AIRDAC_FIRMWARE_RAOP_H
