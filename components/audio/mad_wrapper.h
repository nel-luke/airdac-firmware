#ifndef AIRDAC_FIRMWARE_MAD_WRAPPER_H
#define AIRDAC_FIRMWARE_MAD_WRAPPER_H

#include "audio_common.h"

void run_mad_decoder(const AudioContext_t* audio_ctx);
void init_mad_decoder(void);
void delete_mad_decoder(void);

extern const DecoderWrapper_t mad_wrapper;

#endif //AIRDAC_FIRMWARE_MAD_WRAPPER_H
