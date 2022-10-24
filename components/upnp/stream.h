#ifndef AIRDAC_FIRMWARE_STREAM_H
#define AIRDAC_FIRMWARE_STREAM_H

#include <stddef.h>

typedef void (*buffer_ready_cbt)(void);
typedef void (*stream_disconnected_cbt)(void);

void init_stream(size_t stack_size, int priority, const char* user_agent, int port,
         buffer_ready_cbt buffer_ready_cb, stream_disconnected_cbt stream_disconnected_cb);
void start_stream(char* url, size_t file_size, size_t buffer_size);
void seek_stream(int seek_position);
void go(void);
void stop_stream(void);
void* take_ready_buffer(void);
void release_ready_buffer(void);


#endif //AIRDAC_FIRMWARE_STREAM_H
