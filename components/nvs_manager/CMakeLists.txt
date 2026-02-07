idf_component_register(SRCS "nvs_manager.c"
                    INCLUDE_DIRS "." "${CMAKE_SOURCE_DIR}/components/ui/include"
                    REQUIRES nvs_flash ui)