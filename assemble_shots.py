import os, shutil, glob
from PIL import Image, ImageDraw, ImageFont
SP="/private/tmp/claude-501/-Users-ashok-Library-Application-Support-Claude-scratch-workspaces-9fbb4d3d-2663-4dcd-b22b-654bf7582786-e533bd95-93c8-45a5-94c8-6fab1c9482d0-scratch-2026-09-26-a24ec2/edfb3f92-85f7-4810-8e22-67e88b4e7394/scratchpad/simrun"
OUT="pocketdeck-os-main/docs/screenshots"
OVR={"11-view-bookmarks-row":"Clipping preview","12-bookmarks-list":"Clipping opened in the book (highlighted)"}
def cap(name):
    if name in OVR: return OVR[name]
    t=name[3:] if name[:2].isdigit() else name
    return t.replace("-", " ").capitalize()
def sec(section, names=None):
    d=f"{SP}/walk_{section}/shots"
    fs=sorted(f[:-4] for f in os.listdir(d) if f.endswith(".bmp"))
    if names: fs=[f for f in fs if f in names]
    return [(f"walk_{section}/shots/{f}", cap(f)) for f in fs]
THEMES=[("pocketdeck","PocketDeck (new)"),("lyra","Lyra (default)"),("classic","Classic"),("lyra-extended","Lyra Extended"),
        ("roundedraff","RoundedRaff"),("lyra-carousel","Lyra Carousel"),("minimal","Minimal"),("dashboard","Dashboard")]
SLEEPS=[("light-default","PocketDeck-OS (default)"),("dark","PocketDeck-OS dark"),("page-overlay","Page with PocketDeck-OS card"),
        ("book-cover","Book cover"),("reading-stats","Reading stats"),("minimal","Minimal"),("minimal-stats","Minimal stats"),("dashboard","Dashboard")]
S=[
 ("01-boot-and-sleep","Boot and sleep screens",[("walk_boot/shots/01-boot-splash","Boot splash")]+
   [(f"walk_sleep_{k}/shots/sleep",f"Sleep: {n}") for k,n in SLEEPS]),
 ("02-home-and-library","Home and library",[x for x in sec("lib") if x[0].split('/')[-1][:2] in ("01","02","03")]+sec("lib2")+sec("lib3",["01-recent-books"])),
 ("03-reader-and-dictionary","Reader and dictionary",[x for x in sec("lib") if x[0].split('/')[-1][:2] not in ("01","02","03")]),
 ("04-bookmarks-and-clippings","Bookmarks and clippings",sec("bookmarks")),
 ("05-reading-stats","Reading stats and library dashboard",[x for x in sec("lib3") if "reading-stats" in x[0]]),
 ("06-file-transfer","File transfer",sec("transfer")),
 ("07-settings-and-about","Settings and About",sec("settings")),
 ("08-themes","UI themes",[(f"walk_theme_{k}_{p}/shots/{p}",f"{n}: {p}") for k,n in THEMES for p in ("home","tools","settings")]),
 ("09-tools-launcher","Tools launcher",sec("tools_menu")),
 ("10-pomodoro","Pomodoro",sec("pomodoro")+sec("pomodoro_ring")),
 ("11-world-clock","World Clock",sec("worldclock")),
 ("12-habit-tracker","Habit Tracker",sec("habits")),
 ("13-flashcards","Flashcards",sec("flashcards")),
 ("14-daily-quote","Daily Quote",sec("quote")),
 ("15-knowledge","Knowledge",sec("knowledge")),
 ("16-panchanga","Panchanga (Kannada)",sec("panchanga")),
 ("17-today","Today",sec("today")),
]
if os.path.exists(OUT): shutil.rmtree(OUT)
os.makedirs(OUT)
F=ImageFont.truetype("/System/Library/Fonts/Avenir Next.ttc",22,index=7); FB=ImageFont.truetype("/System/Library/Fonts/Avenir Next.ttc",34,index=0)
def slug(c): 
    s="".join(ch if ch.isalnum() else "-" for ch in c.lower())
    while "--" in s: s=s.replace("--","-")
    return s.strip("-")
