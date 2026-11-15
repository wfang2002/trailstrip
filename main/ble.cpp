#include "trailstrip.h"
#include "telemetry.h"
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
static void advertise();
static int access(uint16_t, uint16_t, ble_gatt_access_ctxt *ctx, void*) {
    uint8_t bytes[20]; uint16_t length=0;
    if(ctx->op!=BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_UNLIKELY;
    if(OS_MBUF_PKTLEN(ctx->om)!=20) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    if(ble_hs_mbuf_to_flat(ctx->om,bytes,sizeof(bytes),&length)) return BLE_ATT_ERR_UNLIKELY;
    return telemetry_receive(bytes,length)?0:BLE_ATT_ERR_VALUE_NOT_ALLOWED;
}
static ble_gatt_chr_def characteristics[2];
static ble_gatt_svc_def services[2];
static int gap_event(ble_gap_event *event,void*) {
    if(event->type==BLE_GAP_EVENT_CONNECT) {
        if(event->connect.status==0){telemetry_connect(true);ESP_LOGI("ble","Phone connected");}
        else advertise();
    } else if(event->type==BLE_GAP_EVENT_DISCONNECT){telemetry_disconnect();advertise();}
    else if(event->type==BLE_GAP_EVENT_ADV_COMPLETE) advertise();
    return 0;
}
static void advertise(){
    ble_hs_adv_fields fields={};
    fields.flags=BLE_HS_ADV_F_DISC_GEN|BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128=const_cast<ble_uuid128_t*>(&service_uuid);fields.num_uuids128=1;fields.uuids128_is_complete=1;
    int rc=ble_gap_adv_set_fields(&fields);
    if(rc){ESP_LOGE("ble","Advertising fields: %d",rc);return;}
    ble_hs_adv_fields scan={};
    const char *name="TRAILSTRIP";
    scan.name=(uint8_t*)name;scan.name_len=10;scan.name_is_complete=1;
    ble_gap_adv_rsp_set_fields(&scan);
    ble_gap_adv_params p={};p.conn_mode=BLE_GAP_CONN_MODE_UND;p.disc_mode=BLE_GAP_DISC_MODE_GEN;
    rc=ble_gap_adv_start(address_type,nullptr,BLE_HS_FOREVER,&p,gap_event,nullptr);
    if(rc)ESP_LOGE("ble","Advertising start: %d",rc);
}
static void synced(){if(ble_hs_id_infer_auto(0,&address_type)==0)advertise();}
static void host_task(void*){nimble_port_run();nimble_port_freertos_deinit();}
void ble_start(){
    ESP_ERROR_CHECK(nimble_port_init());
    ble_svc_gap_init();ble_svc_gatt_init();ble_svc_gap_device_name_set("TRAILSTRIP");
    characteristics[0].uuid=&rx_uuid.u;characteristics[0].access_cb=access;
    characteristics[0].flags=BLE_GATT_CHR_F_WRITE;
    services[0].type=BLE_GATT_SVC_TYPE_PRIMARY;services[0].uuid=&service_uuid.u;services[0].characteristics=characteristics;
    ESP_ERROR_CHECK(ble_gatts_count_cfg(services));ESP_ERROR_CHECK(ble_gatts_add_svcs(services));
    ble_hs_cfg.sync_cb=synced;
    nimble_port_freertos_init(host_task);
}
