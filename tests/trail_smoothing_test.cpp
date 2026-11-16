#include "trail_smoothing.h"
#include <cassert>
#include <cstdio>
using trailstrip::TrailSmoothing;
static void near(float a,float b){assert(std::fabs(a-b)<.02f);}
static Attitude sample(float r,float p=0){return {r,p,true,false,false};}
int main(){
    TrailSmoothing f;
    f.update(sample(0),22,0);assert(!f.enabled());
    f.update(sample(0),23,50);assert(f.enabled());
    f.update(sample(0),18,100);assert(f.enabled());
    f.update(sample(0),17,150);assert(!f.enabled());
    f.update(sample(0),20,200);assert(!f.enabled());
    // Full window: a step reaches half its value after one second and all
    // of it after two seconds. Both numbers use identical averaging.
    f.reset();for(int t=0;t<2000;t+=50)f.update(sample(0),30,t);
    near(f.update(sample(20,10),30,2000).roll,0);
    Attitude out;
    for(int t=2050;t<=3000;t+=50)out=f.update(sample(20,10),30,t);
    near(out.roll,10);near(out.pitch,5);
    for(int t=3050;t<=4000;t+=50)out=f.update(sample(20,10),30,t);
    near(out.roll,20);near(out.pitch,10);
    // Low/stale speed crossfades out within 500ms, then passes through.
    for(int t=4050;t<=4500;t+=50)out=f.update(sample(35),-1,t);
    assert(!f.enabled());near(out.roll,35);
    near(f.update(sample(12),10,4550).roll,12);
    // Invalid readings, frame changes, gaps and explicit zero reset discard history.
    f.update({0,0,false,false,false},30,4600);
    near(f.update(sample(50),30,4650).roll,50);
    auto inverted=sample(-50);inverted.inverted=true;
    near(f.update(inverted,30,4700).roll,-50);
    near(f.update(sample(15),30,6000).roll,15);
    f.reset();near(f.update(sample(0),30,6050).roll,0);
    // Roll averaging must cross +/-180 via the short arc, not through zero.
    f.reset();for(int t=0;t<2000;t+=50)f.update(sample(179),30,t);
    for(int t=2000;t<=3000;t+=50)out=f.update(sample(-179),30,t);
    assert(std::fabs(out.roll)>179);
    // Unequal intervals are weighted by time, not by sample count.
    f.reset();for(int t=0;t<=2000;t+=100)f.update(sample(0),30,t);
    f.update(sample(20),30,2100);
    f.update(sample(20),30,2200);
    f.update(sample(20),30,2500);
    out=f.update(sample(20),30,2900);near(out.roll,8);
    Reading obd{30,1000,true},phone{10,1000,true};
    near(trailstrip::trail_speed(obd,true,phone,true,1000),30);
    near(trailstrip::trail_speed(obd,false,phone,true,1000),36);
    obd.received_ms=0;near(trailstrip::trail_speed(obd,true,phone,true,5500),36);
    near(trailstrip::trail_speed(obd,true,phone,true,6001),-1);
    obd.value=NAN;near(trailstrip::trail_speed(obd,true,phone,false,1000),-1);
    phone.value=-1;near(trailstrip::trail_speed(obd,true,phone,true,1000),-1);
    puts("trail smoothing: window, weighting, hysteresis, fades, resets, wrap and speed sources passed");
}
