#include "obd.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <string>
static uint64_t now=1000;
static void reply(ObdSession &s,const char *text){
    // Simulate arbitrary BLE notification fragmentation, including split prompts.
    for(size_t i=0;i<strlen(text);i++)s.receive(reinterpret_cast<const uint8_t*>(text+i),1,now);
    now+=300;
}
static void expect(ObdSession &s,const char *command){const char *actual=s.next(now);assert(actual && !strcmp(actual,command));assert(s.next(now)==nullptr);}
static void init(ObdSession &s){
    s.reset();expect(s,"ATZ\r");reply(s,"ATZ\r\rELM327 v1.5\r>");
    for(auto command:{"ATE0\r","ATL0\r","ATS0\r","ATH0\r","ATSP0\r"}){expect(s,command);reply(s,"OK\r>");}
}
int main(){
    ObdSession s;init(s);expect(s,"0100\r");reply(s,"SEARCHING...\r41 00 08 18 00 00\r>");assert(s.data().support_known);
    expect(s,"0105\r");reply(s,"0105\r41055E\r>");assert(s.data().coolant.valid && s.data().coolant.value==54);
    expect(s,"010C\r");reply(s,"410C1F40\r>");assert(s.data().rpm.value==2000);
    expect(s,"010D\r");reply(s,"410D2A\r>");assert(s.data().speed.value==42);
    expect(s,"ATRV\r");reply(s,"ATRV\r13.8V\r>");assert(s.data().voltage.valid && s.data().voltage.value==13.8f);
    expect(s,"0105\r");reply(s,"NO DATA\r>");assert(!s.data().coolant.valid);
    expect(s,"010C\r");reply(s,"410CFF\r>");assert(!s.data().rpm.valid); // truncated
    expect(s,"010D\r");reply(s,"7E803410D2A\r>");assert(!s.data().speed.valid); // unexpected headers
    expect(s,"ATRV\r");reply(s,"99.9V\r>");assert(!s.data().voltage.valid);
    expect(s,"0105\r");reply(s,"410D2A\r410520\r>");assert(s.data().coolant.value==-8);
    assert(!s.failed());
    // Only query supported PIDs. Zero supported does not mean zero readings.
    init(s);expect(s,"0100\r");reply(s,"410008000000\r>");
    for(int i=0;i<10;i++){
        expect(s,"0105\r");reply(s,"410550\r>");expect(s,"ATRV\r");reply(s,"12.4V\r>");
    }
    init(s);expect(s,"0100\r");reply(s,"UNABLE TO CONNECT\r>");assert(!s.data().support_known);
    expect(s,"ATRV\r");reply(s,"12.1V\r>");now+=10000;expect(s,"0100\r");reply(s,"410000000000\r>");
    assert(s.data().support_known);expect(s,"ATRV\r");reply(s,"12.0V\r>");
    // Timeouts force a transport reconnect; never associate a late prompt with a new command.
    s.reset();expect(s,"ATZ\r");assert(!s.next(now+4999));assert(!s.failed());assert(!s.next(now+5000));assert(s.failed());
    reply(s,"ELM327 v1.5\r>");assert(s.failed());
    s.reset();expect(s,"ATZ\r");reply(s,"?\r>");assert(s.failed());
    s.reset();expect(s,"ATZ\r");std::string huge(600,'A');reply(s,huge.c_str());assert(s.failed());
    s.reset();expect(s,"ATZ\r");uint8_t invalid=0;s.receive(&invalid,1,now);assert(s.failed());
    init(s);expect(s,"0100\r");assert(!s.next(now+19999));assert(!s.failed());assert(!s.next(now+20000));assert(s.failed());
    s.reset();assert(!s.data().voltage.valid && !s.data().rpm.valid && !s.data().support_known);
    puts("OBD: fragmented replies, prompt serialization, PID capabilities/units, missing/malformed data, timeouts, overflow and reset passed");
}
