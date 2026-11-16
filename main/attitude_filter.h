#pragma once
#include <cmath>

namespace trailstrip {
// Estimate gravity in the sensor frame. Gyro is degrees/second; accel is g.
// Keeping a vector avoids Euler-angle coupling and landscape sign changes.
class AttitudeFilter {
public:
    struct Vector { float x,y,z; };
    bool update(Vector a, Vector w, float dt) {
        const float n=length(a);
        if(!finite(a) || !finite(w) || !std::isfinite(dt))return false;
        if(!initialized_ || dt>0.25f){
            // Never integrate across a missing-sample gap.
            if(n<0.85f || n>1.15f)return false;
            gravity_=scale(a,1/n); initialized_=true; stationary_=0;
            calibrated_=false; rejected_=0; samples_=0; gyro_anchor_=w; sum_={0,0,0};
            anchor_=gravity_;
            return true;
        }
        if(dt<=0)return false;
        const Vector measured=n>0.01f?scale(a,1/n):gravity_;
        // Raw angular rate is not a stillness test before bias is known.
        // Require two seconds of stable acceleration AND stable gyro samples.
        const bool stable=std::fabs(n-1)<0.08f && length(w)<25.f &&
            length(sub(measured,anchor_))<0.01f &&
            length(sub(w,gyro_anchor_))<0.6f &&
            (!calibrated_ || length(sub(w,bias_))<1.f);
        if(stable){
            stationary_+=dt;sum_=add(sum_,w);++samples_;
            if(stationary_>=2.f){
                const Vector average=scale(sum_,1.f/samples_);
                if(!calibrated_){bias_=average;calibrated_=true;gravity_=measured;}
                else bias_=add(bias_,scale(sub(average,bias_),1-std::exp(-stationary_/10.f)));
                // Restart the window so old samples cannot mask temperature drift.
                stationary_=0;samples_=0;sum_={0,0,0};
                anchor_=measured;gyro_anchor_=w;
            }
        }else{
            stationary_=0;samples_=0;sum_={0,0,0};
            anchor_=measured;gyro_anchor_=w;
        }
        if(!calibrated_){
            // Usable, bounded accelerometer-only readings while awaiting rest.
            if(n>0.7f && n<1.3f){
                gravity_=add(gravity_,scale(sub(measured,gravity_),1-std::exp(-dt/.25f)));
                gravity_=scale(gravity_,1/length(gravity_));
            }
            return true;
        }
        const Vector omega=scale(sub(w,bias_),0.01745329252f);
        const float rate=length(omega), angle=rate*dt;
        if(rate>0.000001f){
            const Vector axis=scale(omega,1/rate);
            // In body coordinates a fixed world vector rotates by -omega.
            gravity_=add(add(scale(gravity_,std::cos(angle)),
                            scale(cross(gravity_,axis),std::sin(angle))),
                         scale(axis,dot(axis,gravity_)*(1-std::cos(angle))));
        }
        // Norm alone cannot distinguish braking from gravity. Also limit
        // correction by disagreement with the gyro-predicted direction.
        const float disagreement=length(sub(measured,gravity_));
        // A persistent mismatch must not permanently lock out gravity.
        // Fall back and recalibrate instead of allowing unbounded integration.
        if(n>0.7f && n<1.3f && disagreement>=0.1743f)rejected_+=dt;
        else rejected_=0;
        if(rejected_>=3.f){
            calibrated_=false;gravity_=measured;rejected_=0;
            stationary_=0;samples_=0;sum_={0,0,0};
            anchor_=measured;gyro_anchor_=w;
            return true;
        }
        if(std::fabs(n-1)<0.04f && disagreement<0.1743f){ // about 10 degrees
            const float tau=disagreement<0.0524f?4.f:20.f;
            gravity_=add(gravity_,scale(sub(measured,gravity_),1-std::exp(-dt/tau)));
        }
        gravity_=scale(gravity_,1/length(gravity_));
        return true;
    }
    Vector gravity() const {return gravity_;}
    bool calibrated() const {return calibrated_;}
    Vector bias() const {return bias_;}
private:
    static bool finite(Vector v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
    static float dot(Vector a,Vector b){return a.x*b.x+a.y*b.y+a.z*b.z;}
    static float length(Vector v){return std::sqrt(dot(v,v));}
    static Vector scale(Vector v,float s){return {v.x*s,v.y*s,v.z*s};}
    static Vector add(Vector a,Vector b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
    static Vector sub(Vector a,Vector b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
    static Vector cross(Vector a,Vector b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
    bool initialized_=false,calibrated_=false;
    unsigned samples_=0;
    Vector gyro_anchor_{0,0,0},sum_{0,0,0};
    Vector gravity_{0,-1,0},bias_{0,0,0},anchor_{0,-1,0};
    float stationary_=0,rejected_=0;
};
}
