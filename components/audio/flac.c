#include "flac.h"
#include "audio_common.h"

#include "codecs/FLAC/format.h"
#include "codecs/FLAC/stream_decoder.h"

#include <memory.h>

#include <esp_log.h>
#include <driver/i2s.h>

#define I2S_NUM 0

static const char TAG[] = "audio_flac";

static FLAC__StreamDecoder* decoder_ptr;

static size_t stream_pos;
static size_t total_length;

void** buff;
uint32_t* buff_len;
size_t remaining;
bool fail;


struct file_data {
    uint32_t blocksize;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bits_per_sample;
    uint64_t total_samples;
    uint32_t buffer_bytes;
    uint32_t* bigbuf;
} data;

static FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
    if (fail == true) {
        send_fail();
        return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
    }

    if (remaining == 0) {
        uint32_t bits = 0;
        xTaskNotifyWait(0, ULONG_MAX, &bits, portMAX_DELAY);
        if (bits & STOP) {
            return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
        } else if (bits & CONTINUE) {
            remaining = *buff_len;
            stream_pos = 0;
        }
    }

    size_t size = remaining > *bytes ? *bytes : remaining;
    remaining -= size;
    *bytes = size;

    memcpy(buffer, *buff+stream_pos, *bytes);
    stream_pos += *bytes;

    if (remaining == 0) {
        send_ready();
    }

    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static FLAC__StreamDecoderSeekStatus seek_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data)
{
//     ESP_LOGI(TAG, "Seeker!");
    return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

static FLAC__StreamDecoderTellStatus tell_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
//    ESP_LOGI(TAG, "Don't tell!");
    *absolute_byte_offset = (FLAC__uint64)stream_pos;
    return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

static FLAC__StreamDecoderLengthStatus length_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data)
{
//    ESP_LOGI(TAG, "Going through lengths here");
    *stream_length = (FLAC__uint64)total_length;

    return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool eof_callback(const FLAC__StreamDecoder *decoder, void *client_data)
{
//    ESP_LOGI(TAG, "Not there yet!");
    return false;
}

static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data) {
    int j = 0;
    for (int i = 0; i < data.blocksize; i++) {
        data.bigbuf[j++] = buffer[0][i]<<8;
        data.bigbuf[j++] = buffer[1][i]<<8;
    }

    size_t bytes_written;
    i2s_write(I2S_NUM, data.bigbuf, data.buffer_bytes, &bytes_written, portMAX_DELAY);
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data) {
    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        const FLAC__StreamMetadata_StreamInfo *md = &(metadata->data.stream_info);
//        ESP_LOGI(TAG, "min_blocksize: %d", md->min_blocksize);
//        ESP_LOGI(TAG, "max_blocksize: %d", md->max_blocksize);
//        ESP_LOGI(TAG, "min_framesize: %d", md->min_framesize);
//        ESP_LOGI(TAG, "max_framesize: %d", md->max_framesize);
//        ESP_LOGI(TAG, "sample_rate: %d", md->sample_rate);
//        ESP_LOGI(TAG, "channels: %d", md->channels);
//        ESP_LOGI(TAG, "bits_per_sample: %d", md->bits_per_sample);
//        ESP_LOGI(TAG, "total_samples: %lld", md->total_samples);

        data.blocksize = md->max_blocksize;
        data.sample_rate = md->sample_rate;
        data.channels = md->channels;
        data.bits_per_sample = md->bits_per_sample;
        data.total_samples = md->total_samples;
    }
}

static void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data) {
    ESP_LOGE(TAG, "Decoder failed: %s", FLAC__StreamDecoderErrorStatusString[status]);
    fail = true;
}

void stop_flac_decoder() {
    FLAC__bool b = FLAC__stream_decoder_finish(decoder_ptr);
    assert(b);
    // Check if next song is also a FLAC, otherwise delete
    FLAC__stream_decoder_delete(decoder_ptr);
}

void start_flac_decoder(size_t stream_len, void** audio_buff, uint32_t* audio_len) {
    decoder_ptr = FLAC__stream_decoder_new();
    FLAC__StreamDecoderInitStatus status = FLAC__stream_decoder_init_stream(decoder_ptr,
                                                                            read_callback, seek_callback, tell_callback,
                                                                            length_callback, eof_callback,
                                                                            write_callback, metadata_callback,
                                                                            error_callback, NULL);
    assert(status == FLAC__STREAM_DECODER_INIT_STATUS_OK);
    stream_pos = 0;
    total_length = stream_len;
    remaining = 0;
    fail = false;

    buff = audio_buff;
    buff_len = audio_len;

    FLAC__stream_decoder_process_until_end_of_metadata(decoder_ptr);

    if (data.bits_per_sample == 16) {
        data.buffer_bytes = 2 * data.blocksize * 2;
    } else {
        data.buffer_bytes = 2 * data.blocksize * 4;
    }
    data.bigbuf = malloc(data.buffer_bytes);
}

void continue_flac_decoder(void) {
    FLAC__stream_decoder_process_until_end_of_stream(decoder_ptr);
    free(data.bigbuf);
}