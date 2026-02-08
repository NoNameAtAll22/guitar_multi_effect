idf_component_register(
    SRCS "audio_i2s.c"
    INCLUDE_DIRS "." # Add main directory
    REQUIRES driver board effect_type
)
