#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "board_init.h"
#include "board_pins.h" // For LCD_H_RES, LCD_V_RES
#include "driver/gpio.h" // For GPIO functions
#include "lcd_driver.h"   // For lcd_driver_flush
#include "touch_driver.h" // New include
#include "lvgl_adapter.h" // LVGL Adapter
#include "ui_app.h"       // UI Application
#include "lvgl.h"
#include "nvs_flash.h"      // For NVS flash initialization
#include "nvs_manager.h" // For NVS manager

#include "audio_i2s.h"
#include "audio_fx.h"
#include "audio_task.h"


#define TAG "MAIN"

/* LVGL Task to handle UI updates */
static void lvgl_task(void *pvParameter) {
    ESP_LOGI(TAG, "Starting LVGL Task");
    while (1) {
        /* Periodically call LVGL task handler.
         * It's recommended to call it at least every 5ms. */
        lv_task_handler();
        vTaskDelay(pdMS_TO_TICKS(10)); // Make it 10ms to see if watchdog issue resolves
    }
}

/* ================= MAIN APPLICATION ================= */

#include "board_pins.h" // For PIN_BTN_CLR, BTN_HOLD_MS
#include "freertos/queue.h" // For button event queue
#include "esp_timer.h"      // For timestamping
#include "rom/ets_sys.h"    // For ets_delay_us

// Forward declaration for ISR
static void button_isr_handler(void *arg);

// Queue for button events
typedef enum {
    BUTTON_PRESS,
    BUTTON_RELEASE
} button_event_type_t;

typedef struct {
    button_event_type_t type;
    uint64_t timestamp;
} button_event_t;

static QueueHandle_t button_event_queue = NULL;

#define DEBOUNCE_TIME_MS      50
#define LONG_PRESS_TIME_MS    (BTN_HOLD_MS) // From board_pins.h
#define CLICK_WINDOW_TIME_MS  250 // Max time between clicks for multi-click detection (reduced for responsiveness)

// Internal state for button event task
typedef enum {
    STATE_IDLE,
    STATE_PRESSED,
    STATE_RELEASED_WAITING_FOR_MULTI_CLICK,
} button_state_t;

