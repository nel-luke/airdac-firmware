#include "av_transport.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#include <esp_log.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define TRANSPORTSTATE                  BIT0
#define TRANSPORTSTATUS                 BIT1
#define PLAYBACKSTORAGEMEDIUM           BIT2
#define RECORDSTORAGEMEDIUM             BIT3
#define POSSIBLEPLAYBACKSTORAGEMEDIA    BIT4
#define POSSIBLERECORDSTORAGEMEDIA      BIT5
#define CURRENTPLAYMODE                 BIT6
#define TRANSPORTPLAYSPEED              BIT7
#define RECORDMEDIUMWRITESTATUS         BIT8
#define CURRENTRECORDQUALITYMODE        BIT9
#define POSSIBLERECORDQUALITYMODES      BIT10
#define NUMBEROFTRACKS                  BIT11
#define CURRENTTRACK                    BIT12
#define CURRENTTRACKDURATION            BIT13
#define CURRENTMEDIADURATION            BIT14
#define CURRENTTRACKMETADATA            BIT15
#define CURRENTTRACKURI                 BIT16
#define AVTRANSPORTURI                  BIT17
#define AVTRANSPORTURIMETADATA          BIT18
#define NEXTAVTRANSPORTURI              BIT19
#define NEXTAVTRANSPORTURIMETADATA      BIT20
#define CURRENTTRANSPORTACTIONS         BIT21
static EventGroupHandle_t avt_events;

static const char TAG[] = "av_transport";

enum var_opt {
    NOT_IMPLEMENTED,
    STATE_STOPPED,
    STATE_PAUSED_PLAYBACK,
    STATE_PLAYING,
    STATE_TRANSITIONING,
    STATE_NO_MEDIA_PRESENT,
    STATUS_OK,
    STATUS_ERROR_OCCURRED,
    STORAGE_NONE,
    STORAGE_NETWORK,
    PLAYBACK_NORMAL,
    PLAY_SPEED_1,
    SEEKMODE_TRACK_NR,
    SEEKMODE_REL_TIME,
    SEEKMODE_ABS_TIME,
    SEEKMODE_X_DLNA_REL_BYTE,
    AVAILABLE_ACTIONS,
    NOTHING,
    NUM_OPTS
};
typedef enum var_opt var_opt_t;

static const char* var_opt_str[NUM_OPTS] = {
        "NOT_IMPLEMENTED",
        "STOPPED",
        "PAUSED_PLAYBACK",
        "PLAYING",
        "TRANSITIONING",
        "NO_MEDIA_PRESENT",
        "OK",
        "ERROR_OCCURRED",
        "NONE",
        "NETWORK",
        "NORMAL",
        "1",
        "TRACK_NR",
        "REL_TIME",
        "ABS_TIME",
        "X_DLNA_REL_BYTE",
        "Play,Stop,Pause,Seek,Next,Previous",
        ""
};

struct {
    var_opt_t TransportState;
    var_opt_t TransportStatus;
    var_opt_t PlaybackStorageMedium;
    var_opt_t RecordStorageMedium;
    var_opt_t PossiblePlaybackStorageMedia;
    var_opt_t PossibleRecordStorageMedia;
    var_opt_t CurrentPlayMode;
    var_opt_t TransportPlaySpeed;
    var_opt_t RecordMediumWriteStatus;
    var_opt_t CurrentRecordQualityMode;
    var_opt_t PossibleRecordQualityModes;
    uint32_t NumberOfTracks;
    uint32_t CurrentTrack;
    char CurrentTrackDuration[13];
    char CurrentMediaDuration[13];
    char* CurrentTrackMetaData;
    char* CurrentTrackURI;
    char* AVTransportURI;
    char* AVTransportURIMetaData;
    char* NextAVTransportURI;
    char* NextAVTransportURIMetaData;
    char* CurrentTransportActions;
    char RelativeTimePosition[9];
    char AbsoluteTimePosition[9];
    int32_t RelativeCounterPosition;
    int32_t AbsoluteCounterPosition;
//    char* LastChange;
    char* A_ARG_TYPE_SeekMode;
    char* A_Arg_TYPE_SeekTarget;
    // A_ARG_TYPE_InstanceID is always 0
} avt_state = {
        STATE_NO_MEDIA_PRESENT,
        STATUS_OK,
        STORAGE_NONE,
        NOT_IMPLEMENTED,
        STORAGE_NETWORK,
        NOT_IMPLEMENTED,
        PLAYBACK_NORMAL,
        PLAY_SPEED_1,
        NOT_IMPLEMENTED,
        NOT_IMPLEMENTED,
        NOT_IMPLEMENTED,
        0,
        0,
        "00:00:00.000",
        "00:00:00.000",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "00:00:00",
        "00:00:00",
        2147483647,
        2147483647,
        NULL,
        NULL,
//        NULL
};
static SemaphoreHandle_t avt_mutex;

