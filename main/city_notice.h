#pragma once
#include <cstring>
#include <cstdio>
namespace trailstrip {
class CityNotice {
    char last_[17]={},pending_[17]={};
public:
    void observe(const char *city,bool fresh){
        if(!fresh || !city[0]){pending_[0]=0;return;}
        if(!last_[0]){std::snprintf(last_,sizeof(last_),"%s",city);return;}
        if(std::strcmp(last_,city)!=0){
            std::snprintf(last_,sizeof(last_),"%s",city);
            std::snprintf(pending_,sizeof(pending_),"%s",city);
        }
    }
    bool take(char (&city)[17]){
        if(!pending_[0])return false;
        std::memcpy(city,pending_,sizeof(pending_));pending_[0]=0;return true;
    }
};
}
