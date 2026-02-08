#ifndef AUDIO_FX_H
#define AUDIO_FX_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h" // Include for SemaphoreHandle_t
#include "effect_type.h"

// A single effect in the chain
typedef struct {
    effect_type_t type;
    bool active;
    // We can add pointers to parameter structs here if needed later
} effect_chain_item_t;

#define MAX_EFFECTS_IN_CHAIN 8

// Global structure to hold all controllable parameters
typedef struct {
    /* Gain / Drive */
    float gain_db;
    float od_drive;
    float dist_drive;
    float fuzz_drive;

    /* Compressor */
    float comp_amount;
    float comp_attack; // From UI
    float comp_level;  // From UI

    /* Chorus */
    float ch_rate;
    float ch_depth;
    float ch_tone;     // From UI
    float ch_mix;

    /* Flanger */
    float fl_rate;
    float fl_depth;
    float fl_res;      // From UI
    float fl_mix;

    /* Delay */
    float delay_time_ms;
    float delay_fb;
    float delay_tone;    // From UI
    float delay_mix;

    /* Echo */
    float echo_time_ms;
    float echo_fb;
    float echo_wow;      // From UI
    float echo_mix;

    /* Reverb */
    float reverb_decay; // From UI
    float reverb_damp;  // From UI
    float reverb_predelay; // From UI
    float reverb_mix;

    /* EQs */
    float eq3_bass;
    float eq3_mid;
    float eq3_treb;
    float eq3_vol;
    float eq8_bands[8]; // 63, 125, 250, 500, 1k, 2k, 4k, 8k

} audio_fx_params_t;


// Global state of the audio effects chain
typedef struct {
    effect_chain_item_t chain[MAX_EFFECTS_IN_CHAIN];
    int chain_len;
} audio_fx_chain_t;


/* Public API */

void audio_fx_init(void);
void audio_fx_process(int32_t *buffer, int frames);

// Provides direct access to the parameters for UI control
audio_fx_params_t* audio_fx_get_params(void);

// Provides direct access to the effect chain for UI control
audio_fx_chain_t* audio_fx_get_chain(void);

// Mutex to protect audio parameters and chain during access
extern SemaphoreHandle_t g_audio_params_mutex;


#endif // AUDIO_FX_H
