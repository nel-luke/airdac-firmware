#ifndef AIRDAC_FIRMWARE_CONTROL_COMMON_H
#define AIRDAC_FIRMWARE_CONTROL_COMMON_H

#define CHECK_VAR_H(bit, name, fmt, val)   \
if (changed_variables & (bit)) {    \
    tag_size = snprintf(pos, (total_chars < 0 ? 0 : total_chars), "<" name " val=\"" fmt "\"/>", (val)); \
    total_chars -= tag_size;        \
    pos = pos==NULL ? NULL : pos + tag_size; \
}

#define ARG_START() char* next_pos = NULL
#define GET_ARG(name) char* name = get_argument(arguments, #name, &next_pos)
#define ARG(name) #name, name

enum action_err {
    Action_OK,
    Socket_Failure,
    Invalid_Action,
    Invalid_Args,
    Action_Failed,
    Invalid_Value,
    Out_Of_Range,
    Action_Not_Implemented,
    Out_Of_Memory,
    Need_Reset,
    String_Too_Long,
    Illegal_Type,
    Content_Busy,
    Resource_Not_Found,
    Cannot_Transition,
    No_Contents,
    Speed_Unsupported,
    Seek_Unsupported,
    Illegal_Seek,
    Play_Mode_Unsupported,
    Num_Errs
};

struct action_err_s{
    int code;
    const char* str;
};
extern const struct action_err_s action_err_d[Num_Errs];

typedef enum action_err action_err_t;
extern const char* action_err_str[];

extern const char protocol_info[];

#define UNIMPLEMENTED(name) action_err_t name(char* arguments, char** response) { return Action_Not_Implemented; }
struct action {
    const char* name;
    action_err_t (*handle)(char* arguments, char** response);
};
#define ACTION(name) { #name, name }

char* to_xml(unsigned int num_pairs, ...);
char* get_argument(char* str, const char* name, char** next_pos);

#endif //AIRDAC_FIRMWARE_CONTROL_COMMON_H
