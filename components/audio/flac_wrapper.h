#ifndef AIRDAC_FIRMWARE_FLAC_WRAPPER_H
#define AIRDAC_FIRMWARE_FLAC_WRAPPER_H

#include "audio_common.h"

void init_flac_decoder(void);
void run_flac_decoder(const AudioContext_t* audio_ctx);
void delete_flac_decoder(void);

extern const DecoderWrapper_t flac_wrapper;

#endif //AIRDAC_FIRMWARE_FLAC_WRAPPER_H
