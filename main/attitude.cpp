#include "attitude.h"
#include "attitude_math.h"
#include "landscape.h"
#include "display_preferences.h"
#include "SensorQMI8658.hpp"
#include "driver/i2c_master.h"
#include "nvs.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <cmath>
static SensorQMI8658 imu;
static bool ready=false, calibrated=false;
static float r=0,p=0,r0=0,p0=0;
static int64_t last=0;
static trailstrip::LandscapeOrientation orientation;
// Old "zero" belongs to flat-board axes; never apply it to upright readings.
static constexpr const char *calibration_key="zero_mount_v2";
void attitude_start(){
    orientation.restore(display_preferences_get().inverted);
    i2c_master_bus_handle_t bus=nullptr;
    if(i2c_master_get_bus_handle(0,&bus)!=ESP_OK)return;
    ready=imu.begin(bus,0x6b);
    if(!ready){ESP_LOGE("imu","QMI8658 unavailable");return;}
    imu.configAccelerometer(SensorQMI8658::ACC_RANGE_4G,SensorQMI8658::ACC_ODR_125Hz,SensorQMI8658::LPF_MODE_2);
    imu.enableAccelerometer();
    nvs_handle_t nvs;
    if(nvs_open("trailstrip",NVS_READONLY,&nvs)==ESP_OK){
        float offsets[2];size_t len=sizeof(offsets);
        esp_err_t result=nvs_get_blob(nvs,calibration_key,offsets,&len);
        bool legacy=false;
        if(result==ESP_ERR_NVS_NOT_FOUND){
            len=sizeof(offsets);
            result=nvs_get_blob(nvs,"zero_upright",offsets,&len);
            legacy=true;
        }
        if(result==ESP_OK && len==sizeof(offsets) && std::isfinite(offsets[0]) && std::isfinite(offsets[1])){
            trailstrip::Tilt zero{offsets[0],offsets[1]};
            if(legacy)zero=trailstrip::legacy_upright_zero(zero);
            r0=zero.roll;p0=zero.pitch;calibrated=true;
        }nvs_close(nvs);
    }
    ESP_LOGI("imu","QMI8658 ready; upright driver-facing mount; pitch=-Z, roll unchanged");
}
Attitude attitude_read(){
    float x,y,z;
    const int64_t now=esp_timer_get_time();
    if(ready && imu.getAccelerometer(x,y,z)){
        if(orientation.update(x,y,z,now/1000)){
            display_preferences_set_inverted(orientation.inverted());
            last=0; // Do not blend readings from different sensor coordinate frames.
            ESP_LOGI("imu","Landscape orientation: %s",orientation.inverted()?"180 degrees":"normal");
        }
        const float norm=std::sqrt(x*x+y*y+z*z);
        if(norm>0.7f && norm<1.3f){
            const auto tilt=trailstrip::mounted_tilt(x,y,z,orientation.inverted());
            float nr=tilt.roll;
            float np=tilt.pitch;
            if(last==0)ESP_LOGI("imu", "First sample g=(%.3f, %.3f, %.3f), upright roll=%.1f pitch=%.1f", x,y,z,nr,np);
            float a=last?1-std::exp(-float(now-last)/250000.f):1;
            r+=a*std::remainder(nr-r,360.f);p+=a*(np-p);last=now;
        }
    }else orientation.cancel_candidate();
    return {std::remainder(r-r0,360.f),p-p0,last>0 && now-last<500000,calibrated,orientation.inverted()};
}
bool attitude_zero(){
    if(last==0 || esp_timer_get_time()-last>500000)return false;
    nvs_handle_t nvs;
    if(nvs_open("trailstrip",NVS_READWRITE,&nvs)!=ESP_OK)return false;
    float offsets[2]={r,p};
    esp_err_t e=nvs_set_blob(nvs,calibration_key,offsets,sizeof(offsets));
    if(e==ESP_OK)e=nvs_commit(nvs);
    nvs_close(nvs);
    if(e!=ESP_OK)return false;
    r0=r;p0=p;calibrated=true;return true;
}
