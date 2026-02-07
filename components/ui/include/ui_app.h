#pragma once

#include "lvgl.h"

/* Public API */
void ui_app_init(void);

/* Helpers */
void ui_toggle_active_state(void);
void ui_set_current_name(const char *name);
void ui_app_next_preset(void);
void ui_app_prev_preset(void);
