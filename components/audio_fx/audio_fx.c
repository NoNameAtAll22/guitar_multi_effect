#include "audio_fx.h"
#include <string.h>
#include <math.h>
#include "freertos/semphr.h"
#include "esp_log.h"
#include "effect_type.h"

#define TAG "AUDIO_FX"
#define SAMPLE_RATE 44100
#define PI 3.14159265f

/* ================= BUFFERS ================= */

#define MOD_BUF_SIZE 2048
static float mod_buf[MOD_BUF_SIZE];
static int mod_idx = 0;

#define DELAY_BUF_SIZE 44100
static float delay_buf[DELAY_BUF_SIZE];
static int delay_idx = 0;

/* ================= BIQUAD EQ ================= */

typedef struct {
    float b0, b1, b2;
    float a1, a2;
    float z1, z2;
} biquad_t;

static biquad_t eq8[8];

static const float eq8_freqs[8] = {
    100.0f, 170.0f, 280.0f, 500.0f,
    800.0f, 1400.0f, 2500.0f, 5000.0f
};

/* ================= STATE ================= */

static int chain_target_len = 0;
static int chain_current_len = 0;

static audio_fx_params_t fx_params;
static audio_fx_chain_t  fx_chain;
SemaphoreHandle_t g_audio_params_mutex;

static float rv_state = 0.0f;

/* ================= BIQUAD ================= */

static void biquad_peaking(biquad_t *q, float freq, float Q, float gain_db)
{
    float A = powf(10.0f, gain_db / 40.0f);
    float w0 = 2.0f * PI * freq / SAMPLE_RATE;
    float alpha = sinf(w0) / (2.0f * Q);
    float cosw0 = cosf(w0);

    float b0 = 1 + alpha * A;
    float b1 = -2 * cosw0;
    float b2 = 1 - alpha * A;
    float a0 = 1 + alpha / A;
    float a1 = -2 * cosw0;
    float a2 = 1 - alpha / A;

    q->b0 = b0 / a0;
    q->b1 = b1 / a0;
    q->b2 = b2 / a0;
    q->a1 = a1 / a0;
    q->a2 = a2 / a0;
    q->z1 = q->z2 = 0.0f;
}

static inline float biquad_process(biquad_t *q, float x)
{
    float y = q->b0 * x + q->z1;
    q->z1 = q->b1 * x - q->a1 * y + q->z2;
    q->z2 = q->b2 * x - q->a2 * y;
    return y;
}

/* ================= PUBLIC API ================= */

void audio_fx_set_chain_target_len(int len)
{
    if (len < 0) len = 0;
    if (len > 8) len = 8;


    chain_target_len = len;
    chain_current_len = 0;
}


audio_fx_params_t* audio_fx_get_params(void) { return &fx_params; }
audio_fx_chain_t*  audio_fx_get_chain(void)  { return &fx_chain; }

void audio_fx_set_eq8_band(int band, float gain_db)
{
    if (band < 0 || band >= 8) return;

    if (xSemaphoreTake(g_audio_params_mutex, portMAX_DELAY)) {
        biquad_peaking(&eq8[band], eq8_freqs[band], 1.0f, gain_db);
        xSemaphoreGive(g_audio_params_mutex);
    }
}

void audio_fx_init(void)
{
    g_audio_params_mutex = xSemaphoreCreateMutex();

    memset(mod_buf, 0, sizeof(mod_buf));
    memset(delay_buf, 0, sizeof(delay_buf));
    memset(&fx_params, 0, sizeof(fx_params));

    fx_chain.chain_len = 0;

    for (int i = 0; i < 8; i++)
        biquad_peaking(&eq8[i], eq8_freqs[i], 1.0f, 0.0f);
}

/* ================= FX ================= */

static inline float fx_preamp(float x)
{
    static float dc = 0;
    dc = 0.995f * dc + 0.005f * x;
    return x - dc;
}

static inline float fx_overdrive(float x, float drive)
{
    x *= 1.0f + drive * 25.0f;
    return tanhf(x) * 0.7f;
}

static inline float fx_distortion(float x, float drive)
{
    x *= 1.0f + drive * 20.0f;

    if (x > 0.6f)  x = 0.6f;
    if (x < -0.6f) x = -0.6f;

    return x;
}



static inline float fx_fuzz(float x, float drive)
{
    x *= 1.0f + drive * 25.0f;
    x = copysignf(1.0f, x) * (1.0f - expf(-fabsf(x)));
    return x * 0.45f;
}

