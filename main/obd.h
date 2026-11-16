#pragma once
#include <cstddef>
#include <cstdint>
#include "telemetry.h"
struct ObdData {
    Reading coolant,rpm,speed,voltage;
    bool connected=false,ready=false,support_known=false;
    uint32_t supported=0;
    char status[64]="SEARCHING FOR VEEPEAK";
};
// Transport-independent, bounded ELM327 command/response engine.
class ObdSession {
public:
    void reset();
    const char *next(uint64_t now);
    void receive(const uint8_t *bytes,size_t length,uint64_t now);
    bool failed() const {return fault_;}
    const ObdData &data() const {return data_;}
private:
    ObdData data_;
    char response_[512]={};size_t used_=0;
    int setup_=0,poll_=0,command_=-1;
    bool fault_=false;
    uint64_t deadline_=0,next_ms_=0,probe_ms_=0;
    void complete(uint64_t now);
};
ObdData obd_snapshot();
void obd_ble_start();
void obd_ble_reset();
