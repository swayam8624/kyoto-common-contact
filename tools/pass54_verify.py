"""Verify complete archived runs, exact clock, drift, pair coverage and receipts.
This checks recorded certification receipts, not a fresh geometric oracle.
"""
import argparse,json,math
from fractions import Fraction as Q
from pathlib import Path
import numpy as np


def verify(path):
    s=json.loads((path/'summary.json').read_text())
    leaves=json.loads((path/'leaves.json').read_text())
    attempts=json.loads((path/'attempts.json').read_text())
    with np.load(path/'trajectory.npz') as d:
        x,v,t=d['states'],d['velocities'],d['cells']
    assert s['macros_completed']==s['macros_requested'] and s['reference_parity'] and not s['error'],path
    assert len(attempts)==s['macros_completed'] and all(a['ok'] for a in attempts)
    assert x.shape==v.shape and len(x)==len(leaves)+1==s['total_committed_leaves']+1
    assert np.isfinite(x).all() and np.isfinite(v).all() and len(t)==s['cells']
    assert len(x[0])==s['nodes'] and t.shape==(s['cells'],4)
    pairs=math.comb(len(t),2)
    for a,b,w,r in zip(x,x[1:],v[1:],leaves):
        assert Q(r['dt'])>0 and np.array_equal(b,a+float(Q(r['dt']))*w)
        assert r['certified'] and not r['attempts'][-1]['unresolved'] and r['pair_checks']==pairs
        assert r['j']>.1 and r['kkt']<1e-7 and r['momentum']<1e-6 and r['violation']<1e-7
    assert sum((Q(r['dt']) for r in leaves[:s['main_leaves']]),Q(0))==Q(s['main_time'])==s['macros_completed']*Q(s['dt'])
    assert s['pair_intervals']==pairs*len(leaves)
    if s['rollback'] is not None:
        assert s['rollback'] and s['recovery']
        rollback=json.loads((path/'rollback.json').read_text());recovery=json.loads((path/'recovery.json').read_text())
        assert any(r['path']=='r0' and r['status']=='accepted' for r in rollback)
        assert any(r['path']=='r1' and r['status']!='accepted' for r in rollback)
        assert recovery[0]['status']!='accepted' and sum((Q(r['dt']) for r in recovery if r['status']=='accepted'),Q(0))==Q(s['dt'])
    return s

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('cases',nargs='+',type=Path);a=p.parse_args()
    rows=[verify(x) for x in a.cases]
    print(json.dumps(dict(cases=rows,leaves=sum(r['total_committed_leaves'] for r in rows),pair_intervals=sum(r['pair_intervals'] for r in rows)),indent=2))
