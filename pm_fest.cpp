#include <cstdio>
#include "PanchangaMath.h"
using namespace panchanga;
int main(){ const char* names[]={"-","Ugadi","RamaNavami","AkshayaTritiya","NagaPanchami","Janmashtami","GaneshaChaturthi","MahalayaAmavasya","NavaratriStart","Vijayadashami","NarakaChaturdashi","Deepavali","BaliPadyami","MahaShivaratri","Holi","MakaraSankranti","Sankranti","Ekadashi","Purnima","Amavasya","Sankashti"};
 for(int y=2024;y<=2027;++y) for(int m=1;m<=12;++m) for(int d=1;d<=31;++d){ int dim[]={31,28,31,30,31,30,31,31,30,31,30,31}; if(d>dim[m-1]+((m==2&&y%4==0)?1:0)) continue;
  Day r=compute(y,m,d,5.5,12.9716,77.5946); Special s[2]; int n=specials(r,s,2);
  for(int i=0;i<n;++i){ int id=(int)s[i]; if(id>=1&&id<=15) printf("%04d-%02d-%02d %s\n",y,m,d,names[id]); } } }
