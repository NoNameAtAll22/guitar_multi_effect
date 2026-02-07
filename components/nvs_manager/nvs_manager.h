#pragma once

#include "esp_err.h"
#include "ui_app.h" // For preset_t and MAX_PRESETS

#ifdef __cplusplus
extern "C" {
#endif

// Function to initialize NVS manager (open NVS handle)
esp_err_t nvs_manager_init(void);

// Function to save presets and preset_count to NVS
esp_err_t nvs_manager_save_presets(const preset_t *presets, int count);

// Function to load presets and preset_count from NVS
esp_err_t nvs_manager_load_presets(preset_t *presets, int *count);

#ifdef __cplusplus
}
#endif
