#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct {
    int16_t **audio;
    int chan_ct;
    int sample_ct;
    int sample_rate;
} sound;

struct header_wav {
    unsigned char riff[4];
    uint32_t size;
    unsigned char wave[4];
    unsigned char fmt[4];
    uint32_t fmtlen;
    uint16_t format;
    uint16_t chan_ct;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    unsigned char fact[4];
    uint32_t fact_size;
    uint32_t fact_num_samples;
    unsigned char data[4];
    uint32_t data_size;
};

#define end_buf2_to_int(_buf) ((_buf)[0] | ((_buf)[1] << 8))
#define end_buf4_to_int(_buf) ((_buf)[0] | ((_buf)[1] << 8) | ((_buf)[2] << 16) | ((_buf)[3] << 24))

sound *
load_wav(char *filename, int verbose)
{
    unsigned char buf4[4];
    unsigned char buf2[2];

    struct header_wav header;

    if (verbose)
        printf(" = & = %s = & = \n", filename);

    FILE *f = fopen(filename, "rb");

    // == read header ==

    fread(header.riff, sizeof(header.riff), 1, f);

    fread(buf4, sizeof(buf4), 1, f);
    header.size = end_buf4_to_int(buf4);

    fread(header.wave, sizeof(header.wave), 1, f);

    fread(header.fmt, sizeof(header.fmt), 1, f);

    fread(buf4, sizeof(buf4), 1, f);
    header.fmtlen = end_buf4_to_int(buf4);

    fread(buf2, sizeof(buf2), 1, f);
    header.format = end_buf2_to_int(buf2);

    fread(buf2, sizeof(buf2), 1, f);
    header.chan_ct = end_buf2_to_int(buf2);

    fread(buf4, sizeof(buf4), 1, f);
    header.sample_rate = end_buf4_to_int(buf4);

    fread(buf4, sizeof(buf4), 1, f);
    header.byte_rate = end_buf4_to_int(buf4);

    fread(buf2, sizeof(buf2), 1, f);
    header.block_align = end_buf2_to_int(buf2);

    fread(buf2, sizeof(buf2), 1, f);
    header.bits_per_sample = end_buf2_to_int(buf2);

    if (verbose) {
        printf("[00-03] riff: '%c%c%c%c' \n"
            , header.riff[0]
            , header.riff[1]
            , header.riff[2]
            , header.riff[3]);
        printf("[04-07] size: %d\n", header.size);
        printf("[08-11] wave: '%c%c%c%c' \n"
            , header.wave[0]
            , header.wave[1]
            , header.wave[2]
            , header.wave[3]);
        printf("[12-15] fmt : '%c%c%c%c' \n"
            , header.fmt[0]
            , header.fmt[1]
            , header.fmt[2]
            , header.fmt[3]);
        printf("[16-19] fmtlen: %d\n", header.fmtlen);
        printf("[20-21] format: %d\n", header.format);
        printf("[22-23] chan_ct: %d\n", header.chan_ct);
        printf("[24-27] srate: %d\n", header.sample_rate);
        printf("[28-31] Brate: %d\n", header.byte_rate);
        printf("[32-33] block_align: %d\n", header.block_align);
        printf("[34-35] bits_per_sample: %d\n", header.bits_per_sample);
    }

    if (header.bits_per_sample != 8
        && header.bits_per_sample != 16
        && header.bits_per_sample != 32) {
        printf("bitrate %d not supported (o . o\n", header.bits_per_sample);
        goto close_file_and_return_null;
    }

    if (header.format == 1) {
        // do nothing
    } else if (header.format == 3) {
        if (header.fmtlen != 16) {
            printf("unimplemented data stuff whateve ikdkkkdf\n");
        }

        fread(header.fact, sizeof(header.fact), 1, f);

        fread(buf4, sizeof(buf4), 1, f);
        header.fact_size = end_buf4_to_int(buf4);

        fread(buf4, sizeof(buf4), 1, f);
        header.fact_num_samples = end_buf4_to_int(buf4);

        if (verbose) {
            printf("[36-29] fact: '%c%c%c%c' \n"
                , header.fact[0]
                , header.fact[1]
                , header.fact[2]
                , header.fact[3]);
            printf("[40-43] fact_size: %d\n", header.fact_size);
            printf("[44-47] fact_num_samples: %d\n", header.fact_num_samples);
        }

        // f*** whatever else give me the data

        unsigned char hack = 'x';
        while (hack != 'd')
            fread(&hack, sizeof(hack), 1, f);
        fseek(f, -1, SEEK_CUR);
    } else {
        printf("header format tag unknown (#^# good bye\n");
        goto close_file_and_return_null;
    }

    fread(header.data, sizeof(header.data), 1, f);

    char ch = 0;
    do {
        fread(&ch, sizeof(ch), 1, f);
    } while (ch != 'd');
    fread(buf4, sizeof(char), 3, f);
    fread(buf4, sizeof(buf4), 1, f);
    header.data_size = end_buf4_to_int(buf4);

    uint32_t sample_ct = header.data_size / header.block_align ;
    float duration = (float)header.data_size / header.byte_rate;
    int bytes_per_chan = header.block_align / header.chan_ct;

    if (verbose) {
        printf("[.data] data: '%c%c%c%c' \n"
            , header.data[0]
            , header.data[1]
            , header.data[2]
            , header.data[3]);
        printf("[.data] data_size: %d\n", header.data_size);
        printf("[.calc] sample_ct: %d\n", sample_ct);
        printf("[.calc] duration: %f\n", duration);
        printf("[.calc] bytes per chan: %d\n", bytes_per_chan);
    }

    //  == alloc sound struct ==

    sound *ret = malloc(sizeof(sound));
    if (!ret)
        goto close_file_and_return_null;

    ret->audio = malloc(sizeof(float *) * header.chan_ct);
    if (!ret->audio) {
        free(ret);
        goto close_file_and_return_null;
    }

    for (int i = 0; i < header.chan_ct; ++i) {
        ret->audio[i] = malloc(sizeof(float) * sample_ct);
        if (!ret->audio[i]) {
            // lol
            for (--i; i >= 0; --i) {
                free(ret->audio[i]);
            }
            free(ret->audio);
            free(ret);
            goto close_file_and_return_null;
        }
    }

    ret->chan_ct = header.chan_ct;
    ret->sample_ct = sample_ct;
    ret->sample_rate = header.sample_rate;

    // == read data ==

    if (header.format == 1) {   // PCM
        unsigned char data_buf[header.block_align];

        switch (bytes_per_chan) {
        case 1: {
                for (unsigned int i = 0; i < sample_ct; ++i) {
                    int buf_at = 0;
                    int read = fread(data_buf, sizeof(data_buf), 1, f);
                    if (read) {
                        for (int j = 0; j < header.chan_ct; ++j) {
                            ret->audio[j][i] = data_buf[buf_at];
                            buf_at += bytes_per_chan;
                        }
                    } else {
                        printf("read error: %d\n", read);
                        goto close_file_and_return_null;
                    }
                }
            } break;
        case 2: {
                for (unsigned int i = 0; i < sample_ct; ++i) {
                    int buf_at = 0;
                    int read = fread(data_buf, sizeof(data_buf), 1, f);
                    if (read) {
                        for (int j = 0; j < header.chan_ct; ++j) {
                            int16_t data = (int16_t)end_buf2_to_int(&data_buf[buf_at]);
                            ret->audio[j][i] = data;
                            buf_at += bytes_per_chan;
                        }
                    } else {
                        printf("read error: %d\n", read);
                        goto close_file_and_return_null;
                    }
                }
            } break;
        default:
            printf("weird bytes per channel......\n");
            goto close_file_and_return_null;
            break;
        }
    } else if (header.format == 3) {   // float
        float data[header.chan_ct];
        for (unsigned int i = 0; i < sample_ct; ++i) {
            int read = fread(data, sizeof(data), 1, f);
            if (read) {
                for (int j = 0; j < header.chan_ct; ++j) {
                    ret->audio[j][i] = data[j];
                }
            } else {
                printf("read error: %d\n", read);
                goto close_file_and_return_null;
            }
        }
    }

    fclose(f);
    return ret;

close_file_and_return_null:
    fclose(f);
    return NULL;
}

void
sound_free(sound *s)
{
    for (int i = 0; i < s->chan_ct; ++i)
        free(s->audio[i]);
    free(s->audio);
    free(s);
}

void print_to_c(sound* tune, char* filename) {
    int length = tune->sample_ct / 16;
    FILE *f = fopen(filename, "w");
    fprintf(f, "#define LENGTH %d\n\nshort song[LENGTH] = {\n", length);
    fprintf(f, "\t%d", tune->audio[0][0]);
    for (int i = 1; i < length; i++) {
        fprintf(f, ",\n\t%d", tune->audio[0][i]);
    }
    fprintf(f, "};\n");
}

int main(void) {
    sound* tune = load_wav("song.wav", 1);
    print_to_c(tune, "song.c");
    printf("Success!\n");
    printf("Data: %d", tune->audio[1][123]);
    sound_free(tune);
    return 0;
}