#include "telemetry.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
static void put(uint8_t *p,uint64_t v,int n){for(int i=0;i<n;i++)p[i]=(v>>(8*i))&255;}
int main(int argc,char **argv){
 Telemetry t;uint8_t p[20]={1,5,15,0};
 put(p+4,85,2);put(p+6,120,2);put(p+8,60,2);put(p+10,444,2);put(p+12,991,2);p[14]=61;p[15]=80;put(p+16,20261114,4);
 assert(decode_packet(p,20,1000,t));assert(t.weather.temperature==85 && t.weather.sunset==991);
 PhoneClock clock{1794697200000ULL,1000,-28800,true};
 assert(weather_fresh(t.weather,clock,true,1000));assert(!weather_fresh(t.weather,clock,false,1000));
 assert(weather_fresh(t.weather,clock,true,61000));assert(!weather_fresh(t.weather,clock,true,61001));
 assert(!weather_fresh(t.weather,clock,true,999));
 clock.utc_offset_seconds=50400;assert(!weather_fresh(t.weather,clock,true,1000));clock.utc_offset_seconds=-28800;
 for(int index: {0,2,3,15}){uint8_t old=p[index];p[index]=254;assert(!decode_packet(p,20,2000,t));p[index]=old;}
 put(p+10,1440,2);assert(!decode_packet(p,20,2000,t));put(p+10,444,2);
 put(p+4,701,2);assert(!decode_packet(p,20,2000,t));put(p+4,uint16_t(-235),2);assert(decode_packet(p,20,2000,t));assert(t.weather.temperature==-235);
 p[2]=3;put(p+10,65535,2);put(p+12,65535,2);assert(decode_packet(p,20,2000,t));
 p[2]=0;assert(decode_packet(p,20,2000,t));assert(!weather_fresh(t.weather,clock,true,2000));
 uint8_t city[20]={1,6,1,0};std::memcpy(city+4,"North Vancouver",15);
 assert(decode_packet(city,20,3000,t));assert(std::strcmp(t.city,"North Vancouver")==0);
 city[4]=0xff;assert(!decode_packet(city,20,3000,t));assert(std::strcmp(t.city,"North Vancouver")==0);
 city[4]=0;assert(!decode_packet(city,20,3000,t));
 std::memset(city+4,'A',16);assert(decode_packet(city,20,3000,t));assert(std::strlen(t.city)==16);
 std::memset(city+4,0,16);city[2]=0;assert(decode_packet(city,20,3000,t));assert(!t.city_valid);
 if(argc>1){FILE *file=fopen(argv[1],"rb");assert(file);assert(fread(p,1,20,file)==20);fclose(file);assert(decode_packet(p,20,4000,t));assert(t.weather.temperature==85 && t.weather.date==20261114 && t.weather.sunrise==444 && t.weather.flags==15);}
 puts("weather C++: Swift packet interop, ranges, atomic rejection, freshness, date rollover, city bounds and invalidation passed");
}
