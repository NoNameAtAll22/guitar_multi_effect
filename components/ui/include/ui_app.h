#pragma once

#include "lvgl.h"
#include "effect_type.h"

// Ensure defines are at the top
#define MAX_PRESETS 30
#define MAX_EFFECTS 8 
#define MAX_EFFECT_PARAMS 8 // Enough for 8-Band EQ, or multiple knobs per effect

/* --- DATA MODELS --- */

typedef struct {
    effect_type_t type;
    char name[32];
    int32_t params[MAX_EFFECT_PARAMS]; // Array to hold parameters
    uint8_t param_count; // Number of active parameters for this effect
} effect_item_t;

typedef struct {
    char name[32];
    effect_item_t effects[MAX_EFFECTS];
    int effect_count;
    bool active;
} preset_t;

/* Public API */
void ui_app_init(void);

/* Helpers */
void ui_toggle_active_state(void);
void ui_set_current_name(const char *name);
void ui_app_next_preset(void);
void ui_app_prev_preset(void);