#define CHECK_VAR_OPT(bit, name) CHECK_VAR_H(bit, #name, "%s", var_opt_str[avt_state.name])
#define CHECK_VAR_INT(bit, name) CHECK_VAR_H(bit, #name, "%d", avt_state.name)
#define CHECK_VAR_STR(bit, name) CHECK_VAR_H(bit, #name, "%s", avt_state.name)
#define INIT_STRING(name, var_opt_name) avt_state.name = (char*)var_opt_str[var_opt_name]

static FileInfo_t buffer_info = {0 };

void get_stream_info(FileInfo_t* info) {
    xSemaphoreTake(avt_mutex, portMAX_DELAY);
    memcpy(info, &buffer_info, sizeof(FileInfo_t));
    xSemaphoreGive(avt_mutex);
}

void init_av_transport(void) {
    avt_events = xEventGroupCreate();
    avt_mutex = xSemaphoreCreateMutex();
    xEventGroupSetBits(avt_events, ALL_EVENT_BITS);

    INIT_STRING(CurrentTrackMetaData, NOTHING);
    INIT_STRING(CurrentTrackURI, NOTHING);
    INIT_STRING(AVTransportURI, NOTHING);
    INIT_STRING(AVTransportURIMetaData, NOTHING);
    INIT_STRING(NextAVTransportURI, NOTHING);
    INIT_STRING(NextAVTransportURIMetaData, NOTHING);
    INIT_STRING(CurrentTransportActions, AVAILABLE_ACTIONS);
}

static char* av_transport_changes(uint32_t changed_variables) {
    char* response = NULL;
    char* pos;
    int total_chars = 0;
    int tag_size = 0;

    xSemaphoreTake(avt_mutex, portMAX_DELAY);
print_chars:
    pos = response;
    CHECK_VAR_OPT(TRANSPORTSTATE, TransportState)
    CHECK_VAR_OPT(TRANSPORTSTATUS, TransportStatus)
    CHECK_VAR_OPT(PLAYBACKSTORAGEMEDIUM, PlaybackStorageMedium)
    CHECK_VAR_OPT(RECORDSTORAGEMEDIUM, RecordStorageMedium)
    CHECK_VAR_OPT(POSSIBLEPLAYBACKSTORAGEMEDIA, PossiblePlaybackStorageMedia)
    CHECK_VAR_OPT(POSSIBLERECORDSTORAGEMEDIA, PossibleRecordStorageMedia)
    CHECK_VAR_OPT(CURRENTPLAYMODE, CurrentPlayMode)
    CHECK_VAR_OPT(TRANSPORTPLAYSPEED, TransportPlaySpeed)
    CHECK_VAR_OPT(RECORDMEDIUMWRITESTATUS, RecordMediumWriteStatus)
    CHECK_VAR_OPT(CURRENTRECORDQUALITYMODE, CurrentRecordQualityMode)
    CHECK_VAR_OPT(POSSIBLERECORDQUALITYMODES, PossibleRecordQualityModes)
    CHECK_VAR_INT(NUMBEROFTRACKS, NumberOfTracks)
    CHECK_VAR_OPT(CURRENTTRACK, CurrentTrack)
    CHECK_VAR_STR(CURRENTTRACKDURATION, CurrentTrackDuration)
    CHECK_VAR_STR(CURRENTMEDIADURATION, CurrentMediaDuration)
    CHECK_VAR_STR(CURRENTTRACKMETADATA, CurrentTrackMetaData)
    CHECK_VAR_STR(CURRENTTRACKURI, CurrentTrackURI)
    CHECK_VAR_STR(AVTRANSPORTURI, AVTransportURI)
    CHECK_VAR_STR(AVTRANSPORTURIMETADATA, AVTransportURIMetaData)
    CHECK_VAR_STR(NEXTAVTRANSPORTURI, NextAVTransportURI)
    CHECK_VAR_STR(NEXTAVTRANSPORTURIMETADATA, NextAVTransportURIMetaData)
    CHECK_VAR_STR(CURRENTTRANSPORTACTIONS, CurrentTransportActions)

    if (response == NULL) {
        total_chars *= -1;
        total_chars++;
        response = malloc(total_chars+1);
        goto print_chars;
    }

    xSemaphoreGive(avt_mutex);
    return response;
}

