#include "audio_fx.h"
#include <string.h>
#include <math.h>
#include "freertos/semphr.h"
#include "esp_log.h"

#define TAG "AUDIO_FX"

#define SAMPLE_RATE 44100
#define PI 3.14159265f

/* ================= STATIC BUFFERS ================= */

#define MOD_BUF_SIZE 2048
static float mod_buf[MOD_BUF_SIZE];
static int mod_idx = 0;

#define DELAY_BUF_SIZE 44100 // 1 second max
static float delay_buf[DELAY_BUF_SIZE];
static int delay_idx = 0;

/* ================= INTERNAL STATE ================= */

// The single global instance of all parameters
static audio_fx_params_t fx_params;

// The single global instance of the effect chain
static audio_fx_chain_t fx_chain;

// Global mutex for protecting audio parameters and chain
SemaphoreHandle_t g_audio_params_mutex;

// Reverb state
static float rv_l = 0.0f;
static float rv_r = 0.0f;

/* ================= PUBLIC API IMPLEMENTATION ================= */

audio_fx_params_t* audio_fx_get_params(void) {
    return &fx_params;
}

audio_fx_chain_t* audio_fx_get_chain(void) {
    return &fx_chain;
}

void audio_fx_init(void) {
    g_audio_params_mutex = xSemaphoreCreateMutex();
    if (g_audio_params_mutex == NULL) {
        // Handle error: Mutex creation failed
        // For simplicity, we'll just log and continue, but in a real app,
        // this might require a more robust error handling (e.g., assert, restart)
        ESP_LOGE("AUDIO_FX", "Failed to create audio parameters mutex!");
    }
    // Clear buffers
    memset(mod_buf, 0, sizeof(mod_buf));
    memset(delay_buf, 0, sizeof(delay_buf));
    
    // Set some sane default parameters
    fx_params.gain_db = 0.0f;
    fx_params.od_drive = 0.0f;
    fx_params.dist_drive = 0.0f;
    fx_params.fuzz_drive = 0.0f;
    fx_params.comp_amount = 0.0f;
    fx_params.ch_rate = 0.5f;
    fx_params.ch_depth = 0.4f;
    fx_params.ch_mix = 0.0f;
    fx_params.fl_rate = 0.2f;
    fx_params.fl_depth = 0.5f;
    fx_params.fl_mix = 0.0f;
    fx_params.delay_time_ms = 380.0f;
    fx_params.delay_fb = 0.45f;
    fx_params.delay_mix = 0.0f;
    fx_params.reverb_mix = 0.0f;

    // Default chain to empty
    fx_chain.chain_len = 0;
}


/* ================= EFFECT IMPLEMENTATIONS ================= */

static inline float fx_preamp(float x, audio_fx_params_t* p)
{
    static float dc = 0.0f;
    x *= powf(10.0f, p->gain_db / 20.0f);
    dc = 0.995f * dc + 0.005f * x;
    return x - dc;
}

static inline float fx_compressor(float x, audio_fx_params_t* p)
{
    if (p->comp_amount < 0.01f) return x;
    float a = fabsf(x);
    if (a > 0.3f) // Simple threshold
        x *= 1.0f - p->comp_amount * 0.5f;
    return x;
}

static inline float fx_overdrive(float x, audio_fx_params_t* p)
{
    if (p->od_drive < 0.01f) return x;
    float drive = 1.0f + p->od_drive * 35.0f;
    x *= drive;
    if (x > 0.6f)  x = 0.6f + (x - 0.6f) * 0.2f;
    if (x < -0.6f) x = -0.6f + (x + 0.6f) * 0.2f;
    return tanhf(x) * 0.7f;
}

static inline float fx_distortion(float x, audio_fx_params_t* p)
{
    if (p->dist_drive < 0.01f) return x;
    float drive = 1.0f + p->dist_drive * 12.0f;
    x *= drive;
    x = x / (1.0f + fabsf(x));
    return x;
}

