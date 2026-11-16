#include "obd.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
namespace {
const char *commands[]={"ATZ\r","ATE0\r","ATL0\r","ATS0\r","ATH0\r","ATSP0\r","0100\r","0105\r","010C\r","010D\r","ATRV\r"};
int hex(char c){if(c>='0'&&c<='9')return c-'0';if(c>='A'&&c<='F')return c-'A'+10;if(c>='a'&&c<='f')return c-'a'+10;return -1;}
bool pid_bytes(const char *text,uint8_t pid,uint8_t *out,size_t size){
    // Headers are disabled during setup. Parse complete lines, not arbitrary
    // substrings that might be echoes, headers, or an unrelated ECU response.
    while(*text){
        const char *end=strpbrk(text,"\r\n");if(!end)end=text+strlen(text);
        uint8_t bytes[32];size_t count=0;int high=-1;bool good=true;
        for(auto p=text;p<end;p++){
            if(*p==' '||*p=='\t')continue;
            int value=hex(*p);if(value<0){good=false;break;}
            if(high<0)high=value;
            else {if(count==sizeof(bytes)){good=false;break;}bytes[count++]=(high<<4)|value;high=-1;}
        }
        if(good && high<0 && count==size+2 && bytes[0]==0x41 && bytes[1]==pid){memcpy(out,bytes+2,size);return true;}
        text=*end?end+1:end;
    }
    return false;
}
bool adapter_voltage(const char *text,float &voltage){
    while(*text){
        char *end=nullptr;float value=strtof(text,&end);
        if(end!=text && (*end=='V'||*end=='v') && (end[1]=='\r'||end[1]=='\n'||end[1]==0) && std::isfinite(value) && value>=0 && value<=32){voltage=value;return true;}
        const char *line=strpbrk(text,"\r\n");if(!line)break;text=line+1;
    }
    return false;
}
}
void ObdSession::reset(){*this=ObdSession{};data_.connected=true;snprintf(data_.status,sizeof(data_.status),"INITIALIZING OBD ADAPTER");}
const char *ObdSession::next(uint64_t now){
    if(fault_)return nullptr;
    if(command_>=0){if(now>=deadline_)fault_=true;return nullptr;}
    if(now<next_ms_)return nullptr;
    int next_command=-1;
    if(setup_<6)next_command=setup_;
    else if(!data_.support_known && now>=probe_ms_)next_command=6;
    else{
        for(int n=0;n<4;n++){
            int slot=poll_++%4;int pid=slot==0?5:slot==1?12:13;
            if(slot==3){next_command=10;break;}
            if(data_.support_known && (data_.supported&(1UL<<(32-pid)))){next_command=7+slot;break;}
        }
    }
    if(next_command<0)return nullptr;
    command_=next_command;used_=0;response_[0]=0;
    deadline_=now+(command_==6?20000:5000);
    return commands[command_];
}
void ObdSession::receive(const uint8_t *bytes,size_t size,uint64_t now){
    for(size_t i=0;i<size;i++){
        if(command_<0 || fault_)continue;
        uint8_t c=bytes[i];
        if(c=='>'){response_[used_]=0;complete(now);continue;}
        if(c!= '\r' && c!='\n' && c!='\t' && (c<32 || c>126)){fault_=true;continue;}
        if(used_+1>=sizeof(response_)){fault_=true;continue;}
        response_[used_++]=char(c);
    }
}
void ObdSession::complete(uint64_t now){
    int command=command_;command_=-1;next_ms_=now+250;
    if(command<6){
        bool ok=command==0?strstr(response_,"ELM327")!=nullptr:strstr(response_,"OK")!=nullptr;
        if(!ok){fault_=true;return;}
        ++setup_;return;
    }
    data_.ready=true;
    uint8_t bytes[4]={};
    if(command==6){
        data_.support_known=pid_bytes(response_,0,bytes,4);
        if(data_.support_known){data_.supported=uint32_t(bytes[0])<<24|uint32_t(bytes[1])<<16|uint32_t(bytes[2])<<8|bytes[3];}
        probe_ms_=now+10000;
    }else if(command==10){
        float voltage=0;bool ok=adapter_voltage(response_,voltage);data_.voltage={voltage,now,ok};
    }else{
        int pid=command==7?5:command==8?12:13;
        bool ok=pid_bytes(response_,pid,bytes,command==8?2:1);
        float value=command==7?int(bytes[0])-40:command==8?(bytes[0]*256+bytes[1])/4.f:bytes[0];
        Reading *field=command==7?&data_.coolant:command==8?&data_.rpm:&data_.speed;
        *field={value,now,ok};
    }
    snprintf(data_.status,sizeof(data_.status),data_.support_known?"VEEPEAK / LIVE OBD":"VEEPEAK / WAITING FOR ECU");
}
