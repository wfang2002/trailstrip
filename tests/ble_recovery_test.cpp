#include "ble_recovery.h"
#include <cassert>
#include <cstdio>
int main(){
 using R=trailstrip::BleRecovery;using A=R::Action;R r;
 assert(r.next(0,false,false)==A::none);
 assert(r.next(0,true,false)==A::advertise);
 assert(r.next(0,true,true)==A::none);
 for(int i=1;i<10;i++){r.failures=i;assert(r.next(1000*i,true,false)==A::advertise);}
 r.failures=10;assert(r.next(10000,true,false)==A::reset_host);
 r.connect(20000);assert(r.failures==0);
 assert(r.next(139999,true,false)==A::none);
 assert(r.next(140000,true,false)==A::terminate);
 r.packet(139000);assert(r.next(140000,true,false)==A::none);
 assert(r.next(259000,true,false)==A::terminate);
 r.termination_started(259000);r.packet(260000);
 assert(r.next(268999,true,false)==A::none);
 assert(r.next(269000,true,false)==A::reset_host);
 r.reset();assert(r.next(270000,true,false)==A::advertise);
 for(int i=0;i<50;i++){
  r.connect(i*1000);r.packet(i*1000+500);assert(r.next(i*1000+900,true,false)==A::none);
  r.reset();assert(r.next(i*1000+999,true,false)==A::advertise);
 }
 puts("BLE recovery: advertising retries, host reset, live traffic, idle link expiry, stuck termination and 50 reconnect cycles passed");
}
