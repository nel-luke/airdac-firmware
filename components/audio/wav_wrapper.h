#ifndef AIRDAC_FIRMWARE_WAV_WRAPPER_H
#define AIRDAC_FIRMWARE_WAV_WRAPPER_H

#include "audio_common.h"

void run_wav_decoder(const AudioContext_t* audio_ctx);
void init_wav_decoder(void);
void delete_wav_decoder(void);

extern const DecoderWrapper_t wav_wrapper;

#endif //AIRDAC_FIRMWARE_WAV_WRAPPER_H
