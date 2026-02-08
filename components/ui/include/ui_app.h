#pragma once

#include "lvgl.h"
#include "effect_type.h"
#include <stdbool.h>

#define MAX_PRESETS 30
#define MAX_EFFECTS 8 
#define MAX_EFFECT_PARAMS 8

/* ================= FORWARD DECLARATION ================= */

typedef struct effect_item_t effect_item_t;

/* ================= KNOB USER DATA ================= */

typedef struct {
    effect_item_t *effect;
    uint8_t param_idx;
    lv_obj_t *val_lbl;
} knob_user_data_t;

/* ================= EFFECT ================= */

struct effect_item_t {
    effect_type_t type;
    char name[32];

    int params[MAX_EFFECT_PARAMS];
    uint8_t param_count;

    knob_user_data_t knob_ud[MAX_EFFECT_PARAMS];
};

/* ================= PRESET ================= */

typedef struct {
    char name[32];
    effect_item_t effects[MAX_EFFECTS];
    int effect_count;
    bool active;
} preset_t;

/* ================= PUBLIC API ================= */

void ui_app_init(void);
void ui_toggle_active_state(void);
void ui_app_next_preset(void);
void ui_app_prev_preset(void);
void audio_fx_set_eq8_band(int band, float gain_db);
void nvs_autosave_task(void *arg);    