inline char* get_av_transport_changes(void) {
    uint32_t changed_variables = xEventGroupWaitBits(avt_events, ALL_EVENT_BITS, pdTRUE, pdFALSE, 0);
    return av_transport_changes(changed_variables);
}

inline char* get_av_transport_all(void) {
    return av_transport_changes(ALL_EVENT_BITS);
}

static inline void state_changed(uint32_t variables) {
    xEventGroupSetBits(avt_events, variables);
    flag_event(AV_TRANSPORT_CHANGED);
}

inline char* get_track_url(void) {
    xSemaphoreTake(avt_mutex, portMAX_DELAY);
    char* ret = strdup(avt_state.CurrentTrackURI);
    xSemaphoreGive(avt_mutex);
    return ret;
}

static bool get_stream_value(char* metadata, const char* search, uint32_t* value) {
    char* value_start = strstr(metadata, search);
    if (value_start != NULL) {
        value_start += strlen(search);
        char* value_end = strstr(value_start, "\"");
        *value_end = '\0';
        *value = atol(value_start);
        *value_end = ' ';
    } else {
        return false;
    }

    return true;
}

static action_err_t SetAVTransportURI(char* arguments, char** response) {
    action_err_t ret = Action_OK;

    ARG_START();
    GET_ARG(CurrentURI);
    GET_ARG(CurrentURIMetaData);

    if (CurrentURI == NULL || CurrentURIMetaData == NULL)
        return Invalid_Args;

    xSemaphoreTake(avt_mutex, portMAX_DELAY);

//    if (strlen(avt_state.AVTransportURIMetaData) != 0)
//        free(avt_state.AVTransportURIMetaData);
//
//    avt_state.AVTransportURIMetaData = malloc(strlen(CurrentURIMetaData) + 1);
//    strcpy(avt_state.AVTransportURIMetaData, CurrentURIMetaData);
//    avt_state.CurrentTrackMetaData = avt_state.AVTransportURIMetaData;

    const char mime_search[] = "protocolInfo=\"";
    char* mime_start = strstr(CurrentURIMetaData, mime_search);
    if (mime_start != NULL) {
        mime_start += strlen(mime_search);
        char* mime_end = mime_start;
        for (int i = 0; i < 3; i++) {
            while (*(++mime_end) != ':')
                ;
        }
        *mime_end = '\0';
        if (strstr(protocol_info, mime_start) == NULL)
            ret = Illegal_Type;
        *mime_end = ' '; // Insert a non-zero character to allow searching again
    }

    const char duration_search[] = "duration=\"";
    char* duration_start = strstr(CurrentURIMetaData, duration_search);
    if (duration_start != NULL) {
        duration_start += strlen(duration_search);
        char* duration_end = strstr(duration_start, "\"");
        *duration_end = '\0';
        strcpy(avt_state.CurrentMediaDuration, duration_start);
        strcpy(avt_state.CurrentTrackDuration, duration_start);
        *duration_end = ' ';
    }

    bool success = true;
    success &= get_stream_value(CurrentURIMetaData, "size=\"", &buffer_info.file_size);
    success &= get_stream_value(CurrentURIMetaData, "bitrate=\"", &buffer_info.bitrate);
    success &= get_stream_value(CurrentURIMetaData, "sampleFrequency=\"", &buffer_info.sample_rate);
    success &= get_stream_value(CurrentURIMetaData, "bitsPerSample=\"", &buffer_info.bit_depth);
    success &= get_stream_value(CurrentURIMetaData, "nrAudioChannels=\"", &buffer_info.channels);

    if (!success) {
        ESP_LOGW(TAG, "Failed to retrieve some metadata info. Will have to determine them from the file");
    }

//    printf("File size: %d\nBit rate: %d\nSample rate: %d\nBit depth: %d\nChannels: %d\n",
//           stream_info.file_size, stream_info.bitrate, stream_info.sample_rate,
//           stream_info.bit_depth, stream_info.channels);

    if (strlen(avt_state.AVTransportURI) != 0)
        free(avt_state.AVTransportURI);

    avt_state.AVTransportURI = malloc(strlen(CurrentURI) + 1);
    strcpy(avt_state.AVTransportURI, CurrentURI);
    avt_state.CurrentTrackURI = avt_state.AVTransportURI;

    avt_state.NumberOfTracks = 1;
    avt_state.CurrentTrack = 1;
    switch (avt_state.TransportState) {
        case STATE_NO_MEDIA_PRESENT:
            avt_state.TransportState = STATE_STOPPED;
            flag_event(START_STREAMING);
            break;
        case STATE_PLAYING:
            avt_state.TransportState = STATE_TRANSITIONING;
            break;
        default:
            ;
    }
    flag_event(START_STREAMING);

    xSemaphoreGive(avt_mutex);

    state_changed(AVTRANSPORTURI | AVTRANSPORTURIMETADATA | CURRENTTRACKURI |
                CURRENTTRACKMETADATA | CURRENTTRACKDURATION | CURRENTMEDIADURATION |
                NUMBEROFTRACKS | CURRENTTRACK | TRANSPORTSTATE);
    return ret;
}

