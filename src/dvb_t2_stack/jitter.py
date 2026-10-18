#!/usr/bin/env python3
"""jitter.py dump.ts capacity_bps - PCR timing error of each PCR packet vs its slot in the T2 stream."""
import sys, numpy as np
d=np.fromfile(sys.argv[1],dtype=np.uint8); cap=float(sys.argv[2]); n=len(d)//188; p=d[:n*188].reshape(n,188)
has=((p[:,3]&0x20)!=0)&(p[:,4]>0)&((p[:,5]&0x10)!=0); idx=np.nonzero(has)[0]
pid=((p[idx,1].astype(int)&0x1f)<<8)|p[idx,2]; idx=idx[pid==pid[0]]
b=p[idx,6:12].astype(np.int64); base=(b[:,0]<<25)|(b[:,1]<<17)|(b[:,2]<<9)|(b[:,3]<<1)|(b[:,4]>>7)
pcr=(base*300+(((b[:,4]&1)<<8)|b[:,5]))/27e6
t_slot=idx*188*8/cap
err=(t_slot-pcr); err-=np.median(err); keep=t_slot>5.0   # skip start-up
e=err[keep]*1e3
print(f"PCRs {keep.sum()}  timing error vs slot: p2p {e.max()-e.min():.1f} ms, rms {e.std():.2f} ms, drift {np.polyfit(t_slot[keep],err[keep],1)[0]*1e6:.1f} ppm")
