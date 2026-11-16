#include "attitude.h"
#include "attitude_math.h"
#include "attitude_filter.h"
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
static trailstrip::AttitudeFilter filter;
static trailstrip::LandscapeOrientation orientation;
// Old "zero" belongs to flat-board axes; never apply it to upright readings.
static constexpr const char *calibration_key="zero_mount_v2";
void attitude_start(){
    orientation.restore(display_preferences_get().inverted);
    i2c_master_bus_handle_t bus=nullptr;
    if(i2c_master_get_bus_handle(0,&bus)!=ESP_OK)return;
    ready=imu.begin(bus,0x6b);
    if(!ready){ESP_LOGE("imu","QMI8658 unavailable");return;}
    ready=imu.configAccelerometer(SensorQMI8658::ACC_RANGE_4G,SensorQMI8658::ACC_ODR_125Hz,SensorQMI8658::LPF_MODE_2)==0;
    ready=imu.configGyroscope(SensorQMI8658::GYR_RANGE_256DPS,SensorQMI8658::GYR_ODR_112_1Hz,SensorQMI8658::LPF_MODE_2)==0 && ready;
    ready=imu.enableAccelerometer() && ready;
    ready=imu.enableGyroscope() && ready;
    if(!ready){ESP_LOGE("imu","IMU fusion configuration failed");return;}
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
    ESP_LOGI("imu","QMI8658 ready; upright driver-facing mount; gyro-assisted gravity filter; pitch=-Z, roll unchanged");
}
Attitude attitude_read(){
    float x,y,z,gx,gy,gz;
    static bool was_calibrated=false;
    static int64_t next_diagnostic=0;
    const int64_t now=esp_timer_get_time();
    if(ready && imu.getAccelerometer(x,y,z) && imu.getGyroscope(gx,gy,gz) &&
       filter.update({x,y,z},{gx,gy,gz},last?float(now-last)/1000000.f:0.f)){
        if(filter.calibrated()!=was_calibrated){
            const auto bias=filter.bias();
            ESP_LOGI("imu","Gyro bias %s: (%.3f, %.3f, %.3f) dps",
                     filter.calibrated()?"ready":"reacquiring",bias.x,bias.y,bias.z);
            was_calibrated=filter.calibrated();
        }
        const auto gravity=filter.gravity();
        if(orientation.update(gravity.x,gravity.y,gravity.z,now/1000)){
            display_preferences_set_inverted(orientation.inverted());
            ESP_LOGI("imu","Landscape orientation: %s",orientation.inverted()?"180 degrees":"normal");
        }
        const auto tilt=trailstrip::mounted_tilt(gravity.x,gravity.y,gravity.z,orientation.inverted());
        if(last==0)ESP_LOGI("imu", "First fused sample g=(%.3f, %.3f, %.3f), gyro=(%.3f, %.3f, %.3f) dps",x,y,z,gx,gy,gz);
        r=tilt.roll;p=tilt.pitch;last=now;
        if(now<35000000 && now>=next_diagnostic){
            next_diagnostic=now+5000000;
            ESP_LOGI("imu","Tilt roll=%.2f pitch=%.2f gyro=(%.2f,%.2f,%.2f) mode=%s",
                     r,p,gx,gy,gz,filter.calibrated()?"fused":"waiting for stable bias");
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
