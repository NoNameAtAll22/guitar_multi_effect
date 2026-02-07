#pragma once
#include <stdbool.h>

void board_init_cs_hardening(void);
void board_init_backlight(void);
void board_init_button(void);
void board_set_backlight(bool on);
void board_toggle_backlight(void); // New prototype

void board_init_buses(void);
void board_init_display(void);