#pragma once
#include <cstdint>
namespace trailstrip {
// Accessed exclusively from the NimBLE host event queue.
struct BleRecovery {
    enum class Action { none, advertise, terminate, reset_host };
    bool connected=false, terminating=false;
    uint64_t last_packet_ms=0, termination_ms=0;
    unsigned failures=0;
    void reset(){*this={};}
    void connect(uint64_t now){reset();connected=true;last_packet_ms=now;}
    void packet(uint64_t now){if(!terminating)last_packet_ms=now;}
    void termination_started(uint64_t now){terminating=true;termination_ms=now;}
    Action next(uint64_t now,bool synced,bool advertising) const {
        if(!synced)return Action::none;
        if(failures>=10)return Action::reset_host;
        if(terminating)return now>=termination_ms && now-termination_ms>=10000?Action::reset_host:Action::none;
        if(connected)return now>=last_packet_ms && now-last_packet_ms>=120000?Action::terminate:Action::none;
        return advertising?Action::none:Action::advertise;
    }
};
}
