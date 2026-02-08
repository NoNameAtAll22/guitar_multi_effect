#pragma once
#include "driver/i2s_std.h"

void audio_i2s_init(void);

/* Udostępniamy handle — DSP będzie ich używać */
i2s_chan_handle_t audio_i2s_get_rx(void);
i2s_chan_handle_t audio_i2s_get_tx(void);
