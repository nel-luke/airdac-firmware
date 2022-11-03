#ifndef AIRDAC_FIRMWARE_HELIX_WRAPPER_H
#define AIRDAC_FIRMWARE_HELIX_WRAPPER_H

#include "audio_common.h"

void run_helix_decoder(const AudioContext_t* audio_ctx);
void init_helix_decoder(void);
void delete_helix_decoder(void);

extern const DecoderWrapper_t helix_wrapper;

#endif //AIRDAC_FIRMWARE_HELIX_WRAPPER_H
