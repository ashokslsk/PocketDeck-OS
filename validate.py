import csv, math
from skyfield.api import load, wgs84
from skyfield import almanac
from skyfield.framelib import ecliptic_frame
ts = load.timescale(); eph = load('de421.bsp')
earth, sun, moon = eph['earth'], eph['sun'], eph['moon']
loc = wgs84.latlon(12.9716, 77.5946)
def lon(body, t):
    lat_, lon_, _ = earth.at(t).observe(body).apparent().frame_latlon(ecliptic_frame)
    return lon_.degrees % 360
def ayan(jd):
    T=(jd-2451545)/36525; return 23.86379956 + (5029.0966*T + 1.11113*T*T)/3600
rows=list(csv.reader(open('pm.csv')))
f = almanac.sunrise_sunset(eph, loc)
mis={'tithi':0,'nak':0,'yoga':0,'karana':0}; dsr=[]; dss=[]; dte=[]; dne=[]
def karana_name(h):
    if h==0: return 10
    if h>=57: return 7+(h-57)
    return (h-1)%7
for r in rows:
    y,m,d=map(int,r[0].split('-')); sr=float(r[1]); ss=float(r[2])
    t0=ts.tt_jd(sr-0.4) if False else ts.utc(y,m,d,-5.5); t1=ts.utc(y,m,d,18.5)
    times, ev = almanac.find_discrete(t0, t1, f)
    rise=[t for t,e in zip(times,ev) if e==1]; setv=[t for t,e in zip(times,ev) if e==0]
    if rise: dsr.append((sr-rise[0].ut1)*86400)
    if setv: dss.append((ss-setv[0].ut1)*86400)
    t=ts.ut1_jd(sr); ms=lon(moon,t); su=lon(sun,t); e=(ms-su)%360; a=ayan(sr)
    ti=int(e//12); na=int(((ms-a)%360)//(360/27)); yo=int((((ms-a)+(su-a))%360)//(360/27)); ka=karana_name(int(e//6))
    mis['tithi']+= ti!=int(r[4]); mis['nak']+= na!=int(r[5]); mis['yoga']+= yo!=int(r[6]); mis['karana']+= ka!=int(r[7])
    # tithi end time error: elongation at our end time should be a multiple of 12
    te=float(r[11]); ee=(lon(moon,ts.ut1_jd(te))-lon(sun,ts.ut1_jd(te)))%360; err=((ee+6)%12)-6; dte.append(err/12.19*1440)
    ne=float(r[12]); mn=(lon(moon,ts.ut1_jd(ne))-ayan(ne))%360; errn=((mn+360/54)%(360/27))-360/54; dne.append(errn/13.18*1440)
import statistics as S
print("days",len(rows)); print("mismatches",mis)
print("sunrise diff s: mean %.1f max %.1f"%(S.mean(dsr),max(map(abs,dsr))))
print("sunset  diff s: mean %.1f max %.1f"%(S.mean(dss),max(map(abs,dss))))
print("tithi end err min: max %.2f; nakshatra end err min: max %.2f"%(max(map(abs,dte)),max(map(abs,dne))))
