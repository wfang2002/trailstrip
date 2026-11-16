#include "lvgl.h"
#include "trailstrip.h"
#include "attitude.h"
#include "telemetry.h"
#include "obd.h"
#include "esp_timer.h"
#include "landscape.h"
#include "display_preferences.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cassert>
#include <cstring>
static float roll=8,pitch=12;
static bool valid=true;
static bool clock_demo=false;
static const char *city_demo=nullptr;
static bool sensor_inverted=false,screen_inverted=false;
static uint32_t pixels[640*172];
static bool pressed=false;
static int touch_x=130,touch_y=85,zero_calls=0;
static void input_read(lv_indev_drv_t*,lv_indev_data_t *data){
    const auto physical=trailstrip::landscape_point(touch_x,touch_y,sensor_inverted);
    const auto logical=trailstrip::landscape_point(physical.x,physical.y,screen_inverted);
    data->point={lv_coord_t(logical.x),lv_coord_t(logical.y)};data->state=pressed?LV_INDEV_STATE_PR:LV_INDEV_STATE_REL;
}
static void step(){lv_tick_inc(40);lv_timer_handler();}
static DisplayPreferences preferences;
void display_preferences_load(){}
DisplayPreferences display_preferences_get(){return preferences;}
bool display_preferences_set_roll_rear(bool rear){preferences.roll_rear=rear;return true;}
bool display_preferences_set_inverted(bool value){preferences.inverted=value;return true;}
void attitude_start(){}
Attitude attitude_read(){return {roll,pitch,valid,true,sensor_inverted};}
void board_set_landscape_inverted(bool value){
    if(value!=screen_inverted){screen_inverted=value;lv_obj_invalidate(lv_scr_act());}
}
ObdData obd_snapshot(){
    ObdData data;
    if(const char *mode=std::getenv("TRAILSTRIP_PREVIEW_OBD")){
        data.connected=true;data.ready=true;data.support_known=true;
        data.coolant={94,1000,true};data.rpm={1800,1000,true};data.speed={8,1000,true};data.voltage={13.8f,1000,true};
        std::snprintf(data.status,sizeof(data.status),"VEEPEAK / LIVE OBD");
        if(std::strcmp(mode,"stale")==0){data.coolant.valid=data.rpm.valid=data.speed.valid=data.voltage.valid=false;}
    }
    return data;
}
bool attitude_zero(){++zero_calls;return valid;}
Telemetry telemetry_snapshot(){
    Telemetry t;if(clock_demo)t.clock={1794697200000ULL,0,-28800,true};
    // Optional illustrative elevation (m), speed (km/h), pressure (hPa).
    if(const char *sample=std::getenv("TRAILSTRIP_PREVIEW_EXPLORE")){
        float elevation,speed,pressure;
        if(std::sscanf(sample,"%f,%f,%f",&elevation,&speed,&pressure)==3){
            t.altitude={elevation,1000,true};t.speed={speed/3.6f,1000,true};t.pressure={pressure,1000,true};
        }
    }
    if(const char *sample=std::getenv("TRAILSTRIP_PREVIEW_WEATHER")){
        t.weather={85,120,60,7*60+24,16*60+31,15,61,80,20261114,1000};
        std::snprintf(t.city,sizeof(t.city),"Vancouver");t.city_valid=true;t.city_received_ms=1000;
        if(std::strcmp(sample,"stale")==0)t.weather.date=20261113;
        if(std::strcmp(sample,"polar")==0){t.weather.flags=3;t.weather.temperature=-235;t.weather.high=-180;t.weather.low=-260;t.weather.code=71;t.weather.precipitation=255;}
        if(std::strcmp(sample,"long")==0){std::snprintf(t.city,sizeof(t.city),"North Vancouver");t.weather.code=95;}
    }
    if(city_demo){std::snprintf(t.city,sizeof(t.city),"%s",city_demo);t.city_valid=true;t.city_received_ms=preview_time_us/1000;}
    return t;
}
bool phone_connected(){return city_demo || clock_demo || std::getenv("TRAILSTRIP_PREVIEW_EXPLORE");}

