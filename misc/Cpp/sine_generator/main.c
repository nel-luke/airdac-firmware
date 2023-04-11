#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

#define FILE_NAME   "sine_wave.wav"
#define BIT_DEPTH   (24)
#define SAMPLE_RATE (44100)
#define FREQ        (997)
#define SILENCE_PRE (0)
#define SILENCE_POST (0)
#define SECONDS     (10)
#define VOLUME_DB   (-60)

#define TWOPI           (6.28318531f)
#define PHASE_INC       (TWOPI * FREQ / SAMPLE_RATE)

#define HEADER_BYTES 44

#define SWAP4(_buf) (_buf = ((_buf&0xff)<<24) | ((_buf&0xff00)<<8) | ((_buf>>8)&0xff00) | ((_buf>>24)&0xff))
#define SWAP2(_buf) (_buf = ((_buf&0xff)<<8) | ((_buf>>8)&0xff))

int main() {
    srand(time(NULL));
    printf("Hello, World!\n");

    uint32_t num_samples = 2 * SAMPLE_RATE * SECONDS;
    uint32_t sound_start = 2 * SAMPLE_RATE * SILENCE_PRE;
    uint32_t silence_samples = 2 * SAMPLE_RATE * (SILENCE_PRE + SILENCE_POST);
    uint32_t total_samples = num_samples + silence_samples;

#if BIT_DEPTH == 16
#define AMPLITUDE 90.633966E-6
    uint32_t data_bytes = sizeof(int16_t) * (num_samples+silence_samples);
    int16_t* data = malloc(data_bytes);
    memset(data, 0, total_samples);
#elif  BIT_DEPTH == 24
#define AMPLITUDE 354.0335486E-9
    uint32_t data_bytes = sizeof(int32_t) * (num_samples+silence_samples);
    int32_t* data = malloc(data_bytes);
#elif BIT_DEPTH == 32
#define AMPLITUDE 1.382943467E-9
    uint32_t data_bytes = sizeof(int32_t) * (num_samples+silence_samples);
    int32_t* data = malloc(data_bytes);
#endif

    float fsamp = 0.0f;
    uint16_t isamp = 0;
    static float p = 0.0f;

    for (int i = sound_start; i < (sound_start+num_samples-1); i+=2) {
        double dither = floor((double)(rand()+rand()-RAND_MAX/2)/RAND_MAX);
        fsamp = sinf(p) *pow(10,VOLUME_DB/20);
        isamp = (int16_t) (fsamp * (pow(2, BIT_DEPTH-1) - 1) + dither);

        // Increment and wrap phase
        p += PHASE_INC;
        if (p >= TWOPI)
            p -= TWOPI;

        data[i] = data[i+1] = isamp;
    }


    FILE* f = fopen(FILE_NAME, "wb");
    fwrite("RIFF", 1, 4, f); // RIFF

    uint32_t buf4 = data_bytes+HEADER_BYTES-8;
    //SWAP4(buf4);
    fwrite(&buf4, 4, 1, f); // File size
    fwrite("WAVEfmt ", 1, 8, f);

    buf4 = 16;
    //SWAP4(buf4);
    fwrite(&buf4, 4, 1, f); // Format size

    uint16_t buf2 = 1;
    //SWAP2(buf2);
    fwrite(&buf2, 2, 1, f); // Format type

    buf2 = 2;
    //SWAP2(buf2);
    fwrite(&buf2, 2, 1, f); // Channels

    buf4 = SAMPLE_RATE;
    //SWAP4(buf4);
    fwrite(&buf4, 4, 1, f); // Sample rate

    buf4 = 2*SAMPLE_RATE*BIT_DEPTH/8;
    //SWAP4(buf4);
    fwrite(&buf4, 4, 1, f); // Byte rate

    buf2 = 2*BIT_DEPTH/8;
    //SWAP2(buf2);
    fwrite(&buf2, 2, 1, f); // Bytes per frame

    buf2 = BIT_DEPTH;
    //SWAP2(buf2);
    fwrite(&buf2, 2, 1, f); // Bits per sample

    fwrite("data", 4, 1, f);
    fwrite(&data_bytes, 4, 1, f);
    //fwrite((void*) data, BIT_DEPTH/8, num_samples+silence_samples, f);
    for (int i = 0; i < num_samples+silence_samples; i++)
        fwrite((void*) (data+i), BIT_DEPTH/8, 1, f);
    fclose(f);

    free(data);

    return 0;
}
