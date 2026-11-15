#include "telemetry.h"
#include <cmath>
#include <cstring>
#include <ctime>
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
bool weather_fresh(const Weather &w,const PhoneClock &c,bool connected,uint64_t now){
    int64_t seconds;
    if(!w.flags || now<w.received_ms || now-w.received_ms>60000 || !clock_local_seconds(c,connected,now,seconds))return false;
    time_t epoch=seconds;struct tm local;gmtime_r(&epoch,&local);
    return w.date==uint32_t((local.tm_year+1900)*10000+(local.tm_mon+1)*100+local.tm_mday);
}
const char *weather_description(uint8_t c){
    if(c==0)return "CLEAR";
    if(c<=2)return "PARTLY CLOUDY";
    if(c==3)return "OVERCAST";
    if(c==45||c==48)return "FOG";
    if(c>=51&&c<=57)return "DRIZZLE";
    if(c>=61&&c<=67)return "RAIN";
    if(c>=71&&c<=77)return "SNOW";
    if(c>=80&&c<=82)return "SHOWERS";
    if(c==85||c==86)return "SNOW SHOWERS";
    if(c>=95&&c<=99)return "THUNDERSTORM";
    return "FORECAST";
}
bool decode_packet(const uint8_t *b, size_t size, uint64_t now, Telemetry &s) {
    if(size==20 && b[0]==1 && b[1]==6){
        if(b[2]>1 || b[3])return false;
        char name[17]={};bool ended=false;
        for(int i=0;i<16;i++){
            uint8_t ch=b[i+4];
            if(ch==0)ended=true;
            else if(ended || ch<32 || ch>126)return false;
            name[i]=char(ch);
        }
        if(b[2] && !name[0])return false;
        std::memcpy(s.city,name,sizeof(name));s.city_valid=b[2]!=0;s.city_received_ms=now;return true;
    }
    if(size==20 && b[0]==1 && b[1]==5){
        if((b[2]&0xf0) || b[3])return false;
        auto u16=[&](int i){return uint16_t(b[i]|uint16_t(b[i+1])<<8);};
        auto i16=[&](int i){uint16_t bits=u16(i);int16_t value;std::memcpy(&value,&bits,2);return value;};
        Weather w;w.flags=b[2];w.temperature=i16(4);w.high=i16(6);w.low=i16(8);
        w.sunrise=u16(10);w.sunset=u16(12);w.code=b[14];w.precipitation=b[15];
        for(int i=0;i<4;i++)w.date|=uint32_t(b[16+i])<<(8*i);
        w.received_ms=now;
        auto temp=[](int v){return v>=-1000 && v<=700;};
        if((w.flags&1)&&!temp(w.temperature))return false;
        if((w.flags&2)&&(!temp(w.high)||!temp(w.low)||w.high<w.low||(w.precipitation>100&&w.precipitation!=255)))return false;
        if(((w.flags&4)&&w.sunrise>=1440)||((w.flags&8)&&w.sunset>=1440))return false;
        if(w.flags && (w.date<20200101 || w.date>20991231))return false;
        s.weather=w;return true;
    }
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
