#!/usr/bin/env python3
"""Exact cross-body region-path certificate for linear stored-coordinate motion.

A floating-point witness is only a suggestion: all endpoint projections and
margin tests are exact rational arithmetic. No sampled clearance is accepted.
Unresolved intervals are never called safe. Not a self-contact or orientation gate.
"""
import argparse
import csv
from fractions import Fraction as Q
from itertools import product
import json
from pathlib import Path
import numpy as np
from scipy.spatial import ConvexHull, QhullError


def rational_points(x):
    return [tuple(Q(float(c)) for c in row) for row in x]


def axis_witness(a,b):
    try:
        ha,hb=ConvexHull(a),ConvexHull(b)
        def edges(p,h):
            pairs={tuple(sorted((int(i),int(j)))) for tri in h.simplices for i,j in ((tri[0],tri[1]),(tri[1],tri[2]),(tri[2],tri[0]))}
            return np.array([p[j]-p[i] for i,j in sorted(pairs)])
        ea,eb=edges(a,ha),edges(b,hb)
        axes=np.vstack((np.eye(3),ha.equations[:,:3],hb.equations[:,:3],np.cross(ea[:,None,:],eb[None,:,:]).reshape(-1,3)))
    except QhullError:
        axes=np.eye(3)
    norms=np.linalg.norm(axes,axis=1)
    good=np.isfinite(norms)&(norms>0)
    axes=axes[good]/norms[good,None]
    axes=np.vstack((axes,-axes))
    gap=(b@axes.T).min(axis=0)-(a@axes.T).max(axis=0)
    return axes[int(np.argmax(gap))]


def endpoint_gap(a,b,axis):
    def dot(p):return sum((x*y for x,y in zip(p,axis)),Q(0))
    return min(map(dot,b))-max(map(dot,a))


def certify(a0,a1,b0,b1,margin=0.,max_depth=10):
    arrays=[np.asarray(x,dtype=float) for x in (a0,a1,b0,b1)]
    if any(x.ndim!=2 or x.shape[1]!=3 or len(x)==0 or not np.isfinite(x).all() for x in arrays):raise ValueError('invalid geometry')
    if arrays[0].shape!=arrays[1].shape or arrays[2].shape!=arrays[3].shape:raise ValueError('topology changed')
    if not np.isfinite(margin) or margin<0 or not 0<=max_depth<=20:raise ValueError('invalid gate parameters')
    m=Q(float(margin)); visited=0; deepest=0
    def midpoint(a,b):return [tuple((x+y)/2 for x,y in zip(p,q)) for p,q in zip(a,b)]
    def f(a):return np.asarray(a,dtype=float)
    def rec(a,c,b,d,depth):
        nonlocal visited,deepest
        visited+=1;deepest=max(deepest,depth)
        am,bm=midpoint(a,c),midpoint(b,d)
        # Cheap coordinate witnesses are attempted before expensive hull search.
        candidates=[*np.eye(3),*-np.eye(3)]
        def valid(w):
            axis=tuple(Q(float(x)) for x in w);norm2=sum(x*x for x in axis)
            if norm2==0:return False
            gap=min(endpoint_gap(a,b,axis),endpoint_gap(c,d,axis))
            return gap>0 and gap*gap>m*m*norm2
        if any(valid(w) for w in candidates):return True
        for x,y in ((am,bm),(a,b),(c,d)):
            if valid(axis_witness(f(x),f(y))):return True
        if depth==max_depth:return False
        # If the midpoint has no floating witness, it may be intersecting.
        # This is only a fail-closed early exit, never permission to certify.
        w=axis_witness(f(am),f(bm));axis=tuple(Q(float(x)) for x in w)
        if endpoint_gap(am,bm,axis)<=0:return False
        return rec(a,am,b,bm,depth+1) and rec(am,c,bm,d,depth+1)
    safe=rec(*[rational_points(x) for x in arrays],0)
    return safe,visited,deepest


