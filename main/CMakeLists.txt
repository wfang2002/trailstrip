idf_component_register(SRCS "board.cpp" "main.cpp" "attitude.cpp" "telemetry.cpp" "ble.cpp" "ui.cpp" "vehicle.cpp" "display_preferences.cpp"
    INCLUDE_DIRS "."
    REQUIRES i2c_bsp lcd_bl_pwm_bsp sensor_qmi8658 nvs_flash bt esp_timer
             espressif__esp_lcd_axs15231b espressif__esp_io_expander_tca9554 lvgl__lvgl)
