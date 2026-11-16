#include "city_notice.h"
#include <cassert>
#include <cstring>
int main(){
 trailstrip::CityNotice n;char city[17];
 n.observe("",false);assert(!n.take(city));
 n.observe("Vancouver",true);assert(!n.take(city));
 n.observe("Vancouver",true);assert(!n.take(city));
 n.observe("",false);n.observe("Vancouver",true);assert(!n.take(city));
 n.observe("North Vancouver",true);assert(n.take(city));assert(!strcmp(city,"North Vancouver"));assert(!n.take(city));
 n.observe("Burnaby",true);n.observe("Richmond",true);assert(n.take(city));assert(!strcmp(city,"Richmond"));
 n.observe("Vancouver",true);n.observe("",false);assert(!n.take(city));
 n.observe("Burnaby",true);assert(n.take(city));assert(!strcmp(city,"Burnaby"));
}
