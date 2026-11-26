#!/usr/bin/env python3
"""One-command Kyoto research campaign. Build, execute, validate, plot, archive.

The default is the full width-64 campaign, stock pinned IPC, physical Metal,
all registered CTest regressions, four independent dynamics families, and
paid benchmarks. No previously archived measurement is silently reused.
A failed/blocked stage cannot become a success via report generation.
Run in a clean tracked checkout; generated output is always a new folder.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import shutil
import signal
import subprocess
import sys
import time
import traceback
import xml.etree.ElementTree as ET
import zipfile

import research_closeout_report as report


class Campaign:
    def __init__(self, args):
        self.args = args
        self.source = args.source.resolve()
        self.out = args.output.resolve()
        self.out.mkdir(parents=True, exist_ok=False)
        self.logs = self.out/'logs'; self.logs.mkdir()
        self.data = self.out/'raw'; self.data.mkdir()
        self.work = self.out/'_work'; self.work.mkdir()
        self.env = dict(os.environ, OPENBLAS_NUM_THREADS='1', OMP_NUM_THREADS='1',
                        MKL_NUM_THREADS='1', MPLBACKEND='Agg', PYTHONUNBUFFERED='1')
        self.manifest = {'schema_version': 2, 'started_utc': datetime.now(timezone.utc).isoformat(),
            'platform':platform.platform(),'machine':platform.machine(),'source':str(self.source),
            'max_width':args.width,'stages':{},'comparisons':{},'receipts':{},'physics_csv':{},
            'all_listed_gates_passed':False,
            'scope':'Defined experimental gates only; not proof of universal contact handling or speed superiority.'}
        self.save()

    def save(self):
        p=self.out/'campaign_manifest.json';tmp=p.with_suffix('.tmp')
        tmp.write_text(json.dumps(self.manifest,indent=2)+'\n',encoding='utf-8');tmp.replace(p)

    def mark(self, name, status, detail=None, seconds=0):
        self.manifest['stages'][name]={'status':status,'seconds':seconds,'detail':detail}
        self.save();print(f'[{status.upper():>21}] {name} ({seconds:.1f}s)',flush=True)
        return status=='passed'

    def block(self,name,why):
        return self.mark(name,'blocked',why)

    def run(self,name,argv,stdout=None,validate=None,cwd=None):
        """Run one command with separate stdout/stderr and atomic stage records.

        Timeout kills the entire process group (including build children).
        Partial output remains inspectable but is never marked passed.
        """
        argv=[str(a) for a in argv];out=Path(stdout) if stdout else self.logs/f'{name}.stdout.log'
        err=self.logs/f'{name}.stderr.log';out.parent.mkdir(parents=True,exist_ok=True)
        detail={'command':argv,'stdout':str(out.relative_to(self.out)),
                'stderr':str(err.relative_to(self.out))}
        print(f'\n[RUN] {name}: '+ ' '.join(argv),flush=True);start=time.monotonic();code=None
        self.manifest['stages'][name]={'status':'running','detail':detail,'seconds':0};self.save()
        try:
            with out.open('w',encoding='utf-8') as fo,err.open('w',encoding='utf-8') as fe:
                p=subprocess.Popen(argv,cwd=cwd or self.source,env=self.env,stdout=fo,stderr=fe,
                                   start_new_session=(os.name=='posix'))
                try:code=p.wait(timeout=self.args.timeout)
                except (subprocess.TimeoutExpired,KeyboardInterrupt) as exc:
                    if os.name=='posix':
                        try:os.killpg(p.pid,signal.SIGTERM)
                        except ProcessLookupError:pass
                    else:p.terminate()
                    try:p.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        if os.name=='posix':
                            try:os.killpg(p.pid,signal.SIGKILL)
                            except ProcessLookupError:pass
                        else:p.kill()
                        p.wait()
                    if isinstance(exc,KeyboardInterrupt):
                        detail['error']='interrupted';self.mark(name,'interrupted',detail,time.monotonic()-start);raise
                    detail['error']='command timed out';code=124
            detail['exit_code']=code
            if code==77:return self.mark(name,'blocked_device',detail,time.monotonic()-start)
            if code!=0:return self.mark(name,'failed',detail,time.monotonic()-start)
            if validate is not None:
                evidence=validate(out)
                if evidence is not None:detail['validation']=evidence
            return self.mark(name,'passed',detail,time.monotonic()-start)
        except KeyboardInterrupt:raise
        except Exception as exc:
            detail['exit_code']=code;detail['error']=f'{type(exc).__name__}: {exc}'
            with err.open('a',encoding='utf-8') as fe:fe.write('\n'+traceback.format_exc())
            return self.mark(name,'failed',detail,time.monotonic()-start)

    def configure(self,name,ipc,exact):
        build=self.work/name
        opts=['-DCMAKE_BUILD_TYPE=Release','-DKYOTO_WITH_OFFICIAL_IPC='+('ON' if ipc else 'OFF'),
              '-DKYOTO_WITH_EXACT_SELF_VOLUME='+('ON' if exact else 'OFF'),'-DIPC_TOOLKIT_WITH_SIMD=OFF']
        # Homebrew installs Boost outside the compiler's default include
        # roots on ARM macOS. Use its actual installed prefix if available;
        # never modify system headers or install packages globally.
        if exact and platform.system()=='Darwin' and shutil.which('brew'):
            try:
                prefix=subprocess.check_output(['brew','--prefix','boost'],text=True,timeout=20).strip()
                if (Path(prefix)/'include/boost/multiprecision/cpp_int.hpp').is_file():
                    opts+=['-DBOOST_ROOT='+prefix,'-DBoost_INCLUDE_DIR='+str(Path(prefix)/'include')]
            except (OSError,subprocess.SubprocessError):pass
        if self.run(name+'_configure',['cmake','-S',self.source,'-B',build,*opts]):return build
        return None

    def build_all(self,build):
        return self.run('build_all',['cmake','--build',build,'--parallel',self.args.parallel])

    def executable(self,build,target):
        for p in (build/target,build/'Release'/target,build/(target+'.exe'),build/'Release'/(target+'.exe')):
            if p.is_file():return p
        return None

    def app(self,name,target,build,args=(),stdout=None,validate=None):
        if build is None:return self.block(name,'required CMake configuration unavailable')
        exe=self.executable(build,target)
        if exe is None:
            if not self.run(name+'_build',['cmake','--build',build,'--target',target,'--parallel',self.args.parallel]):
                return self.block(name,'target build failed')
            exe=self.executable(build,target)
            if exe is None:return self.block(name,'built executable missing')
        return self.run(name,[exe,*args],stdout,validate)

    def python_env(self):
        # The standard-library runner can always record build/device failures.
        # Use current environment if all required scientific packages exist;
        # otherwise create a private venv and install only missing dependencies
        # in that venv. The user's global Python environment is untouched.
        modules=('numpy','scipy','casadi','matplotlib','PIL','pandas')
        missing=[m for m in modules if importlib.util.find_spec(m) is None]
        if not missing:
            self.mark('python_dependencies','passed',{'python':sys.executable,'missing':[]});return Path(sys.executable)
        if self.args.no_install:
            self.block('python_dependencies',f'missing modules: {missing}; --no-install selected');return None
        venv=self.work/'venv'
        if not self.run('python_venv',[sys.executable,'-m','venv',venv]):
            self.block('python_dependencies','isolated venv creation failed');return None
        py=venv/('Scripts/python.exe' if os.name=='nt' else 'bin/python')
        if not self.run('python_dependencies',[py,'-m','pip','install','numpy','scipy','casadi==3.7.2','matplotlib','Pillow','pandas']):return None
        return py

    def audit_tests(self,build,ipc,exact,metal):
        if build is None:return self.block('ctest','CMake configure failed')
        xml=self.raw('ctest.xml')
        cmd=['ctest','--test-dir',build,'--output-on-failure','--no-tests=error','--parallel','1','--output-junit',xml,'--build-config','Release']
        if not metal:cmd+=['-E','[Mm]etal']
        def verify(_):
            root=ET.parse(xml).getroot();tests=root.findall('.//testcase');names={t.attrib.get('name') for t in tests}
            required={'fused_axis_sap_dynamic_topology_and_recovery','fused_native_solver_smoke','persistent_typed_FEM_physical_parity','typed_connected_FEM_self_contact','synchronized_two_body_FEM_contact_and_rollback'}
            if exact:required|={'exact_transaction_native_smoke','exact_continuous_native_FEM_leaf_volume'}
            if ipc:required|={'pinned_IPC_LBVH_inside_native_typed_FEM','pinned_official_ipc_lbvh_feature_coverage','pinned_IPC_connected_FEM_accepted_and_rejected_CCD','long_coupled_ipc_wrapper_and_native_candidate_parity'}
            if metal:required|={'physical_Metal_tiled_mask_smoke','physical_Metal_solver_bridge_smoke'}
            report.require(tests and required<=names,'required regression tests missing: '+str(required-names))
            bad=[t.attrib.get('name') for t in tests if t.find('skipped') is not None or t.find('failure') is not None or t.find('error') is not None or t.attrib.get('status') not in (None,'run','passed')]
            report.require(not bad,'skipped or failed CTest rows: '+str(bad))
            self.manifest['receipts']['ctest']={'tests':len(tests),'failed':0,'skipped':0};return self.manifest['receipts']['ctest']
        return self.run('ctest',cmd,validate=verify)

    def raw(self,name):return self.data/name
    def record(self,name,value):self.manifest['receipts'][name]=value;return value

    def comparison(self,name,kind,path,ipc=False):
        c=report.matched(path,kind,self.args.width,ipc)
        c['path']=str(Path(path).relative_to(self.out));self.manifest['comparisons'][name]=c
        return {'observations':c['rows'],'matched_groups':c['groups'],'scenes':c['scenes'],'arms':len(c['arms'])}

    def execute(self):
        a=self.args;w=str(a.width)
        self.run('revision',['git','rev-parse','HEAD'])
        self.run('tracked_source_status',['git','status','--porcelain','--untracked-files=no'],validate=lambda p:report.require(not p.read_text().strip(),'tracked source is dirty'))
        self.run('compiler',['c++','--version']);self.run('cmake_version',['cmake','--version'])
        self.run('system',['uname','-a'])
        py=self.python_env()
        if py:self.run('python_versions',[py,'-c','import sys,numpy,scipy,casadi,matplotlib; print(sys.version); print(numpy.__version__,scipy.__version__,casadi.__version__,matplotlib.__version__)'])
        want_ipc=not a.without_ipc
        want_metal=not a.without_metal and platform.system()=='Darwin'
        # Prefer ONE stock CMake build. If dependency setup fails, keep
        # native/exact work running rather than erase the evidence.
        build=None;ipc=False;exact=False
        if want_ipc:
            build=self.configure('stock_ipc_exact',True,True);ipc=build is not None;exact=ipc
        else:self.block('stock_ipc_exact_configure','explicit --without-ipc; full gate not satisfied')
        if build is None:
            build=self.configure('native_exact',False,True);exact=build is not None
        if build is None:build=self.configure('native',False,False)
        if ipc:
            pin=build/'_deps/kyoto_official_ipc_toolkit-src'
            self.run('stock_ipc_pin',['git','-C',pin,'rev-parse','HEAD'],validate=lambda p:report.require(p.read_text().strip()=='19bcab7e1f5a0d2addfcd28513422ff248ea504e','upstream pin differs'))
            self.run('stock_ipc_clean',['git','-C',pin,'diff','--exit-code','HEAD'])
        if build is not None:
            shutil.copyfile(build/'CMakeCache.txt',self.raw('CMakeCache.txt'))
            self.build_all(build)
        else:self.block('build_all','all configurations failed')
        self.audit_tests(build,ipc,exact,want_metal)
        # Native paid full solver and exact transaction are separately timed.
        self.app('fused_solver','kyoto_pass40_fused_solver',build,[w],self.raw('fused_solver.csv'),lambda p:self.comparison('full_solver','solver',p))
        def exact_check(p):
            log=(self.logs/'exact_transaction.stderr.log').read_text()
            report.require('PASS exact-only post-drift rejection, initial overlap and recovery' in log,'exact rollback controls missing')
            self.manifest['exact_csv']=str(p.relative_to(self.out));return self.record('exact_transaction',report.exact_transaction(p,a.width))
        self.app('exact_transaction','kyoto_pass41_exact_transaction',build if exact else None,[w],self.raw('exact_transaction.csv'),exact_check)
        # Keep the earlier longitudinal audits, negative controls and
        # optimization evidence in the same fresh evidence directory.
        self.app('long_volume_and_direct_ccd','kyoto_pass39_long_physical_audit',build if ipc and exact else None,[],self.raw('long_volume_and_direct_ccd.csv'),lambda p:self.record('long_volume_and_direct_ccd',validate_long(p,True)))
        self.app('long_ipc_wrapper','kyoto_pass39_long_ipc_wrapper',build if ipc else None,[],self.raw('long_ipc_wrapper.csv'),lambda p:self.record('long_ipc_wrapper',validate_long(p,False)))
        self.app('coupled_volume_scaling','kyoto_pass39_coupled_volume_campaign',build if exact else None,[w],self.raw('coupled_volume_scaling.csv'),lambda p:self.record('coupled_volume_scaling',validate_volume(p,a.width)))
        for name,target in [('cross_witness_reuse','kyoto_pass39_cross_witness_reuse'),('cross_volume_sweep','kyoto_pass39_cross_volume_sweep'),('scalar_projection','kyoto_pass39_projection_solver'),('cached_sap_solver','kyoto_pass39_cached_sap_solver')]:
            self.app(name,target,build,[],self.raw(name+'.csv'),lambda p: {'rows':len(report.read_csv(p,('arm','elapsed_ns')))})
        for name,target in [('exact_candidate_abba','kyoto_pass39_exact_volume_abba'),('exact_factored_abba','kyoto_pass39b_factored_axis_abba')]:
            self.app(name,target,build if exact else None,[],self.raw(name+'.csv'),lambda p:{'rows':len(report.read_csv(p,('arm','elapsed_ns')))})
        floor=self.raw('native_floor')
        floor_ok=self.app('native_floor','kyoto_pass39_native_floor_replay',build if exact else None,[floor],validate=lambda _: {'steps':len(report.read_csv(floor/'steps.csv')),'exact_chords':len(report.read_csv(floor/'exact_chords.csv'))})
        if floor_ok and py:
            self.run('floor_independent',[py,self.source/'tools/pass39_independent_replay.py',floor,self.raw('floor_independent.csv')],validate=lambda _:{'rows':len(report.read_csv(self.raw('floor_independent.csv')))})
        else:self.block('floor_independent','native floor evidence or scientific Python unavailable')
        # Separate external dynamics from native integrator: generate fresh
        # trajectories and replay exactly those paths against stock IPC.
        independent=[('two_body','pass40_ipopt_contact.py','kyoto_pass40_ipopt_replay','ipopt_receipts.csv',6,None),
                     ('three_body','pass40_three_body_contact.py','kyoto_pass40_three_body_replay','ipopt_receipts.csv',4,'active_pairs'),
                     ('adaptive','pass40_adaptive_contact.py','kyoto_pass40_three_body_replay','ipopt_receipts.csv',4,'active_pairs'),
                     ('nonconvex','pass40_nonconvex_contact.py','kyoto_pass40_nonconvex_replay','receipts.csv',4,'active_planes')]
        paths={}
        for name,script,target,csvname,scenes,active in independent:
            dst=self.raw(name);trajectory=dst/'trajectory.txt';receipt=dst/csvname
            def check(_):
                report.require(trajectory.is_file() and trajectory.stat().st_size>0,'missing independently generated trajectory')
                stats=report.independent_receipt(receipt,scenes,active)
                self.manifest['physics_csv'][name]=str(receipt.relative_to(self.out));return self.record(name,stats)
            generated=py is not None and self.run(name+'_generate',[py,self.source/'tools'/script,dst],validate=check)
            if not generated:
                if py is None:self.block(name+'_generate','scientific Python dependencies unavailable')
                self.block(name+'_replay','independent generator did not pass');continue
            paths[name]=trajectory
            self.app(name+'_replay',target,build if ipc and exact else None,[trajectory],self.raw(name+'_replay.csv'),lambda p:self.record(name+'_replay',report.replay(p,scenes)))
        if all(k in paths for k in ('adaptive','three_body','nonconvex')):
            # A native-only fallback still measures all eleven methods, but
            # cannot claim the official IPC arm or complete stock campaign.
            target='kyoto_pass41_common_ipc' if ipc else 'kyoto_pass41_common_native'
            self.app('common_broadphase',target,build,[paths['adaptive'],paths['three_body'],paths['nonconvex'],w],self.raw('common_broadphase.csv'),lambda p:self.comparison('common_broadphase','common',p,ipc))
        else:self.block('common_broadphase','one or more independent trajectories unavailable')
        # Run the physical device benchmarks sequentially, outside all CPU
        # timing stages. GPU bridge retains mandatory exhaustive parity and
        # failure-injection controls; do not interpret as full GPU dynamics.
        if want_metal:
            dst=self.raw('metal')
            self.app('metal_query','kyoto_pass40_metal_tiled_mask',build,[dst,w],validate=lambda _:self.comparison('metal_query','metal',dst/'tiled_metal.csv'))
            dst=self.raw('metal_bridge')
            def bridge_check(_):
                log=(self.logs/'metal_solver_bridge.stderr.log').read_text()
                report.require('PASS missing candidate, malformed readback, rollback and mode switching' in log,'device rollback controls missing')
                report.require((dst/'device.txt').read_text().strip(),'device identity missing')
                return self.record('metal_solver_bridge',report.bridge(dst/'metal_solver_bridge.csv',a.width))
            self.app('metal_solver_bridge','kyoto_pass40_metal_solver_bridge',build,[dst,w],validate=bridge_check)
        else:
            self.block('metal_query','physical Metal requires macOS and device; or --without-metal')
            self.block('metal_solver_bridge','physical device bridge not executed')
        if py and 'nonconvex' in paths:
            self.run('nonconvex_geometry',[py,self.source/'tools/render_closeout_geometry.py',paths['nonconvex'],self.out/'geometry'],validate=lambda _:report.require((self.out/'geometry/impact_vs_control.gif').stat().st_size>0,'animation missing'))
        else:self.block('nonconvex_geometry','independent trajectory or Python unavailable')
        return py

    def finish(self,py,interrupted=False):
        # Reporting can be executed by the private Python environment, too.
        # Stage success is separate from scientific success.
        self.manifest['finished_utc']=datetime.now(timezone.utc).isoformat()
        self.manifest['all_listed_gates_passed']=False;self.save()
        if py:
            # Report subprocess receives only fresh validated metadata.
            self.run('report',[py,self.source/'tools/render_closeout_report.py',self.out],validate=lambda _:report.require((self.out/'index.html').is_file(),'offline dashboard missing'))
        else:self.block('report','plotting environment unavailable')
        complete=not interrupted and self.args.width==64 and not self.args.without_ipc and not self.args.without_metal and all(v['status']=='passed' for v in self.manifest['stages'].values())
        self.manifest['all_listed_gates_passed']=complete
        self.manifest['finished_utc']=datetime.now(timezone.utc).isoformat();self.save()
        # Refresh HTML after the report subprocess and final status; do not
        # leave a stale running stage in the offline dashboard.
        try:
            report.write_dashboard(self.out,self.manifest)
            self.archive()
        except Exception as exc:
            self.manifest['all_listed_gates_passed']=False;self.mark('archive','failed',str(exc));complete=False
        report.terminal(self.manifest)
        print('\n'+('ALL DECLARED EXPERIMENTAL GATES PASSED' if complete else 'INCOMPLETE — inspect failed/blocked stages; no missing result was inferred'),flush=True)
        print(f'\nEvidence directory: {self.out}\nDashboard: {self.out/"index.html"}\nArchive: {self.out/"research-evidence.zip"}',flush=True)
        return 0 if complete else 1

    def archive(self):
        # Build/venv artifacts are deliberately excluded; logs, all raw
        # receipts, plots, trajectory inputs and manifest remain included.
        paths=sorted(p for p in self.out.rglob('*') if p.is_file() and '_work' not in p.relative_to(self.out).parts and p.name not in ('SHA256SUMS','research-evidence.zip','research-evidence.tmp'))
        def digest(p):
            h=hashlib.sha256()
            with p.open('rb') as f:
                for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk)
            return h.hexdigest()
        sums=self.out/'SHA256SUMS';sums.write_text(''.join(f'{digest(p)}  {p.relative_to(self.out).as_posix()}\n' for p in paths))
        dst=self.out/'research-evidence.zip';tmp=self.out/'research-evidence.tmp'
        with zipfile.ZipFile(tmp,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
            for p in [*paths,sums]:z.write(p,p.relative_to(self.out).as_posix())
        with zipfile.ZipFile(tmp) as z:report.require(z.testzip() is None,'archive integrity failure')
        tmp.replace(dst)


def validate_long(path,direct):
    rows=report.read_csv(path,('scene','step','leaf','dt'))
    fields=('self0','cross0','missing0','selfskin','crossskin','missingskin') if direct else ('self_hits','cross_hits','missing')
    for r in rows:
        report.require(all(report.integer(r[k],k)==0 for k in fields),'accepted long path event or missing candidate')
        if direct:report.require(r['certified']=='1' and all(int(r[k])==0 for k in ('orientation_unknown','unknown0','unknown1','unknown2','unknown3')),'uncertified long volume')
    steps={int(r['scene']):set() for r in rows}
    for r in rows:steps[int(r['scene'])].add(int(r['step']))
    report.require(set(steps)==set(range(6)) and all(v and v==set(range(len(v))) and len(v)>=20 for v in steps.values()),'long trajectory coverage missing')
    return {'rows':len(rows),'scenes':len(steps),'steps':sum(map(len,steps.values())),'ccd_checks':sum(sum(int(r[k]) for k in (('vf0','ee0','vfskin','eeskin') if direct else ('vf','ee'))) for r in rows)}


def validate_volume(path,width):
    rows=report.read_csv(path,('scene','width','step','leaf','certified','orientation_unknown'))
    good=[r for r in rows if r['scene']!='rejected_cross'];bad=[r for r in rows if r['scene']=='rejected_cross']
    report.require(len(bad)==1 and bad[0]['certified']=='0','negative swept volume control absent')
    report.require(all(r['certified']=='1' and all(int(r[k])==0 for k in ('orientation_unknown','unknown0','unknown1','unknown2','unknown3')) for r in good),'unresolved accepted swept volume')
    expect={(s,str(w),str(step)) for w in report.WIDTHS if w<=width for s in ('U_near','U_separated') for step in range(4)}|{('cross_torus','8',str(i)) for i in range(12)}
    report.require({(r['scene'],r['width'],r['step']) for r in good}==expect,'incomplete volume scaling')
    return {'accepted_leaves':len(good),'exact_pairs':sum(sum(int(r[f'test{k}']) for k in range(4)) for r in good),'negative_controls':1}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=Path(__file__).resolve().parent.parent)
    parser.add_argument('--output',type=Path,default=None)
    parser.add_argument('--width',type=int,choices=report.WIDTHS,default=64)
    parser.add_argument('--parallel',type=int,default=min(4,os.cpu_count() or 1))
    parser.add_argument('--timeout',type=int,default=7200,help='seconds per process; timeout is failure, not skipped success')
    parser.add_argument('--without-metal',action='store_true',help='CPU-only; full declared gate remains incomplete')
    parser.add_argument('--without-ipc',action='store_true',help='native-only; full declared gate remains incomplete')
    parser.add_argument('--no-install',action='store_true',help='do not install missing scientific packages into isolated local venv')
    args=parser.parse_args()
    if args.parallel<1 or args.timeout<1:parser.error('parallel and timeout must be positive')
    if not (args.source/'CMakeLists.txt').is_file():parser.error('source must be the Kyoto repository root')
    if args.output is None:args.output=args.source/'output'/('research-final-'+datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    c=Campaign(args);py=None;interrupted=False
    try:py=c.execute()
    except KeyboardInterrupt:interrupted=True;c.mark('interrupted','interrupted','operator stopped campaign')
    except Exception as exc:c.mark('orchestration','failed',str(exc));(c.logs/'orchestration.traceback.log').write_text(traceback.format_exc())
    return c.finish(py or (Path(sys.executable) if importlib.util.find_spec('matplotlib') else None),interrupted)


if __name__=='__main__':raise SystemExit(main())
