#ifndef AIRDAC_FIRMWARE_CONTROL_COMMON_H
#define AIRDAC_FIRMWARE_CONTROL_COMMON_H

#define CHECK_VAR_H(bit, name, fmt, val)   \
if (changed_variables & (bit)) {    \
    tag_size = snprintf(pos, (total_chars < 0 ? 0 : total_chars), "<" name " val=\"" fmt "\"/>", (val)); \
    total_chars -= tag_size;        \
    pos = pos==NULL ? NULL : pos + tag_size; \
}

#define GET_ARG(name) char* name = get_argument(arguments, #name)
#define ARG(name) #name, name

struct action {
    const char* name;
    void (*handle)(char* arguments, char** response);
};
#define ACTION(name) { #name, name }

char* to_xml(unsigned int num_pairs, ...);
char* get_argument(char* str, const char* name);

#endif //AIRDAC_FIRMWARE_CONTROL_COMMON_H
