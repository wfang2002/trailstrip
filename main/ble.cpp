#include "trailstrip.h"
#include "telemetry.h"
#include "ble_recovery.h"
#include "esp_timer.h"
#include "nimble/nimble_npl.h"
#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
// UUIDs: a87e0001/2-b5a3-f393-e0a9-e50e24dcca9e (custom Trailstrip protocol).
static const ble_uuid128_t service_uuid = BLE_UUID128_INIT(0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,0x93,0xf3,0xa3,0xb5,0x01,0x00,0x7e,0xa8);
static const ble_uuid128_t rx_uuid = BLE_UUID128_INIT(0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,0x93,0xf3,0xa3,0xb5,0x02,0x00,0x7e,0xa8);
static uint8_t address_type;
static uint16_t connection_handle=BLE_HS_CONN_HANDLE_NONE;
static trailstrip::BleRecovery recovery;
static ble_npl_callout maintenance;
static uint64_t now_ms(){return esp_timer_get_time()/1000;}
static void schedule(uint32_t delay=1000){ble_npl_callout_reset(&maintenance,ble_npl_time_ms_to_ticks32(delay));}
static int advertise();
static int access(uint16_t handle, uint16_t, ble_gatt_access_ctxt *ctx, void*) {
    uint8_t bytes[20]; uint16_t length=0;
    if(ctx->op!=BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_UNLIKELY;
    if(OS_MBUF_PKTLEN(ctx->om)!=20) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    if(ble_hs_mbuf_to_flat(ctx->om,bytes,sizeof(bytes),&length)) return BLE_ATT_ERR_UNLIKELY;
    if(!telemetry_receive(bytes,length))return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    if(handle==connection_handle)recovery.packet(now_ms());
    return 0;
}
static ble_gatt_chr_def characteristics[2];
static ble_gatt_svc_def services[2];
static int gap_event(ble_gap_event *event,void*) {
    if(event->type==BLE_GAP_EVENT_CONNECT) {
        if(event->connect.status==0){
            connection_handle=event->connect.conn_handle;recovery.connect(now_ms());
            telemetry_disconnect();telemetry_connect(true);
            ESP_LOGI("ble","Phone connected handle=%u",connection_handle);
        }else ESP_LOGW("ble","Connection failed status=%d",event->connect.status);
        schedule(100);
    }else if(event->type==BLE_GAP_EVENT_DISCONNECT){
        ESP_LOGI("ble","Disconnected handle=%u reason=%d",event->disconnect.conn.conn_handle,event->disconnect.reason);
        connection_handle=BLE_HS_CONN_HANDLE_NONE;recovery.reset();telemetry_disconnect();schedule(100);
    }else if(event->type==BLE_GAP_EVENT_ADV_COMPLETE){
        ESP_LOGI("ble","Advertising complete reason=%d",event->adv_complete.reason);schedule(100);
    }
    return 0;
}
static int advertise(){
    ble_hs_adv_fields fields={};
    fields.flags=BLE_HS_ADV_F_DISC_GEN|BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128=const_cast<ble_uuid128_t*>(&service_uuid);fields.num_uuids128=1;fields.uuids128_is_complete=1;
    int rc=ble_gap_adv_set_fields(&fields);
    if(rc)return rc;
    ble_hs_adv_fields scan={};
    const char *name="TRAILSTRIP";
    scan.name=(uint8_t*)name;scan.name_len=10;scan.name_is_complete=1;
    rc=ble_gap_adv_rsp_set_fields(&scan);
    if(rc)return rc;
    ble_gap_adv_params p={};p.conn_mode=BLE_GAP_CONN_MODE_UND;p.disc_mode=BLE_GAP_DISC_MODE_GEN;
    rc=ble_gap_adv_start(address_type,nullptr,BLE_HS_FOREVER,&p,gap_event,nullptr);
    if(!rc)ESP_LOGI("ble","Advertising started");
    return rc;
}
static void maintain(ble_npl_event*){
    if(ble_hs_synced() && recovery.connected){
        ble_gap_conn_desc desc;
        if(ble_gap_conn_find(connection_handle,&desc)==BLE_HS_ENOTCONN){
            ESP_LOGW("ble","Clearing missing connection");
            connection_handle=BLE_HS_CONN_HANDLE_NONE;recovery.reset();telemetry_disconnect();
        }
    }
    using Action=trailstrip::BleRecovery::Action;
    switch(recovery.next(now_ms(),ble_hs_synced(),ble_gap_adv_active())){
        case Action::advertise:{
            int rc=ble_hs_id_infer_auto(0,&address_type);
            if(!rc)rc=advertise();
            if(rc){++recovery.failures;ESP_LOGW("ble","Advertising retry %u error=%d",recovery.failures,rc);}
            else recovery.failures=0;
            break;
        }
        case Action::terminate:{
            ESP_LOGW("ble","No phone packets for 120s; releasing handle=%u",connection_handle);
            int rc=ble_gap_terminate(connection_handle,BLE_ERR_REM_USER_CONN_TERM);
            if(!rc)recovery.termination_started(now_ms());
            else{++recovery.failures;ESP_LOGW("ble","Terminate failed error=%d",rc);}
            break;
        }
        case Action::reset_host:
            ESP_LOGW("ble","Resetting stalled BLE host (device stays running)");
            telemetry_disconnect();recovery.reset();connection_handle=BLE_HS_CONN_HANDLE_NONE;
            ble_hs_sched_reset(BLE_HS_ECONTROLLER);
            break;
        case Action::none:break;
    }
    schedule();
}
static void reset(int reason){
    ESP_LOGW("ble","Host reset reason=%d",reason);
    connection_handle=BLE_HS_CONN_HANDLE_NONE;recovery.reset();telemetry_disconnect();
}
static void synced(){ESP_LOGI("ble","Host synced");schedule(100);}
static void host_task(void*){nimble_port_run();nimble_port_freertos_deinit();}
void ble_start(){
    ESP_ERROR_CHECK(nimble_port_init());
    ble_svc_gap_init();ble_svc_gatt_init();ble_svc_gap_device_name_set("TRAILSTRIP");
    characteristics[0].uuid=&rx_uuid.u;characteristics[0].access_cb=access;
    characteristics[0].flags=BLE_GATT_CHR_F_WRITE;
    services[0].type=BLE_GATT_SVC_TYPE_PRIMARY;services[0].uuid=&service_uuid.u;services[0].characteristics=characteristics;
    ESP_ERROR_CHECK(ble_gatts_count_cfg(services));ESP_ERROR_CHECK(ble_gatts_add_svcs(services));
    ble_npl_callout_init(&maintenance,nimble_port_get_dflt_eventq(),maintain,nullptr);
    ble_hs_cfg.sync_cb=synced;ble_hs_cfg.reset_cb=reset;
    nimble_port_freertos_init(host_task);
}