#include <esp_log.h>
static action_err_t SetNextAVTransportURI(char* arguments, char** response) {
    ARG_START();
    GET_ARG(NextURI);
    GET_ARG(NextURIMetaData);

    ESP_LOGI("NextURI", "I'm called!");
    if (NextURI == NULL || NextURIMetaData == NULL)
        return Invalid_Args;

    xSemaphoreTake(avt_mutex, portMAX_DELAY);
    if (strlen(avt_state.NextAVTransportURI) != 0)
        free(avt_state.NextAVTransportURI);

    avt_state.NextAVTransportURI = malloc(strlen(NextURI) + 1);
    strcpy(avt_state.NextAVTransportURI, NextURI);

    if (strlen(avt_state.NextAVTransportURIMetaData) != 0)
        free(avt_state.NextAVTransportURIMetaData);

    avt_state.NextAVTransportURIMetaData = malloc(strlen(NextURIMetaData) + 1);
    strcpy(avt_state.NextAVTransportURIMetaData, NextURIMetaData);
    xSemaphoreGive(avt_mutex);

    state_changed(NEXTAVTRANSPORTURI | NEXTAVTRANSPORTURIMETADATA);
    return Action_OK;
}

static action_err_t GetMediaInfo(char* arguments, char** response) {
    xSemaphoreTake(avt_mutex, portMAX_DELAY);
    char NrTracks[3];
    itoa(avt_state.NumberOfTracks, NrTracks, 10);
    const char* MediaDuration = avt_state.CurrentMediaDuration;
    const char* CurrentURI = avt_state.AVTransportURI;
    const char* CurrentURIMetaData = avt_state.AVTransportURIMetaData;
    const char* NextURI = avt_state.NextAVTransportURI;
    const char* NextURIMetaData = avt_state.NextAVTransportURIMetaData;
    const char* PlayMedium = var_opt_str[avt_state.PlaybackStorageMedium];
    const char* RecordMedium = var_opt_str[avt_state.RecordStorageMedium];
    const char* WriteStatus = var_opt_str[avt_state.RecordMediumWriteStatus];

    *response = to_xml(9, ARG(NrTracks), ARG(MediaDuration), ARG(CurrentURI),
                       ARG(CurrentURIMetaData), ARG(NextURI), ARG(NextURIMetaData),
                       ARG(PlayMedium), ARG(RecordMedium), ARG(WriteStatus));
    xSemaphoreGive(avt_mutex);

    return Action_OK;
}

