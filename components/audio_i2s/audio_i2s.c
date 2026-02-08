#include "audio_i2s.h"
#include "board_pins.h"
#include "esp_log.h"

#define TAG "AUDIO_I2S"
#define SAMPLE_RATE 44100

static i2s_chan_handle_t rx = NULL;
static i2s_chan_handle_t tx = NULL;

void audio_i2s_init(void)
{
    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx, &rx));

    i2s_std_config_t cfg = {
        .clk_cfg = {
            .sample_rate_hz = SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_32BIT,
            I2S_SLOT_MODE_STEREO
        ),
        .gpio_cfg = {
            .mclk = PIN_MCLK,
            .bclk = PIN_BCLK,
            .ws   = PIN_WS,
            .din  = PIN_ADC,
            .dout = PIN_DAC,
        },
    };

    cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx, &cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx, &cfg));

    ESP_ERROR_CHECK(i2s_channel_enable(rx));
    ESP_ERROR_CHECK(i2s_channel_enable(tx));

    ESP_LOGI(TAG, "I2S ADC/DAC initialized (32-bit, 44.1kHz)");
}

i2s_chan_handle_t audio_i2s_get_rx(void)
{
    return rx;
}

i2s_chan_handle_t audio_i2s_get_tx(void)
{
    return tx;
}
