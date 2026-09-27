#include <cstdio>
#include "PanchangaMath.h"
using namespace panchanga;
int main(){ int ds[][3]={{2026,3,18},{2026,3,19},{2026,9,4},{2026,9,5},{2027,9,30},{2027,10,1}};
 for(auto&x:ds){ Day r=compute(x[0],x[1],x[2],5.5,12.9716,77.5946);
  printf("%d-%02d-%02d masa %d adhika %d tithiSunrise %d nishita %d\n",x[0],x[1],x[2],r.masa,r.adhika,tithiAt(kaalaTime(r,Kaala::Sunrise,0)),tithiAt(kaalaTime(r,Kaala::Nishita,0))); } }
