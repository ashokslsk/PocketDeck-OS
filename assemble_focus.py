# Focused gallery: the Panchanga and Mantras screens only.
import os, shutil
from PIL import Image, ImageDraw, ImageFont
SP="/private/tmp/claude-501/-Users-ashok-Library-Application-Support-Claude-scratch-workspaces-9fbb4d3d-2663-4dcd-b22b-654bf7582786-e533bd95-93c8-45a5-94c8-6fab1c9482d0-scratch-2026-09-26-a24ec2/edfb3f92-85f7-4810-8e22-67e88b4e7394/scratchpad/simrun"
OUT="pocketdeck-os-main/docs/screenshots/panchanga-and-mantras"
def shots(sec): 
    d=f"{SP}/walk_{sec}/shots"; return [(os.path.join(d,f), f[3:-4].replace('-',' ').capitalize()) for f in sorted(os.listdir(d)) if f.endswith('.png')]
groups=[("Panchanga (Kannada)",shots("panchanga")),("Panchanga (English)",shots("panchanga_en")),
        ("Mantras and japa",shots("mantras")),("Kannada in your own files",shots("kn_knowledge"))]
if os.path.exists(OUT): shutil.rmtree(OUT)
os.makedirs(OUT)
F=ImageFont.truetype("/System/Library/Fonts/Avenir Next.ttc",20,index=7)
md=["# Panchanga and Mantras screenshots","","Captured from the X3 simulator (528 x 792, portrait) with the sample SD card pack: the Kannada font",
    "`tools/fonts/kannada.knf`, the 1976-2075 festival calendar and `tools/mantras/mantras.json`.",""]
allims=[]
n=0
for title,items in groups:
    md+=[f"## {title}",""]
    names=[]
    for src,cap in items:
        n+=1; name=f"{n:02d}-{cap.lower().replace(' ','-')}"[:48]+".png"
        im=Image.open(src).convert("RGB"); im.save(os.path.join(OUT,name),optimize=True); allims.append((im,cap)); names.append((name,cap))
    for k in range(0,len(names),4):
        chunk=names[k:k+4]
        md.append("| "+" | ".join(f"![{c}]({f})" for f,c in chunk)+" |"); md.append("|"+"---|"*len(chunk))
        md.append("| "+" | ".join(c for _,c in chunk)+" |"); md.append("")
cols=8; tw,th=220,330; rows=(len(allims)+cols-1)//cols
ov=Image.new("RGB",(cols*(tw+10)+10,rows*(th+34)+10),"white"); dr=ImageDraw.Draw(ov)
for i,(im,cap) in enumerate(allims):
    x=10+(i%cols)*(tw+10); y=10+(i//cols)*(th+34)
    ov.paste(im.resize((tw,th),Image.LANCZOS),(x,y)); dr.rectangle((x-1,y-1,x+tw,y+th),outline="#999")
    dr.text((x,y+th+6),cap[:26],font=F,fill="#222")
ov.save(os.path.join(OUT,"overview.png"),optimize=True)
md.insert(4,"![All screens](overview.png)"); md.insert(5,"")
open(os.path.join(OUT,"README.md"),"w").write("\n".join(md)+"\n")
print(n,"screens")
