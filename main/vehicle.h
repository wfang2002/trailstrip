#pragma once
#include "lvgl.h"
// Code-native Wrangler-inspired line art; no runtime image decoder or heap assets.
lv_obj_t *vehicle_create(lv_obj_t *parent, bool side, int x, int y, int width, int height);
void vehicle_set_attitude(lv_obj_t *vehicle, float degrees, bool valid);

// Rear view reverses only visual roll rotation, retaining the sensor reading.
void vehicle_set_rear(lv_obj_t *vehicle, bool rear);

// Fixed top view, nose right. Bits 0..3: front-left, front-right, rear-left, rear-right.
lv_obj_t *vehicle_top_create(lv_obj_t *parent,int x,int y,int width,int height);
void vehicle_top_set_low(lv_obj_t *vehicle,uint8_t corners,bool valid);
