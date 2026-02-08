idf_component_register(
    SRCS "audio_fx.c"
    INCLUDE_DIRS "include" # Add main directory
    REQUIRES driver board effect_type
)