#ifndef AIRDAC_FIRMWARE_STREAM_H
#define AIRDAC_FIRMWARE_STREAM_H

#include <stddef.h>
#include <stdint.h>

#define STREAM_CONFIG_STRUCT            \
    int port;                           \
    const char* user_agent;             \
    size_t buffer_count;                \
    size_t buffer_length;               \
    void (*buffer_ready_cb)(void);         \
    void (*stream_finished_cb)(void);      \
    void (*stream_failed_cb)(void);

struct StreamConfig {
    STREAM_CONFIG_STRUCT
};
typedef struct StreamConfig StreamConfig_t;

void stream_get_content_info(const char* url, char* content_type, size_t* content_length);
void init_stream(size_t stack_size, int priority, const StreamConfig_t* config);
void start_stream(const char* url, size_t file_size);
void seek_stream(size_t seek_position);
void stop_stream(void);
void stream_take_buffer(const uint8_t** buffer, size_t* buffer_length);
void stream_release_buffer(void);


#endif //AIRDAC_FIRMWARE_STREAM_H
