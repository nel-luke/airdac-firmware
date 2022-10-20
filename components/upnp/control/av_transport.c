#include "av_transport.h"
#include "control_common.h"
#include "../upnp_common.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

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

static const char* var_opt_string[NUM_OPTS] = {
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
    char CurrentTrackDuration[9];
    char CurrentMediaDuration[9];
    const char* CurrentTrackMetaData;
    char* CurrentTrackURI;
    char* AVTransportURI;
    const char* AVTransportURIMetaData;
    char* NextAVTransportURI;
    const char* NextAVTransportURIMetaData;
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
        "00:00:00",
        "00:00:00",
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

#define CHECK_VAR_OPT(bit, name) CHECK_VAR_H(bit, #name, "%s", var_opt_string[avt_state.name])
#define CHECK_VAR_INT(bit, name) CHECK_VAR_H(bit, #name, "%d", avt_state.name)
#define CHECK_VAR_STR(bit, name) CHECK_VAR_H(bit, #name, "%s", avt_state.name)
#define INIT_STRING(name, var_opt_name) avt_state.name = (char*)var_opt_string[var_opt_name]

void init_av_transport(void) {
    avt_events = xEventGroupCreate();
    avt_mutex = xSemaphoreCreateMutex();
    xEventGroupSetBits(avt_events, ALL_EVENT_BITS);

    INIT_STRING(CurrentTrackMetaData, NOT_IMPLEMENTED);
    INIT_STRING(CurrentTrackURI, NOTHING);
    INIT_STRING(AVTransportURI, NOTHING);
    INIT_STRING(AVTransportURIMetaData, NOT_IMPLEMENTED);
    INIT_STRING(NextAVTransportURI, NOTHING);
    INIT_STRING(NextAVTransportURIMetaData, NOT_IMPLEMENTED);
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
    send_event(AV_TRANSPORT_CHANGED);
}

static void SetAVTransportURI(char* arguments, char** response) {
    GET_ARG(CurrentURI);
    // GET_ARG(CurrentURIMetaData);

    if (CurrentURI != NULL) {
        xSemaphoreTake(avt_mutex, portMAX_DELAY);
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
                break;
            case STATE_PLAYING:
                avt_state.TransportState = STATE_TRANSITIONING;
                break;
            default:
                ;
        }
        xSemaphoreGive(avt_mutex);
    }

    state_changed(AVTRANSPORTURI | AVTRANSPORTURIMETADATA | CURRENTTRACKURI |
                        NUMBEROFTRACKS | CURRENTTRACK | TRANSPORTSTATE);
}

static void SetNextAVTransportURI(char* arguments, char** response) {
    GET_ARG(NextURI);
    // GET_ARG(CurrentURIMetaData);

    if (NextURI != NULL) {
        xSemaphoreTake(avt_mutex, portMAX_DELAY);
        if (strlen(avt_state.NextAVTransportURI) != 0)
            free(avt_state.NextAVTransportURI);

        avt_state.NextAVTransportURI = malloc(strlen(NextURI) + 1);
        strcpy(avt_state.NextAVTransportURI, NextURI);

        xSemaphoreGive(avt_mutex);
    }

    state_changed(NEXTAVTRANSPORTURI);
}

static void GetMediaInfo(char* arguments, char** response) {
    xSemaphoreTake(avt_mutex, portMAX_DELAY);
    char NrTracks[3];
    itoa(avt_state.NumberOfTracks, NrTracks, 10);
    const char* MediaDuration = avt_state.CurrentMediaDuration;
    const char* CurrentURI = avt_state.AVTransportURI;
    const char* CurrentURIMetaData = avt_state.AVTransportURIMetaData;
    const char* NextURI = avt_state.NextAVTransportURI;
    const char* NextURIMetaData = avt_state.NextAVTransportURIMetaData;
    const char* PlayMedium = var_opt_string[avt_state.PlaybackStorageMedium];
    const char* RecordMedium = var_opt_string[avt_state.RecordStorageMedium];
    const char* WriteStatus = var_opt_string[avt_state.RecordMediumWriteStatus];

    *response = to_xml(9, ARG(NrTracks), ARG(MediaDuration), ARG(CurrentURI),
                       ARG(CurrentURIMetaData), ARG(NextURI), ARG(NextURIMetaData),
                       ARG(PlayMedium), ARG(RecordMedium), ARG(WriteStatus));
    xSemaphoreGive(avt_mutex);
}

