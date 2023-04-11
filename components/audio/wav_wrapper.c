#include "wav_wrapper.h"

#include <memory.h>

#include <esp_heap_caps.h>
#include <esp_log.h>

#define BUFF_LEN 384000

static const char TAG[] = "audio_wav";

static struct wav_stat {
    uint16_t channels;
    uint16_t block_alignment;
    uint16_t bit_depth;
    uint32_t sample_rate;
    uint8_t* in_buffer;

    size_t channel_samples;
    int32_t* left_buff;
    int32_t* right_buff;
} *stat;

void delete_wav_decoder(void) {
    free(stat->in_buffer);
    free(stat);
}

#define ROT2(_buff) (((_buff)[1] << 8) | (_buff)[0])
#define ROT4(_buff) (((_buff)[3] << 24) | ((_buff)[2] << 16) | ((_buff)[1] << 8) | (_buff)[0])
#define dump(_buff) printf("%x %x %x %x\n", (_buff)[0], (_buff)[1], (_buff)[2], (_buff)[3])

static bool read_wav_header() {
    uint16_t format = 0;
    memcpy(&format, stat->in_buffer+20, 2);
    memcpy(&stat->channels, stat->in_buffer+22, 2);
    memcpy(&stat->sample_rate, stat->in_buffer+24, 4);
    memcpy(&stat->block_alignment, stat->in_buffer+32, 2);
    memcpy(&stat->bit_depth, stat->in_buffer+34, 2);

    if (format != 1) {
        ESP_LOGE(TAG, "WAV format is not PCM");
        return false;
    }

    if (stat->channels == 1) {
        ESP_LOGI(TAG, "Playing mono");
    } else if (stat->channels > 2) {
        ESP_LOGW(TAG, "More than 2 channels not supported");
        return false;
    }
//
//    const char header_info[] =
//        "\nFormat: %d"
//        "\nChannels: %d"
//        "\nSample rate: %d"
//        "\nBlock alignment: %d"
//        "\nBits per sample: %d\n"
//    ;
//    printf(header_info,
//        format,
//        stat->channels,
//        stat->sample_rate,
//        stat->block_alignment,
//        stat->bit_depth
//    );

    return true;
}

void run_wav_decoder(const AudioContext_t* audio_ctx) {
    size_t read_size = audio_ctx->fill_buffer(stat->in_buffer, BUFF_LEN);
    if (read_size == 0)
        return;

    if (read_wav_header() == false) {
        audio_ctx->decoder_failed();
        return;
    }

    stat->channel_samples = BUFF_LEN / (stat->block_alignment*20);
    stat->left_buff = heap_caps_malloc(sizeof(int32_t) * stat->channel_samples, MALLOC_CAP_SPIRAM);

    if (stat->channels == 1) {
        stat->right_buff = stat->left_buff;
    } else if (stat->channels == 2) {
        stat->right_buff = heap_caps_malloc(sizeof(int32_t) * stat->channel_samples, MALLOC_CAP_SPIRAM);
    }

    assert(BUFF_LEN % stat->block_alignment == 0);


    uint8_t* data_start = NULL;
    size_t begin = 0;
    while (data_start == NULL) {
        // Find 'd'
        for (size_t i = begin; i < read_size; i++) {
            if (stat->in_buffer[i] == 'd') {
                data_start = stat->in_buffer + i;
                break;
            }
        }
        while (data_start == NULL) {
//            ESP_LOGI(TAG, "Not found. New buffer");
            read_size = audio_ctx->fill_buffer(stat->in_buffer, BUFF_LEN);
            if (read_size == 0)
                return;

            begin = 0;
            for (size_t i = begin; i < read_size; i++) {
                if (stat->in_buffer[i] == 'd') {
                    data_start = stat->in_buffer + i;
                    break;
                }
            }
        }

        size_t offset = data_start - stat->in_buffer;

        if (BUFF_LEN - offset < 4) {
            memmove(stat->in_buffer, data_start, BUFF_LEN - offset);
            read_size = audio_ctx->fill_buffer(stat->in_buffer + offset, BUFF_LEN - offset);
            if (read_size == 0)
                return;
            data_start = stat->in_buffer;
            offset = 0;
        }
        const char str[] = "ata";
        for (int i = 0; i < 3; i++) {
            if (data_start[i+1] != str[i]) {
                data_start = NULL;
                break;
            }
        }
        begin = offset+1;
    }

    begin += 3;
    if (begin < BUFF_LEN) {
        memmove(stat->in_buffer, stat->in_buffer + begin, BUFF_LEN - begin);
    }
    read_size = audio_ctx->fill_buffer(stat->in_buffer + BUFF_LEN - begin, begin);

    if (read_size == 0)
        return;

    const size_t bytes_per_sample = stat->block_alignment / stat->channels;
    size_t in_i = 0;
    read_size = BUFF_LEN;

    bool run = true;
    while (run) {
        for (int i = 0; i < stat->channel_samples; i++) {
            for (int j = 0; j < bytes_per_sample; j++) {
                stat->left_buff[i] = stat->in_buffer[in_i++] << 8*j;
            }
            if (stat->channels == 2) {
                for (int j = 0; j < bytes_per_sample; j++) {
                    stat->right_buff[i] = stat->in_buffer[in_i++] << 8*j;
                }
            }
        }

        run = audio_ctx->write(stat->left_buff, stat->right_buff, stat->channel_samples, stat->sample_rate, stat->bit_depth);

        if (in_i == read_size) {
            if (audio_ctx->eof()) {
                audio_ctx->decoder_finished();
                run = false;
            }

            read_size = audio_ctx->fill_buffer(stat->in_buffer, BUFF_LEN);

            if (read_size == 0)
                run = false;

            in_i = 0;
        }
    }

    free(stat->left_buff);

    if (stat->channels == 2)
        free(stat->right_buff);
}

void init_wav_decoder(void) {
    stat = malloc(sizeof(struct wav_stat));
    stat->in_buffer = heap_caps_malloc(BUFF_LEN, MALLOC_CAP_SPIRAM);
}

const DecoderWrapper_t wav_wrapper = {
        .init = init_wav_decoder,
        .run = run_wav_decoder,
        .delete = delete_wav_decoder
};