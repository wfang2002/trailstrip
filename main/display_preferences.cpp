#include "display_preferences.h"
#include "nvs.h"
#include "esp_log.h"

namespace {
DisplayPreferences current;
constexpr const char *key="ui_flags_v1";
bool save(DisplayPreferences next){
    if(next.inverted==current.inverted && next.roll_rear==current.roll_rear)return true;
    nvs_handle_t handle;
    esp_err_t result=nvs_open("trailstrip",NVS_READWRITE,&handle);
    if(result==ESP_OK){
        const uint8_t flags=(next.inverted?1:0)|(next.roll_rear?2:0);
        result=nvs_set_u8(handle,key,flags);
        if(result==ESP_OK)result=nvs_commit(handle);
        nvs_close(handle);
    }
    if(result!=ESP_OK){ESP_LOGE("preferences","Save failed: %s",esp_err_to_name(result));return false;}
    current=next;
    ESP_LOGI("preferences","Saved: landscape=%d, roll=%s",current.inverted?180:0,current.roll_rear?"rear":"front");
    return true;
}
}
void display_preferences_load(){
    current={};
    nvs_handle_t handle;
    esp_err_t result=nvs_open("trailstrip",NVS_READONLY,&handle);
    if(result==ESP_OK){
        uint8_t flags=0;
        result=nvs_get_u8(handle,key,&flags);
        nvs_close(handle);
        if(result==ESP_OK && flags<=3)current={(flags&1)!=0,(flags&2)!=0};
        else if(result!=ESP_ERR_NVS_NOT_FOUND)ESP_LOGW("preferences","Invalid/unreadable UI settings; using defaults");
    }else if(result!=ESP_ERR_NVS_NOT_FOUND){
        ESP_LOGW("preferences","Cannot read UI settings: %s",esp_err_to_name(result));
    }
    ESP_LOGI("preferences","Restored: landscape=%d, roll=%s",current.inverted?180:0,current.roll_rear?"rear":"front");
}
DisplayPreferences display_preferences_get(){return current;}
bool display_preferences_set_inverted(bool value){auto next=current;next.inverted=value;return save(next);}
bool display_preferences_set_roll_rear(bool value){auto next=current;next.roll_rear=value;return save(next);}
