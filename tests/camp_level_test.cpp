#include "camp_level.h"
#include <cassert>
#include <cstring>
#include <limits>
int main(){
 using trailstrip::camp_level;
 assert(camp_level(0,0,true).low_corners==0);
 assert(std::strcmp(camp_level(.5f,-.5f,true).label,"NEAR LEVEL")==0);
 assert(camp_level(1,0,true).low_corners==5); // left wheels
 assert(camp_level(-1,0,true).low_corners==10);
 assert(camp_level(0,1,true).low_corners==12); // nose up: rear low
 assert(camp_level(0,-1,true).low_corners==3);
 assert(camp_level(1,-1,true).low_corners==1); // front left
 assert(camp_level(-1,-1,true).low_corners==2);
 assert(camp_level(1,1,true).low_corners==4);
 assert(camp_level(-1,1,true).low_corners==8);
 assert(std::strcmp(camp_level(1,0,true).label,"LEFT SIDE LOW")==0);
 assert(std::strcmp(camp_level(-1,0,true).label,"RIGHT SIDE LOW")==0);
 assert(std::strcmp(camp_level(1,-1,true).label,"FRONT LEFT LOW")==0);
 assert(std::strcmp(camp_level(-1,1,true).label,"REAR RIGHT LOW")==0);
 assert(!camp_level(1,1,false).valid);
 assert(camp_level(1,1,false).low_corners==0);
 assert(!camp_level(std::numeric_limits<float>::quiet_NaN(),0,true).valid);
}
