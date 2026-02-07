#include "board_pins.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "spi_bus.h" // New include
#include "lcd_driver.h" // New include

void board_init_cs_hardening(void)
{
    gpio_config_t cfg = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    cfg.pin_bit_mask = (1ULL << PIN_LCD_CS) | (1ULL << PIN_TP_CS);
    gpio_config(&cfg);

    gpio_set_level(PIN_LCD_CS, 1);
    gpio_set_level(PIN_TP_CS, 1);
}

void board_init_backlight(void)
{
    gpio_set_direction(PIN_LCD_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_LCD_LED, 1);
}

void board_set_backlight(bool on)
{
    gpio_set_level(PIN_LCD_LED, on ? 1 : 0);
}

static bool backlight_state = true; // Initial state is ON

void board_toggle_backlight(void)
{
    backlight_state = !backlight_state;
    board_set_backlight(backlight_state);
}

void board_init_button(void)
{
    gpio_config_t btn_cfg = {
        .pin_bit_mask = 1ULL << PIN_BTN_CLR,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    gpio_config(&btn_cfg);
}

// New functions
void board_init_buses(void)
{
    spi_bus_init();
}

void board_init_display(void)
{
    lcd_driver_init();
}
