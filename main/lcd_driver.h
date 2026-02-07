#pragma once

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include <stdbool.h>

esp_lcd_panel_handle_t lcd_driver_init(void);
void lcd_driver_set_rotation(bool rotate_180);
void lcd_driver_clear(void);
void lcd_driver_draw_point(int x, int y);
void lcd_driver_flush(int x1, int y1, int x2, int y2, const void *color_data);
