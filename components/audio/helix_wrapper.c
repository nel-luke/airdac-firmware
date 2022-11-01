#include "helix_wrapper.h"
#include "codecs/helix-aac/aacdec.h"

#include <sys/param.h>
#include <memory.h>
#include <esp_log.h>
#include <driver/i2s.h>

#define SYNCH_WORD_LEN 4
#define AAC_MAX_OUTPUT_SIZE 1024 * 3
#define AAC_MAX_FRAME_SIZE 2100

static HAACDecoder decoder;

static const char TAG[] = "audio_helix";

struct aac_stat {
    size_t buffer_size;
    uint8_t* frame_buffer;
    int16_t* pwm_buffer;
    bool active;
    uint32_t frame_counter;
} static* stat;

//void run_helix_decoder(AudioContext_t* audio_ctx) {
//    size_t bytes = 0;
//    int offset = 0;
//    do {
//        bytes = audio_ctx->fill_buffer(stat->in_buffer, AAC_MAINBUF_SIZE);
//        ESP_LOGI(TAG, "Retrieved %d bytes", bytes);
//        if (bytes == 0) {
//            audio_ctx->decoder_finished();
//            return;
//        }
//
//        offset = AACFindSyncWord(stat->in_buffer, AAC_MAINBUF_SIZE);
//    } while (offset < 0);
//
//    ESP_LOGI(TAG, "Found offset at %d! Shifting data", offset);
//    size_t fill_size = AAC_MAINBUF_SIZE - offset;
//    memmove(stat->in_buffer, stat->in_buffer+offset, fill_size);
//    bytes = audio_ctx->fill_buffer(stat->in_buffer+offset, fill_size);
//
//    ESP_LOGI(TAG, "Filled %d bytes", bytes);
//    offset = AACFindSyncWord(stat->in_buffer, AAC_MAINBUF_SIZE);
//    ESP_LOGI(TAG, "Offset is now %d", offset);
//
//    uint8_t* in_ptr = stat->in_buffer;
//    bytes = AAC_MAINBUF_SIZE;
//    while (1) {
//        ESP_LOGI(TAG, "Giving %d", bytes);
//        int ret = AACDecode(decoder, &in_ptr, (int*)&bytes, stat->out_buffer);
//        ESP_LOGI(TAG, "Bytes left: %d | return: %d\n", bytes, ret);
//
//        if (ret == 0) {
//            size_t bytes_written;
//            i2s_write(I2S_NUM_0, stat->out_buffer, AAC_MAX_NSAMPS*sizeof(int16_t), &bytes_written, portMAX_DELAY);
//        }
//
//        ESP_LOGI(TAG, "in_ptr is %d away, bytes is %d", in_ptr-stat->in_buffer, bytes);
//
//        offset = AACFindSyncWord(stat->in_buffer+5, AAC_MAINBUF_SIZE);
//        while (offset < 0) {
//            bytes = audio_ctx->fill_buffer(stat->in_buffer, AAC_MAINBUF_SIZE);
//            ESP_LOGI(TAG, "Retrieved %d bytes", bytes);
//            if (bytes == 0) {
//                audio_ctx->decoder_finished();
//                return;
//            }
//            offset = AACFindSyncWord(stat->in_buffer, AAC_MAINBUF_SIZE);
//        }
//
//        ESP_LOGI(TAG, "New offset at %d. Shifting data", offset);
//        if (offset > 0) {
//            fill_size = AAC_MAINBUF_SIZE - offset;
//            memmove(stat->in_buffer, stat->in_buffer+offset, fill_size);
//            bytes = audio_ctx->fill_buffer(stat->in_buffer+offset, fill_size);
//
//            if (bytes == 0) {
//                audio_ctx->decoder_finished();
//                return;
//            }
//        }
//
//        bytes = AAC_MAINBUF_SIZE;
//    }
//    AACFlushCodec(decoder);
//}

struct Range {
    int start;
    int end;
};

int findSynchWord(int offset) {
    int result = AACFindSyncWord(stat->frame_buffer+offset, stat->buffer_size-offset);
    return result < 0 ? result : result + offset;
}

/// returns valid start and end synch word.
void synchronizeFrame(struct Range* range) {
    range->start = findSynchWord(0);
    range->end = findSynchWord(range->start + SYNCH_WORD_LEN);
    ESP_LOGI(TAG, "-> frameRange -> %d - %d", range->start, range->end);
    if (range->start < 0){
        // there is no Synch in the buffer at all -> we can ignore all data
        range->end = -1;
        if (stat->buffer_size==AAC_MAX_FRAME_SIZE) {
            stat->buffer_size = 0;
            ESP_LOGI(TAG, "-> buffer cleared");
        }
    } else if (range->start>0) {
        // make sure that buffer starts with a synch word
        ESP_LOGI(TAG, "-> moving to new start %d", range->start);
        stat->buffer_size -= range->start;
        assert(stat->buffer_size<=AAC_MAX_FRAME_SIZE);

        memmove(stat->frame_buffer, stat->frame_buffer + range->start, stat->buffer_size);
        range->end -= range->start;
        range->start = 0;
        ESP_LOGI(TAG, "-> we are at beginning of synch word");
    } else if (range->start==0) {
        ESP_LOGI(TAG, "-> we are at beginning of synch word");
        if (range->end < 0 && stat->buffer_size == AAC_MAX_FRAME_SIZE){
            stat->buffer_size = 0;
            ESP_LOGI(TAG, "-> buffer cleared");
        }
    }
}

