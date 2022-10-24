#ifndef AIRDAC_FIRMWARE_AUDIO_COMMON_H
#define AIRDAC_FIRMWARE_AUDIO_COMMON_H

#define STOP                    BIT0
#define CONTINUE                BIT1
#define START_FLAC_DECODER      BIT2

void send_ready(void);
void send_fail(void);

#endif //AIRDAC_FIRMWARE_AUDIO_COMMON_H
