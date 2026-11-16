#include "obd.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_npl.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include <cstring>
#include <cstdio>
#include <cctype>
namespace {
enum class Phase {idle,scanning,pending,connecting,service,characteristics,descriptors,subscribing,ready,closing};
Phase phase=Phase::idle;
ble_npl_callout timer;
uint16_t handle=BLE_HS_CONN_HANDLE_NONE,svc_start=0,svc_end=0,notify_handle=0,notify_end=0,write_handle=0,cccd=0;
uint8_t write_properties=0;
ble_addr_t candidate;
uint64_t deadline=0,retry_at=0;
uintptr_t generation=1;
bool write_pending=false;
ObdSession session;
ObdData published;
portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
uint64_t now_ms(){return esp_timer_get_time()/1000;}
void publish(const char *status=nullptr){
    ObdData next=phase==Phase::ready?session.data():ObdData{};
    next.connected=handle!=BLE_HS_CONN_HANDLE_NONE;
    if(status)snprintf(next.status,sizeof(next.status),"%s",status);
    portENTER_CRITICAL(&mux);published=next;portEXIT_CRITICAL(&mux);
}
bool current(uint16_t conn,void *arg){return conn==handle && reinterpret_cast<uintptr_t>(arg)==generation && phase!=Phase::closing;}
void *token(){return reinterpret_cast<void*>(generation);}
void schedule(){ble_npl_callout_reset(&timer,ble_npl_time_ms_to_ticks32(100));}
void clear(){
    ++generation;phase=Phase::idle;handle=BLE_HS_CONN_HANDLE_NONE;write_pending=false;
    svc_start=svc_end=notify_handle=notify_end=write_handle=cccd=0;write_properties=0;
    session.reset();retry_at=now_ms()+5000;
}
void fail(const char *reason,int error=0){
    ESP_LOGW("obd","%s error=%d",reason,error);
    if(handle!=BLE_HS_CONN_HANDLE_NONE){
        phase=Phase::closing;deadline=now_ms()+10000;
        int rc=ble_gap_terminate(handle,BLE_ERR_REM_USER_CONN_TERM);
        if(rc==BLE_HS_ENOTCONN)clear();
    }else{
        if(phase==Phase::connecting)ble_gap_conn_cancel();
        if(phase==Phase::scanning)ble_gap_disc_cancel();
        clear();
    }
    publish(reason);
}
int gap(ble_gap_event *,void*);
int subscription(uint16_t conn,const ble_gatt_error *error,ble_gatt_attr*,void *arg){
    if(!current(conn,arg) || phase!=Phase::subscribing)return 0;
    if(error->status){fail("OBD SUBSCRIBE FAILED",error->status);return 0;}
    phase=Phase::ready;session.reset();write_pending=false;publish();
    ESP_LOGI("obd","Veepeak UART ready notify=%u write=%u",notify_handle,write_handle);return 0;
}
int descriptor(uint16_t conn,const ble_gatt_error *error,uint16_t,const ble_gatt_dsc *d,void *arg){
    if(!current(conn,arg) || phase!=Phase::descriptors)return 0;
    if(!error->status){if(ble_uuid_u16(&d->uuid.u)==0x2902)cccd=d->handle;return 0;}
    if(error->status!=BLE_HS_EDONE || !cccd){fail("OBD NOTIFY DESCRIPTOR MISSING",error->status);return 0;}
    uint8_t enable[]={1,0};phase=Phase::subscribing;deadline=now_ms()+10000;
    int rc=ble_gattc_write_flat(conn,cccd,enable,2,subscription,token());if(rc)fail("OBD SUBSCRIBE FAILED",rc);return 0;
}
int characteristic(uint16_t conn,const ble_gatt_error *error,const ble_gatt_chr *chr,void *arg){
    if(!current(conn,arg) || phase!=Phase::characteristics)return 0;
    if(!error->status){
        // Discovery is in ascending handle order; bound CCCD lookup to FFF1 only.
        if(notify_handle && !notify_end && chr->def_handle>notify_handle)notify_end=chr->def_handle-1;
        uint16_t uuid=ble_uuid_u16(&chr->uuid.u);
        if(uuid==0xfff1 && (chr->properties & BLE_GATT_CHR_PROP_NOTIFY))notify_handle=chr->val_handle;
        if(uuid==0xfff2 && (chr->properties & (BLE_GATT_CHR_PROP_WRITE|BLE_GATT_CHR_PROP_WRITE_NO_RSP))){write_handle=chr->val_handle;write_properties=chr->properties;}
        return 0;
    }
    if(error->status!=BLE_HS_EDONE || !notify_handle || !write_handle){fail("UNSUPPORTED VEEPEAK PROFILE",error->status);return 0;}
    if(!notify_end)notify_end=svc_end;
    phase=Phase::descriptors;deadline=now_ms()+10000;
    int rc=ble_gattc_disc_all_dscs(conn,notify_handle,notify_end,descriptor,token());if(rc)fail("OBD DESCRIPTOR LOOKUP FAILED",rc);return 0;
}
int service(uint16_t conn,const ble_gatt_error *error,const ble_gatt_svc *svc,void *arg){
    if(!current(conn,arg) || phase!=Phase::service)return 0;
    if(!error->status){svc_start=svc->start_handle;svc_end=svc->end_handle;return 0;}
    if(error->status!=BLE_HS_EDONE || !svc_start){fail("VEEPEAK SERVICE FFF0 MISSING",error->status);return 0;}
    phase=Phase::characteristics;deadline=now_ms()+10000;
    int rc=ble_gattc_disc_all_chrs(conn,svc_start,svc_end,characteristic,token());if(rc)fail("OBD CHARACTERISTIC LOOKUP FAILED",rc);return 0;
}
int written(uint16_t conn,const ble_gatt_error *error,ble_gatt_attr*,void *arg){
    if(!current(conn,arg) || phase!=Phase::ready)return 0;
    write_pending=false;if(error->status)fail("OBD WRITE FAILED",error->status);return 0;
}
bool veepeak(const uint8_t *name,size_t length){
    const char prefix[]="VEEPEAK";
    if(length<7)return false;
    for(size_t i=0;i<7;i++)if(std::toupper(name[i])!=prefix[i])return false;
    return length==7 || name[7]==' ' || name[7]=='-' || name[7]=='_';
}
int gap(ble_gap_event *event,void *arg){
    // Scan/connection GAP callbacks belong only to the scanner, never the phone.
    if(reinterpret_cast<uintptr_t>(arg)!=generation){
        if(event->type==BLE_GAP_EVENT_CONNECT && event->connect.status==0)ble_gap_terminate(event->connect.conn_handle,BLE_ERR_REM_USER_CONN_TERM);
        return 0;
    }
    if(event->type==BLE_GAP_EVENT_DISC && phase==Phase::scanning){
        ble_hs_adv_fields fields={};
        if(!ble_hs_adv_parse_fields(&fields,event->disc.data,event->disc.length_data) && fields.name && veepeak(fields.name,fields.name_len)){
            candidate=event->disc.addr;phase=Phase::pending;ble_gap_disc_cancel();
            ESP_LOGI("obd","Found VEEPEAK rssi=%d",event->disc.rssi);publish("CONNECTING TO VEEPEAK");
        }
    }else if(event->type==BLE_GAP_EVENT_DISC_COMPLETE && phase==Phase::scanning){phase=Phase::idle;retry_at=now_ms()+10000;}
    else if(event->type==BLE_GAP_EVENT_CONNECT){
        if(event->connect.status){ESP_LOGW("obd","Scanner connect failed status=%d",event->connect.status);clear();publish("VEEPEAK CONNECT FAILED / RETRY");return 0;}
        handle=event->connect.conn_handle;phase=Phase::service;deadline=now_ms()+10000;
        ESP_LOGI("obd","Scanner connected handle=%u",handle);publish("DISCOVERING VEEPEAK SERVICE");
        ble_uuid16_t uuid=BLE_UUID16_INIT(0xfff0);
        int rc=ble_gattc_disc_svc_by_uuid(handle,&uuid.u,service,token());if(rc)fail("OBD SERVICE LOOKUP FAILED",rc);
    }else if(event->type==BLE_GAP_EVENT_DISCONNECT && event->disconnect.conn.conn_handle==handle){
        ESP_LOGI("obd","Scanner disconnected reason=%d",event->disconnect.reason);clear();publish("VEEPEAK DISCONNECTED / RETRY");
    }else if(event->type==BLE_GAP_EVENT_NOTIFY_RX && phase==Phase::ready && event->notify_rx.conn_handle==handle && event->notify_rx.attr_handle==notify_handle){
        uint8_t bytes[256];uint16_t size=0;
        int rc=ble_hs_mbuf_to_flat(event->notify_rx.om,bytes,sizeof(bytes),&size);
        if(rc){fail("OBD RESPONSE TOO LARGE",rc);return 0;}
        session.receive(bytes,size,now_ms());publish();
    }
    return 0;
}
void tick(ble_npl_event*){
    uint64_t now=now_ms();
    if(!ble_hs_synced()){schedule();return;}
    if(phase==Phase::closing){
        if(now>=deadline){ESP_LOGW("obd","Stuck scanner disconnect; resetting shared BLE host");ble_hs_sched_reset(BLE_HS_ECONTROLLER);clear();publish("OBD LINK RECOVERY");}
    }else if(phase==Phase::pending){
        uint8_t type;int rc=ble_hs_id_infer_auto(0,&type);
        if(!rc){phase=Phase::connecting;deadline=now+12000;rc=ble_gap_connect(type,&candidate,10000,nullptr,gap,token());}
        if(rc)fail("VEEPEAK CONNECT RETRY",rc);
    }else if(phase==Phase::idle && now>=retry_at){
        uint8_t type;int rc=ble_hs_id_infer_auto(0,&type);
        ble_gap_disc_params p={};p.itvl=0x100;p.window=0x30;p.filter_duplicates=1;
        if(!rc){phase=Phase::scanning;rc=ble_gap_disc(type,4000,&p,gap,token());}
        if(rc){phase=Phase::idle;retry_at=now+5000;publish("VEEPEAK SCAN RETRY");ESP_LOGW("obd","Scan failed rc=%d",rc);}
        else{publish("SEARCHING FOR VEEPEAK");ESP_LOGI("obd","Scanning for VEEPEAK");}
    }else if(phase==Phase::ready){
        const char *command=session.next(now);
        if(session.failed())fail("OBD RESPONSE ERROR / RETRY");
        else if(command){
            // Never overlap GATT writes or ELM commands. Missing write ACK also
            // expires through the session deadline and reconnects the scanner.
            if(write_pending){fail("OBD WRITE ACK TIMEOUT");}
            else{
                ESP_LOGD("obd","TX %s",command);
                int rc;
                if(write_properties & BLE_GATT_CHR_PROP_WRITE){write_pending=true;rc=ble_gattc_write_flat(handle,write_handle,command,strlen(command),written,token());}
                else rc=ble_gattc_write_no_rsp_flat(handle,write_handle,command,strlen(command));
                if(rc)fail("OBD COMMAND WRITE FAILED",rc);
            }
        }
    }else if(phase!=Phase::idle && phase!=Phase::scanning && now>=deadline)fail("OBD CONNECTION SETUP TIMEOUT");
    schedule();
}
}
ObdData obd_snapshot(){portENTER_CRITICAL(&mux);auto copy=published;portEXIT_CRITICAL(&mux);return copy;}
void obd_ble_reset(){clear();publish("SEARCHING FOR VEEPEAK");}
void obd_ble_start(){ble_npl_callout_init(&timer,nimble_port_get_dflt_eventq(),tick,nullptr);schedule();}
