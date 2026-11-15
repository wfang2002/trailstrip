#include "telemetry.h"
#include <cassert>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <limits>
static void put(uint8_t *p,int i,float value){
    uint32_t b;std::memcpy(&b,&value,4);
    for(int n=0;n<4;n++)p[4+4*i+n]=(b>>(8*n))&255;
}
int main(){
    Telemetry t;uint8_t p[20]={1,1,15,0};
    put(p,0,1284);put(p,1,2.5);put(p,2,359);put(p,3,8);
    assert(decode_packet(p,20,100,t));assert(t.altitude.value==1284);assert(t.speed.value==2.5f);
    assert(fresh(t.speed,5100));assert(!fresh(t.speed,5101));assert(!fresh(t.speed,99));
    assert(!decode_packet(p,19,200,t));p[0]=2;assert(!decode_packet(p,20,200,t));p[0]=1;
    put(p,1,-1);assert(!decode_packet(p,20,200,t));assert(t.speed.value==2.5f);
    p[2]=13;assert(decode_packet(p,20,200,t));assert(!t.speed.valid);
    put(p,0,std::numeric_limits<float>::quiet_NaN());assert(!decode_packet(p,20,300,t));
    p[1]=2;p[2]=3;put(p,0,1013.25);put(p,1,12.3);
    assert(decode_packet(p,20,400,t));assert(t.pressure.value==1013.25f);
    put(p,0,101325);assert(!decode_packet(p,20,400,t));
    p[1]=3;p[2]=7;put(p,0,2400);put(p,1,45);put(p,2,12600);
    assert(decode_packet(p,20,500,t));assert(t.camp_distance.value==2400);
    put(p,1,360);assert(!decode_packet(p,20,600,t));
    p[3]=1;assert(!decode_packet(p,20,600,t));
    puts("telemetry: valid packets, malformed packets, units, missing values and expiry passed");
}
