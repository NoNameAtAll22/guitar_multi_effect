#include <stdlib.h>
#include <math.h>
#include "lcd_driver.h"
#include "board_pins.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static esp_lcd_panel_handle_t panel_handle = NULL;

esp_lcd_panel_handle_t lcd_driver_init(void)
{
    /* ---------- HARD RESET LCD ---------- */
    gpio_set_direction(PIN_LCD_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(PIN_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    /* ---------- LCD IO ---------- */
    esp_lcd_panel_io_handle_t lcd_io;
    esp_lcd_panel_io_spi_config_t lcd_io_cfg = {
        .dc_gpio_num = PIN_LCD_DC,
        .cs_gpio_num = PIN_LCD_CS,
        .pclk_hz = 40 * 1000 * 1000, // 40 MHz
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_HOST, &lcd_io_cfg, &lcd_io));

    /* ---------- LCD PANEL ---------- */
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = -1,
        .color_space = ESP_LCD_COLOR_SPACE_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(lcd_io, &panel_cfg, &panel_handle));

    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_disp_on_off(panel_handle, true);

    /* Default Rotation (0 deg): Mirror X=true, Y=false */
    lcd_driver_set_rotation(true);

    return panel_handle;
}

void lcd_driver_set_rotation(bool rotate_180)
{
    if (!panel_handle) return;

    if (rotate_180) {
        /* 180 Degrees: Invert both mirrors relative to 0 deg */
        /* If 0 deg is (true, false), 180 is (false, true) */
        esp_lcd_panel_mirror(panel_handle, false, true);
    } else {
        /* 0 Degrees */
        esp_lcd_panel_mirror(panel_handle, true, false);
    }
    /* Swap XY is always false for Portrait */
    esp_lcd_panel_swap_xy(panel_handle, false);
}

void lcd_driver_clear(void)
{
    if (!panel_handle) return;
    
    uint16_t *buf = heap_caps_malloc(LCD_H_RES * LCD_V_RES * 2, MALLOC_CAP_DMA);
    if (!buf) return;

    for (int i = 0; i < LCD_H_RES * LCD_V_RES; i++) buf[i] = 0x0000;
    esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_H_RES, LCD_V_RES, buf);
    free(buf);
}

void lcd_driver_draw_point(int x, int y)
{
    if (!panel_handle) return;
    if (x < 0 || x >= LCD_H_RES || y < 0 || y >= LCD_V_RES) return;

    uint16_t color = 0x0000; // BLACK
    esp_lcd_panel_draw_bitmap(panel_handle, x, y, x + 1, y + 1, &color);
}

/* Used by LVGL to flush buffer to screen */
void lcd_driver_flush(int x1, int y1, int x2, int y2, const void *color_data)
{
    if (!panel_handle) return;
    /* esp_lcd_panel_draw_bitmap takes x_end, y_end as exclusive (like LVGL v9 requirements typically match or need +1 depending on driver)
       ESP-IDF docs: "x_end: End X coordinate, exclusive."
       LVGL v9 flush: area->x2 is inclusive. 
       So we need to pass x2 + 1, y2 + 1 to esp_lcd.
    */
    esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2 + 1, y2 + 1, color_data);
}