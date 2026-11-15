#include "attitude_math.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>

static void close(float actual,float expected){assert(std::fabs(actual-expected)<0.001f);}
static void check(float x,float y,float z,float roll,float pitch){
    const auto t=trailstrip::upright_tilt(x,y,z);
    close(t.roll,roll);close(t.pitch,pitch);
}
int main(){
    // Upright, driver-facing rest must be level, not the old -90-degree roll.
    check(0,-1,0,0,0);
    const float s=0.5f,c=std::sqrt(3.f)/2;
    // Roll rotates gravity within the screen plane, without changing pitch.
    check(-s,-c,0,30,0);check(s,-c,0,-30,0);
    // Nose-up/down changes the screen-normal component, without changing roll.
    check(0,-c,-s,0,30);check(0,-c,s,0,-30);
    // Simultaneous vehicle roll/pitch expressed as the sensor's gravity vector.
    for(float roll:{-40.f,-15.f,0.f,15.f,40.f})
        for(float pitch:{-35.f,0.f,35.f}){
            float r=roll*0.01745329252f,p=pitch*0.01745329252f;
            check(-sinf(r)*cosf(p),-cosf(r)*cosf(p),-sinf(p),roll,pitch);
        }
    // Scaling gravity magnitude must not change the computed angles.
    check(-s*1.1f,-c*1.1f,0,30,0);
    // Existing calibration must still zero the same physical position.
    const auto zero=trailstrip::legacy_upright_zero({2.f,30.f});
    close(zero.roll,2.f);close(zero.pitch,-30.f);
    const auto old_level=trailstrip::upright_tilt(0,-c,s);
    close(old_level.pitch-zero.pitch,0.f);
    puts("attitude: upright zero, independent roll/pitch, signs and compound tilt passed");
}
