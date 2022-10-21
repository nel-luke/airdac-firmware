#include "rendering_control.h"
#include "control_common.h"
#include "../upnp_common.h"

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

static action_err_t ListPresets(char* arguments, char** response) {
    xSemaphoreTake(rcs_mutex, portMAX_DELAY);
    const char* CurrentPresetNameList = rcs_state.PresetNameList;

    *response = to_xml(1, ARG(CurrentPresetNameList));
    xSemaphoreGive(rcs_mutex);
    
    return Action_OK;
}

// There is only one preset, so there is no need to set it
static action_err_t SelectPresets(char* arguments, char** response) {
//    xSemaphoreTake(rcs_mutex, portMAX_DELAY);
//
//    xSemaphoreGive(rcs_mutex);

    state_changed(PRESETNAMELIST);
    return Action_OK;
}

UNIMPLEMENTED(GetBrightness)
UNIMPLEMENTED(SetBrightness)
UNIMPLEMENTED(GetContrast)
UNIMPLEMENTED(SetContrast)
UNIMPLEMENTED(GetSharpness)
UNIMPLEMENTED(SetSharpness)
UNIMPLEMENTED(GetRedVideoGain)
UNIMPLEMENTED(SetRedVideoGain)
UNIMPLEMENTED(GetGreenVideoGain)
UNIMPLEMENTED(SetGreenVideoGain)
UNIMPLEMENTED(GetBlueVideoGain)
UNIMPLEMENTED(SetBlueVideoGain)
UNIMPLEMENTED(GetRedVideoBlackLevel)
UNIMPLEMENTED(SetRedVideoBlackLevel)
UNIMPLEMENTED(GetGreenVideoBlackLevel)
UNIMPLEMENTED(SetGreenVideoBlackLevel)
UNIMPLEMENTED(GetBlueVideoBlackLevel)
UNIMPLEMENTED(SetBlueVideoBlackLevel)
UNIMPLEMENTED(GetColorTemperature)
UNIMPLEMENTED(SetColorTemperature)
UNIMPLEMENTED(GetHorizontalKeystone)
UNIMPLEMENTED(SetHorizontalKeystone)
UNIMPLEMENTED(GetVerticalKeystone)
UNIMPLEMENTED(SetVerticalKeystone)

// There is only a master channel, so the answer is the same regardless of channel sent
static action_err_t GetMute(char* arguments, char** response) {
    char CurrentMute[2] = { 0 };

    xSemaphoreTake(rcs_mutex, portMAX_DELAY);
    CurrentMute[0] = rcs_state.Mute ? '1' : '0';

    *response = to_xml(1, ARG(CurrentMute));
    xSemaphoreGive(rcs_mutex);

    return Action_OK;
}

static action_err_t SetMute(char* arguments, char** response) {
    ARG_START();
    GET_ARG(DesiredMute);

    if (DesiredMute != NULL && (DesiredMute[0] == '0' || DesiredMute[0] == '1') ) {
        xSemaphoreTake(rcs_mutex, portMAX_DELAY);
        rcs_state.Mute = DesiredMute[0] == '1' ? true : false;
        xSemaphoreGive(rcs_mutex);
    } else {
        return Invalid_Args;
    }

    state_changed(MUTE);
    return Action_OK;
}

static action_err_t GetVolume(char* arguments, char** response) {
    char CurrentVolume[4] = { 0 };

    xSemaphoreTake(rcs_mutex, portMAX_DELAY);
    itoa(rcs_state.Volume, CurrentVolume, 10);

    *response = to_xml(1, ARG(CurrentVolume));
    xSemaphoreGive(rcs_mutex);

    return Action_OK;
}

