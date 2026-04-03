idf_component_register(
    SRCS "i2c_equipment.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES SensorLib i2c_bsp project_config
)