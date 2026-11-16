#include "attitude_filter.h"
#include "attitude_math.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <initializer_list>

using Filter=trailstrip::AttitudeFilter;
static trailstrip::Tilt tilt(const Filter &f,bool inverted=false){auto g=f.gravity();return trailstrip::mounted_tilt(g.x,g.y,g.z,inverted);}
static void near(float a,float b,float tolerance=.1f){assert(std::fabs(a-b)<tolerance);}
static void prime(Filter &f,Filter::Vector a,Filter::Vector w={0,0,0}){
    f.update(a,w,0);
    for(int i=0;i<45;i++)f.update(a,w,.05f);
    assert(f.calibrated());
}
int main(){
    for(bool inverted:{false,true}){
        const float sign=inverted?-1:1;
        Filter f;prime(f,{0,-sign,0});
        // Five seconds each of acceleration and braking, without body rotation.
        for(float acceleration:{-.3f,.3f,-.1f,.1f}){
            for(int i=0;i<(std::fabs(acceleration)>.2f?40:100);i++)assert(f.update({0,-sign,acceleration},{0,0,0},.05f));
            near(tilt(f,inverted).pitch,0,1.5f);
            for(int i=0;i<400;i++)f.update({0,-sign,0},{0,0,0},.05f);
        }
        // True pitch rotation must follow immediately, in either mounting.
        Filter pitch;prime(pitch,{0,-sign,0});
        for(int i=1;i<=60;i++){
            float angle=i*.5f*.01745329252f;
            pitch.update({0,-sign*std::cos(angle),-std::sin(angle)},{-sign*10,0,0},.05f);
        }
        near(tilt(pitch,inverted).pitch,30);
        // A pitch-changing climb while accelerating must still track rotation.
        Filter moving;prime(moving,{0,-sign,0});
        for(int i=1;i<=60;i++){
            float angle=i*.5f*.01745329252f;
            moving.update({0,-sign*std::cos(angle),-std::sin(angle)+.3f},{-sign*10,0,0},.05f);
        }
        near(tilt(moving,inverted).pitch,30,.3f);
        Filter roll;prime(roll,{0,-sign,0});
        for(int i=1;i<=60;i++){
            float angle=i*.5f*.01745329252f;
            roll.update({-sign*std::sin(angle),-sign*std::cos(angle),0},{0,0,10},.05f);
        }
        near(tilt(roll,inverted).roll,30);
    }
    // A physical 180-degree screen flip stays continuous in sensor space.
    Filter flip;prime(flip,{0,-1,0});
    for(int i=1;i<=180;i++){
        float angle=i*.01745329252f;
        flip.update({-std::sin(angle),-std::cos(angle),0},{0,0,20},.05f);
    }
    near(tilt(flip,true).roll,0);
    // Coupled pose: body-X rotation mixes roll and pitch; vector integration
    // must preserve both rather than adding gyro axes to Euler angles.
    Filter compound;prime(compound,{-.5f,-.8660254f,0});
    for(int i=1;i<=60;i++){
        float angle=i*.5f*.01745329252f;
        compound.update({-.5f,-.8660254f*std::cos(angle),-.8660254f*std::sin(angle)},
                        {-10,0,0},.05f);
    }
    const auto expected=trailstrip::upright_tilt(-.5f,-.75f,-.4330127f);
    near(tilt(compound).roll,expected.roll);near(tilt(compound).pitch,expected.pitch);
    Filter f;assert(!f.update({0,0,0},{0,0,0},0));
    assert(f.update({0,-1,0},{0,0,0},0));
    // Vertical shock / free fall must not pull the estimate toward acceleration.
    for(int i=0;i<20;i++)f.update({1,-2,1},{0,0,0},.05f);
    near(tilt(f).pitch,0);near(tilt(f).roll,0);
    f.update({0,0,0},{0,0,0},.05f);near(tilt(f).pitch,0);
    assert(!f.update({0,-1,0},{NAN,0,0},.05f));
    // Reacquire after a gap rather than integrating stale gyro over that gap.
    assert(f.update({0,-.8660254f,-.5f},{80,0,0},1));near(tilt(f).pitch,30);
    // Stationary gyro offset is learned without changing user zero calibration.
    Filter biased;biased.update({0,-1,0},{.2f,0,0},0);
    for(int i=0;i<1200;i++)biased.update({0,-1,0},{.2f,0,0},.05f);
    near(tilt(biased).pitch,0,.1f);
    // Regression: actual observed large gyro rates, treated as a static bias.
    // Include the device's slightly high acceleration magnitude (about 1.036 g).
    Filter large;const Filter::Vector rest{.229f,.861f,.529f},offset{-8.078f,-9.836f,-3.695f};
    const auto target=trailstrip::upright_tilt(rest.x,rest.y,rest.z);
    large.update(rest,offset,0);
    for(int i=0;i<2400;i++){
        large.update(rest,offset,.05f);
        near(tilt(large).roll,target.roll,.1f);near(tilt(large).pitch,target.pitch,.1f);
    }
    assert(large.calibrated());
    // Unexpected bias after calibration must recover rather than locking out
    // acceleration forever. Also exercise recovery beyond the former 10° gate.
    Filter recovery;prime(recovery,{0,-1,0});
    for(int i=0;i<400;i++)recovery.update({0,-1,0},offset,.05f);
    assert(recovery.calibrated());near(tilt(recovery).roll,0,.1f);near(tilt(recovery).pitch,0,.1f);
    // Changing poses cannot qualify as stationary startup calibration.
    Filter motion;motion.update({0,-1,0},{10,0,0},0);
    for(int i=1;i<40;i++){
        float angle=i*.01f;
        motion.update({0,-std::cos(angle),std::sin(angle)},{10,0,0},.05f);
    }
    assert(!motion.calibrated());
    puts("fusion: acceleration/braking, real rotation, inverted mount, shock, gaps, bias passed");
}
