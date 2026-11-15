#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace trailstrip {
constexpr int screen_width=640;
constexpr int screen_height=172;
struct ScreenPoint {int x,y;};
// The same involution maps physical -> logical touch or logical -> physical pixels.
inline ScreenPoint landscape_point(int x,int y,bool inverted){
    return inverted?ScreenPoint{screen_width-1-x,screen_height-1-y}:ScreenPoint{x,y};
}
// Convert a complete logical 640x172 frame to the panel's native 172x640 order.
// Add the 180-degree flip during the existing copy; no extra framebuffer is needed.
template<class Pixel>
void landscape_to_panel(const Pixel *source,Pixel *destination,bool inverted){
    for(int x=0;x<screen_width;x++)
        for(int y=0;y<screen_height;y++){
            const int index=inverted ? y*screen_width+(screen_width-1-x)
                                     : (screen_height-1-y)*screen_width+x;
            destination[x*screen_height+y]=source[index];
        }
}

class LandscapeOrientation {
public:
    void restore(bool inverted){inverted_=inverted;pending_=false;}
    bool inverted() const {return inverted_;}
    void cancel_candidate(){pending_=false;}
    // Input is raw accelerometer g, before zero calibration or orientation remapping.
    // Ignore shocks, face-up/down poses, and portrait poses. Require 600 ms of
    // consecutive, confident samples before switching between the two landscapes.
    bool update(float x,float y,float z,uint64_t now_ms){
        const float norm=std::sqrt(x*x+y*y+z*z);
        if(!std::isfinite(norm) || norm<0.85f || norm>1.15f ||
           std::fabs(y)<0.75f*norm || std::fabs(x)>0.5f*norm){
            cancel_candidate();return false;
        }
        const bool candidate=y>0;
        if(candidate==inverted_){cancel_candidate();return false;}
        if(!pending_ || candidate!=candidate_ || now_ms<since_ || now_ms-last_sample_>150){
            pending_=true;candidate_=candidate;since_=now_ms;
        }
        last_sample_=now_ms;
        if(now_ms-since_<600)return false;
        inverted_=candidate;pending_=false;return true;
    }
private:
    bool inverted_=false,pending_=false,candidate_=false;
    uint64_t since_=0,last_sample_=0;
};
}
