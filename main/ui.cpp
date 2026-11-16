#include "trailstrip.h"
#include "attitude.h"
#include "telemetry.h"
#include "obd.h"
#include "lvgl.h"
#include "vehicle.h"
#include "camp_level.h"
#include "city_notice.h"
#include "display_preferences.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <cmath>
#include <cstdio>
#include <ctime>
#include <initializer_list>
static lv_obj_t *content,*status,*values[4],*bubble,*vehicles[2],*pointers[2],*title,*dots[5];
static int page=0;
static lv_obj_t *camp_jeep=nullptr,*camp_guidance=nullptr;
static lv_obj_t *clock_segments[6][7],*clock_forecast,*clock_temperature,*clock_sunrise,*clock_sunset;
static bool roll_rear=false;
static int64_t feedback_until=0;
static lv_obj_t *roll_view_label=nullptr;
static lv_obj_t *zero_dialog=nullptr,*city_popup=nullptr;
static trailstrip::CityNotice city_notice;
static int64_t city_popup_until=0;
static void toggle_roll_view(lv_event_t*){
    if(city_popup)return;
    auto input=lv_indev_get_act();
    if(input && lv_indev_get_gesture_dir(input)!=LV_DIR_NONE)return;
    roll_rear=!roll_rear;
    if(!display_preferences_set_roll_rear(roll_rear)){
        lv_label_set_text(status,"VIEW CHANGED / SAVE FAILED");
        feedback_until=esp_timer_get_time()+2000000;
    }
    vehicle_set_rear(vehicles[0],roll_rear);
    lv_label_set_text(roll_view_label,roll_rear?"REAR / TAP":"FRONT / TAP");
}
static const char *names[]={"TRAIL","CAMP","EXPLORE","CLOCK","VEHICLE"};
static lv_color_t amber(){return lv_color_hex(0xffb547);}
static lv_obj_t *label(lv_obj_t *parent,const char *text,int x,int y,const lv_font_t *font,lv_color_t color){
    auto o=lv_label_create(parent);lv_label_set_text(o,text);lv_obj_set_pos(o,x,y);
    lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,color,0);return o;
}
static lv_obj_t *rule(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color){
    auto o=lv_obj_create(parent);lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);return o;
}
static void city_notice_tick(){
    const auto t=telemetry_snapshot();
    const int64_t now=esp_timer_get_time();
    if(city_popup && now>=city_popup_until){
        for(auto input=lv_indev_get_next(nullptr);input;input=lv_indev_get_next(input))lv_indev_wait_release(input);
        lv_obj_del(city_popup);city_popup=nullptr;
        ESP_LOGI("city","Popup closed after five seconds");
    }
    const uint64_t ms=now/1000;
    const bool fresh_city=phone_connected() && t.city_valid && ms>=t.city_received_ms && ms-t.city_received_ms<=60000;
    city_notice.observe(t.city,fresh_city);
    // Calibration confirmation takes priority; keep only the latest fresh city.
    if(zero_dialog)return;
    char name[17];if(!city_notice.take(name))return;
    if(city_popup)lv_obj_del(city_popup);
    city_popup=lv_obj_create(lv_layer_top());lv_obj_remove_style_all(city_popup);
    lv_obj_set_pos(city_popup,0,0);lv_obj_set_size(city_popup,640,172);
    lv_obj_set_style_bg_color(city_popup,lv_color_black(),0);lv_obj_set_style_bg_opa(city_popup,LV_OPA_90,0);
    lv_obj_clear_flag(city_popup,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(city_popup,LV_OBJ_FLAG_CLICKABLE);
    auto panel=lv_obj_create(city_popup);lv_obj_remove_style_all(panel);
    lv_obj_set_pos(panel,12,12);lv_obj_set_size(panel,616,148);
    lv_obj_set_style_bg_color(panel,lv_color_hex(0x080b0d),0);lv_obj_set_style_bg_opa(panel,LV_OPA_COVER,0);
    lv_obj_set_style_border_color(panel,amber(),0);lv_obj_set_style_border_width(panel,2,0);lv_obj_set_style_radius(panel,8,0);
    lv_obj_clear_flag(panel,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_GESTURE_BUBBLE);
    auto heading=label(panel,"Current location",0,0,&lv_font_montserrat_20,amber());
    lv_obj_align(heading,LV_ALIGN_TOP_MID,0,22);
    lv_point_t size;lv_txt_get_size(&size,name,&lv_font_montserrat_48,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    auto city=label(panel,name,0,0,size.x>580?&lv_font_montserrat_32:&lv_font_montserrat_48,lv_color_white());
    lv_obj_align(city,LV_ALIGN_CENTER,0,18);
    lv_obj_update_layout(city_popup);
    lv_obj_invalidate(city_popup);
    city_popup_until=esp_timer_get_time()+5000000;
    ESP_LOGI("city","Showing city popup for 5 seconds: %s",name);
    // Do not let a touch already held during popup creation act on the page below.
    for(auto input=lv_indev_get_next(nullptr);input;input=lv_indev_get_next(input))lv_indev_wait_release(input);
}
static lv_obj_t *scale(lv_obj_t *parent,int x){
    rule(parent,x+36,9,1,96,0x8a8d8b);
    for(int i=0;i<5;i++){
        char text[16];snprintf(text,sizeof(text),"%d°",30-15*i);
        auto t=label(parent,text,x,3+24*i,&lv_font_montserrat_12,lv_color_hex(0xb4b5af));
        lv_obj_set_width(t,31);lv_obj_set_style_text_align(t,LV_TEXT_ALIGN_RIGHT,0);
        rule(parent,x+32,9+24*i,10,1,0x8a8d8b);
    }
    return label(parent,"<",x+42,48,&lv_font_montserrat_16,amber());
}
static bool clock_ready(){
    int64_t seconds;
    return clock_local_seconds(telemetry_snapshot().clock,phone_connected(),esp_timer_get_time()/1000,seconds);
}
static int next_page(int current,int direction){
    int next=(current+direction+5)%5;
    if(next==3 && !clock_ready())next=(next+direction+5)%5;
    return next;
}
static void render_page();
static void select(lv_event_t *e){if(zero_dialog || city_popup)return;page=(int)(intptr_t)lv_event_get_user_data(e);if(page==3 && !clock_ready())page=2;render_page();}
static void close_zero_dialog(){
    if(zero_dialog){auto dialog=zero_dialog;zero_dialog=nullptr;lv_obj_del(dialog);}
}
static void zero_choice(lv_event_t *event){
    const bool confirm=(intptr_t)lv_event_get_user_data(event)!=0;
    close_zero_dialog();
    if(confirm){
        lv_label_set_text(status,attitude_zero()?"ZERO SAVED":"ZERO FAILED / HOLD STILL");
        feedback_until=esp_timer_get_time()+2000000;
    }
}
static void zero(lv_event_t*){
    if(zero_dialog || city_popup)return;
    zero_dialog=lv_obj_create(lv_scr_act());lv_obj_remove_style_all(zero_dialog);
    lv_obj_set_size(zero_dialog,640,172);lv_obj_set_pos(zero_dialog,0,0);
    lv_obj_set_style_bg_color(zero_dialog,lv_color_hex(0x080b0d),0);
    lv_obj_set_style_bg_opa(zero_dialog,LV_OPA_COVER,0);
    lv_obj_clear_flag(zero_dialog,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(zero_dialog,LV_OBJ_FLAG_CLICKABLE);
    auto heading=label(zero_dialog,"SET ZERO?",0,15,&lv_font_montserrat_20,amber());
    lv_obj_align(heading,LV_ALIGN_TOP_MID,0,15);
    auto message=label(zero_dialog,"Park level and hold still. Replace saved calibration?",0,51,&lv_font_montserrat_16,lv_color_white());
    lv_obj_align(message,LV_ALIGN_TOP_MID,0,51);
    for(int i=0;i<2;i++){
        auto button=lv_btn_create(zero_dialog);lv_obj_set_pos(button,130+i*210,99);lv_obj_set_size(button,170,52);
        lv_obj_set_style_bg_color(button,i?amber():lv_color_hex(0x303633),0);
        auto text=label(button,i?"CONFIRM":"CANCEL",0,0,&lv_font_montserrat_16,i?lv_color_black():lv_color_white());lv_obj_center(text);
        lv_obj_add_event_cb(button,zero_choice,LV_EVENT_CLICKED,(void*)(intptr_t)i);
    }
}
static void gesture(lv_event_t*){
    if(zero_dialog || city_popup)return;
    auto d=lv_indev_get_gesture_dir(lv_indev_get_act());
    if(d==LV_DIR_LEFT)page=next_page(page,1);
    else if(d==LV_DIR_RIGHT)page=next_page(page,-1);
    else return;
    render_page();
}
static void render_page(){
    close_zero_dialog();
    lv_obj_clean(content);roll_view_label=nullptr;bubble=nullptr;vehicles[0]=vehicles[1]=nullptr;
    pointers[0]=pointers[1]=nullptr;camp_jeep=nullptr;camp_guidance=nullptr;
    lv_label_set_text(title,names[page]);
    for(int i=0;i<5;i++)lv_obj_set_style_bg_color(dots[i],i==page?amber():lv_color_hex(0x606360),0);
    for(auto &v:values)v=nullptr;
    if(page==0){
        rule(content,319,4,1,108,0x555a58);
        pointers[0]=scale(content,0);pointers[1]=scale(content,322);
        vehicles[0]=vehicle_create(content,false,54,0,153,116);
        vehicle_set_rear(vehicles[0],roll_rear);
        lv_obj_add_flag(vehicles[0],LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(vehicles[0],toggle_roll_view,LV_EVENT_SHORT_CLICKED,nullptr);
        vehicles[1]=vehicle_create(content,true,375,0,159,116);
        label(content,"ROLL",219,17,&lv_font_montserrat_16,lv_color_hex(0xb4b5af));
        label(content,"PITCH",540,17,&lv_font_montserrat_16,lv_color_hex(0xb4b5af));
        values[0]=label(content,"--",214,43,&lv_font_montserrat_48,lv_color_white());
        values[1]=label(content,"--",535,43,&lv_font_montserrat_48,lv_color_white());
        roll_view_label=label(content,roll_rear?"REAR / TAP":"FRONT / TAP",215,100,&lv_font_montserrat_12,amber());
    }else if(page==1){
        // Concentric target and crosshairs; bubble retains the established axis directions.
        for(int diameter: {100,50}){
            auto ring=lv_obj_create(content);lv_obj_remove_style_all(ring);
            lv_obj_set_size(ring,diameter,diameter);lv_obj_set_pos(ring,64-diameter/2,58-diameter/2);
            lv_obj_set_style_radius(ring,LV_RADIUS_CIRCLE,0);
            lv_obj_set_style_border_width(ring,diameter==100?2:1,0);
            lv_obj_set_style_border_color(ring,lv_color_hex(0xb4b5af),0);
            lv_obj_clear_flag(ring,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
        }
        rule(content,14,58,100,1,0x606360);rule(content,64,8,1,100,0x606360);
        bubble=rule(content,57,51,14,14,0xffb547);lv_obj_set_style_radius(bubble,LV_RADIUS_CIRCLE,0);
        rule(content,125,8,1,100,0x555a58);rule(content,302,8,1,100,0x555a58);rule(content,479,8,1,100,0x555a58);
        values[0]=label(content,"--",136,26,&lv_font_montserrat_48,lv_color_white());
        values[1]=label(content,"--",313,26,&lv_font_montserrat_48,lv_color_white());
        label(content,"ROLL",180,84,&lv_font_montserrat_16,amber());
        label(content,"PITCH",353,84,&lv_font_montserrat_16,amber());
        auto front=label(content,"FRONT >",486,1,&lv_font_montserrat_12,lv_color_hex(0x929890));
        lv_obj_set_width(front,148);lv_obj_set_style_text_align(front,LV_TEXT_ALIGN_CENTER,0);
        camp_jeep=vehicle_top_create(content,495,13,130,53);
        camp_guidance=label(content,"HOLD STILL",486,68,&lv_font_montserrat_12,amber());
        lv_obj_set_width(camp_guidance,148);lv_obj_set_style_text_align(camp_guidance,LV_TEXT_ALIGN_CENTER,0);
        auto btn=lv_btn_create(content);lv_obj_remove_style_all(btn);lv_obj_set_pos(btn,505,87);lv_obj_set_size(btn,110,28);
        lv_obj_set_style_radius(btn,5,0);lv_obj_set_style_border_width(btn,1,0);lv_obj_set_style_border_color(btn,amber(),0);
        lv_obj_set_style_bg_color(btn,lv_color_hex(0x303633),LV_STATE_PRESSED);lv_obj_set_style_bg_opa(btn,LV_OPA_COVER,LV_STATE_PRESSED);
        auto text=label(btn,"SET ZERO",0,0,&lv_font_montserrat_12,amber());lv_obj_center(text);lv_obj_add_event_cb(btn,zero,LV_EVENT_CLICKED,nullptr);
    }else if(page==2){
        const char* titles[]={"ELEVATION / m","SPEED / km/h","PRESSURE / hPa"};
        for(int i=0;i<3;i++){label(content,titles[i],20+i*210,5,&lv_font_montserrat_16,amber());values[i]=label(content,"--",20+i*210,32,&lv_font_montserrat_48,lv_color_white());lv_obj_set_width(values[i],190);lv_label_set_long_mode(values[i],LV_LABEL_LONG_CLIP);}
    }else if(page==4){
        const char *headings[]={"COOLANT / °C","ENGINE / RPM","SPEED / km/h","OBD VOLTS"};
        for(int i=0;i<4;i++){
            const int x=10+i*158;
            if(i)rule(content,x-6,6,1,100,0x555a58);
            label(content,headings[i],x,8,&lv_font_montserrat_12,amber());
            values[i]=label(content,"--",x,38,&lv_font_montserrat_48,lv_color_white());
            lv_obj_set_width(values[i],148);lv_label_set_long_mode(values[i],LV_LABEL_LONG_CLIP);
        }
    }else{
        clock_temperature=label(content,"--°C",14,0,&lv_font_montserrat_32,lv_color_white());
        clock_sunrise=label(content,"RISE  --:--",14,54,&lv_font_montserrat_16,amber());
        clock_sunset=label(content,"SET   --:--",14,81,&lv_font_montserrat_16,amber());
        rule(content,177,3,1,106,0x555a58);
        clock_forecast=label(content,"WEATHER UNAVAILABLE",199,0,&lv_font_montserrat_12,amber());
        lv_obj_set_width(clock_forecast,425);lv_label_set_long_mode(clock_forecast,LV_LABEL_LONG_CLIP);
        // Vector digits stay crisp at full size without a large transformed text layer.
        const int rects[7][4]={{8,0,44,7},{52,8,7,39},{52,53,7,39},{8,93,44,7},{1,53,7,39},{1,8,7,39},{8,46,44,7}};
        for(int d=0;d<6;d++){
            int x=204+d*62+(d/2)*20;
            for(int segment=0;segment<7;segment++){
                const auto &r=rects[segment];
                clock_segments[d][segment]=rule(content,x+r[0]*4/5,28+r[1]*4/5,r[2]*4/5,r[3]*4/5,0xf5f4ed);
                lv_obj_set_style_radius(clock_segments[d][segment],2,0);
            }
        }
        for(int x: {320,464}){rule(content,x,51,6,6,0xffb547);rule(content,x,83,6,6,0xffb547);}
    }
}
static void tick(lv_timer_t*){
    city_notice_tick();
    const bool ready=clock_ready();
    auto clock_hit=lv_obj_get_parent(dots[3]);
    if(ready)lv_obj_clear_flag(clock_hit,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(clock_hit,LV_OBJ_FLAG_HIDDEN);
    if(page==3 && !ready){page=2;render_page();}
    auto a=attitude_read();
    board_set_landscape_inverted(a.inverted);
    if(page<2){
        if(a.valid){
            float angles[2]={a.roll,a.pitch};
            for(int i=0;i<2;i++){
                char text[32];snprintf(text,sizeof(text),page==0?"%.0f°":"%+.1f°",angles[i]);
                lv_point_t size;lv_txt_get_size(&size,text,&lv_font_montserrat_48,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
                const int available_width=page==0?101:160;
                lv_obj_set_style_text_font(values[i],size.x>available_width?&lv_font_montserrat_32:&lv_font_montserrat_48,0);
                lv_label_set_text(values[i],text);
            }
        }
        else{lv_label_set_text(values[0],"--");lv_label_set_text(values[1],"--");}
        if(esp_timer_get_time()>feedback_until)lv_label_set_text(status,!a.valid?"IMU UNAVAILABLE / MOTION":a.calibrated?(page==0?"VEHICLE ATTITUDE":"LEVEL ASSIST"):(page==0?"VEHICLE ATTITUDE  /  ZERO IN CAMP":"LEVEL ASSIST  /  SET ZERO ON LEVEL SURFACE"));
        // Screen Y increases downward: positive pitch moves the level bubble upward.
        if(bubble){
            float dx=a.valid?a.roll*3:0,dy=a.valid?-a.pitch*3:0;
            float distance=std::hypot(dx,dy);if(distance>40){dx*=40/distance;dy*=40/distance;}
            lv_obj_set_pos(bubble,57+(int)dx,51+(int)dy);lv_obj_set_style_opa(bubble,a.valid?LV_OPA_COVER:LV_OPA_30,0);
            const auto guide=trailstrip::camp_level(a.roll,a.pitch,a.valid);
            vehicle_top_set_low(camp_jeep,guide.low_corners,guide.valid);lv_label_set_text(camp_guidance,guide.label);
        }
        if(vehicles[0]){
            for(int i=0;i<2;i++){
                float angle=i?a.pitch:a.roll;
                vehicle_set_attitude(vehicles[i],angle,a.valid);
                if(a.valid){lv_obj_clear_flag(pointers[i],LV_OBJ_FLAG_HIDDEN);lv_obj_set_y(pointers[i],49-(int)(fmaxf(-30,fminf(30,angle))*1.6f));}
                else lv_obj_add_flag(pointers[i],LV_OBJ_FLAG_HIDDEN);
            }
        }
    }else if(page==2){
        auto t=telemetry_snapshot();uint64_t now=esp_timer_get_time()/1000;
        Reading fields[]={t.altitude,t.speed,t.pressure};
        for(int i=0;i<3;i++){
            char text[32]="--";
            if(fresh(fields[i],now))snprintf(text,sizeof(text),i==0?"%.0f":"%.1f",fields[i].value*(i==1?3.6:1));
            lv_point_t size;lv_txt_get_size(&size,text,&lv_font_montserrat_48,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
            lv_obj_set_style_text_font(values[i],size.x>190?&lv_font_montserrat_32:&lv_font_montserrat_48,0);
            lv_label_set_text(values[i],text);
        }
        lv_label_set_text(status,!phone_connected()?"WAITING FOR IPHONE / BLUETOOTH":fresh(t.altitude,now)?"PHONE LOCATION / LIVE":"PHONE CONNECTED / LOCATION STALE");
        if(fresh(t.camp_distance,now) && fresh(t.camp_bearing,now) && fresh(t.trip_distance,now)){char text[100];snprintf(text,sizeof(text),"CAMP %.2f km / %.0f° TRUE   |   TRIP %.2f km   |   PHONE",t.camp_distance.value/1000,t.camp_bearing.value,t.trip_distance.value/1000);lv_label_set_text(status,text);}
    }else if(page==4){
        const auto obd=obd_snapshot();const uint64_t now=esp_timer_get_time()/1000;
        const Reading readings[]={obd.coolant,obd.rpm,obd.speed,obd.voltage};
        for(int i=0;i<4;i++){
            char text[24]="--";
            if(obd.connected && fresh(readings[i],now))snprintf(text,sizeof(text),i==3?"%.1f":"%.0f",readings[i].value);
            lv_point_t size;lv_txt_get_size(&size,text,&lv_font_montserrat_48,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
            lv_obj_set_style_text_font(values[i],size.x>148?&lv_font_montserrat_32:&lv_font_montserrat_48,0);
            lv_label_set_text(values[i],text);
        }
        const bool ecu=fresh(obd.coolant,now)||fresh(obd.rpm,now)||fresh(obd.speed,now);
        lv_label_set_text(status,obd.ready && obd.support_known && !ecu?"VEEPEAK / ECU DATA UNAVAILABLE":obd.status);
    }else{
        int64_t seconds;
        if(clock_local_seconds(telemetry_snapshot().clock,phone_connected(),esp_timer_get_time()/1000,seconds)){
            time_t epoch=seconds;struct tm local;gmtime_r(&epoch,&local);
            char text[32];strftime(text,sizeof(text),"%H:%M:%S",&local);
            const uint8_t masks[]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
            for(int d=0;d<6;d++){
                int digit=text[d+d/2]-'0';
                for(int segment=0;segment<7;segment++){
                    if(masks[digit]&(1<<segment))lv_obj_clear_flag(clock_segments[d][segment],LV_OBJ_FLAG_HIDDEN);
                    else lv_obj_add_flag(clock_segments[d][segment],LV_OBJ_FLAG_HIDDEN);
                }
            }
            strftime(text,sizeof(text),"%a %d %b %Y",&local);
            const auto t=telemetry_snapshot();const uint64_t now=esp_timer_get_time()/1000;
            bool city_ok=t.city_valid && now>=t.city_received_ms && now-t.city_received_ms<=60000;
            char footer[80];snprintf(footer,sizeof(footer),"%s  /  %s",text,city_ok?t.city:"LOCATION UNAVAILABLE");lv_label_set_text(status,footer);
            const auto &w=t.weather;bool weather_ok=weather_fresh(w,t.clock,phone_connected(),now);
            char value[48];
            if(weather_ok && (w.flags&1))snprintf(value,sizeof(value),"%.0f°C",w.temperature/10.f);else snprintf(value,sizeof(value),"--°C");
            lv_label_set_text(clock_temperature,value);
            if(weather_ok && (w.flags&4))snprintf(value,sizeof(value),"RISE  %02u:%02u",w.sunrise/60,w.sunrise%60);else snprintf(value,sizeof(value),"RISE  --:--");
            lv_label_set_text(clock_sunrise,value);
            if(weather_ok && (w.flags&8))snprintf(value,sizeof(value),"SET   %02u:%02u",w.sunset/60,w.sunset%60);else snprintf(value,sizeof(value),"SET   --:--");
            lv_label_set_text(clock_sunset,value);
            char forecast[100];
            if(weather_ok && (w.flags&2)){
                char rain[20]="";if(w.precipitation<=100)snprintf(rain,sizeof(rain),"  %u%%",w.precipitation);
                snprintf(forecast,sizeof(forecast),"TODAY %s  H%.0f° L%.0f°%s",weather_description(w.code),w.high/10.f,w.low/10.f,rain);
            }else snprintf(forecast,sizeof(forecast),"WEATHER UNAVAILABLE / STALE");
            lv_label_set_text(clock_forecast,forecast);
        }
    }
}
void ui_start(){
    roll_rear=display_preferences_get().roll_rear;
    auto root=lv_scr_act();lv_obj_set_style_bg_color(root,lv_color_hex(0x080b0d),0);lv_obj_clear_flag(root,LV_OBJ_FLAG_SCROLLABLE);
    auto title_button=lv_btn_create(root);lv_obj_remove_style_all(title_button);
    lv_obj_set_pos(title_button,8,0);lv_obj_set_size(title_button,130,29);
    lv_obj_add_event_cb(title_button,[](lv_event_t*){if(zero_dialog || city_popup)return;page=next_page(page,1);render_page();},LV_EVENT_CLICKED,nullptr);
    title=label(title_button,"TRAIL",0,4,&lv_font_montserrat_20,amber());
    rule(root,140,16,486,1,0x9a9e98);
    content=lv_obj_create(root);lv_obj_remove_style_all(content);lv_obj_set_pos(content,0,30);lv_obj_set_size(content,640,116);lv_obj_clear_flag(content,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(root,gesture,LV_EVENT_GESTURE,nullptr);
    rule(root,10,149,620,1,0x9a9e98);
    status=label(root,"STARTING",10,155,&lv_font_montserrat_12,lv_color_hex(0xb4b5af));
    lv_obj_set_width(status,450);lv_label_set_long_mode(status,LV_LABEL_LONG_CLIP);
    for(int i=0;i<5;i++){
        auto hit=lv_btn_create(root);lv_obj_remove_style_all(hit);lv_obj_set_pos(hit,462+i*35,146);lv_obj_set_size(hit,35,26);
        dots[i]=rule(hit,12,10,8,8,0x606360);lv_obj_set_style_radius(dots[i],LV_RADIUS_CIRCLE,0);
        lv_obj_add_event_cb(hit,select,LV_EVENT_CLICKED,(void*)(intptr_t)i);
    }
    render_page();tick(nullptr);lv_timer_create(tick,50,nullptr);
}
void trailstrip_start(){attitude_start();ui_start();ESP_LOGI("trailstrip","UI ready 640x172; animated Jeep views; BLE telemetry available");}
