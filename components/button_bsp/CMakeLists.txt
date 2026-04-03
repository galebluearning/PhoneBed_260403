idf_component_register(
    SRCS "multi_button.c" "button_bsp.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_timer driver project_config
)