static void GetTransportInfo(char* arguments, char** response) {
    xSemaphoreTake(avt_mutex, portMAX_DELAY);
    const char* CurrentTransportState = var_opt_string[avt_state.TransportState];
    const char* CurrentTransportStatus = var_opt_string[avt_state.TransportStatus];
    const char* CurrentSpeed = var_opt_string[avt_state.TransportPlaySpeed];

    *response = to_xml(3, ARG(CurrentTransportState), 
                       ARG(CurrentTransportStatus), ARG(CurrentSpeed));
    xSemaphoreGive(avt_mutex);
}

static void GetPositionInfo(char* arguments, char** response) {
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
}

static void GetDeviceCapabilities(char* arguments, char** response) {
    xSemaphoreTake(avt_mutex, portMAX_DELAY);
    const char* PlayMedia = var_opt_string[avt_state.PossiblePlaybackStorageMedia];
    const char* RecMedia = var_opt_string[avt_state.PossibleRecordStorageMedia];
    const char* RecQualityModes = var_opt_string[avt_state.PossibleRecordQualityModes];

    *response = to_xml(3, ARG(PlayMedia), ARG(RecMedia), ARG(RecQualityModes));
    xSemaphoreGive(avt_mutex);
}

static void GetTransportSettings(char* arguments, char** response) {
    xSemaphoreTake(avt_mutex, portMAX_DELAY);
    const char* PlayMode = var_opt_string[avt_state.CurrentPlayMode];
    const char* RecQualityMode = var_opt_string[avt_state.CurrentRecordQualityMode];

    *response = to_xml(2, ARG(PlayMode), ARG(RecQualityMode));
    xSemaphoreGive(avt_mutex);
}

static void Stop(char* arguments, char** response) {
    xSemaphoreTake(avt_mutex, portMAX_DELAY);
    switch (avt_state.TransportState) {
        case STATE_NO_MEDIA_PRESENT:
            break;
        default:
            avt_state.TransportState = STATE_STOPPED;
    }
    xSemaphoreGive(avt_mutex);

    state_changed(TRANSPORTSTATE);
}

static void Play(char* arguments, char** response) {
    xSemaphoreTake(avt_mutex, portMAX_DELAY);
    if (strlen(avt_state.AVTransportURI) != 0) {
        switch (avt_state.TransportState) {
            case STATE_STOPPED:
            case STATE_PLAYING:
            case STATE_PAUSED_PLAYBACK:
                avt_state.TransportState = STATE_PLAYING;
                break;
            default:
                ;
        }
    }
    xSemaphoreGive(avt_mutex);

    state_changed(TRANSPORTSTATE);
}

static void Pause(char* arguments, char** response) {
    xSemaphoreTake(avt_mutex, portMAX_DELAY);
    if (avt_state.TransportState == STATE_PLAYING)
        avt_state.TransportState = STATE_PAUSED_PLAYBACK;
    xSemaphoreGive(avt_mutex);

    state_changed(TRANSPORTSTATE);
}

static void Seek(char* arguments, char** response) {
    GET_ARG(Unit);
    GET_ARG(Target);

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
    }

//    avt_state.TransportState = STATE_TRANSITIONING;
    xSemaphoreGive(avt_mutex);

//    state_changed(TRANSPORTSTATE);
}

static void Next(char* arguments, char** response) {
    xSemaphoreTake(avt_mutex, portMAX_DELAY);
    if (avt_state.TransportState & (STATE_PLAYING | STATE_STOPPED | STATE_PAUSED_PLAYBACK)) {

    }
    xSemaphoreGive(avt_mutex);
}

static void Previous(char* arguments, char** response) {
    xSemaphoreTake(avt_mutex, portMAX_DELAY);
    if (avt_state.TransportState & (STATE_PLAYING | STATE_STOPPED | STATE_PAUSED_PLAYBACK)) {

    }
    xSemaphoreGive(avt_mutex);
}

static void SetPlayMode(char* arguments, char** response) {
//    xSemaphoreTake(avt_mutex, portMAX_DELAY);

//    xSemaphoreGive(avt_mutex);

    state_changed(CURRENTPLAYMODE);
}

static void GetCurrentTransportAction(char* arguments, char** response) {
    xSemaphoreTake(avt_mutex, portMAX_DELAY);
    const char* Actions = avt_state.CurrentTransportActions;

    *response = to_xml(1, ARG(Actions));
    xSemaphoreGive(avt_mutex);
}

#define NUM_ACTIONS 15
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
        ACTION(Seek),
        ACTION(Next),
        ACTION(Previous),
        ACTION(SetPlayMode),
        ACTION(GetCurrentTransportAction)
};

bool av_transport_execute(const char* action_name, char* arguments, char** response) {
    int i = 0;
    while (i < NUM_ACTIONS) {
        if (strcmp(action_name, action_list[i].name) == 0) {
            (*action_list[i].handle)(arguments, response);
            break;
        }
        i++;
    }

    return i < NUM_ACTIONS;
}