// --- Button Event Task ---
static void button_event_task(void *pvParameter) {
    button_event_t event;
    uint64_t press_start_time_ms = 0;
    uint64_t last_click_time_ms = 0;
    int click_counter = 0;
    bool long_press_handled = false;

    ESP_LOGI(TAG, "Button Event Task Started (New Logic)");

    while (1) {
        // Wait for an event, or timeout after CLICK_WINDOW_TIME_MS for multi-click detection
        if (xQueueReceive(button_event_queue, &event, pdMS_TO_TICKS(CLICK_WINDOW_TIME_MS)) == pdPASS) {
            uint64_t current_time_ms = esp_timer_get_time() / 1000;

            // Debounce check: ignore events too close to each other
            // This also helps with active-high button stability
            if (press_start_time_ms != 0 && (current_time_ms - press_start_time_ms < DEBOUNCE_TIME_MS)) {
                 // Ignore if an event comes too quickly after a press.
                 // This primarily prevents spurious releases after a press on a bouncy button.
                 // We only care about stable state changes.
                 if (event.type == BUTTON_RELEASE && gpio_get_level(PIN_BTN_CLR) == 1) continue; // Still high, ignore this release
                 if (event.type == BUTTON_PRESS && gpio_get_level(PIN_BTN_CLR) == 0) continue; // Still low, ignore this press
            }

            if (event.type == BUTTON_PRESS) {
                // If a press comes outside the multi-click window, reset counter
                if (current_time_ms - last_click_time_ms > CLICK_WINDOW_TIME_MS) {
                    click_counter = 0;
                }
                click_counter++;
                press_start_time_ms = current_time_ms; // Mark start of this press
                long_press_handled = false; // Reset long press flag for this new press
                ESP_LOGI(TAG, "Press detected, click_counter: %d", click_counter);

                // Wait for release, or long press timeout
                button_event_t release_event;
                if (xQueueReceive(button_event_queue, &release_event, pdMS_TO_TICKS(LONG_PRESS_TIME_MS)) == pdPASS) {
                    // Button released before long press timeout
                    if (release_event.type == BUTTON_RELEASE) {
                        uint64_t press_duration = release_event.timestamp - press_start_time_ms;
                        if (press_duration >= DEBOUNCE_TIME_MS) { // Ensure it's a valid press
                            // This was a short press (or part of a multi-click)
                            last_click_time_ms = release_event.timestamp; // Mark time of release for multi-click window
                        } else {
                            // Too short press, probably bounce, ignore this click count
                            click_counter--;
                        }
                    }
                } else {
                    // Timeout: Long press detected
                    ESP_LOGI(TAG, "Long Press Detected!");
                    board_toggle_backlight();
                    touch_driver_toggle_enabled();
                    long_press_handled = true; // Mark long press as handled
                    // Clear any pending release events in queue that belong to this long press
                    while(xQueueReceive(button_event_queue, &release_event, 0) == pdPASS) {
                        if (release_event.type == BUTTON_RELEASE) break; // Found the release
                    }
                    click_counter = 0; // Reset click counter after long press
                    press_start_time_ms = 0; // Reset press start time
                    last_click_time_ms = 0;
                }
            }
        } else { // xQueueReceive timed out (no event for CLICK_WINDOW_TIME_MS)
            // Process clicks if any (only if a long press wasn't just handled)
            if (!long_press_handled && click_counter > 0) {
                if (click_counter == 1) {
                    ESP_LOGI(TAG, "Short Press Detected!");
                    ui_toggle_active_state();
                } else if (click_counter == 2) {
                    ESP_LOGI(TAG, "Double Click Detected!");
                    ui_app_next_preset();
                } else if (click_counter == 3) {
                    ESP_LOGI(TAG, "Triple Click Detected!");
                    ui_app_prev_preset();
                } else {
                    ESP_LOGW(TAG, "More than 3 clicks detected, ignoring: %d", click_counter);
                }
            }
            // Reset for next sequence
            click_counter = 0;
            press_start_time_ms = 0;
            last_click_time_ms = 0;
            long_press_handled = false;
        }
    }
}

// --- ISR Handler ---
static void IRAM_ATTR button_isr_handler(void *arg) {
    uint32_t gpio_num = (uint32_t)arg;
    button_event_t event;
    event.timestamp = esp_timer_get_time() / 1000; // ms

    if (gpio_get_level(gpio_num) == 1) { // Button Pressed (Active High)
        event.type = BUTTON_PRESS;
    } else { // Button Released (Active Low)
        event.type = BUTTON_RELEASE;
    }
    xQueueSendFromISR(button_event_queue, &event, NULL);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    nvs_manager_init(); // Initialize NVS manager

    ESP_LOGI(TAG, "App Start - Display Test");

    // Initialize board components (SPI bus, display, etc.)
    board_init_buses();
    board_init_display(); // This initializes the LCD driver
    lcd_driver_clear();   // Clear screen to black immediately
    touch_driver_init();
    board_init_button(); // Configure button GPIO and interrupt

    audio_i2s_init();
    audio_fx_init();

    // Create button event queue
    button_event_queue = xQueueCreate(10, sizeof(button_event_t));
    if (button_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create button event queue");
        return;
    }

    // Install GPIO ISR service
    gpio_install_isr_service(0);
    // Add button ISR handler
    gpio_isr_handler_add(PIN_BTN_CLR, button_isr_handler, (void*)PIN_BTN_CLR);

    // Ensure backlight is on
    gpio_set_direction(PIN_LCD_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_LCD_LED, 1);

    // Initialize LVGL
    lvgl_adapter_init();

    // Initialize UI application
    ui_app_init();

    // Create a FreeRTOS task for LVGL
    xTaskCreatePinnedToCore(lvgl_task, "lvgl_task", 4096 * 2, NULL, 5, NULL, 0);

    // Create a FreeRTOS task for button events
    xTaskCreatePinnedToCore(button_event_task, "button_event_task", 4096, NULL, 10, NULL, 1);

    audio_task_start();

    // Main application loop (can be used for other tasks or remain empty)
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Sleep for 1 second
    }

}