md=["# PocketDeck-OS screenshot walkthrough","",
    "Every screen of PocketDeck-OS 1.0.0, captured from the CrossInk simulator with the **Xteink X3** profile (792x528 panel, portrait). "
    "The library is public-domain books from Project Gutenberg, the dictionary is built from Princeton WordNet 3.0, and tool data comes from "
    "`sd-sample/tools` plus a few seeded days of habits and to-dos. Clocks are set to UTC+5:30 (Bengaluru).","",
    "Captured headlessly with the simulator's `CROSSPOINT_SIM_INPUT_SCRIPT` / `CROSSPOINT_SIM_SCREENSHOTS` variables "
    "(`SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software`). The simulator-only `CROSSINK_SIMULATOR_START_SCREEN=boot|tools|settings` opens a screen directly.","",
    "![Overview](overview.png)",""]
thumbs=[]; total=0; toc=[]
for folder,title,items in S:
    d=os.path.join(OUT,folder); os.makedirs(d)
    toc.append(f"- [{folder[:2]}. {title}](#{slug(folder[:2]+'-'+title)}) ({len(items)})")
    md+=[f"## {folder[:2]}. {title}",""]
    names=[]; ims=[]
    for i,(src,c) in enumerate(items,1):
        name=f"{i:02d}-{slug(c)}"[:60]
        im=Image.open(os.path.join(SP,src+".bmp")).convert("RGB"); im.save(os.path.join(d,name+".png"), optimize=True)
        names.append((name,c)); ims.append(im); thumbs.append((im,f"{folder[:2]}.{i:02d}")); total+=1
    for k in range(0,len(names),4):
        chunk=names[k:k+4]
        md.append("| " + " | ".join(f"![{c}]({folder}/{n}.png)" for n,c in chunk) + " |")
        md.append("|" + "---|"*len(chunk))
        md.append("| " + " | ".join(f"**{n[:2]}** {c}" for n,c in chunk) + " |")
        md.append("")
    tw,th=262,394; cols=min(len(ims),6); rows=(len(ims)+cols-1)//cols
    sheet=Image.new("RGB",(cols*(tw+16)+16, rows*(th+60)+16),"#f2f2f2"); dr=ImageDraw.Draw(sheet)
    for i,(im,(n,c)) in enumerate(zip(ims,names)):
        x=16+(i%cols)*(tw+16); y=16+(i//cols)*(th+60)
        sheet.paste(im.resize((tw,th),Image.LANCZOS),(x,y)); dr.rectangle((x-1,y-1,x+tw,y+th),outline="#999")
        cc=c if dr.textlength(c,font=F)<tw-30 else c[:24]+"…"
        dr.text((x,y+th+10),f"{n[:2]} {cc}",font=F,fill="#222")
    sheet.save(os.path.join(d,"_contact-sheet.png"), optimize=True)
md[md.index("![Overview](overview.png)")+1:md.index("![Overview](overview.png)")+1]=["","## Contents",""]+toc+[""]
cols=12; tw,th=160,240; rows=(len(thumbs)+cols-1)//cols
ov=Image.new("RGB",(cols*(tw+10)+10, 90+rows*(th+38)),"white"); dr=ImageDraw.Draw(ov)
dr.text((14,22),f"PocketDeck-OS 1.0.0: {total} screens (Xteink X3 simulator)",font=FB,fill="black")
for i,(im,lab) in enumerate(thumbs):
    x=10+(i%cols)*(tw+10); y=90+(i//cols)*(th+38)
    ov.paste(im.resize((tw,th),Image.LANCZOS),(x,y)); dr.rectangle((x-1,y-1,x+tw,y+th),outline="#aaa")
    dr.text((x,y+th+4),lab,font=F,fill="#333")
ov.save(os.path.join(OUT,"overview.png"), optimize=True)
open(os.path.join(OUT,"README.md"),"w").write("\n".join(md)+"\n")
print(total,"screenshots in",len(S),"sections")
