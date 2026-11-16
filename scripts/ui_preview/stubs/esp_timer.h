#pragma once
#include <cstdint>
inline int64_t preview_time_us=1000000;
inline int64_t esp_timer_get_time(){return preview_time_us;}
