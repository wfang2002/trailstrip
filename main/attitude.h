#pragma once
struct Attitude {float roll=0,pitch=0;bool valid=false,calibrated=false;bool inverted=false;};
void attitude_start();
Attitude attitude_read();
bool attitude_zero();
