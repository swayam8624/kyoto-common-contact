#!/usr/bin/env python3
"""Run the outstanding Kyoto physical gates; retain all logs and failures.

Default requires real Metal hardware and the pinned IPC build. A CPU-only
run is intentionally not a complete milestone. Do not overwrite receipts.
"""
import argparse
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path.cwd())
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--width', type=int, choices=(1, 4, 16, 32, 64), default=64)
    parser.add_argument('--parallel', type=int, default=min(4, os.cpu_count() or 1))
    parser.add_argument('--timeout', type=int, default=7200)
    parser.add_argument('--without-metal', action='store_true', help='Record GPU as blocked; final status is incomplete')
    args = parser.parse_args()
    if args.parallel < 1 or args.timeout < 1:
        parser.error('parallel and timeout must be positive')
    source, out = args.source.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    manifest = {'source': str(source), 'platform': platform.platform(), 'max_width': args.width,
                'stages': {}, 'all_listed_gates_passed': False}
    target = out / 'campaign_manifest.json'
    def save():
        temp = target.with_suffix('.tmp')
        temp.write_text(json.dumps(manifest, indent=2)+'\n')
        temp.replace(target)
    def mark(name, status, detail):
        manifest['stages'][name] = {'status': status, 'detail': detail}
        save(); print(name+': '+status, flush=True)
    def run(name, command):
        log = out / (name+'.log')
        begin = time.monotonic()
        with log.open('w', encoding='utf-8') as f:
            f.write('command: '+json.dumps(list(map(str, command)))+'\n'); f.flush()
            try:
                code = subprocess.run(list(map(str, command)), cwd=source, stdout=f, stderr=subprocess.STDOUT,
                                      timeout=args.timeout, check=False).returncode
            except (OSError, subprocess.TimeoutExpired) as e:
                f.write('\n'+str(e)+'\n'); code = 124 if isinstance(e, subprocess.TimeoutExpired) else 127
        status = 'passed' if code == 0 else 'blocked_device' if code == 77 else 'failed'
        mark(name, status, {'exit_code': code, 'log': str(log), 'seconds': time.monotonic()-begin})
        return code == 0
    run('revision', ['git', 'rev-parse', 'HEAD'])
    run('compiler', ['c++', '--version'])
    gates = [sys.executable, source/'tools/validate_native_research.py', '--source', source,
             '--output', out/'regressions', '--exact', '--ipc', '--parallel', str(args.parallel), '--timeout', str(args.timeout)]
    if not args.without_metal:
        gates += ['--metal']
    # One command may time out before all tests finish. Separate full
    # campaign builds still run; partial regression manifest is retained.
    run('regressions', gates)
    native = out/'campaign-build'
    configured = run('campaign_configure', ['cmake','-S',source,'-B',native,'-DCMAKE_BUILD_TYPE=Release','-DKYOTO_WITH_EXACT_SELF_VOLUME=ON'])
    campaigns = [
        ('persistent', 'kyoto_pass36_paid_persistent_fem_abba', [], 'native_paid_cold_warm_abba.csv'),
        ('cpu', 'kyoto_pass37_cpu_physical_scaling', [str(args.width)], 'matched_cpu_candidate_scaling.csv'),
        ('floor', 'kyoto_pass39_native_floor_replay', [], 'exact_chords.csv'),
        ('metal', 'kyoto_pass37_metal_physical_scaling', [str(args.width)], 'metal_paid_candidate_scaling.csv'),
    ]
    for name, exe, extra, required in campaigns:
        if name == 'metal' and args.without_metal:
            mark(name, 'blocked_device', 'explicitly excluded; no GPU claim'); continue
        if not configured:
            mark(name, 'blocked_configuration', 'campaign CMake configuration failed'); continue
        if not run(name+'_build', ['cmake','--build',native,'--target',exe,'--parallel',str(args.parallel)]):
            mark(name, 'blocked_build', 'target compilation failed'); continue
        dst = out/name
        if run(name, [native/exe, dst, *extra]):
            p = dst/required
            if not p.is_file() or p.stat().st_size == 0:
                mark(name, 'failed', 'required raw receipt missing or empty')
    if manifest['stages'].get('cpu', {}).get('status') == 'passed':
        run('cpu_summary', [sys.executable,source/'tools/summarize_pass37_scaling.py',out/'cpu/matched_cpu_candidate_scaling.csv',out/'cpu/paired_summary.json'])
    else:
        mark('cpu_summary', 'blocked_source', 'CPU source campaign not passed')
    if manifest['stages'].get('floor', {}).get('status') == 'passed':
        run('floor_independent', [sys.executable,source/'tools/pass39_independent_replay.py',out/'floor',out/'independent.csv'])
        run('floor_visuals', [sys.executable,source/'tools/pass39_visualize.py',out/'floor',out/'visuals'])
    else:
        mark('floor_independent','blocked_source','floor source not passed')
        mark('floor_visuals','blocked_source','floor source not passed')
    ipc_source = out/'regressions/build-ipc/pass34-connected-ipc'
    ipc_log = out/'regressions/ipc/kyoto_pass34_connected_ipc_audit.test.log'
    if ipc_log.is_file() and '100% tests passed' in ipc_log.read_text(errors='replace'):
        run('coupled_visuals', [sys.executable,source/'tools/render_pass34_leaf_evidence.py',ipc_source,out/'coupled-visuals'])
    else:
        mark('coupled_visuals','blocked_source','pinned IPC accepted/rejected leaf audit did not pass')
    manifest['all_listed_gates_passed'] = all(s['status']=='passed' for s in manifest['stages'].values()) and not args.without_metal and args.width==64
    save()
    print('ALL LISTED GATES PASSED' if manifest['all_listed_gates_passed'] else 'INCOMPLETE: inspect campaign_manifest.json', flush=True)
    return 0 if manifest['all_listed_gates_passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
