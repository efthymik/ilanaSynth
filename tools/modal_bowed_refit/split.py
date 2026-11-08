import sys, os, mido, numpy as np, soundfile as sf
NAMES=['C','Db','D','Eb','E','F','Gb','G','Ab','A','Bb','B']
def events(mid):
    m=mido.MidiFile(mid); t=0; on={}; ev=[]
    for msg in m:
        t+=msg.time
        if msg.type=='note_on' and msg.velocity>0: on[msg.note]=(t,msg.velocity)
        elif msg.type in('note_off','note_on') and msg.note in on:
            s,v=on.pop(msg.note); ev.append((s,t-s,msg.note,v))
    return ev
src, mid, out, vel, pad = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]), float(sys.argv[5])
x, sr = sf.read(src, always_2d=True)
os.makedirs(out, exist_ok=True)
n=0
for s,d,note,v in events(mid):
    if v!=vel: continue
    a=int(s*sr); b=min(len(x), int((s+d+pad)*sr))
    nm=NAMES[note%12]+str(note//12-1)
    seg=np.concatenate([np.zeros((int(0.5*sr),x.shape[1])), x[a:b]])
    sf.write(os.path.join(out, "r.%s%s.stereo.wav"%(nm,nm)), seg, sr, subtype="PCM_24")
    n+=1
print(out, n, "notes", sr, "peak dB", round(20*np.log10(np.abs(x).max()+1e-12),1))
