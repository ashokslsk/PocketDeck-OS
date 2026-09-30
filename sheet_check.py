import sys,os,glob
from PIL import Image
SP="/private/tmp/claude-501/-Users-ashok-Library-Application-Support-Claude-scratch-workspaces-9fbb4d3d-2663-4dcd-b22b-654bf7582786-e533bd95-93c8-45a5-94c8-6fab1c9482d0-scratch-2026-09-26-a24ec2/edfb3f92-85f7-4810-8e22-67e88b4e7394/scratchpad"
sec=sys.argv[1]; start=int(sys.argv[2]); n=int(sys.argv[3]); out=sys.argv[4]
fs=sorted(glob.glob(f"{SP}/simrun/walk_{sec}/shots/*.png"))[start:start+n]
ims=[Image.open(f).convert('RGB').resize((352,528)) for f in fs]
s=Image.new('RGB',(362*len(ims)-10,528),'white')
for i,im in enumerate(ims): s.paste(im,(i*362,0))
s.save(out); print([os.path.basename(f) for f in fs])
