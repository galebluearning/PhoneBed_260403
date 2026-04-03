idf_component_register(
    SRCS "esp_wifi_bsp.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_event nvs_flash project_config
    REQUIRES esp_wifi
)