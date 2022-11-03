#ifndef AIRDAC_FIRMWARE_AUDIO_H
#define AIRDAC_FIRMWARE_AUDIO_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef void (*audio_callback)(void);

struct AudioDecoderConfig {
    const char* content_type;
    size_t file_size;
    audio_callback decoder_ready_cb;
    audio_callback decoder_finished_cb;
    audio_callback decoder_failed_cb;
    void (*wrote_samples_cb)(uint32_t samples, uint32_t sample_rate);
};
typedef struct AudioDecoderConfig AudioDecoderConfig_t;

//struct AudioBufferConfig {
////    size_t size;
//    size_t sample_rate;
//    size_t bit_depth;
//    size_t channels;
//};
//typedef struct AudioBufferConfig AudioBufferConfig_t;

void audio_start(size_t stack_size, int priority);
bool audio_init_decoder(const AudioDecoderConfig_t* config);
//void audio_init_buffer(const AudioBufferConfig_t* config);
void audio_decoder_continue(const uint8_t* new_buffer, size_t buffer_length);
void audio_reset(void);
void audio_pause_playback(void);
void audio_resume_playback(void);

#endif //AIRDAC_FIRMWARE_AUDIO_H