/// decodes the data and removes the decoded frame from the buffer
void decode(struct Range* r) {
    ESP_LOGI(TAG, "decode %d", r->end);
    int len = stat->buffer_size - r->start;
    int bytesLeft =  len;
    uint8_t* ptr = stat->frame_buffer + r->start;

    int result = AACDecode(decoder, &ptr, &bytesLeft, stat->pwm_buffer);
    int decoded = len - bytesLeft;
    assert(decoded == ptr-(stat->frame_buffer + r->start));
    if (result == 0){
        ESP_LOGI(TAG, "-> bytesLeft %d -> %d  = %d ", stat->buffer_size, bytesLeft, decoded);
        ESP_LOGI(TAG, "-> End of frame (%d) vs end of decoding (%d)", r->end, decoded);

        // return the decoded result
    //    struct _AACFrameInfo info;
    //    AACGetLastFrameInfo(decoder, &info);
    //    provideResult(info);

        // remove processed data from buffer
        if (decoded<=stat->buffer_size) {
            stat->buffer_size -= decoded;
            //assert(buffer_size<=maxFrameSize());
            memmove(stat->frame_buffer, stat->frame_buffer+r->start+decoded, stat->buffer_size);
            ESP_LOGI(TAG, " -> decoded %d bytes - remaining buffer_size: %d", decoded, stat->buffer_size);
        } else {
            ESP_LOGW(TAG, " -> decoded %d > buffersize %d", decoded, stat->buffer_size);
            stat->buffer_size = 0;
        }
    } else {
        // decoding error
        ESP_LOGI(TAG, " -> decode error: %d - removing frame!", result);
        int ignore = decoded;
        if (ignore == 0) ignore = r->end;
        // We advance to the next synch world
        if (ignore <= stat->buffer_size){
            stat->buffer_size -= ignore;
            memmove(stat->frame_buffer, stat->frame_buffer+ignore, stat->buffer_size);
        }  else {
            stat->buffer_size = 0;
        }
    }
}

void run_helix_decoder(const AudioContext_t* ctx) {
    size_t start = 0;
    // we can not write more then the AAC_MAX_FRAME_SIZE
    size_t write_len = MIN(ctx->total_bytes(), AAC_MAX_FRAME_SIZE-stat->buffer_size);
    while(start < ctx->total_bytes()){
        // we have some space left in the buffer
        stat->buffer_size = ctx->fill_buffer(stat->frame_buffer, write_len);
        struct Range r;
        synchronizeFrame(&r);
        // Decode if we have a valid start and end synch word
        if( r.start >= 0 && r.end > r.start && (r.end - r.start)<=AAC_MAX_FRAME_SIZE) {
            decode(&r);
        } else {
            ESP_LOGW(TAG, " -> invalid frame size: %d / max: %d", r.end-r.start, AAC_MAX_FRAME_SIZE);
        }
        stat->frame_counter++;
        start += stat->buffer_size;
        ESP_LOGI(TAG,"-> Written %zu of %zu - Counter %zu", start, ctx->total_bytes(), stat->frame_counter);
        write_len = MIN(ctx->bytes_elapsed(), AAC_MAX_FRAME_SIZE - stat->buffer_size);;
    }
}

void delete_helix_decoder(void) {
    AACFreeDecoder(decoder);
    free(stat);
}

void init_helix_decoder(void) {
    decoder = AACInitDecoder();
    stat = malloc(sizeof(struct aac_stat));
    memset(stat, 0, sizeof(struct aac_stat));

    if (stat->frame_buffer == NULL) {
        ESP_LOGI(TAG,"allocating frame_buffer with %zu bytes", AAC_MAX_FRAME_SIZE);
        stat->frame_buffer = malloc(AAC_MAX_FRAME_SIZE);
    }
    if (stat->pwm_buffer == NULL) {
        ESP_LOGI(TAG,"allocating pwm_buffer with %zu bytes", AAC_MAX_OUTPUT_SIZE);
        stat->pwm_buffer = malloc(AAC_MAX_OUTPUT_SIZE);
    }
    if (stat->pwm_buffer==NULL || stat->frame_buffer==NULL){
        ESP_LOGE(TAG, "Not enough memory for buffers");
        stat->active = false;
        return;
    }
    memset(stat->frame_buffer,0, AAC_MAX_FRAME_SIZE);
    memset(stat->pwm_buffer,0, AAC_MAX_OUTPUT_SIZE);
    stat->active = true;
}