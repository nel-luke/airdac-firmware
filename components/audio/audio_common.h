#ifndef AIRDAC_FIRMWARE_AUDIO_COMMON_H
#define AIRDAC_FIRMWARE_AUDIO_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define STOP_DECODER                    BIT0
#define CONTINUE_DECODER                BIT1
#define RUN_DECODER      BIT2

struct AudioContext {
//    void (*set_sample_rate)(size_t sample_rate);
    size_t (*get_buffer)(uint8_t* encoded_buffer, size_t buff_len);
    void (*write)(const int32_t* left_samples, const int32_t* right_samples, size_t sample_length, unsigned int sample_rate, unsigned int bit_depth);
    void (*decoder_finished)(void);
    void (*decoder_failed)(void);
    size_t (*bytes_elapsed)(void);
    size_t (*total_bytes)(void);
};
typedef struct AudioContext AudioContext_t;

#endif //AIRDAC_FIRMWARE_AUDIO_COMMON_H
