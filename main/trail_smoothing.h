#pragma once
#include "attitude.h"
#include "telemetry.h"
#include <algorithm>
#include <cmath>

namespace trailstrip {
// OBD speed is km/h; phone speed is m/s. A negative result means unavailable.
inline float trail_speed(const Reading &obd,bool obd_connected,const Reading &phone,bool phone_connected,uint64_t now){
    auto usable=[now](const Reading &r){return fresh(r,now)&&std::isfinite(r.value)&&r.value>=0;};
    if(obd_connected&&usable(obd))return obd.value;
    if(phone_connected&&usable(phone))return phone.value*3.6f;
    return -1;
}
class TrailSmoothing {
public:
    void reset(){count_=0;head_=0;enabled_=false;mix_=0;}
    bool enabled()const{return enabled_;}
    Attitude update(Attitude a,float speed,uint64_t now){
        if(!a.valid || !std::isfinite(a.roll) || !std::isfinite(a.pitch)){
            reset();a.valid=false;return a;
        }
        if(count_ && (a.inverted!=inverted_ || now<at(count_-1).time || now-at(count_-1).time>500))reset();
        inverted_=a.inverted;
        if(!std::isfinite(speed)||speed<0)enabled_=false;
        else if(speed>22)enabled_=true;
        else if(speed<18)enabled_=false;
        float roll=a.roll;
        const uint64_t elapsed=count_?now-at(count_-1).time:0;
        if(count_)roll=at(count_-1).roll+std::remainder(roll-at(count_-1).roll,360.f);
        // Preserve a boundary sample for a time-weighted, trailing two seconds.
        const uint64_t start=now>2000?now-2000:0;
        while(count_>1 && at(1).time<=start){head_=(head_+1)%capacity;--count_;}
        if(count_ && !elapsed){at(count_-1)={now,roll,a.pitch};}
        else{
            if(count_==capacity){head_=(head_+1)%capacity;--count_;}
            at(count_++)={now,roll,a.pitch};
        }
        float sum_roll=0,sum_pitch=0;uint64_t duration=0;
        for(unsigned i=0;i+1<count_;++i){
            const auto &s=at(i);
            const uint64_t dt=at(i+1).time-std::max(start,s.time);
            sum_roll+=s.roll*dt;sum_pitch+=s.pitch*dt;duration+=dt;
        }
        // Half-second crossfade avoids a jump at either speed threshold.
        const float step=elapsed/500.f;
        mix_=enabled_?std::min(1.f,mix_+step):std::max(0.f,mix_-step);
        if(duration){
            a.roll=std::remainder(roll+mix_*(sum_roll/duration-roll),360.f);
            a.pitch+=mix_*(sum_pitch/duration-a.pitch);
        }
        return a;
    }
private:
    struct Sample{uint64_t time;float roll,pitch;};
    static constexpr unsigned capacity=128; // 6.4 s at the normal 50 ms UI tick.
    Sample samples_[capacity]{};
    Sample &at(unsigned index){return samples_[(head_+index)%capacity];}
    unsigned count_=0,head_=0;
    bool enabled_=false,inverted_=false;
    float mix_=0;
};
}
