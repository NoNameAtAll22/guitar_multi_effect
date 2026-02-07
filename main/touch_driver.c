#include <stdio.h>
#include "touch_driver.h"
#include "board_pins.h"
#include "esp_lcd_touch_xpt2046.h"
#include "driver/gpio.h"

static esp_lcd_touch_handle_t tp_handle = NULL;
static volatile bool touch_irq_flag = false;
static bool _is_rotated_180 = false;
static bool touch_enabled = true; // New variable, initialized to enabled

/* Filtering state variables */
static int avg_x = -1, avg_y = -1;

static void IRAM_ATTR touch_isr_handler(void *arg)
{
    touch_irq_flag = true;
}

static int32_t map(int32_t x, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void touch_driver_init(void)
{
    /* ---------- TOUCH IO ---------- */
    esp_lcd_panel_io_handle_t tp_io;
    esp_lcd_panel_io_spi_config_t tp_io_cfg = {
        .cs_gpio_num = PIN_TP_CS,
        .dc_gpio_num = -1,
        .pclk_hz = 2 * 1000 * 1000, // 2 MHz for stability
        .spi_mode = 0,
        .trans_queue_depth = 3,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 0,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_HOST, &tp_io_cfg, &tp_io));

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = -1,
        .int_gpio_num = PIN_TP_IRQ,
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_spi_xpt2046(tp_io, &tp_cfg, &tp_handle));

    /* ---------- TOUCH IRQ ---------- */
    gpio_config_t irq_cfg = {
        .pin_bit_mask = 1ULL << PIN_TP_IRQ,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&irq_cfg);

    // Removed: gpio_install_isr_service(0); // ISR service is installed once in app_main
    gpio_isr_handler_add(PIN_TP_IRQ, touch_isr_handler, NULL);
    
    /* Default: No rotation */
    touch_driver_set_rotation(true);
}

void touch_driver_set_rotation(bool rotate_180)
{
    _is_rotated_180 = rotate_180;
}

bool touch_driver_read(int *out_x, int *out_y)
{
    if (!tp_handle || !touch_enabled) return false;

    /* Check both IRQ flag and pin level */
    bool is_touched = (touch_irq_flag || gpio_get_level(PIN_TP_IRQ) == 0);

    if (is_touched) {
        touch_irq_flag = false;
        esp_lcd_touch_read_data(tp_handle);

        esp_lcd_touch_point_data_t points[1];
        uint8_t point_cnt = 0;

        /* Get raw data using new API */
        bool valid = (esp_lcd_touch_get_data(tp_handle, points, &point_cnt, 1) == ESP_OK);

        if (valid && point_cnt > 0) {
            /* Map raw coordinates */
            int raw_x = map(points[0].x, CAL_X_MIN, CAL_X_MAX, LCD_H_RES, 0);
            int raw_y = map(points[0].y, CAL_Y_MIN, CAL_Y_MAX, LCD_V_RES, 0);

            /* Clamp */
            if (raw_x < 0) raw_x = 0; else if (raw_x >= LCD_H_RES) raw_x = LCD_H_RES - 1;
            if (raw_y < 0) raw_y = 0; else if (raw_y >= LCD_V_RES) raw_y = LCD_V_RES - 1;

            /* Apply Rotation */
            if (_is_rotated_180) {
                // X mapping is usually naturally inverted by map() or hardware relative to 180 deg screen.
                // If touch was mirrored on X, removing the inversion here should fix it.
                // raw_x = (LCD_H_RES - 1) - raw_x; // REMOVED: X-axis is already inverted by map function
                raw_y = (LCD_V_RES - 1) - raw_y; // Keep Y inverted
            }

            /* Low Pass Filter */
            if (avg_x == -1) {
                avg_x = raw_x;
                avg_y = raw_y;
            } else {
                avg_x = (avg_x * 3 + raw_x) / 4;
                avg_y = (avg_y * 3 + raw_y) / 4;
            }

            *out_x = avg_x;
            *out_y = avg_y;
            return true;
        }
    } 
    
    /* Reset filter if touch released */
    avg_x = -1;
    avg_y = -1;
    return false;
}

void touch_driver_toggle_enabled(void)
{
    touch_enabled = !touch_enabled;
    if (touch_enabled) {
        gpio_intr_enable(PIN_TP_IRQ);
    } else {
        gpio_intr_disable(PIN_TP_IRQ);
    }
}