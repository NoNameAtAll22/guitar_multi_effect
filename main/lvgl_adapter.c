#include "lvgl.h"
#include "lcd_driver.h"
#include "touch_driver.h"
#include "board_pins.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_cache.h" // For Cache Sync

static const char *TAG = "LVGL_ADAPTER";

/* LVGL Display & Input Objects */
static lv_display_t *display = NULL;
static lv_indev_t *indev_touch = NULL;

/* Buffer for LVGL drawing (1/10 screen size is typical recommendation) */
#define BUF_SIZE (LCD_H_RES * LCD_V_RES / 10)
static uint16_t *buf1 = NULL;
static uint16_t *buf2 = NULL; // Double buffering enabled

/* ================= FLUSH CALLBACK ================= */
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    /* Fix Byte Swap (Endianness) manually since hardware flag is missing */
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    
    lv_draw_sw_rgb565_swap(px_map, w * h);

    /* FORCE CACHE WRITEBACK to RAM before DMA starts */
    /* Size in bytes: w * h * 2 (16-bit color) */
    esp_cache_msync(px_map, w * h * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);

    /* px_map contains the rendered image (RGB565) */
    lcd_driver_flush(area->x1, area->y1, area->x2, area->y2, px_map);

    /* Inform LVGL that flushing is ready */
    lv_display_flush_ready(disp);
}

/* ================= READ CALLBACK ================= */
static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    int x, y;
    if (touch_driver_read(&x, &y)) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

/* ================= TICK TIMER ================= */
static void lv_tick_task(void *arg)
{
    lv_tick_inc(1); // Tell LVGL that 1ms has passed
}

void lvgl_adapter_init(void)
{
    /* 1. Init LVGL core */
    lv_init();

    /* 2. Allocate buffers */
    /* MALLOC_CAP_DMA is crucial for SPI DMA transfers! */
    buf1 = heap_caps_malloc(BUF_SIZE * sizeof(uint16_t), MALLOC_CAP_DMA);
    buf2 = heap_caps_malloc(BUF_SIZE * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!buf1 || !buf2) {
        ESP_LOGE(TAG, "Failed to allocate LVGL buffers");
        return;
    }

    /* 3. Create Display */
    display = lv_display_create(LCD_H_RES, LCD_V_RES);
    lv_display_set_buffers(display, buf1, buf2, BUF_SIZE * sizeof(uint16_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, disp_flush_cb);

    /* 4. Create Input Device */
    indev_touch = lv_indev_create();
    lv_indev_set_type(indev_touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev_touch, touch_read_cb);

    /* 5. Set up Tick Timer (FreeRTOS timer or esp_timer) */
    const esp_timer_create_args_t tick_timer_args = {
        .callback = &lv_tick_task,
        .name = "lv_tick"
    };
    esp_timer_handle_t tick_timer;
    esp_timer_create(&tick_timer_args, &tick_timer);
    esp_timer_start_periodic(tick_timer, 1000); // 1ms period

    ESP_LOGI(TAG, "LVGL Adapter Initialized");
}