static inline float fx_fuzz(float x, audio_fx_params_t* p)
{
    if (p->fuzz_drive < 0.01f) return x;
    x *= 1.0f + p->fuzz_drive * 80.0f;
    return copysignf(1.0f, x) * (1.0f - expf(-fabsf(x)));
}

static inline float fx_chorus(float x, audio_fx_params_t* p)
{
    if (p->ch_mix < 0.01f) return x;
    
    static float lfo = 0.0f;
    
    mod_buf[mod_idx] = x;

    lfo += 2 * PI * (0.1f + p->ch_rate * 2.0f) / SAMPLE_RATE;
    if (lfo > 2 * PI) lfo -= 2 * PI;

    float d = 200 + sinf(lfo) * (p->ch_depth * 40.0f);
    int rp = (mod_idx - (int)d + MOD_BUF_SIZE) % MOD_BUF_SIZE;
    float wet = mod_buf[rp];
    
    // mod_idx is shared, which is okay for chorus/flanger if not used together
    mod_idx = (mod_idx + 1) % MOD_BUF_SIZE;
    return x * (1.0f - p->ch_mix) + wet * p->ch_mix;
}

static inline float fx_flanger(float x, audio_fx_params_t* p)
{
    if (p->fl_mix < 0.01f) return x;
    
    static float lfo = 0.0f;
    lfo += 2 * PI * (0.05f + p->fl_rate * 1.0f) / SAMPLE_RATE;
    if (lfo > 2 * PI) lfo -= 2 * PI;

    float d = 50 + sinf(lfo) * (p->fl_depth * 30.0f);
    int rp = (mod_idx - (int)d + MOD_BUF_SIZE) % MOD_BUF_SIZE;
    
    // Feedback for flanger
    float wet = mod_buf[rp];
    mod_buf[mod_idx] = x + wet * p->fl_res;

    mod_idx = (mod_idx + 1) % MOD_BUF_SIZE;
    return x * (1.0f - p->fl_mix) + wet * p->fl_mix;
}


static inline float fx_delay(float x, audio_fx_params_t* p)
{
    if (p->delay_mix < 0.01f) return x;

    int ds = (int)(p->delay_time_ms * SAMPLE_RATE / 1000.0f);
    if (ds >= DELAY_BUF_SIZE) ds = DELAY_BUF_SIZE - 1;

    int rp = (delay_idx - ds + DELAY_BUF_SIZE) % DELAY_BUF_SIZE;
    float d = delay_buf[rp];

    delay_buf[delay_idx] = x + d * p->delay_fb;
    delay_idx = (delay_idx + 1) % DELAY_BUF_SIZE;

    return x * (1.0f - p->delay_mix) + d * p->delay_mix;
}

// NEW: Echo (simple delay with filtering)
static inline float fx_echo(float x, audio_fx_params_t* p) {
    if (p->echo_mix < 0.01f) return x;
    
    static float filtered_d = 0.0f;

    int ds = (int)(p->echo_time_ms * SAMPLE_RATE / 1000.0f);
    if (ds >= DELAY_BUF_SIZE) ds = DELAY_BUF_SIZE - 1;

    int rp = (delay_idx - ds + DELAY_BUF_SIZE) % DELAY_BUF_SIZE;
    float d = delay_buf[rp];

    // Simple low-pass filter on feedback to make it darker (like tape)
    filtered_d = 0.6f * d + 0.4f * filtered_d;

    delay_buf[delay_idx] = x + filtered_d * p->echo_fb;
    delay_idx = (delay_idx + 1) % DELAY_BUF_SIZE;

    return x * (1.0f - p->echo_mix) + filtered_d * p->echo_mix;
}


