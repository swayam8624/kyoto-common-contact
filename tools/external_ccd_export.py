"""Deterministic external mesh replay. Never interpolate missing source frames."""
import argparse, hashlib,json,re
from pathlib import Path
import numpy as np
from plyfile import PlyData

SCENES=('armadillo-rollers','cloth-ball','cloth-funnel','n-body-simulation','puffer-ball','rod-twist')
def mesh(p):
    if p.suffix=='.ply':
        d=PlyData.read(p)
        v=np.column_stack([d['vertex'][k] for k in ('x','y','z')]).astype(np.float64)
        f=np.array([list(x) for x in d['face'].data['vertex_indices']],dtype=np.int64)
    else:
        v=[];f=[]
        for line in p.read_text().splitlines():
            t=line.split()
            if not t:continue
            if t[0]=='v':v.append([float(x) for x in t[1:4]])
            if t[0]=='f':
                ids=[int(x.split('/')[0]) for x in t[1:]]
                assert len(ids)==3 and all(i>0 for i in ids)
                f.append([i-1 for i in ids])
        v=np.array(v,dtype=np.float64);f=np.array(f,dtype=np.int64)
    assert v.ndim==2 and v.shape[1]==3 and len(v) and np.isfinite(v).all()
    assert f.ndim==2 and f.shape[1]==3 and len(f) and f.min()>=0 and f.max()<len(v)
    assert np.all(f[:,0]!=f[:,1]) and np.all(f[:,1]!=f[:,2]) and np.all(f[:,0]!=f[:,2])
    return v,f
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def export(root,out):
    out.mkdir(parents=True,exist_ok=False);rows=[]
    for name in SCENES:
        paths={}
        for p in sorted((root/name/'frames').glob('*')):
            if p.suffix.lower() not in ('.obj','.ply'):continue
            digits=re.findall(r'\d+',p.stem)
            if not digits:raise ValueError(p)
            i=int(digits[-1])
            if i in paths:raise ValueError(('duplicate frame',name,i))
            paths[i]=p
        # Select the first contiguous run of up to four original frames.
        starts=[i for i in sorted(paths) if i+1 in paths]
        if not starts:
            rows.append(dict(scene=name,status='no_consecutive_source_frames',available=sorted(paths)));continue
        i=starts[0];ids=[i]
        while len(ids)<4 and ids[-1]+1 in paths:ids.append(ids[-1]+1)
        frames=[mesh(paths[j]) for j in ids]
        v,f=frames[0]
        if not all(a.shape==v.shape and np.array_equal(g,f) for a,g in frames):
            rows.append(dict(scene=name,status='topology_changed',frames=ids));continue
        for scope in ('patch','full'):
            if scope=='full' and len(f)>50000:
                rows.append(dict(scene=name,scope=scope,status='predeclared_size_cap',faces=len(f)));continue
            # Stable spatial neighborhood at the initial frame, fixed for all subsequent frames.
            selected=np.arange(len(f))
            if scope=='patch' and len(f)>256:
                c=v[f].mean(axis=1);center=np.median(c,axis=0)
                selected=np.sort(np.argsort(np.sum((c-center)**2,axis=1),kind='stable')[:256])
            ff=f[selected]
            # Full scene preserves isolated vertices as well.
            keep=np.arange(len(v)) if scope=='full' else np.unique(ff)
            remap=np.full(len(v),-1,dtype=np.int64);remap[keep]=np.arange(len(keep))
            ff=remap[ff];edges=np.unique(np.sort(np.concatenate((ff[:,[0,1]],ff[:,[1,2]],ff[:,[2,0]])),axis=1),axis=0)
            p=out/(name+'_'+scope+'.txt')
            with p.open('w') as s:
                s.write(f'{len(keep)} {len(edges)} {len(ff)} {len(ids)}\n')
                np.savetxt(s,edges,fmt='%d');np.savetxt(s,ff,fmt='%d')
                for a,_ in frames:np.savetxt(s,a[keep],fmt='%.17g')
            rows.append(dict(scene=name,scope=scope,status='exported',frames=ids,vertices=len(keep),edges=len(edges),faces=len(ff),file=p.name,sha256=digest(p),source=[dict(path=str(paths[j].relative_to(root)),sha256=digest(paths[j])) for j in ids]))
    (out/'manifest.json').write_text(json.dumps(rows,indent=2)+'\n')
    assert any(r['status']=='exported' for r in rows),'no usable external sequences'
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('root',type=Path);p.add_argument('out',type=Path);a=p.parse_args();export(a.root,a.out)
