"""Verify audit fails closed on malformed eleven-arm receipts."""
import csv,json,sys,tempfile
from pathlib import Path
from pass59_analyze import read

def run(path):
    with path.open() as f:rows=list(csv.DictReader(f));fields=list(rows[0])
    cases=[('missing_record',lambda x:x[1:]),('duplicate_record',lambda x:x+[x[0]]),('candidate_drift',lambda x:[dict(x[0],candidates=str(int(x[0]['candidates'])+1)),*x[1:]]),('wrong_slot',lambda x:[dict(x[0],slot='1'),*x[1:]]),('zero_timing',lambda x:[dict(x[0],ns='0'),*x[1:]])]
    results=[]
    for label,mutate in cases:
        with tempfile.TemporaryDirectory() as d:
            q=Path(d)/'bad.csv'
            with q.open('w') as f:
                w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(mutate(rows))
            try:read(q,11)
            except AssertionError:results.append(dict(control=label,rejected=True))
            else:raise AssertionError(label+' accepted')
    return results
if __name__=='__main__':print(json.dumps(run(Path(sys.argv[1])),indent=2))
