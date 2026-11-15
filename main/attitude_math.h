#pragma once
#include <cmath>

namespace trailstrip {
struct Tilt { float roll; float pitch; };

// Landscape display upright and facing the driver; expected rest gravity
// is approximately (0, -1, 0) g. Vehicle axes in sensor space:
// Upright reference is -Y. Pitch uses -Z following the physical mount check.
// Positive roll means vehicle right side down; positive pitch means nose up.
// Roll convention is unchanged; pitch direction corrected from user feedback.
inline Tilt upright_tilt(float sensor_x, float sensor_y, float sensor_z) {
    constexpr float degrees_per_radian = 57.295779513f;
    const float up = -sensor_y;
    const float right = sensor_x;
    const float forward = -sensor_z;
    return {std::atan2(-right, up) * degrees_per_radian,
            std::atan2(forward, std::hypot(right, up)) * degrees_per_radian};
}
// Normalize either landscape mounting orientation before applying saved offsets.
inline Tilt mounted_tilt(float x,float y,float z,bool inverted){
    const float sign=inverted?-1.f:1.f;
    return upright_tilt(sign*x,sign*y,z);
}
// The previous zero_upright key stored pitch with the opposite sign.
inline Tilt legacy_upright_zero(Tilt saved) { return {saved.roll, -saved.pitch}; }
}
