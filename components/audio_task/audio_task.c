#include "audio_task.h"
#include "audio_i2s.h"
#include "audio_fx.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

#define TAG "AUDIO_TASK"

#define AUDIO_FRAMES   128   // dokładnie jak w działającym kodzie

static void audio_task(void *arg)
{
    ESP_LOGI(TAG, "Audio task started");

    i2s_chan_handle_t rx = audio_i2s_get_rx();
    i2s_chan_handle_t tx = audio_i2s_get_tx();

    int32_t buffer[AUDIO_FRAMES * 2];
    size_t bytes_read;
    size_t bytes_written;

    while (1) {
        // --- READ ADC ---
        esp_err_t err = i2s_channel_read(
            rx,
            buffer,
            sizeof(buffer),
            &bytes_read,
            portMAX_DELAY
        );

        if (err != ESP_OK || bytes_read == 0) continue;

        int frames = bytes_read / (2 * sizeof(int32_t));

        // --- DSP ---
        audio_fx_process(buffer, frames);

        // --- WRITE DAC ---
        i2s_channel_write(
            tx,
            buffer,
            frames * 2 * sizeof(int32_t),
            &bytes_written,
            portMAX_DELAY
        );
    }
}

void audio_task_start(void)
{
    xTaskCreatePinnedToCore(
        audio_task,
        "audio_task",
        4096,
        NULL,
        24,     // WYSOKI PRIORYTET (ważne!)
        NULL,
        1       // core 1 (LVGL siedzi na 0)
    );
}