static inline float fx_chorus(float x)
{
    static float lfo = 0;
    mod_buf[mod_idx] = x;

    lfo += 0.002f;
    if (lfo > 2 * PI) lfo -= 2 * PI;

    int rp = (mod_idx - (int)(200 + sinf(lfo) * 40) + MOD_BUF_SIZE) % MOD_BUF_SIZE;
    float y = mod_buf[rp];

    mod_idx = (mod_idx + 1) % MOD_BUF_SIZE;
    return 0.7f * x + 0.3f * y;
}

static inline float fx_flanger(float x)
{
    static float lfo = 0;
    lfo += 0.004f;
    if (lfo > 2 * PI) lfo -= 2 * PI;

    int rp = (mod_idx - (int)(50 + sinf(lfo) * 20) + MOD_BUF_SIZE) % MOD_BUF_SIZE;
    float y = mod_buf[rp];

    mod_buf[mod_idx] = x + y * 0.6f;
    mod_idx = (mod_idx + 1) % MOD_BUF_SIZE;

    return 0.7f * x + 0.3f * y;
}

static inline float fx_delay(float x)
{
    int rp = (delay_idx - 12000 + DELAY_BUF_SIZE) % DELAY_BUF_SIZE;
    float d = delay_buf[rp];
    delay_buf[delay_idx] = x + d * 0.35f;
    delay_idx = (delay_idx + 1) % DELAY_BUF_SIZE;
    return x * 0.7f + d * 0.45f;
}

static inline float fx_echo(float x)
{
    int rp = (delay_idx - 8000 + DELAY_BUF_SIZE) % DELAY_BUF_SIZE;
    float d = delay_buf[rp];
    delay_buf[delay_idx] = x + d * 0.25f;
    delay_idx = (delay_idx + 1) % DELAY_BUF_SIZE;
    return x * 0.75f + d * 0.35f;
}

static inline float fx_reverb(float x)
{
    rv_state = rv_state * 0.95f + x * 0.05f;
    return 0.7f * x + 0.3f * rv_state;
}

static inline float fx_eq3(float x)
{
    static float low = 0, mid = 0, high = 0;

    low  += 0.05f * (x - low);
    high += 0.05f * (x - high);

    float l = low;
    float h = x - high;
    float m = x - l - h;

    l *= powf(10.0f, fx_params.eq3_bass / 20.0f);
    m *= powf(10.0f, fx_params.eq3_mid  / 20.0f);
    h *= powf(10.0f, fx_params.eq3_treb / 20.0f);

    return (l + m + h) * powf(10.0f, fx_params.eq3_vol / 20.0f);
}


static inline float fx_eq8(float x)
{
    for (int i = 0; i < 8; i++)
        x = biquad_process(&eq8[i], x);
    return x;
}

static inline float fx_limiter(float x)
{
    if (x > 0.95f) return 0.95f;
    if (x < -0.95f) return -0.95f;
    return x;
}

/* ================= PROCESS ================= */

void audio_fx_process(int32_t *buffer, int frames)
{
    for (int i = 0; i < frames; i++) {

        float x = (float)buffer[i * 2] / 8388608.0f;
        x *= 0.05f;
        x = fx_preamp(x);

        if (chain_current_len < chain_target_len)
            chain_current_len++;
        else if (chain_current_len > chain_target_len)
            chain_current_len--;

        for (int j = 0; j < chain_current_len; j++) {
            switch (fx_chain.chain[j].type) {
                case FX_GAIN:
                    x *= powf(10.0f, fx_params.gain_db / 20.0f);
                    break;
                case FX_OVERDRIVE:
                    x = fx_overdrive(x, fx_params.od_drive);
                    break;
                case FX_DISTORTION:
                    x = fx_distortion(x, fx_params.dist_drive);
                    break;
                case FX_FUZZ:
                    x = fx_fuzz(x, fx_params.fuzz_drive);
                    break;
                case FX_CHORUS:
                    x = fx_chorus(x);
                    break;
                case FX_FLANGER:
                    x = fx_flanger(x);
                    break;
                case FX_DELAY:
                    x = fx_delay(x);
                    break;
                case FX_ECHO:
                    x = fx_echo(x);
                    break;
                case FX_REVERB:
                    x = fx_reverb(x);
                    break;
                case FX_EQ_3BAND:
                    x = fx_eq3(x);
                    break;
                case FX_EQ_8BAND:
                    x = fx_eq8(x);
                    break;
                default:
                    break;
            }
        }

        x = fx_limiter(x);

        int32_t o = (int32_t)(x * 2147483647.0f);
        buffer[i * 2]     = o;
        buffer[i * 2 + 1] = o;
    }
}
