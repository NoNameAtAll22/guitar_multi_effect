idf_component_register(SRCS "ui_app.c"
                        INCLUDE_DIRS "include" "${CMAKE_SOURCE_DIR}/managed_components/lvgl__lvgl" "${CMAKE_SOURCE_DIR}/managed_components/lvgl__lvgl/src" "${CMAKE_SOURCE_DIR}/components/nvs_manager"
                        REQUIRES lvgl nvs_manager)
