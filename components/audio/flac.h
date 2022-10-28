#ifndef AIRDAC_FIRMWARE_FLAC_H
#define AIRDAC_FIRMWARE_FLAC_H

#include "audio_common.h"

void init_flac_decoder(void);
void run_flac_decoder(const AudioContext_t* audio_ctx);
void delete_flac_decoder(void);

#endif //AIRDAC_FIRMWARE_FLAC_H