static action_err_t SetVolume(char* arguments, char** response) {
    ARG_START();
    GET_ARG(DesiredVolume);

    if (DesiredVolume != NULL) {
        int volume = atol(DesiredVolume);

        if (volume < MIN_VOL || volume > MAX_VOL)
            return Out_Of_Range;

        int volume_db = floor(log10((double) volume / 100) * 2560);

        xSemaphoreTake(rcs_mutex, portMAX_DELAY);
        rcs_state.Volume = volume;
        rcs_state.VolumeDB = volume_db;
        xSemaphoreGive(rcs_mutex);
    } else {
        return Invalid_Args;
    }

    state_changed(VOLUME | VOLUMEDB);
    return Action_OK;
}

static action_err_t GetVolumeDB(char* arguments, char** response) {
    char CurrentVolume[6] = { 0 };

    xSemaphoreTake(rcs_mutex, portMAX_DELAY);
    itoa(rcs_state.VolumeDB, CurrentVolume, 10);

    *response = to_xml(1, ARG(CurrentVolume));
    xSemaphoreGive(rcs_mutex);

    return Action_OK;
}

static action_err_t SetVolumeDB(char* arguments, char** response) {
    ARG_START();
    GET_ARG(DesiredVolume);

    if (DesiredVolume != NULL) {
        int volume_db = atol(DesiredVolume);

        if (volume_db < MIN_VOL_DB || volume_db > MAX_VOL_DB)
            return Out_Of_Range;

        int volume = floor(pow10((double) volume_db / 2560) * 100);

        xSemaphoreTake(rcs_mutex, portMAX_DELAY);
        rcs_state.VolumeDB = volume;
        rcs_state.Volume = volume_db;
        xSemaphoreGive(rcs_mutex);
    } else {
        return Invalid_Args;
    }

    state_changed(VOLUMEDB | VOLUME);
    return Action_OK;
}

static action_err_t GetVolumeDBRange(char* arguments, char** response) {
    int MinValue = MIN_VOL_DB;
    int MaxValue = MAX_VOL_DB;

    *response = to_xml(2, ARG(MinValue), ARG(MaxValue));
    return Action_OK;
}

UNIMPLEMENTED(GetLoudness)
UNIMPLEMENTED(SetLoudness)

#define NUM_ACTIONS 35
static const struct action action_list[NUM_ACTIONS] = {
        ACTION(ListPresets),
        ACTION(SelectPresets),
        ACTION(GetBrightness),
        ACTION(SetBrightness),
        ACTION(GetContrast),
        ACTION(SetContrast),
        ACTION(GetSharpness),
        ACTION(SetSharpness),
        ACTION(GetRedVideoGain),
        ACTION(SetRedVideoGain),
        ACTION(GetGreenVideoGain),
        ACTION(SetGreenVideoGain),
        ACTION(GetBlueVideoGain),
        ACTION(SetBlueVideoGain),
        ACTION(GetRedVideoBlackLevel),
        ACTION(SetRedVideoBlackLevel),
        ACTION(GetGreenVideoBlackLevel),
        ACTION(SetGreenVideoBlackLevel),
        ACTION(GetBlueVideoBlackLevel),
        ACTION(SetBlueVideoBlackLevel),
        ACTION(GetColorTemperature),
        ACTION(SetColorTemperature),
        ACTION(GetHorizontalKeystone),
        ACTION(SetHorizontalKeystone),
        ACTION(GetVerticalKeystone),
        ACTION(SetVerticalKeystone),
        ACTION(GetMute),
        ACTION(SetMute),
        ACTION(GetVolume),
        ACTION(SetVolume),
        ACTION(GetVolumeDB),
        ACTION(SetVolumeDB),
        ACTION(GetVolumeDBRange),
        ACTION(GetLoudness),
        ACTION(SetLoudness)
};

action_err_t rendering_control_execute(const char* action_name, char* arguments, char** response) {
    action_err_t err = Invalid_Action;
    int i = 0;
    while (i < NUM_ACTIONS) {
        if (strcmp(action_name, action_list[i].name) == 0) {
            err = (*action_list[i].handle)(arguments, response);
            break;
        }
        i++;
    }

    return err;
}