static action_err_t GetTransportInfo(char* arguments, char** response) {
    xSemaphoreTake(avt_mutex, portMAX_DELAY);
    const char* CurrentTransportState = var_opt_str[avt_state.TransportState];
    const char* CurrentTransportStatus = var_opt_str[avt_state.TransportStatus];
    const char* CurrentSpeed = var_opt_str[avt_state.TransportPlaySpeed];

    *response = to_xml(3, ARG(CurrentTransportState), 
                       ARG(CurrentTransportStatus), ARG(CurrentSpeed));
    xSemaphoreGive(avt_mutex);

    return Action_OK;
}

static action_err_t GetPositionInfo(char* arguments, char** response) {
    xSemaphoreTake(avt_mutex, portMAX_DELAY);
    char Track[3];
    itoa(avt_state.CurrentTrack, Track, 10);
    const char* TrackDuration = avt_state.CurrentMediaDuration;
    const char* TrackMetaData = avt_state.CurrentTrackMetaData;
    const char* TrackURI = avt_state.CurrentTrackURI;
    const char* RelTime = avt_state.RelativeTimePosition;
    const char* AbsTime = avt_state.AbsoluteTimePosition;
    char RelCount[12];
    itoa(avt_state.RelativeCounterPosition, RelCount, 10);
    char AbsCount[12];
    itoa(avt_state.AbsoluteCounterPosition, AbsCount, 10);

    *response = to_xml(8, ARG(Track), ARG(TrackDuration), ARG(TrackMetaData), ARG(TrackURI),
                       ARG(RelTime), ARG(AbsTime), ARG(RelCount), ARG(AbsCount));
    xSemaphoreGive(avt_mutex);

    return Action_OK;
}

static action_err_t GetDeviceCapabilities(char* arguments, char** response) {
    xSemaphoreTake(avt_mutex, portMAX_DELAY);
    const char* PlayMedia = var_opt_str[avt_state.PossiblePlaybackStorageMedia];
    const char* RecMedia = var_opt_str[avt_state.PossibleRecordStorageMedia];
    const char* RecQualityModes = var_opt_str[avt_state.PossibleRecordQualityModes];

    *response = to_xml(3, ARG(PlayMedia), ARG(RecMedia), ARG(RecQualityModes));
    xSemaphoreGive(avt_mutex);

    return Action_OK;
}

static action_err_t GetTransportSettings(char* arguments, char** response) {
    xSemaphoreTake(avt_mutex, portMAX_DELAY);
    const char* PlayMode = var_opt_str[avt_state.CurrentPlayMode];
    const char* RecQualityMode = var_opt_str[avt_state.CurrentRecordQualityMode];

    *response = to_xml(2, ARG(PlayMode), ARG(RecQualityMode));
    xSemaphoreGive(avt_mutex);

    return Action_OK;
}

static action_err_t Stop(char* arguments, char** response) {
    action_err_t ret = Action_OK;

    xSemaphoreTake(avt_mutex, portMAX_DELAY);
    switch (avt_state.TransportState) {
        case STATE_NO_MEDIA_PRESENT:
            ret = Cannot_Transition;
            break;
        default:
            avt_state.TransportState = STATE_STOPPED;
    }
    xSemaphoreGive(avt_mutex);

    state_changed(TRANSPORTSTATE);
    return ret;
}

static action_err_t Play(char* arguments, char** response) {
    action_err_t ret = Action_OK;

    xSemaphoreTake(avt_mutex, portMAX_DELAY);
    if (strlen(avt_state.AVTransportURI) != 0) {
        switch (avt_state.TransportState) {
            case STATE_STOPPED:
            case STATE_PLAYING:
            case STATE_PAUSED_PLAYBACK:
                avt_state.TransportState = STATE_PLAYING;
                break;
            default:
                ret = Cannot_Transition;
        }
    } else {
        ret = No_Contents;
    }
    xSemaphoreGive(avt_mutex);

    state_changed(TRANSPORTSTATE);
    return ret;
}

static action_err_t Pause(char* arguments, char** response) {
    action_err_t ret = Action_OK;

    xSemaphoreTake(avt_mutex, portMAX_DELAY);
    switch (avt_state.TransportState) {
        case STATE_PLAYING:
            avt_state.TransportState = STATE_PAUSED_PLAYBACK;
            break;
        default:
            ret = Cannot_Transition;
    }
    xSemaphoreGive(avt_mutex);

    state_changed(TRANSPORTSTATE);
    return ret;
}

