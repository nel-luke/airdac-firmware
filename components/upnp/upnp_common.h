#ifndef AIRDAC_FIRMWARE_UPNP_COMMON_H
#define AIRDAC_FIRMWARE_UPNP_COMMON_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define ESP_VER_STR STR(ESP_IDF_VERSION_MAJOR) "." STR(ESP_IDF_VERSION_MINOR) "." STR(ESP_IDF_VERSION_PATCH)

#define AV_TRANSPORT_CHANGED        BIT0
#define CONNECTION_MANAGER_CHANGED  BIT1
#define RENDERING_CONTROL_CHANGED   BIT2

#define AV_TRANSPORT_SEND_ALL       BIT3
#define SEND_PROTOCOL_INFO          BIT4
#define RENDERING_CONTROL_SEND_ALL  BIT5

#define EVENTING_CLEAN_SUBSCRIBERS  BIT6
#define DISCOVERY_SEND_NOTIFY       BIT7
#define START_STREAMING             BIT8
#define BUFFER_READY                BIT9
#define DECODER_READY               BIT10
#define RESUME_PLAYBACK             BIT13
#define PAUSE_PLAYBACK              BIT14
#define STOP_PLAYBACK               BIT15
#define RESET_PLAYBACK              BIT16

#define ALL_EVENT_BITS     0x00FFFFFF

extern const char* useragent_STR;

#define SERVER_STR "esp-idf/" ESP_VER_STR " UPnP/1.0 AirDAC/1.0"
extern const char* server_STR;

struct FileInfo {
    uint32_t file_size;
    uint32_t bitrate;
    uint32_t sample_rate;
    uint32_t bit_depth;
    uint32_t channels;
};
typedef struct FileInfo FileInfo_t;

char* get_date(void);

void start_events(void);
uint32_t get_events(void);
void flag_event(uint32_t event);
void unflag_event(uint32_t event);

#endif //AIRDAC_FIRMWARE_UPNP_COMMON_H
