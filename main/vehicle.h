#pragma once
#include "lvgl.h"
// Code-native Wrangler-inspired line art; no runtime image decoder or heap assets.
lv_obj_t *vehicle_create(lv_obj_t *parent, bool side, int x, int y, int width, int height);
void vehicle_set_attitude(lv_obj_t *vehicle, float degrees, bool valid);

// Rear view reverses only visual roll rotation, retaining the sensor reading.
void vehicle_set_rear(lv_obj_t *vehicle, bool rear);
