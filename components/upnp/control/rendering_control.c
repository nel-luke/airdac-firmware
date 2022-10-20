#include "rendering_control.h"
#include "control_common.h"
#include "../upnp_common.h"

#include <sys/param.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define MIN_VOL 0
#define MAX_VOL 100
#define MIN_VOL_DB -5120
#define MAX_VOL_DB 0
#define CLAMP(a, mini, maxi) MAX(MIN((a), (maxi)), (mini))

static const char DefaultPreset[] = "FactoryDefaults";

#define PRESETNAMELIST  BIT0
#define MUTE            BIT1
#define VOLUME          BIT2
#define VOLUMEDB        BIT3
static EventGroupHandle_t rcs_events;

struct {
    const char* PresetNameList;
    bool Mute;
    uint16_t Volume;
    int16_t  VolumeDB;
} static rcs_state = {
    DefaultPreset,
    false,
    100,
    0
};
static SemaphoreHandle_t rcs_mutex;


#define CHECK_VAR_INT(bit, name) CHECK_VAR_H(bit, #name, "%d", rcs_state.name)

void init_rendering_control(void) {
    rcs_events = xEventGroupCreate();
    rcs_mutex = xSemaphoreCreateMutex();
    xEventGroupSetBits(rcs_events, ALL_EVENT_BITS);
}

static char* rendering_control_changes(uint32_t changed_variables) {
    char* response = NULL;
    char* pos;
    int total_chars = 0;
    int tag_size = 0;

    xSemaphoreTake(rcs_mutex, portMAX_DELAY);
print_chars:
    pos = response;
    CHECK_VAR_INT(MUTE, Mute)
    CHECK_VAR_INT(VOLUME, Volume)
    CHECK_VAR_INT(VOLUMEDB, VolumeDB)

    if (response == NULL) {
        total_chars *= -1;
        total_chars++;
        response = malloc(total_chars+1);
        goto print_chars;
    }

    xSemaphoreGive(rcs_mutex);
    return response;
}

inline char* get_rendering_control_changes(void) {
    uint32_t changed_variables = xEventGroupWaitBits(rcs_events, ALL_EVENT_BITS, pdTRUE, pdFALSE, 0);
    return rendering_control_changes(changed_variables);
}

inline char* get_rendering_control_all(void) {
    return rendering_control_changes(ALL_EVENT_BITS);
}

static inline void state_changed(uint32_t variables) {
    xEventGroupSetBits(rcs_events, variables);
    send_event(RENDERING_CONTROL_CHANGED);
}

static void ListPresets(char* arguments, char** response) {
    xSemaphoreTake(rcs_mutex, portMAX_DELAY);
    const char* CurrentPresetNameList = rcs_state.PresetNameList;

    *response = to_xml(1, ARG(CurrentPresetNameList));
    xSemaphoreGive(rcs_mutex);
}

// There is only one preset, so there is no need to set it
static void SelectPresets(char* arguments, char** response) {
//    xSemaphoreTake(rcs_mutex, portMAX_DELAY);
//
//    xSemaphoreGive(rcs_mutex);

    state_changed(PRESETNAMELIST);
}

// There is only a master channel, so the answer is the same regardless of channel sent
static void GetMute(char* arguments, char** response) {
    char CurrentMute[2] = { 0 };

    xSemaphoreTake(rcs_mutex, portMAX_DELAY);
    CurrentMute[0] = rcs_state.Mute ? '1' : '0';

    *response = to_xml(1, ARG(CurrentMute));
    xSemaphoreGive(rcs_mutex);
}

static void SetMute(char* arguments, char** response) {
    GET_ARG(DesiredMute);

    if (DesiredMute != NULL) {
        xSemaphoreTake(rcs_mutex, portMAX_DELAY);
        rcs_state.Mute = DesiredMute[0] == '1' ? true : false;
        xSemaphoreGive(rcs_mutex);
    }

    state_changed(MUTE);
}

static void GetVolume(char* arguments, char** response) {
    char CurrentVolume[4] = { 0 };

    xSemaphoreTake(rcs_mutex, portMAX_DELAY);
    itoa(rcs_state.Volume, CurrentVolume, 10);

    *response = to_xml(1, ARG(CurrentVolume));
    xSemaphoreGive(rcs_mutex);
}

static void SetVolume(char* arguments, char** response) {
    GET_ARG(DesiredVolume);

    if (DesiredVolume != NULL) {
        int volume = CLAMP(atol(DesiredVolume), MIN_VOL, MAX_VOL);
        int volume_db = CLAMP(floor(log10((double) volume / 100) * 2560), MIN_VOL_DB, MAX_VOL_DB);

        xSemaphoreTake(rcs_mutex, portMAX_DELAY);
        rcs_state.Volume = volume;
        rcs_state.VolumeDB = volume_db;
        xSemaphoreGive(rcs_mutex);
    }

    state_changed(VOLUME | VOLUMEDB);
}

static void GetVolumeDB(char* arguments, char** response) {
    char CurrentVolume[6] = { 0 };

    xSemaphoreTake(rcs_mutex, portMAX_DELAY);
    itoa(rcs_state.VolumeDB, CurrentVolume, 10);

    *response = to_xml(1, ARG(CurrentVolume));
    xSemaphoreGive(rcs_mutex);
}

static void SetVolumeDB(char* arguments, char** response) {
    GET_ARG(DesiredVolume);

    if (DesiredVolume != NULL) {
        int volume_db = CLAMP(atol(DesiredVolume), MIN_VOL_DB, MAX_VOL_DB);
        int volume = CLAMP(floor(pow10((double) volume_db / 2560) * 100), MIN_VOL, MAX_VOL);

        xSemaphoreTake(rcs_mutex, portMAX_DELAY);
        rcs_state.VolumeDB = volume;
        rcs_state.Volume = volume_db;
        xSemaphoreGive(rcs_mutex);
    }

    state_changed(VOLUMEDB | VOLUME);
}

static void GetVolumeDBRange(char* arguments, char** response) {
    int MinValue = MIN_VOL_DB;
    int MaxValue = MAX_VOL_DB;

    *response = to_xml(2, ARG(MinValue), ARG(MaxValue));
}

#define NUM_ACTIONS 9
static const struct action action_list[NUM_ACTIONS] = {
        ACTION(ListPresets),
        ACTION(SelectPresets),
        ACTION(GetMute),
        ACTION(SetMute),
        ACTION(GetVolume),
        ACTION(SetVolume),
        ACTION(GetVolumeDB),
        ACTION(SetVolumeDB),
        ACTION(GetVolumeDBRange),
};

bool rendering_control_execute(const char* action_name, char* arguments, char** response) {
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