static inline float fx_reverb(float x, audio_fx_params_t* p)
{
    if (p->reverb_mix < 0.01f) return x;
    // Simplified reverb
    rv_l = (rv_l * 0.95f) + (x * 0.04f);
    x = x * (1.0f - p->reverb_mix) + rv_l * p->reverb_mix;
    return x;
}

// NEW: 3-Band EQ (simple filters)
static inline float fx_eq_3band(float x, audio_fx_params_t* p) {
    static float lf = 0.0f, hf = 0.0f;
    float l, m, h;

    // Low-pass filter (bass)
    lf += 0.1f * (x - lf);
    l = lf;

    // High-pass filter (treble)
    hf += 0.1f * (x - hf);
    h = x - hf;

    // Mid is what's left
    m = x - l - h;

    l *= powf(10.0f, p->eq3_bass / 20.0f);
    m *= powf(10.0f, p->eq3_mid / 20.0f);
    h *= powf(10.0f, p->eq3_treb / 20.0f);

    return (l + m + h) * powf(10.0f, p->eq3_vol / 20.0f);
}

// NEW: 8-Band EQ (Placeholder - this is complex)
static inline float fx_eq_8band(float x, audio_fx_params_t* p) {
    // NOTE: A real 8-band EQ requires a proper filter bank (e.g., biquad filters).
    // This is a placeholder and will not function correctly without a real DSP library.
    // For now, it just applies a simple gain based on the first band.
    return x * powf(10.0f, p->eq8_bands[0] / 20.0f);
}


static inline float fx_limiter(float x)
{
    if (x > 0.95f)  return 0.95f;
    if (x < -0.95f) return -0.95f;
    return x;
}


void audio_fx_process(int32_t *buffer, int frames)
{
    if (xSemaphoreTake(g_audio_params_mutex, portMAX_DELAY) == pdTRUE) {
        for (int i = 0; i < frames; i++) {
            // Get mono sample from left channel
            float x = (float)buffer[i * 2] / 8388608.0f;   // PCM1808 = 24-bit ADC
            x *= 0.05;   // −12 dB (BEZPIECZNY START)


            // --- MASTER BYPASS ---
            if (fx_chain.chain_len == 0) {
                 int32_t o = (int32_t)(x * 2147483647.0f);
                 buffer[i*2] = o;
                 buffer[i*2+1] = o;
                 continue;
            }

            // --- APPLY EFFECTS IN CHAIN ORDER ---
            for (int j = 0; j < fx_chain.chain_len; j++) {
                if (!fx_chain.chain[j].active) continue;

                switch (fx_chain.chain[j].type) {
                    case FX_GAIN:       x = fx_preamp(x, &fx_params); break;
                    case FX_COMPRESSOR: x = fx_compressor(x, &fx_params); break;
                    case FX_OVERDRIVE:  x = fx_overdrive(x, &fx_params); break;
                    case FX_DISTORTION: x = fx_distortion(x, &fx_params); break;
                    case FX_FUZZ:       x = fx_fuzz(x, &fx_params); break;
                    case FX_CHORUS:     x = fx_chorus(x, &fx_params); break;
                    case FX_FLANGER:    x = fx_flanger(x, &fx_params); break;
                    case FX_DELAY:      x = fx_delay(x, &fx_params); break;
                    case FX_ECHO:       x = fx_echo(x, &fx_params); break;
                    case FX_REVERB:     x = fx_reverb(x, &fx_params); break;
                    case FX_EQ_3BAND:   x = fx_eq_3band(x, &fx_params); break;
                    case FX_EQ_8BAND:   x = fx_eq_8band(x, &fx_params); break;
                    case FX_NONE:
                    default: break;
                }
            }
            
            // Final limiter
            x = fx_limiter(x);

            // Write mono sample to both channels
            int32_t out_sample = (int32_t)(x * 2147483647.0f);
            buffer[i*2] = out_sample;
            buffer[i*2+1] = out_sample;
        }
        xSemaphoreGive(g_audio_params_mutex);
    }
}
