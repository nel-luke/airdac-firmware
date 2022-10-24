#ifndef AIRDAC_FIRMWARE_AUDIO_H
#define AIRDAC_FIRMWARE_AUDIO_H

#include <stddef.h>
#include <stdbool.h>

typedef void (*audio_callback)(void);

struct AudioDecoderConfig {
    const char* content_type;
    size_t file_size;
    audio_callback decoder_ready_cb;
    audio_callback metadata_finished_cb;
    audio_callback decoder_fail_cb;

};
typedef struct AudioDecoderConfig AudioDecoderConfig_t;

struct AudioBufferConfig {
//    size_t size;
    size_t sample_rate;
    size_t bit_depth;
    size_t channels;
};
typedef struct AudioBufferConfig AudioBufferConfig_t;

void audio_start(size_t stack_size, int priority);
bool audio_init_decoder(const AudioDecoderConfig_t* config);
void audio_init_buffer(const AudioBufferConfig_t* config);
void audio_decoder_continue(void* buff, size_t buff_len);
void audio_reset(void);

#endif //AIRDAC_FIRMWARE_AUDIO_H
