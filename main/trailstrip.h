#pragma once
void board_start();
void trailstrip_start();
void ble_start();
void ui_start();

// Call from the LVGL task/lock; logical resolution remains 640 x 172.
void board_set_landscape_inverted(bool inverted);
