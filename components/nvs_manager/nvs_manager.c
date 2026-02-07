#include "nvs_manager.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "NVS_MANAGER";
static nvs_handle_t s_nvs_handle;

#define STORAGE_NAMESPACE "storage"
#define KEY_PRESETS "presets"
#define KEY_PRESET_COUNT "preset_count"

esp_err_t nvs_manager_init(void) {
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &s_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "NVS opened successfully.");
    }
    return err;
}

esp_err_t nvs_manager_save_presets(const preset_t *presets, int count) {
    esp_err_t err;
    if (s_nvs_handle == 0) return ESP_FAIL; // NVS not initialized

    err = nvs_set_i32(s_nvs_handle, KEY_PRESET_COUNT, count);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save preset count (%s)", esp_err_to_name(err));
        return err;
    }

    if (count > 0) {
        size_t required_size = count * sizeof(preset_t);
        err = nvs_set_blob(s_nvs_handle, KEY_PRESETS, presets, required_size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save presets blob (%s)", esp_err_to_name(err));
            return err;
        }
    } else {
        // If count is 0, erase the blob to free space
        err = nvs_erase_key(s_nvs_handle, KEY_PRESETS);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) { // NVS_NOT_FOUND is OK if no presets were saved
            ESP_LOGE(TAG, "Failed to erase presets blob (%s)", esp_err_to_name(err));
            return err;
        }
    }

    err = nvs_commit(s_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS changes (%s)", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Presets saved successfully. Count: %d", count);
    }
    return err;
}

esp_err_t nvs_manager_load_presets(preset_t *presets, int *count) {
    esp_err_t err;
    if (s_nvs_handle == 0) return ESP_FAIL; // NVS not initialized

    int32_t loaded_count = 0;
    err = nvs_get_i32(s_nvs_handle, KEY_PRESET_COUNT, &loaded_count);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to load preset count (%s)", esp_err_to_name(err));
        return err;
    }
    
    if (loaded_count <= 0 || loaded_count > MAX_PRESETS) {
        ESP_LOGW(TAG, "No presets found or invalid count (%d), starting with empty/dummy data.", loaded_count);
        *count = 0;
        return ESP_ERR_NVS_NOT_FOUND; // Indicate no valid data loaded
    }

    size_t required_size = loaded_count * sizeof(preset_t);
    size_t actual_size = required_size;
    err = nvs_get_blob(s_nvs_handle, KEY_PRESETS, presets, &actual_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load presets blob (%s)", esp_err_to_name(err));
        *count = 0;
        return err;
    }

    if (actual_size != required_size) {
        ESP_LOGE(TAG, "Loaded blob size (%zu) does not match expected size (%zu)", actual_size, required_size);
        *count = 0;
        return ESP_FAIL;
    }

    *count = loaded_count;
    ESP_LOGI(TAG, "Presets loaded successfully. Count: %d", *count);
    return ESP_OK;
}