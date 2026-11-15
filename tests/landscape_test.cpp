#include "landscape.h"
#include "attitude_math.h"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace trailstrip;
static void stable(LandscapeOrientation &o,float y,uint64_t start){
    for(uint64_t t=start;t<start+600;t+=50)assert(!o.update(0,y,0,t));
    assert(o.update(0,y,0,start+600));
}
int main(){
    LandscapeOrientation orientation;
    stable(orientation,1,0);assert(orientation.inverted());
    // Portrait, horizontal, shocks and intermittent samples never authorize a flip.
    for(uint64_t t=650;t<1500;t+=50)assert(!orientation.update(1,0,0,t));
    for(uint64_t t=1500;t<2500;t+=50)assert(!orientation.update(0,0,1,t));
    assert(orientation.inverted());
    for(uint64_t t=2500;t<2800;t+=50)assert(!orientation.update(0,-1,0,t));
    assert(!orientation.update(0,-2,0,2800));
    for(uint64_t t=2850;t<3150;t+=50)assert(!orientation.update(0,-1,0,t));
    assert(!orientation.update(0,-1,0,4000)); // Gap resets the 600 ms window.
    assert(orientation.inverted());
    orientation.cancel_candidate();stable(orientation,-1,4100);assert(!orientation.inverted());
    for(uint64_t t=4800;t<6000;t+=50)assert(!orientation.update(0,0.7f,0.7f,t));
    assert(!orientation.inverted());
    stable(orientation,1,6100);assert(orientation.inverted());

    // Each source pixel must reach the proper panel address in both orientations,
    // and a tap at that physical location must return to the original logical point.
    std::vector<unsigned> source(screen_width*screen_height),normal(source.size()),flipped(source.size());
    for(unsigned i=0;i<source.size();i++)source[i]=i;
    landscape_to_panel(source.data(),normal.data(),false);
    landscape_to_panel(source.data(),flipped.data(),true);
    for(int y=0;y<screen_height;y++)for(int x=0;x<screen_width;x++){
        const int i=y*screen_width+x;
        for(bool upside:{false,true}){
            const auto p=landscape_point(x,y,upside);
            const int panel_index=p.x*screen_height+(screen_height-1-p.y);
            assert((upside?flipped:normal)[panel_index]==unsigned(i));
            const auto touch=landscape_point(p.x,p.y,upside);
            assert(touch.x==x && touch.y==y);
        }
    }
    for(unsigned i=0;i<source.size();i++)assert(flipped[i]==normal[source.size()-1-i]);
    // Physical 180-degree mounting reverses sensor X/Y, but not sensor Z.
    const float x=-0.25f,y=-0.9f,z=0.2f;
    const auto upright=upright_tilt(x,y,z);
    const auto remapped=mounted_tilt(-x,-y,z,true);
    assert(std::fabs(upright.roll-remapped.roll)<0.0001f);
    assert(std::fabs(upright.pitch-remapped.pitch)<0.0001f);
    puts("landscape: debounce, ambiguous poses, sample gaps, every pixel/touch mapping and mount axes passed");
}