def read_trace(path):
    with path.open() as stream:
        def row(count,cast):
            fields=stream.readline().split()
            if len(fields)!=count:raise ValueError('truncated or ragged trace')
            return [cast(x) for x in fields]
        while True:
            line=stream.readline()
            if not line:break
            sid,n,cells,steps,na=map(int,line.split())
            if not (0<na<n and cells>0 and steps>0):raise ValueError('invalid trace header')
            topology=np.array([row(4,int) for _ in range(cells)])
            if topology.min()<0 or topology.max()>=n:raise ValueError('invalid topology')
            states=[np.array([row(3,float) for _ in range(n)]) for _ in range(steps+1)]
            yield sid,na,topology,states


def audit(trace,regions,output,margin=0.0005):
    meta=json.loads(regions.read_text());by_id={x['scene']:x for x in meta}
    if len(by_id)!=len(meta):raise ValueError('duplicate region metadata')
    seen=set();rows=[]
    for sid,na,cells,states in read_trace(trace):
        if sid in seen:raise ValueError('duplicate scene')
        seen.add(sid);data=by_id[sid];groups=[];covered=[]
        body_a=np.all(cells<na,axis=1);body_b=np.all(cells>=na,axis=1)
        if not np.all(body_a|body_b):raise ValueError('cross-body tet')
        for key,mask,offset in [('regions_a',body_a,0),('regions_b',body_b,na)]:
            local=cells[mask]-offset;ids=[];nodes=[]
            for region in data[key]:
                c=region['cells'];p=region['nodes']
                if not c or min(c)<0 or max(c)>=len(local):raise ValueError('invalid region cell')
                if len(p)!=len(set(p)) or set(p)!=set(local[c].ravel()):raise ValueError('invalid region nodes')
                ids.extend(c);nodes.append(np.array(p)+offset)
            if sorted(ids)!=list(range(len(local))):raise ValueError('incomplete region coverage')
            groups.append(nodes)
        for step,(x,y) in enumerate(zip(states,states[1:])):
            for (i,a),(j,b) in product(enumerate(groups[0]),enumerate(groups[1])):
                safe,visited,depth=certify(x[a],y[a],x[b],y[b],margin)
                rows.append((sid,step,i,j,int(safe),visited,depth))
    if seen!=set(by_id) or not rows:raise ValueError('missing scenes')
    with output.open('w',newline='') as f:
        writer=csv.writer(f);writer.writerow(['scene','step','region_a','region_b','certified','visited','depth']);writer.writerows(rows)
    print(json.dumps({'region_intervals':len(rows),'certified':sum(r[4] for r in rows),'unresolved':sum(not r[4] for r in rows),'max_depth':max(r[6] for r in rows),'margin':margin}))
    return all(r[4] for r in rows)


def self_test():
    a=np.array(list(product([0.,1.],repeat=3)))
    assert certify(a,a,a+[2,0,0],a+[2,0,0],.5)[0]
    assert not certify(a,a,a+[2,0,0],a+[-2,0,0],0.)[0] # tunnelling
    assert not certify(a,a,a+[1,0,0],a+[1,0,0],0.)[0] # touching
    assert not certify(a,a,a+[2,0,0],a+[2,0,0],1.)[0] # strict margin
    assert certify(a,a,a+[2,0,0],a+[2,0,0],np.nextafter(1.,0.))[0]
    rng=np.random.default_rng(4217)
    for _ in range(100):
        r,_=np.linalg.qr(rng.normal(size=(3,3)));t=rng.uniform(-10,10,3)
        left=a@r+t;right=(a+[2,0,0])@r+t;delta=rng.uniform(-.2,.2,3)
        assert certify(left,left+delta,right,right+delta,.5)[0]
        assert not certify(left,left,right,(a+[-2,0,0])@r+t,0.)[0]
    print('PASS: 205 deterministic positive/negative certificate controls')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--self-test',action='store_true');p.add_argument('--trace',type=Path);p.add_argument('--regions',type=Path);p.add_argument('--output',type=Path);p.add_argument('--margin',type=float,default=.0005);a=p.parse_args()
    if a.self_test:self_test()
    if a.trace:
        if not a.regions or not a.output:p.error('--trace requires --regions and --output')
        if not audit(a.trace,a.regions,a.output,a.margin):raise SystemExit(1)
    elif not a.self_test:p.error('select --self-test or --trace')
