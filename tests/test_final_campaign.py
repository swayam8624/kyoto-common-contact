#!/usr/bin/env python3
"""Regression tests for receipt validation and fail-closed orchestration."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import research_closeout_report as report
from run_final_campaign import Campaign


class ReceiptTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def write(self,name,rows):
        p=self.root/name
        with p.open('w',newline='') as f:
            w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
        return p
    def metal(self):
        return [dict(width=1,near=near,block=b,leaf=leaf,slot=slot,arm=arm,ns=100*(slot+1),candidates=7)
                for near in (0,1) for b in range(8) for leaf in range(4)
                for slot,arm in enumerate(report.METAL)]
    def test_complete_metal(self):
        c=report.matched(self.write('m.csv',self.metal()),'metal',1)
        self.assertEqual(c['rows'],256);self.assertEqual(c['groups'],64)
        self.assertEqual(c['paired_geomean']['fused_sap'],1.)
        self.assertAlmostEqual(c['paired_geomean']['spatial_first'],2.)
    def test_reject_truncated_and_duplicate_and_wrong_counts(self):
        for mutate in (lambda x:x[:-1],lambda x:x+[x[0]],lambda x:[dict(x[0],candidates=8),*x[1:]],lambda x:[dict(x[0],ns='nan'),*x[1:]],lambda x:[dict(x[0],slot=1),*x[1:]],lambda x:[dict(x[0],block=-1),*x[1:]]):
            with self.subTest(mutation=mutate):
                with self.assertRaises(ValueError):report.matched(self.write('m.csv',mutate(self.metal())),'metal',1)
    def test_reject_entire_missing_frame(self):
        rows=[r for r in self.metal() if r['leaf']!=2]
        with self.assertRaises(ValueError):report.matched(self.write('m.csv',rows),'metal',1)
    def test_reject_ragged_csv(self):
        p=self.root/'r.csv';p.write_text('a,b\n1\n')
        with self.assertRaises(ValueError):report.read_csv(p,('a','b'))
    def test_replay_coverage_and_event_rejection(self):
        rows=[dict(scene=s,step=t,skin=p,certified=1,exact_pairs=3,vf=5,ee=7,self_hits=0,cross_hits=0,missing=0) for s in range(4) for t in range(32) for p in (0.,.0005)]
        self.assertEqual(report.replay(self.write('r.csv',rows),4)['exact_pairs'],384)
        for mutation in (rows[:-1],[dict(rows[0],missing=1),*rows[1:]], [dict(rows[0],certified=0),*rows[1:]]):
            with self.assertRaises(ValueError):report.replay(self.write('r.csv',mutation),4)


class RunnerTests(unittest.TestCase):
    def test_failures_timeout_and_archive(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);out=root/'out';args=argparse.Namespace(source=root,output=out,width=1,timeout=1,without_ipc=True,without_metal=True)
            c=Campaign(args)
            self.assertTrue(c.run('real_success',[sys.executable,'-c','print("done")'],validate=lambda p:report.require(p.read_text().strip()=='done','wrong output')))
            self.assertFalse(c.run('process_failure',[sys.executable,'-c','raise SystemExit(9)']))
            self.assertFalse(c.run('device_skip',[sys.executable,'-c','raise SystemExit(77)']))
            self.assertFalse(c.run('receipt_failure',[sys.executable,'-c','print("incomplete")'],validate=lambda p:report.require(False,'receipt incomplete')))
            self.assertFalse(c.run('timeout',[sys.executable,'-c','import time; time.sleep(3)']))
            c.block('unavailable','not present')
            self.assertEqual(c.manifest['stages']['timeout']['detail']['exit_code'],124)
            self.assertEqual(c.manifest['stages']['device_skip']['status'],'blocked_device')
            (c.work/'exclude.bin').write_bytes(b'large build output')
            c.save();report.write_dashboard(out,c.manifest);c.archive()
            with zipfile.ZipFile(out/'research-evidence.zip') as z:
                self.assertIsNone(z.testzip());self.assertNotIn('_work/exclude.bin',z.namelist())
                sums=z.read('SHA256SUMS').decode().splitlines()
                for line in sums:
                    digest,name=line.split('  ',1);self.assertEqual(hashlib.sha256(z.read(name)).hexdigest(),digest)
                m=json.loads(z.read('campaign_manifest.json'));self.assertFalse(m['all_listed_gates_passed']);self.assertEqual(m['stages']['receipt_failure']['status'],'failed')

if __name__=='__main__':unittest.main()
