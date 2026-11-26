"""External exact-TOI labels: check positive queries are retained by broad phase.
This is query-level recall, not full-scene timing or narrow-phase accuracy.
"""
import csv,json,hashlib,re
from fractions import Fraction
from pathlib import Path
import argparse,subprocess
SCENES=('armadillo-rollers','cloth-ball','cloth-funnel','n-body-simulation','puffer-ball','rod-twist')
def run(root,exe,out):
    out.mkdir(parents=True,exist_ok=False);manifest=[];total=0
    for scene in SCENES:
        files=sorted((root/scene/'queries').glob('*.csv'))
        for kind in ('ee','vf'):
            candidates=[p for p in files if re.search(kind+r'\.csv$',p.name)]
            if not candidates:
                manifest.append(dict(scene=scene,kind=kind,status='missing'));continue
            for p in candidates[:2]:
                label=root/scene/'mma_bool'/(p.stem+'_mma_bool.json')
                labels=json.loads(label.read_text())
                assert isinstance(labels,list) and all(type(x) is bool for x in labels)
                # 128 positive and 128 negative queries maximum, evenly spaced in each class.
                selected={}
                for value in (True,False):
                    ids=[i for i,x in enumerate(labels) if x==value];n=min(128,len(ids))
                    for j in range(n):selected[ids[j*len(ids)//n]]=value
                records=[];line_count=0;block=[]
                with p.open() as f:
                    for row in csv.reader(f):
                        if not row:continue
                        line_count+=1;block.append(row)
                        if len(block)!=8:continue
                        i=line_count//8-1
                        if i in selected:
                            pts=[tuple(Fraction(int(row[k]),int(row[k+1])) for k in (0,2,4)) for row in block]
                            a=[0,1,4,5] if kind=='ee' else [0,4]
                            b=[2,3,6,7] if kind=='ee' else [1,2,3,5,6,7]
                            overlap=all(max(pts[j][k] for j in a)>=min(pts[j][k] for j in b) and max(pts[j][k] for j in b)>=min(pts[j][k] for j in a) for k in range(3))
                            assert not selected[i] or overlap,('published positive outside exact rational AABB',p,i)
                            records.append((i,selected[i],overlap,pts))
                        block=[]
                assert not block and line_count==8*len(labels) and len(records)==len(selected)
                data=out/(scene+'_'+p.stem+'.txt')
                with data.open('w') as f:
                    for i,yes,overlap,pts in records:
                        f.write(f'{i} {kind} {int(yes)} {int(overlap)} '+' '.join(format(float(x),'.17g') for v in pts for x in v)+'\n')
                result=subprocess.run([str(exe),str(data)],capture_output=True,text=True)
                (out/(scene+'_'+p.stem+'.csv')).write_text(result.stdout)
                (out/(scene+'_'+p.stem+'.log')).write_text(result.stderr)
                assert result.returncode==0,(p,result.stderr)
                rr=list(csv.DictReader(result.stdout.splitlines()))
                assert len(rr)==len(records)
                assert all(int(r['published_positive'])==int(yes) and int(r['query_id'])==i and int(r['rational_aabb'])==int(ov) for r,(i,yes,ov,_) in zip(rr,records))
                manifest.append(dict(scene=scene,kind=kind,file=p.name,status='executed',all_queries=len(labels),selected=len(rr),positives=sum(int(r['published_positive']) for r in rr),rational_aabb=sum(int(r['rational_aabb']) for r in rr),retained=sum(int(r['fused']) for r in rr),source_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),label_sha256=hashlib.sha256(label.read_bytes()).hexdigest()))
                total+=len(rr)
                (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    assert total>0,'no external queries executed'
    print(json.dumps(manifest,indent=2))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('root',type=Path);p.add_argument('exe',type=Path);p.add_argument('out',type=Path);a=p.parse_args();run(a.root,a.exe.resolve(),a.out)
