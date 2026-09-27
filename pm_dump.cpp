#include <cstdio>
#include "PanchangaMath.h"
int main(){ using namespace panchanga;
 for(int y=2021;y<=2031;++y) for(int m=1;m<=12;++m) for(int d=1;d<=31;++d){
   if(d>28){ int dim[]={31,28,31,30,31,30,31,31,30,31,30,31}; int lim=dim[m-1]+((m==2&&(y%4==0))?1:0); if(d>lim) continue;}
   Day r=compute(y,m,d,5.5,12.9716,77.5946);
   printf("%04d-%02d-%02d,%.6f,%.6f,%d,%d,%d,%d,%d,%d,%d,%d,%.6f,%.6f\n",y,m,d,r.sunriseJd,r.sunsetJd,r.vara,r.tithi,r.nakshatra,r.yoga,r.karana,r.masa,r.adhika,r.samvatsara,r.tithiEndJd,r.nakshatraEndJd);
 }}
