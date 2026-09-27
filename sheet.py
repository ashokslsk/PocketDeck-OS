import sys, glob, os
from PIL import Image
d=sys.argv[1]; cols=int(sys.argv[2]) if len(sys.argv)>2 else 4
fs=sorted(glob.glob(os.path.join(d,'*.bmp')))
ims=[Image.open(f).convert('RGB') for f in fs]
for f,im in zip(fs,ims): im.save(f[:-4]+'.png')
w,h=ims[0].size; s=0.5; tw,th=int(w*s),int(h*s); rows=(len(ims)+cols-1)//cols
sheet=Image.new('RGB',(tw*cols+8*(cols-1),th*rows+8*(rows-1)),'#888')
for i,im in enumerate(ims): sheet.paste(im.resize((tw,th)),((i%cols)*(tw+8),(i//cols)*(th+8)))
sheet.save(os.path.join(d,'sheet.png')); print(len(ims), w, h)
