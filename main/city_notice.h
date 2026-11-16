#pragma once
#include <cstring>
#include <cstdio>
namespace trailstrip {
class CityNotice {
    char shown_[17]={},pending_[17]={};
public:
    void observe(const char *city,bool fresh){
        if(!fresh || !city[0]){pending_[0]=0;return;}
        if(std::strcmp(shown_,city)!=0)std::snprintf(pending_,sizeof(pending_),"%s",city);
        else pending_[0]=0;
    }
    bool take(char (&city)[17]){
        if(!pending_[0])return false;
        std::memcpy(city,pending_,sizeof(pending_));
        // A city is acknowledged only when its popup is actually requested.
        // Temporary invalid packets while deferred must not swallow the change.
        std::memcpy(shown_,pending_,sizeof(shown_));pending_[0]=0;return true;
    }
};
}
