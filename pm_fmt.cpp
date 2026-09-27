#include <cstdio>
#include <cmath>
#include "PanchangaMath.h"
using namespace panchanga;
static void hm(char* b, double jd, bool nearest){ double h=localHours(jd,5.5)+(nearest?0.5/60:0); int hh=(int)h%24, mm=(int)((h-std::floor(h))*60); sprintf(b,"%02d:%02d",hh,mm);}
int main(){ int ds[][3]={{2021,9,27},{2022,1,14},{2023,3,22},{2024,10,12},{2025,8,27},{2026,11,8},{2029,2,11},{2031,6,15}};
 const char* T[]={"Pratipada","Dwitiya","Tritiya","Chaturthi","Panchami","Shashthi","Saptami","Ashtami","Navami","Dashami","Ekadashi","Dwadashi","Trayodashi","Chaturdashi","Purnima","Pratipada","Dwitiya","Tritiya","Chaturthi","Panchami","Shashthi","Saptami","Ashtami","Navami","Dashami","Ekadashi","Dwadashi","Trayodashi","Chaturdashi","Amavasya"};
 const char* N[]={"Ashwini","Bharani","Krittika","Rohini","Mrigashira","Ardra","Punarvasu","Pushya","Ashlesha","Magha","Purva Phalguni","Uttara Phalguni","Hasta","Chitra","Swati","Vishakha","Anuradha","Jyeshtha","Mula","Purva Ashadha","Uttara Ashadha","Shravana","Dhanishtha","Shatabhisha","Purva Bhadrapada","Uttara Bhadrapada","Revati"};
 const char* Y[]={"Vishkambha","Priti","Ayushman","Saubhagya","Shobhana","Atiganda","Sukarma","Dhriti","Shula","Ganda","Vriddhi","Dhruva","Vyaghata","Harshana","Vajra","Siddhi","Vyatipata","Variyana","Parigha","Shiva","Siddha","Sadhya","Shubha","Shukla","Brahma","Indra","Vaidhriti"};
 const char* K[]={"Bava","Balava","Kaulava","Taitila","Garaja","Vanija","Vishti","Shakuni","Chatushpada","Nagava","Kimstughna"};
 const char* M[]={"Chaitra","Vaishakha","Jyeshtha","Ashadha","Shravana","Bhadrapada","Ashwina","Kartika","Margashirsha","Pausha","Magha","Phalguna"};
 for(auto&d:ds){ Day r=compute(d[0],d[1],d[2],5.5,12.9716,77.5946); char a[8],b[8],t[8],n[8],y[8],k[8],rh[8];
  hm(a,r.sunriseJd,true);hm(b,r.sunsetJd,true);hm(t,r.tithiEndJd,false);hm(n,r.nakshatraEndJd,false);hm(y,r.yogaEndJd,false);hm(k,r.karanaEndJd,false);hm(rh,r.rahuStartJd,true);
  printf("%04d-%02d-%02d|%s|%s|%s %s|%s %s|%s %s|%s %s|%s%s|rahu %s|special %d\n",d[0],d[1],d[2],a,b,T[r.tithi],t,N[r.nakshatra],n,Y[r.yoga],y,K[r.karana],k,r.adhika?"Adhika ":"",M[r.masa],rh,(int)special(r)); } }
