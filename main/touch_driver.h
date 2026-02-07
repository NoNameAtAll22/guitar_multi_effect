#pragma once

#include <stdbool.h>

void touch_driver_init(void);
void touch_driver_set_rotation(bool rotate_180);
bool touch_driver_read(int *out_x, int *out_y);
void touch_driver_toggle_enabled(void);
