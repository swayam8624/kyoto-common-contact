"""Fail-closed audit for archived mask replay and real Metal physical integration."""
import argparse,csv,json
from collections import defaultdict
from pathlib import Path

def rows(path):
    with path.open(newline='') as f:return list(csv.DictReader(f))

def key(r):return (r['scene'],int(r['width']),r['regime'],int(r['leaf']))

def audit(replay,stock):
    expected={}
    for r in rows(stock):
        k=key(r);n=int(r['candidates'])
        assert expected.setdefault(k,n)==n
    assert len(expected)==300
    actual={};precision=[];totals=defaultdict(int)
    for r in rows(replay):
        k=key(r);n=int(r['candidates'])
        assert n>=0
        if k[0]=='precision':
            assert k[1:3]==(1,'control') and n>0
            precision.append(k[3])
        else:
            assert k not in actual
            actual[k]=n
        for col in ('packed_dispatches','tiled_dispatches','packed_words','tiled_words'):
            v=int(r[col]);assert v>=0
            totals[col]+=v
        assert int(r['packed_dispatches'])>0 and int(r['packed_words'])>0
        assert int(r['tiled_words'])%32==0
        assert bool(int(r['tiled_dispatches']))==bool(int(r['tiled_words']))
    assert sorted(precision)==[0,1,2] and actual==expected
    assert totals['tiled_dispatches']>0 and totals['tiled_words']>0
    return dict(archived_frames=len(actual),precision_controls=len(precision),**totals)

def physical(folder,maximum,log):
    data=rows(folder/'metal_solver_bridge.csv')
    expected={(w,n,0,k) for w in (1,4,16,32,64) if w<=maximum for n in (0,1) for k in range(4)}
    if maximum>=4:expected|={(4,n,1,k) for n in (0,1) for k in range(8)}
    actual=set();totals=defaultdict(int);impulses=defaultdict(int)
    for r in data:
        k=tuple(int(r[x]) for x in ('width','near','rotated','step'))
        assert k not in actual
        actual.add(k)
        for col in ('leaves','self_impulses','calls','keys','tile_slots','tile_jobs','mask_words'):
            n=int(r[col]);assert n>=0;totals[col]+=n
        assert int(r['leaves'])>0 and int(r['calls'])>0
        assert int(r['mask_words'])==32*int(r['tile_jobs'])
        impulses[k[:3]]+=int(r['self_impulses'])
    assert actual==expected and totals['tile_jobs']>0 and totals['mask_words']>0
    for k,n in impulses.items():assert (n>0)==bool(k[1])
    assert (folder/'device.txt').read_text().strip()
    assert 'PASS missing candidate, malformed readback, rollback and mode switching' in log.read_text()
    return dict(physical_steps=len(actual),**totals,rollback_and_switch_controls=True)

def main():
    p=argparse.ArgumentParser();p.add_argument('replay',type=Path);p.add_argument('stock',type=Path);p.add_argument('--device',type=Path);p.add_argument('--bridge',type=Path);p.add_argument('--bridge-log',type=Path);p.add_argument('--width',type=int,default=16);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    result=audit(a.replay,a.stock);result['execution']='scalar-host-only'
    if a.device:
        assert a.bridge and a.bridge_log
        meta=dict(line.split('=',1) for line in a.device.read_text().splitlines())
        assert meta['backend']=='Metal' and meta['completed']=='1' and meta['name'] and int(meta['registry_id'])>0 and int(meta['max_buffer'])>0
        result.update(execution='real-Metal',device=meta,physical=physical(a.bridge,a.width,a.bridge_log))
    else:assert not a.bridge and not a.bridge_log
    a.out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