static void flush(lv_disp_drv_t *d,const lv_area_t *area,lv_color_t *colors){
    for(int y=area->y1;y<=area->y2;y++)for(int x=area->x1;x<=area->x2;x++){
        if(x>=0&&x<640&&y>=0&&y<172){
            const auto physical=trailstrip::landscape_point(x,y,screen_inverted);
            pixels[physical.y*640+physical.x]=lv_color_to32(*colors);
        }
        ++colors;
    }
    lv_disp_flush_ready(d);
}
int main(int argc,char **argv){
    if(argc<2){fprintf(stderr,"preview output.ppm [roll pitch page valid roll_taps inverted saved_rear clock_sync disconnect_after_render]\n");return 1;}
    if(argc>2)roll=std::atof(argv[2]);if(argc>3)pitch=std::atof(argv[3]);
    if(argc>5)valid=std::atoi(argv[5]);
    if(argc>7)sensor_inverted=std::atoi(argv[7]);
    preferences={sensor_inverted,argc>8 && std::atoi(argv[8])!=0};
    screen_inverted=preferences.inverted;
    clock_demo=argc>9 && std::atoi(argv[9]);
    lv_init();static lv_color_t buffer[640*172];static lv_disp_draw_buf_t draw;
    lv_disp_draw_buf_init(&draw,buffer,nullptr,640*172);
    static lv_disp_drv_t display;lv_disp_drv_init(&display);
    display.hor_res=640;display.ver_res=172;display.draw_buf=&draw;display.flush_cb=flush;lv_disp_drv_register(&display);
    static lv_indev_drv_t input;lv_indev_drv_init(&input);input.type=LV_INDEV_TYPE_POINTER;input.read_cb=input_read;lv_indev_drv_register(&input);

    ui_start();
    for(int i=0;i<(argc>4?std::atoi(argv[4]):0);i++)lv_event_send(lv_obj_get_child(lv_scr_act(),0),LV_EVENT_CLICKED,nullptr);
    lv_tick_inc(60);lv_timer_handler();lv_refr_now(nullptr);
    for(int i=0;i<(argc>6?std::atoi(argv[6]):0);i++){pressed=true;step();pressed=false;step();}
    if(argc>10 && std::atoi(argv[10])){
        clock_demo=false;step();step();
        assert(std::strcmp(lv_label_get_text(lv_obj_get_child(lv_obj_get_child(lv_scr_act(),0),0)),"EXPLORE")==0);
    }
    if(const char *action=std::getenv("TRAILSTRIP_PREVIEW_ZERO")){
        auto tap=[](int x,int y){touch_x=x;touch_y=y;pressed=true;step();pressed=false;step();};
        tap(560,131);assert(zero_calls==0);
        // Modal blocks the title behind it.
        tap(40,15);assert(zero_calls==0);
        assert(std::strcmp(lv_label_get_text(lv_obj_get_child(lv_obj_get_child(lv_scr_act(),-1),0)),"SET ZERO?")==0);
        if(std::strcmp(action,"prompt")!=0){
            bool confirm=std::strcmp(action,"confirm")==0;
            tap(confirm?425:215,125);assert(zero_calls==(confirm?1:0));
            assert(std::strcmp(lv_label_get_text(lv_obj_get_child(lv_obj_get_child(lv_scr_act(),0),0)),"CAMP")==0);
            if(confirm)assert(std::strcmp(lv_label_get_text(lv_obj_get_child(lv_scr_act(),4)),valid?"ZERO SAVED":"ZERO FAILED / HOLD STILL")==0);
        }
    }
    if(const char *mode=std::getenv("TRAILSTRIP_PREVIEW_CITY")){
        auto children=lv_obj_get_child_cnt(lv_layer_top());
        city_demo="North Vancouver";step();step();
        if(const char *zero=std::getenv("TRAILSTRIP_PREVIEW_ZERO")){
            if(std::strcmp(zero,"prompt")==0){
                assert(lv_obj_get_child_cnt(lv_layer_top())==children);
                touch_x=215;touch_y=125;pressed=true;step();pressed=false;step();step();
                assert(zero_calls==0);step();step();
            }
        }
        assert(lv_obj_get_child_cnt(lv_layer_top())==children+1);
        auto panel=lv_obj_get_child(lv_obj_get_child(lv_layer_top(),-1),0);
        assert(std::strcmp(lv_label_get_text(lv_obj_get_child(panel,0)),"Entering city of")==0);
        assert(std::strcmp(lv_label_get_text(lv_obj_get_child(panel,1)),city_demo)==0);
        preview_time_us+=4999000;step();step();
        assert(lv_obj_get_child_cnt(lv_layer_top())==children+1);
        if(std::strcmp(mode,"expire")==0){
            preview_time_us+=1000;step();step();assert(lv_obj_get_child_cnt(lv_layer_top())==children);
            preview_time_us+=5000000;step();step();assert(lv_obj_get_child_cnt(lv_layer_top())==children);
        }
    }
    lv_refr_now(nullptr);
    FILE *file=fopen(argv[1],"wb");if(!file)return 2;
    fprintf(file,"P6\n640 172\n255\n");
    for(auto value:pixels){lv_color32_t c;c.full=value;unsigned char rgb[]={c.ch.red,c.ch.green,c.ch.blue};fwrite(rgb,1,3,file);}
    fclose(file);
}
