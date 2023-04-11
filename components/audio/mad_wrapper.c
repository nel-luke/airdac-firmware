#include "mad_wrapper.h"
#include "audio_common.h"

#include "codecs/mad/mad.h"

#include <memory.h>
#include <stdio.h>

#include <esp_log.h>

#define MADX_INPUT_BUFFER_SIZE	(5*1152*8)

static const char TAG[] = "audio_mad";

enum mad_sig {
    ERROR_OCCURED,
    MORE_INPUT,
    FLUSH_BUFFER,
    CALL_AGAIN
};

struct mad_struct {
    struct mad_stream 	stream;
    struct mad_frame 	frame;
    struct mad_synth 	synth;
    mad_timer_t 		timer;
    unsigned long		frame_cnt;
} static* mad;

struct mad_stat {
    size_t 		write_size;
    size_t		readsize;
//    size_t      offset;
    size_t		remaining;

    // Will reference some
    // "middle part" of in_buffer:
    uint8_t 	*buffstart;
    uint8_t 	in_buffer[600 + MADX_INPUT_BUFFER_SIZE + MAD_BUFFER_GUARD];
} static* stat;

static enum mad_sig run_mad(void) {
    uint8_t* guard_ptr  = NULL;

    // Executes on initial loop
    if (stat->readsize == 0 && stat->remaining == 0 && mad->stream.buffer == NULL) {
        stat->readsize = MADX_INPUT_BUFFER_SIZE;
        stat->remaining = 0;
        return MORE_INPUT;
    }

    if (mad->stream.buffer == NULL || mad->stream.error == MAD_ERROR_BUFLEN) {
        if(stat->readsize < MADX_INPUT_BUFFER_SIZE) {
            guard_ptr = stat->buffstart + stat->readsize;
            memset(guard_ptr, 0, MAD_BUFFER_GUARD);
            stat->readsize += MAD_BUFFER_GUARD;
        }
        // Pipe the new buffer content to libmad's
        // stream decoder facility.

        mad_stream_buffer(&mad->stream, stat->buffstart, stat->readsize + stat->remaining );
        mad->stream.buffer = stat->in_buffer;

        if ((stat->buffstart[0] == 0xff && (stat->buffstart[1] & 0xe0) == 0xe0)) {
            ESP_LOGI(TAG, "Synced!");
            mad->stream.sync = 0;
        }
        mad->stream.md_len = 511;

        mad->stream.error = MAD_ERROR_NONE;
    }

    if (mad_frame_decode(&mad->frame, &mad->stream) ) {
        if(MAD_RECOVERABLE(mad->stream.error) ) {
            if(mad->stream.error != MAD_ERROR_LOSTSYNC ||
               mad->stream.this_frame != guard_ptr ||
               // Suppress error if caused by ID3 tag.
               (mad->stream.this_frame[0] != 'I' &&		// ID3v2
                mad->stream.this_frame[1] != 'D' &&
                mad->stream.this_frame[2] != '3') ||
               (mad->stream.this_frame[0] != 'T' &&		// ID3v1
                mad->stream.this_frame[1] != 'A' &&
                mad->stream.this_frame[2] != 'G') ) {
                printf("Recoverable frame level error (%s)\n",
                       mad_stream_errorstr(&mad->stream));
            }

            return CALL_AGAIN;
        } else if(mad->stream.error == MAD_ERROR_BUFLEN) {
            printf("Need more input (%s)\n", mad_stream_errorstr(&mad->stream));
            stat->remaining = mad->stream.bufend - mad->stream.next_frame;

//            stat->offset = mad->stream.this_frame - stat->buffstart;
            stat->readsize = MADX_INPUT_BUFFER_SIZE - stat->remaining;
            memmove(stat->in_buffer, mad->stream.next_frame - 600, 600 + stat->remaining);

            return MORE_INPUT;
        } else {
            return ERROR_OCCURED;
        }
    }

    mad->frame_cnt++;
    mad_timer_add(&mad->timer, mad->frame.header.duration);
    mad_synth_frame(&mad->synth, &mad->frame);
    return FLUSH_BUFFER;
}

void run_mad_decoder(const AudioContext_t* audio_ctx) {
    bool run = true;
    mad->stream.buffer = NULL;
    stat->readsize = 0;
    stat->remaining = 0;
//    stat->offset = 0;
    stat->buffstart = stat->in_buffer+600;

//    printf("MAD STREAM Before:\n"
//           "input buffer: 0x%x\n"
//           "Buffer end: 0x%x\n"
//           "Skip length: %ld\n"
//           "sync found: %d\n"
//           "freerate: %ld\n"
//           "this_frame: 0x%x\n"
//           "next_frame: 0x%x\n"
//           "md_len: %d\n"
//           "options: 0x%x\n"
//           "error: %d\n\n",
//           (size_t)mad->stream.buffer, (size_t)mad->stream.bufend, mad->stream.skiplen, mad->stream.sync,
//           mad->stream.freerate, (size_t)mad->stream.this_frame, (size_t)mad->stream.next_frame,
//           mad->stream.md_len, mad->stream.options, mad->stream.error);

    while (run) {
        enum mad_sig ret = run_mad();

        switch (ret) {
            case CALL_AGAIN:
                continue;
            case MORE_INPUT:
                if (audio_ctx->eof()) {
                    audio_ctx->decoder_finished();
                    run = false;
                    break;
                }

                stat->readsize = audio_ctx->fill_buffer(stat->buffstart + stat->remaining, stat->readsize);
                if (stat->readsize == 0)
                    run = false;

                break;
            case FLUSH_BUFFER:
                run = audio_ctx->write(mad->synth.pcm.samples[0], mad->synth.pcm.channels == 1
                                                            ? mad->synth.pcm.samples[0] : mad->synth.pcm.samples[1],
                                 mad->synth.pcm.length, mad->synth.pcm.samplerate, 32);
                break;
            case ERROR_OCCURED:
                ESP_LOGE(TAG, "Error code %s\n", mad_stream_errorstr(&mad->stream));
                audio_ctx->decoder_failed();
                run = false;
                break;
            default:
                abort();
        }
    }

    memset(stat, 0, sizeof(struct mad_stat));
    mad_timer_reset(&mad->timer);
    mad_synth_finish(&mad->synth);
    mad_frame_finish(&mad->frame);
    mad_stream_finish(&mad->stream);
}

void delete_mad_decoder(void) {
    free(mad);
    free(stat);
}

void init_mad_decoder(void) {
    mad = malloc(sizeof(struct mad_struct));
    stat = malloc(sizeof(struct mad_stat));

    assert(mad != NULL);
    assert(stat != NULL);

    mad_stream_init(&mad->stream);
    mad_frame_init(&mad->frame);
    mad_synth_init(&mad->synth);
    mad_timer_reset(&mad->timer);
}

const DecoderWrapper_t mad_wrapper = {
        .init = init_mad_decoder,
        .run = run_mad_decoder,
        .delete = delete_mad_decoder
};