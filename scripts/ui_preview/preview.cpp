#include "lvgl.h"
#include "trailstrip.h"
#include "attitude.h"
#include "telemetry.h"
#include "landscape.h"
#include "display_preferences.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
static float roll=8,pitch=12;
static bool valid=true;
static bool sensor_inverted=false,screen_inverted=false;
static uint32_t pixels[640*172];
static bool pressed=false;
static void input_read(lv_indev_drv_t*,lv_indev_data_t *data){
    const auto physical=trailstrip::landscape_point(130,85,sensor_inverted);
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
bool attitude_zero(){return true;}
Telemetry telemetry_snapshot(){return {};}
bool phone_connected(){return false;}
bool fresh(const Reading&,uint64_t){return false;}
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
    if(argc<2){fprintf(stderr,"preview output.ppm [roll pitch page valid roll_taps inverted saved_rear]\n");return 1;}
    if(argc>2)roll=std::atof(argv[2]);if(argc>3)pitch=std::atof(argv[3]);
    if(argc>5)valid=std::atoi(argv[5]);
    if(argc>7)sensor_inverted=std::atoi(argv[7]);
    preferences={sensor_inverted,argc>8 && std::atoi(argv[8])!=0};
    screen_inverted=preferences.inverted;
    lv_init();static lv_color_t buffer[640*172];static lv_disp_draw_buf_t draw;
    lv_disp_draw_buf_init(&draw,buffer,nullptr,640*172);
    static lv_disp_drv_t display;lv_disp_drv_init(&display);
    display.hor_res=640;display.ver_res=172;display.draw_buf=&draw;display.flush_cb=flush;lv_disp_drv_register(&display);
    static lv_indev_drv_t input;lv_indev_drv_init(&input);input.type=LV_INDEV_TYPE_POINTER;input.read_cb=input_read;lv_indev_drv_register(&input);
    ui_start();
    for(int i=0;i<(argc>4?std::atoi(argv[4]):0);i++)lv_event_send(lv_obj_get_child(lv_scr_act(),0),LV_EVENT_CLICKED,nullptr);
    lv_tick_inc(60);lv_timer_handler();lv_refr_now(nullptr);
    for(int i=0;i<(argc>6?std::atoi(argv[6]):0);i++){pressed=true;step();pressed=false;step();}
    lv_refr_now(nullptr);
    FILE *file=fopen(argv[1],"wb");if(!file)return 2;
    fprintf(file,"P6\n640 172\n255\n");
    for(auto value:pixels){lv_color32_t c;c.full=value;unsigned char rgb[]={c.ch.red,c.ch.green,c.ch.blue};fwrite(rgb,1,3,file);}
    fclose(file);
}
