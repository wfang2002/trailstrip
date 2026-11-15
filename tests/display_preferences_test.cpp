#include "display_preferences.h"
#include "landscape.h"
#include "nvs.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <optional>
static std::optional<uint8_t> disk,pending;
static unsigned commits=0;
static bool fail_commit=false,fail_open=false;
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *out){
    assert(std::strcmp(name,"trailstrip")==0);
    if(fail_open)return ESP_FAIL;
    if(mode==NVS_READONLY && !disk)return ESP_ERR_NVS_NOT_FOUND;
    *out=1;return ESP_OK;
}
esp_err_t nvs_get_u8(nvs_handle_t,const char *key,uint8_t *value){
    assert(std::strcmp(key,"ui_flags_v1")==0);
    if(!disk)return ESP_ERR_NVS_NOT_FOUND;
    *value=*disk;return ESP_OK;
}
esp_err_t nvs_set_u8(nvs_handle_t,const char *key,uint8_t value){
    assert(std::strcmp(key,"ui_flags_v1")==0);pending=value;return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t){
    if(fail_commit)return ESP_FAIL;
    disk=pending;commits++;return ESP_OK;
}
void nvs_close(nvs_handle_t){pending.reset();}
const char *esp_err_to_name(esp_err_t){return "test error";}
static void expect(bool inverted,bool rear){auto p=display_preferences_get();assert(p.inverted==inverted && p.roll_rear==rear);}
int main(){
    display_preferences_load();expect(false,false);assert(commits==0);
    // Store both choices, then reload as on a fresh boot. Updating one retains the other.
    assert(display_preferences_set_roll_rear(true));
    assert(display_preferences_set_inverted(true));
    display_preferences_load();expect(true,true);
    assert(display_preferences_set_roll_rear(false));display_preferences_load();expect(true,false);
    assert(display_preferences_set_inverted(false));display_preferences_load();expect(false,false);
    assert(display_preferences_set_roll_rear(true));display_preferences_load();expect(false,true);
    auto count=commits;
    for(int i=0;i<100;i++){assert(display_preferences_set_roll_rear(true));assert(display_preferences_set_inverted(false));}
    assert(commits==count); // Stable readings do not repeatedly write flash.
    fail_commit=true;assert(!display_preferences_set_inverted(true));expect(false,true);
    fail_commit=false;display_preferences_load();expect(false,true);
    assert(display_preferences_set_inverted(true));display_preferences_load();expect(true,true);
    fail_open=true;assert(!display_preferences_set_roll_rear(false));expect(true,true);
    fail_open=false;
    disk=0xff;display_preferences_load();expect(false,false);
    // Restore orientation before sensor samples: ambiguous/flat startup retains saved landscape.
    trailstrip::LandscapeOrientation o;o.restore(true);
    for(uint64_t t=0;t<1000;t+=50){assert(!o.update(0,0,1,t));assert(o.inverted());}
    for(uint64_t t=1000;t<1600;t+=50)assert(!o.update(0,-1,0,t));
    assert(o.update(0,-1,0,1600));assert(!o.inverted());
    puts("preferences: all combinations survive reload; independent settings, no-op writes, failures, invalid data and saved orientation startup passed");
}
