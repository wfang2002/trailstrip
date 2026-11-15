#include "telemetry.h"
#include <cmath>
#include <cstring>
#ifndef TRAILSTRIP_HOST_TEST
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static Telemetry current;
static bool connected=false;
#endif
bool fresh(const Reading &r, uint64_t now) {
    return r.valid && now >= r.received_ms && now-r.received_ms <= 5000;
}
bool clock_local_seconds(const PhoneClock &c, bool connected, uint64_t now, int64_t &seconds) {
    if(!connected || !c.valid || now<c.received_ms || now-c.received_ms>60000)return false;
    seconds=int64_t((c.unix_ms+now-c.received_ms)/1000)+c.utc_offset_seconds;
    return true;
}
bool decode_packet(const uint8_t *b, size_t size, uint64_t now, Telemetry &s) {
    if(size==20 && b[0]==1 && b[1]==4){
        if(b[2]!=1 || b[3]!=0 || b[16] || b[17] || b[18] || b[19])return false;
        uint64_t ms=0;uint32_t offset_bits=0;
        for(int i=0;i<8;i++)ms|=uint64_t(b[4+i])<<(8*i);
        for(int i=0;i<4;i++)offset_bits|=uint32_t(b[12+i])<<(8*i);
        int32_t offset;std::memcpy(&offset,&offset_bits,4);
        // Supported dates: 2020 through 2099; actual timezone offsets in seconds.
        if(ms<1577836800000ULL || ms>=4102444800000ULL || offset < -43200 || offset > 50400)return false;
        s.clock={ms,now,offset,true};return true;
    }
    // v1: version, kind, validity bitmask, reserved=0, four little-endian float32s.
    if(size!=20 || b[0]!=1 || b[1]<1 || b[1]>3 || b[3]!=0 || (b[2]&0xf0)) return false;
    float v[4];
    for(int i=0;i<4;i++) {
        uint32_t bits=uint32_t(b[4+4*i]) | uint32_t(b[5+4*i])<<8 |
            uint32_t(b[6+4*i])<<16 | uint32_t(b[7+4*i])<<24;
        std::memcpy(&v[i],&bits,4);
        if((b[2]&(1<<i)) && !std::isfinite(v[i])) return false;
    }
    auto valid=[&](int i){return (b[2]&(1<<i))!=0;};
    if(b[1]==1 && ((valid(1)&&v[1]<0) || (valid(2)&&(v[2]<0||v[2]>=360)) || (valid(3)&&v[3]<0))) return false;
    if(b[1]==2 && ((valid(0)&&(v[0]<100||v[0]>1200)) || (valid(2)&&v[2]<0) || valid(3))) return false;
    if(b[1]==3 && ((valid(0)&&v[0]<0) || (valid(1)&&(v[1]<0||v[1]>=360)) || (valid(2)&&v[2]<0) || valid(3))) return false;
    Reading *fields[4];
    if(b[1]==1){fields[0]=&s.altitude;fields[1]=&s.speed;fields[2]=&s.course;fields[3]=&s.location_accuracy;}
    else if(b[1]==2){fields[0]=&s.pressure;fields[1]=&s.relative_altitude;fields[2]=&s.vertical_accuracy;fields[3]=nullptr;}
    else {fields[0]=&s.camp_distance;fields[1]=&s.camp_bearing;fields[2]=&s.trip_distance;fields[3]=nullptr;}
    for(int i=0;i<4;i++) if(fields[i]) *fields[i]={v[i],now,valid(i)};
    return true;
}
#ifndef TRAILSTRIP_HOST_TEST
Telemetry telemetry_snapshot(){portENTER_CRITICAL(&mux);auto t=current;portEXIT_CRITICAL(&mux);return t;}
bool telemetry_receive(const uint8_t *b,size_t n){
    const uint64_t now=esp_timer_get_time()/1000;
    portENTER_CRITICAL(&mux);bool ok=decode_packet(b,n,now,current);portEXIT_CRITICAL(&mux);return ok;
}
void telemetry_disconnect(){portENTER_CRITICAL(&mux);current={};connected=false;portEXIT_CRITICAL(&mux);}
void telemetry_connect(bool value){portENTER_CRITICAL(&mux);connected=value;portEXIT_CRITICAL(&mux);}
bool phone_connected(){portENTER_CRITICAL(&mux);bool c=connected;portEXIT_CRITICAL(&mux);return c;}
#endif
