#ifndef AIRDAC_FIRMWARE_FLAC_H
#define AIRDAC_FIRMWARE_FLAC_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

void start_flac_decoder(size_t stream_len, void** audio_buff, uint32_t* audio_len);
void continue_flac_decoder(void);
void stop_flac_decoder(void);

#endif //AIRDAC_FIRMWARE_FLAC_H
