#include "trailstrip.h"
#include "display_preferences.h"
#include "nvs_flash.h"
#include "esp_log.h"
extern "C" void app_main() {
    // Preserve NVS on an incompatible version instead of silently erasing calibration.
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_LOGI("trailstrip", "TRAILSTRIP 0.1.0 / Waveshare 3.49 V%d", TRAILSTRIP_BOARD_V2 ? 2 : 1);
    display_preferences_load();
    ble_start();
    board_start();
}