UNIMPLEMENTED(Record)

static action_err_t Seek(char* arguments, char** response) {
    action_err_t ret = Action_OK;

    ARG_START();
    GET_ARG(Unit);
    GET_ARG(Target);

    if (Unit == NULL || Target == NULL)
        return Invalid_Args;

    xSemaphoreTake(avt_mutex, portMAX_DELAY);
    if (avt_state.TransportState & (STATE_PLAYING | STATE_STOPPED | STATE_PAUSED_PLAYBACK)) {
        if (avt_state.A_ARG_TYPE_SeekMode != NULL)
            free(avt_state.A_ARG_TYPE_SeekMode);

        avt_state.A_ARG_TYPE_SeekMode = malloc(strlen(Unit)+1);
        strcpy(avt_state.A_ARG_TYPE_SeekMode, Unit);

        if (avt_state.A_Arg_TYPE_SeekTarget != NULL)
            free(avt_state.A_Arg_TYPE_SeekTarget);

        avt_state.A_Arg_TYPE_SeekTarget = malloc(strlen(Target)+1);
        strcpy(avt_state.A_Arg_TYPE_SeekTarget, Target);
    } else {
        ret = Cannot_Transition;
    }

//    avt_state.TransportState = STATE_TRANSITIONING;
    xSemaphoreGive(avt_mutex);

//    state_changed(TRANSPORTSTATE);
    return ret;
}

static action_err_t Next(char* arguments, char** response) {
    action_err_t ret = Action_OK;

    xSemaphoreTake(avt_mutex, portMAX_DELAY);
    switch (avt_state.TransportState) {
        case STATE_PLAYING:
        case STATE_STOPPED:
        case STATE_PAUSED_PLAYBACK:
            break;
        default:
            ret = Cannot_Transition;
    }
    xSemaphoreGive(avt_mutex);

    return ret;
}

static action_err_t Previous(char* arguments, char** response) {
    action_err_t ret = Action_OK;

    xSemaphoreTake(avt_mutex, portMAX_DELAY);
    switch (avt_state.TransportState) {
        case STATE_PLAYING:
        case STATE_STOPPED:
        case STATE_PAUSED_PLAYBACK:
            break;
        default:
            ret = Cannot_Transition;
    }
    xSemaphoreGive(avt_mutex);

    return ret;
}

static action_err_t SetPlayMode(char* arguments, char** response) {
    ARG_START();
    GET_ARG(NewPlayMode);

    if (NewPlayMode == NULL)
        return Invalid_Args;
//    xSemaphoreTake(avt_mutex, portMAX_DELAY);

//    xSemaphoreGive(avt_mutex);

    state_changed(CURRENTPLAYMODE);
    return Action_OK;
}

UNIMPLEMENTED(SetRecordQualityMode)

static action_err_t GetCurrentTransportAction(char* arguments, char** response) {
    xSemaphoreTake(avt_mutex, portMAX_DELAY);
    const char* Actions = avt_state.CurrentTransportActions;

    *response = to_xml(1, ARG(Actions));
    xSemaphoreGive(avt_mutex);

    return Action_OK;
}

#define NUM_ACTIONS 17
static const struct action action_list[NUM_ACTIONS] = {
        ACTION(SetAVTransportURI),
        ACTION(SetNextAVTransportURI),
        ACTION(GetMediaInfo),
        ACTION(GetTransportInfo),
        ACTION(GetPositionInfo),
        ACTION(GetDeviceCapabilities),
        ACTION(GetTransportSettings),
        ACTION(Stop),
        ACTION(Play),
        ACTION(Pause),
        ACTION(Record),
        ACTION(Seek),
        ACTION(Next),
        ACTION(Previous),
        ACTION(SetPlayMode),
        ACTION(SetRecordQualityMode),
        ACTION(GetCurrentTransportAction)
};

action_err_t av_transport_execute(const char* action_name, char* arguments, char** response) {
    action_err_t err = Invalid_Action;
    int i = 0;
    while (i < NUM_ACTIONS) {
        if (strcmp(action_name, action_list[i].name) == 0) {
            err = action_list[i].handle(arguments, response);
            break;
        }
        i++;
    }

    return err;
}