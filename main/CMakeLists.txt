idf_component_register(SRCS "main.c" "board_init.c" "spi_bus.c" "lcd_driver.c" "touch_driver.c" "lvgl_adapter.c" "audio_fx.c"
                    INCLUDE_DIRS "." "${CMAKE_SOURCE_DIR}/components/nvs_manager"
                    REQUIRES esp_lcd_ili9341 lvgl esp_lcd_touch_xpt2046 esp_timer esp_mm ui esp-dsp driver nvs_flash nvs_manager)
