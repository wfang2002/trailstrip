#include "telemetry.h"
#include <cassert>
#include <cstdio>
#include <ctime>
#include <initializer_list>
static void put(uint8_t *p,uint64_t v,int n){for(int i=0;i<n;i++)p[i]=(v>>(8*i))&255;}
int main(){
 Telemetry t;int64_t seconds=0;uint8_t p[20]={1,4,1,0};
 assert(!clock_local_seconds(t.clock,true,0,seconds));
 put(p+4,1794697200123ULL,8);put(p+12,uint32_t(-28800),4);
 assert(decode_packet(p,20,1000,t));assert(t.clock.utc_offset_seconds==-28800);
 assert(clock_local_seconds(t.clock,true,1877,seconds));assert(seconds==1794668401);
 assert(!clock_local_seconds(t.clock,false,1877,seconds));
 assert(!clock_local_seconds(t.clock,true,999,seconds));
 assert(clock_local_seconds(t.clock,true,61000,seconds));
 assert(!clock_local_seconds(t.clock,true,61001,seconds));
 auto saved=t.clock;
 for(int i: {0,2,3,16,17,18,19}){auto old=p[i];p[i]=255;assert(!decode_packet(p,20,2000,t));p[i]=old;assert(t.clock.unix_ms==saved.unix_ms);}
 assert(!decode_packet(p,19,2000,t));
 put(p+4,0,8);assert(!decode_packet(p,20,2000,t));
 put(p+4,4102444800000ULL,8);assert(!decode_packet(p,20,2000,t));
 put(p+4,1794697200123ULL,8);put(p+12,50401,4);assert(!decode_packet(p,20,2000,t));
 put(p+12,uint32_t(-43201),4);assert(!decode_packet(p,20,2000,t));
 for(int offset: {-43200,0,20700,50400}){
  put(p+12,uint32_t(offset),4);assert(decode_packet(p,20,3000,t));
  assert(clock_local_seconds(t.clock,true,3877,seconds));assert(seconds==1794697201LL+offset);
 }
 // Midnight and date rollover in phone-local time.
 put(p+4,1794700799500ULL,8);put(p+12,0,4);assert(decode_packet(p,20,4000,t));
 assert(clock_local_seconds(t.clock,true,4500,seconds));assert(seconds==1794700800);
 t={};assert(!clock_local_seconds(t.clock,true,4500,seconds));
 puts("clock: integer precision, timezone offsets, ticking, expiry, disconnect and malformed packets passed");
}
