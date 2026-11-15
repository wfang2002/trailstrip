# Third-party sources

`main/board.cpp`, `main/user_config.h`, and the `i2c_bsp` and `lcd_bl_pwm_bsp` components adapt Waveshare's LVGL V8 example:

- https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.49-V2
- Commit `1c157e6e8e68b89fd4dc400f46bf1724cb64a57e`
- Source directory `ESP-IDF/09_LVGL_V8_Test`
- Apache-2.0 license retained at `components/WAVESHARE-LICENSE`.
- Modifications: Trailstrip entry point, revision selection, V2 SYS_EN assertion, ISR initialization, checked display transfers, corrected touch bounds, I2C error types/timeouts, and removal of demo/backlight tasks.

V1 reset and pin configuration were checked against the official V1 example on 2026-11-09:
https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.49/tree/main/Examples/ESP-IDF/09_LVGL_V8_Test

`components/sensor_qmi8658` contains only the QMI8658 header dependency closure from the SensorLib copy distributed in Waveshare's `ESP-IDF/03_I2C_QMI8658` example at the same V2 commit. Original Lewis He copyright and MIT license are retained. Other sensor drivers and firmware blobs were not copied.

ESP-IDF Component Manager downloads LVGL and Espressif display/expander dependencies. Their licenses are retained in the managed packages. Versions and hashes are recorded in `dependencies.lock`; generated dependency directories are excluded from Git.
