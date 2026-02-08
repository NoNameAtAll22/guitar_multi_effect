idf_component_register(
    SRCS "audio_task.c"
    INCLUDE_DIRS "include"
    REQUIRES audio_i2s audio_fx effect_type
)
