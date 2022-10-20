#include "control_common.h"

#include <string.h>
#include <stdarg.h>

char* to_xml(unsigned int num_pairs, ...) {
    va_list args1, args2;
    va_start(args1, num_pairs);
    va_copy(args2, args1);

    unsigned int total_chars = 0;
    for (int i = 0; i < num_pairs; i++) {
        total_chars += 5 + 2*strlen(va_arg(args1, const char*)) + strlen(va_arg(args1, const char*)); //<></>
    }
    va_end(args1);

    char* result = malloc(total_chars+1);
    char* pos = result;
    for (int i = 0; i < num_pairs; i++) {
        const char* tag = va_arg(args2, const char*);
        const char* value = va_arg(args2, const char*);
        *pos++ = '<';
        memcpy(pos, tag, strlen(tag));
        pos += strlen(tag);
        *pos++ = '>';
        memcpy(pos, value, strlen(value));
        pos += strlen(value);
        *pos++ = '<';
        *pos++ = '/';
        memcpy(pos, tag, strlen(tag));
        pos += strlen(tag);
        *pos++ = '>';
    }
    *pos = '\0';
    va_end(args2);

    return result;
}

char* get_argument(char* str, const char* name) {
    char* arg_start = strstr(str, name);

    if (arg_start == NULL)
        return NULL;

    arg_start = strstr(arg_start, ">");
    char* arg_end = strstr(arg_start, "<");
    *arg_end = '\0';

    return arg_start + 1;
}