# ESP32-TWAI-CAN - IDF component registration
# (does not affect PlatformIO/Arduino builds, which use library.json srcFilter)
idf_component_register(SRCS "src/ESP32-TWAI-CAN.cpp"
                       INCLUDE_DIRS "src"
                       REQUIRES esp_driver_twai
                                esp_driver_gpio)
