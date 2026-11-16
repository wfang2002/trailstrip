#pragma once
#include <cmath>
#include <cstdint>
namespace trailstrip {
struct CampLevel {uint8_t low_corners;const char *label;bool valid;};
// Camp guidance corrected from mounted-device feedback: positive roll = left low.
// Positive pitch = rear low. Sensor readings and bubble directions are unchanged.
// Guidance is relative to the saved calibration, not measured wheel heights.
inline CampLevel camp_level(float roll,float pitch,bool valid){
    if(!valid || !std::isfinite(roll) || !std::isfinite(pitch))return {0,"HOLD STILL",false};
    constexpr float tolerance=.5f;
    int side=roll>tolerance?-1:roll<-tolerance?1:0;
    int axle=pitch>tolerance?1:pitch<-tolerance?-1:0;
    if(!side && !axle)return {0,"NEAR LEVEL",true};
    uint8_t mask=(side==1?10:side==-1?5:15)&(axle==1?12:axle==-1?3:15);
    const char *label=side==1?(axle==1?"REAR RIGHT LOW":axle==-1?"FRONT RIGHT LOW":"RIGHT SIDE LOW"):
        side==-1?(axle==1?"REAR LEFT LOW":axle==-1?"FRONT LEFT LOW":"LEFT SIDE LOW"):
        axle==1?"REAR LOW":"FRONT LOW";
    return {mask,label,true};
}
}
