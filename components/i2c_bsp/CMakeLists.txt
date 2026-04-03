idf_component_register(
    SRCS "i2c_bsp.c"
    INCLUDE_DIRS "."
    REQUIRES driver espressif__esp_io_expander_tca9554
    PRIV_REQUIRES project_config espressif__esp_io_expander
)