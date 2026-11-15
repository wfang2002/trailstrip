#pragma once
#include <cstdint>
using esp_err_t=int;
using nvs_handle_t=unsigned;
constexpr esp_err_t ESP_OK=0,ESP_ERR_NVS_NOT_FOUND=1,ESP_FAIL=2;
constexpr int NVS_READONLY=0,NVS_READWRITE=1;
esp_err_t nvs_open(const char*,int,nvs_handle_t*);
esp_err_t nvs_get_u8(nvs_handle_t,const char*,uint8_t*);
esp_err_t nvs_set_u8(nvs_handle_t,const char*,uint8_t);
esp_err_t nvs_commit(nvs_handle_t);
void nvs_close(nvs_handle_t);
const char *esp_err_to_name(esp_err_t);
