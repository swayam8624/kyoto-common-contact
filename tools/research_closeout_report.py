#!/usr/bin/env python3
"""Validate matched campaign receipts and render only measured results.

Candidate-query timings and full solver timings have different boundaries;
never pool them. Block sums are paired within an identical scene, then an
unweighted geometric mean of their ratios is reported. Median latencies
are descriptive; they are not confidence intervals or a novelty claim.
"""
from collections import defaultdict
import csv
import html
import json
import math
from pathlib import Path
import statistics

WIDTHS = (1, 4, 16, 32, 64)
COMMON = ('exhaustive', 'rebuilt_sap', 'rebuilt_bvh', 'retained_refit',
          'retained_frontier', 'persistent_x', 'adaptive_axis', 'persistent_axis',
          'cached_axis', 'spatial_first', 'fused')
METAL = ('spatial_first', 'fused_sap', 'metal_packed', 'metal_tiled')
SOLVER = ('retained_bvh', 'cached_sap', 'spatial_first', 'fused')


def require(ok, why):
    if not ok:
        raise ValueError(why)


def read_csv(path, required=()):
    """Reject truncated/ragged CSV rather than reporting a partial pass."""
    with Path(path).open(newline='', encoding='utf-8') as f:
        r = csv.DictReader(f)
        require(r.fieldnames is not None and set(required) <= set(r.fieldnames),
                f'{path}: required columns missing')
        require(len(r.fieldnames) == len(set(r.fieldnames)), f'{path}: duplicate columns')
        rows = list(r)
    require(rows and all(None not in x and all(v is not None for v in x.values()) for x in rows),
            f'{path}: empty or ragged receipt')
    return rows


def number(value, label, positive=False):
    n = float(value)
    require(math.isfinite(n) and (n > 0 if positive else n >= 0), f'{label}: invalid value {value}')
    return n


def integer(value, label):
    n = int(value)
    require(n >= 0, f'{label}: negative integer')
    return n


def matched(path, kind, width, stock_ipc=False):
    """Check coverage, exact arm sets, paired physical counts and warmup exclusion.

    A successful process with an incomplete CSV cannot satisfy this gate.
    Native C++ executables compare *full candidate vectors* before writing;
    this independent parser additionally checks receipt consistency.
    """
    widths = [x for x in WIDTHS if x <= width]
    if kind == 'common':
        arms = COMMON + (('ipc_lbvh',) if stock_ipc else ())
        scene = ('scene',); frame = ('leaf',); time = 'ns'; baseline = 'persistent_axis'
        expected = {(f'{fam}_{i}',) for fam in ('adaptive','fixed','nonconvex') for i in range(4)}
        expected |= {(f'native_{w}_{n}',) for w in widths for n in (0,1)}
        invariants = ('candidates','vertices','edges','faces'); blocks = len(arms)
    elif kind == 'metal':
        arms = METAL; scene = ('width','near'); frame = ('leaf',); time = 'ns'; baseline = 'fused_sap'
        expected = {(str(w),str(n)) for w in widths for n in (0,1)}
        invariants = ('candidates',); blocks = 8
    elif kind == 'solver':
        arms = SOLVER; scene = ('width','regime'); frame = (); time = 'elapsed_ns'; baseline = 'retained_bvh'
        expected = {(str(w),n) for w in widths for n in ('near','separated')}
        invariants = ('macrosteps','leaves','self_impulses','rejections','certificates'); blocks = 4
    else:
        raise ValueError(f'unknown comparison kind: {kind}')
    rows = read_csv(path, (*scene,*frame,'block','slot','arm',time,*invariants))
    groups = defaultdict(list)
    for r in rows:
        key = tuple(r[k] for k in scene); b = integer(r['block'],'block')
        require(b < blocks and r['arm'] in arms, f'{path}: unexpected arm/block')
        leaf = tuple(integer(r[k],k) for k in frame)
        groups[(key,b,leaf)].append(r)
        number(r[time],time,True)
        for k in invariants: integer(r[k],k)
        if kind == 'solver':
            require(int(r['macrosteps']) == 4 and int(r['leaves']) >= 4, 'missing physical steps')
            require((int(r['self_impulses']) > 0) == (r['regime']=='near'), 'vacuous solver regime')
    seen = {k[0] for k in groups}
    require(seen == expected, f'{path}: incomplete scenes: missing={expected-seen}, extra={seen-expected}')
    leaves = defaultdict(set); sums = defaultdict(lambda: defaultdict(float)); latency = defaultdict(list)
    for (key,b,leaf), group in groups.items():
        require(len(group)==len(arms) and {r['arm'] for r in group}==set(arms), f'{path}: incomplete paired group {key,b,leaf}')
        require({integer(r['slot'],'slot') for r in group}==set(range(len(arms))), 'duplicate order slots')
        for field in invariants:
            require(len({r[field] for r in group})==1, f'{path}: paired invariant {field} differs')
        leaves[key,b].add(leaf)
        for r in group:
            ns = float(r[time]); sums[key,b][r['arm']] += ns
            if kind=='solver': ns /= int(r['macrosteps'])
            latency[key,r['arm']].append(ns/1e6)
    for key in expected:
        first = leaves[key,0]
        require(first and all(leaves[key,b]==first for b in range(blocks)), f'{path}: unequal frame coverage in {key}')
        if frame:
            require(first=={(i,) for i in range(len(first))}, f'{path}: noncontiguous frame indices')
            if kind=='common' and not key[0].startswith('native_'):
                require(len(first)==32, 'independent trajectory truncated')
            else:
                require(len(first)>=4, 'native trajectory has fewer than four leaves')
    ratios = {a:math.exp(statistics.fmean(math.log(s[baseline]/s[a]) for s in sums.values())) for a in arms}
    medians = [{'scene':list(key),'arm':a,'median_ms':statistics.median(v),'observations':len(v)} for (key,a),v in sorted(latency.items())]
    return {'kind':kind,'path':str(path),'arms':list(arms),'baseline':baseline,'rows':len(rows),
            'groups':len(groups),'scenes':len(seen),'blocks':blocks,'paired_geomean':ratios,'medians':medians,
            'latency_boundary':'paid macrostep (4-step trajectory divided by 4)' if kind=='solver' else 'paid candidate query'}


def replay(path, scenes):
    rows=read_csv(path,('scene','step','skin','certified','exact_pairs','vf','ee','self_hits','cross_hits','missing'))
    seen=set(); pairs=0; ccd=0
    for r in rows:
        key=(integer(r['scene'],'scene'),integer(r['step'],'step'),float(r['skin']))
        require(key not in seen and key[2] in (0.,.0005), 'duplicate or invalid replay key');seen.add(key)
        require(r['certified']=='1' and all(integer(r[k],k)==0 for k in ('self_hits','cross_hits','missing')), 'uncertified replay or possible contact')
        ccd+=integer(r['vf'],'vf')+integer(r['ee'],'ee')
        if key[2]==0.: pairs+=integer(r['exact_pairs'],'exact_pairs')
    require(seen=={(s,t,p) for s in range(scenes) for t in range(32) for p in (0.,.0005)}, 'incomplete replay coverage')
    return {'rows':len(rows),'physical_steps':len(rows)//2,'ccd_checks':ccd,'exact_pairs':pairs}


def independent_receipt(path, scenes, active_field=None):
    rows=read_csv(path,('scene','step','min_j','momentum_error','stationarity_inf','complementarity_inf'))
    seen=set()
    for r in rows:
        k=(integer(r['scene'],'scene'),integer(r['step'],'step'))
        require(k not in seen, 'duplicate implicit step');seen.add(k)
        require(number(r['min_j'],'Jacobian',True)>.1,'nonpositive/too-small Jacobian')
        require(number(r['momentum_error'],'momentum')<=1e-6,'momentum gate failed')
        require(number(r['stationarity_inf'],'stationarity')<=1e-7,'stationarity gate failed')
        require(number(r['complementarity_inf'],'complementarity')<=1e-7,'complementarity gate failed')
    require(seen=={(s,t) for s in range(scenes) for t in range(32)},'incomplete independent steps')
    summary={'steps':len(rows),'min_j':min(float(r['min_j']) for r in rows),
             'max_momentum_error':max(float(r['momentum_error']) for r in rows),
             'max_stationarity':max(float(r['stationarity_inf']) for r in rows)}
    if active_field:
        summary['simultaneous_contact_steps']=sum(integer(r[active_field],active_field)>=2 for r in rows)
        require(summary['simultaneous_contact_steps']>0,'vacuous simultaneous contact')
    return summary


def exact_transaction(path,width):
    rows=read_csv(path,('scene','width','near','rotated','step','cells','leaves','exact_pairs','initial_pairs','native_ns','guarded_ns','self_impulses','cross_impulses','certified_min_j'))
    expected={('connected_U',w,n,0,s) for w in WIDTHS if w<=width for n in (0,1) for s in range(4)}
    if width>=4: expected|={('rotated_U',4,n,1,s) for n in (0,1) for s in range(8)}
    expected|={('cross_torus',8,1,0,s) for s in range(12)}
    seen=set()
    for r in rows:
        k=(r['scene'],int(r['width']),int(r['near']),int(r['rotated']),int(r['step']))
        require(k not in seen,'duplicate exact macrostep');seen.add(k)
        for key in ('cells','leaves','exact_pairs','initial_pairs','native_ns','guarded_ns','certified_min_j'):number(r[key],key,True)
    require(seen==expected,'incomplete exact transaction cases')
    return {'steps':len(rows),'leaves':sum(int(r['leaves']) for r in rows),
            'leaf_exact_pairs':sum(int(r['exact_pairs']) for r in rows),
            'initial_exact_pairs':sum(int(r['initial_pairs']) for r in rows),
            'median_guard_over_native':statistics.median(float(r['guarded_ns'])/float(r['native_ns']) for r in rows)}


def bridge(path,width):
    rows=read_csv(path,('width','near','rotated','step','leaves','self_impulses','calls','keys','tile_slots','tile_jobs','mask_words'))
    expected={(w,n,0,s) for w in WIDTHS if w<=width for n in (0,1) for s in range(4)}
    if width>=4:expected|={(4,n,1,s) for n in (0,1) for s in range(8)}
    seen=set()
    for r in rows:
        k=tuple(integer(r[x],x) for x in ('width','near','rotated','step'));require(k not in seen,'duplicate bridge step');seen.add(k)
        require(all(integer(r[x],x)>0 for x in ('leaves','calls','keys','tile_jobs','mask_words')), 'vacuous device bridge')
    require(seen==expected,'incomplete device bridge')
    return {'steps':len(rows),'leaves':sum(int(r['leaves']) for r in rows),'producer_calls':sum(int(r['calls']) for r in rows),'candidate_keys':sum(int(r['keys']) for r in rows)}


def table(headers, rows):
    strings=[[str(x) for x in r] for r in [headers,*rows]]
    widths=[max(len(r[i]) for r in strings) for i in range(len(headers))]
    line=lambda r:' | '.join(v.ljust(w) for v,w in zip(r,widths))
    return '\n'.join([line(strings[0]),'-+-'.join('-'*w for w in widths),*(line(r) for r in strings[1:])])


def terminal(manifest):
    print('\n=== EXPERIMENTAL GATES ===',flush=True)
    print(table(['Stage','Status','Seconds'],[[k,v['status'],f"{v.get('seconds',0):.2f}"] for k,v in manifest['stages'].items()]))
    for name,comp in manifest.get('comparisons',{}).items():
        print(f"\n=== {name.upper()}: {comp['latency_boundary']} ===")
        print(f"{comp['rows']:,} observations; {comp['groups']:,} matched groups; {comp['scenes']} workloads; reference={comp['baseline']}")
        print(table(['Algorithm','Paired geomean speed ratio'],[[a,f'{v:.3f}x'] for a,v in sorted(comp['paired_geomean'].items(),key=lambda x:-x[1])]))
        if comp['kind'] in ('metal','solver'):
            widest=max(int(m['scene'][0]) for m in comp['medians'])
            print(f'Width {widest}: median paid latency (ms)')
            regs=['0','1'] if comp['kind']=='metal' else ['separated','near']
            vals={(m['scene'][1],m['arm']):m['median_ms'] for m in comp['medians'] if int(m['scene'][0])==widest}
            print(table(['Regime',*comp['arms']],[[('separated' if r=='0' else 'near' if r=='1' else r),*[f'{vals[r,a]:.3f}' for a in comp['arms']]] for r in regs]))
    print('\n=== SAFETY AND PHYSICS RECEIPTS ===')
    for name,s in manifest.get('receipts',{}).items():
        print(name+': '+', '.join(f'{k}={v:.7g}' if isinstance(v,float) else f'{k}={v:,}' if isinstance(v,int) else f'{k}={v}' for k,v in s.items()))


def render(out,manifest):
    """Write SVG/PNG plots, human-readable HTML, and exact JSON summaries.

    Missing or failed campaigns never generate fabricated measurements.
    Charts come exclusively from successfully validated receipt objects.
    """
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib.ticker import ScalarFormatter
    out=Path(out);plots=out/'graphs';plots.mkdir(exist_ok=True)
    figures=[]
    def save(fig,name):
        for ext in ('svg','png'):fig.savefig(plots/f'{name}.{ext}',bbox_inches='tight',dpi=150)
        plt.close(fig);figures.append(f'graphs/{name}.svg')
    for name,c in manifest.get('comparisons',{}).items():
        ranked=sorted(c['paired_geomean'].items(),key=lambda x:x[1])
        fig,ax=plt.subplots(figsize=(9,max(3,.43*len(ranked)+1.4)));ax.barh([x[0] for x in ranked],[x[1] for x in ranked]);ax.axvline(1,color='gray',linestyle='--');ax.set_xlabel(f"Paired geometric-mean speed ratio vs {c['baseline']} (higher is faster)");ax.set_title(f"{name}: {c['rows']:,} measured observations");ax.grid(axis='x',alpha=.2);save(fig,name+'_speed_ratios')
        if c['kind'] in ('metal','solver','common'):
            fig,axes=plt.subplots(1,2,figsize=(13,5),sharey=True)
            for ax,reg in zip(axes,('separated','near')):
                for arm in c['arms']:
                    pts=[]
                    for r in c['medians']:
                        if r['arm']!=arm:continue
                        key=r['scene']
                        if c['kind']=='common':
                            if not key[0].startswith('native_'):continue
                            _,w,n=key[0].split('_');rreg='near' if n=='1' else 'separated'
                        else:w,rreg=key;rreg={'0':'separated','1':'near'}.get(rreg,rreg)
                        if rreg==reg:pts.append((int(w),r['median_ms']))
                    if pts:
                        pts.sort();ax.plot([x[0] for x in pts],[x[1] for x in pts],marker='o',label=arm)
                ax.set_title(reg);ax.set_xscale('log',base=2);ax.set_yscale('log');ax.set_xlabel('U-solid width (cells)');ax.set_xticks(sorted({int(x[0].split('_')[1]) for x in [r['scene'] for r in c['medians']] if x[0].startswith('native_')}) if c['kind']=='common' else sorted({int(r['scene'][0]) for r in c['medians']}));ax.xaxis.set_major_formatter(ScalarFormatter());ax.grid(alpha=.25)
            axes[0].set_ylabel('Median paid latency (ms, logarithmic)');axes[1].legend(fontsize=8,loc='best');fig.suptitle(f"{name}: {c['latency_boundary']}");save(fig,name+'_scaling')
    for name,path in manifest.get('physics_csv',{}).items():
        rows=read_csv(out/path,('scene','step','min_j'))
        fig,ax=plt.subplots(figsize=(9,4.5))
        for scene in sorted({r['scene'] for r in rows},key=int):
            rr=sorted((r for r in rows if r['scene']==scene),key=lambda r:int(r['step']));ax.plot([int(r['step']) for r in rr],[float(r['min_j']) for r in rr],label='Scene '+scene)
        ax.set(title=name+' — minimum element Jacobian',xlabel='Implicit step',ylabel='Minimum J');ax.grid(alpha=.2);ax.legend();save(fig,name+'_jacobian')
        impulse=[k for k in rows[0] if k.startswith('impulse')]
        if impulse:
            fig,ax=plt.subplots(figsize=(9,4.5))
            for scene in sorted({r['scene'] for r in rows},key=int):
                rr=sorted((r for r in rows if r['scene']==scene),key=lambda r:int(r['step']));ax.plot([int(r['step']) for r in rr],[sum(float(r[k]) for k in impulse) for r in rr],label='Scene '+scene)
            ax.set(title=name+' — summed reported constraint impulse channels',xlabel='Implicit step',ylabel='Impulse (N s)');ax.grid(alpha=.2);ax.legend();save(fig,name+'_impulses')
    exact=manifest.get('exact_csv')
    if exact:
        rows=read_csv(out/exact,('scene','width','near','native_ns','guarded_ns'))
        groups=defaultdict(list)
        for r in rows:groups[(r['scene'],r['width'],r['near'])].append(float(r['guarded_ns'])/float(r['native_ns']))
        fig,ax=plt.subplots(figsize=(10,5));keys=list(groups);ax.barh([f'{s} {w} {"near" if n=="1" else "separated"}' for s,w,n in keys],[statistics.median(groups[k]) for k in keys]);ax.set_xlabel('Median exact-gated / native cost (higher is more expensive)');ax.set_title('Exact safety gate adds a stronger contract; not a speed comparison');save(fig,'exact_guard_cost')
    write_dashboard(out,manifest,figures)
    (out/'measurement_summary.json').write_text(json.dumps({'comparisons':manifest.get('comparisons',{}),'receipts':manifest.get('receipts',{})},indent=2)+'\n')
    return len(figures)


def write_dashboard(out,manifest,figures=None):
    """Refresh the HTML after the final manifest is committed, without plotting."""
    out=Path(out)
    if figures is None:
        figures=[p.relative_to(out).as_posix() for p in sorted((out/'graphs').glob('*.svg'))]
    # Offline dashboard uses only local relative URLs and escaped strings.
    stage_rows=''.join(f'<tr><td>{html.escape(k)}</td><td>{html.escape(v["status"])}</td><td>{v.get("seconds",0):.2f}</td></tr>' for k,v in manifest['stages'].items())
    geometry=[p.relative_to(out).as_posix() for folder in ('geometry','automatic_geometry') for pattern in ('*.png','*.gif') for p in sorted((out/folder).glob(pattern))]
    figures=list(figures)+geometry
    images=''.join(f'<figure><img src="{html.escape(p)}" loading="lazy" alt="{html.escape(Path(p).stem)}"><figcaption>{html.escape(Path(p).stem.replace("_"," "))}</figcaption></figure>' for p in figures)
    tables=[]
    for name,c in manifest.get('comparisons',{}).items():
        rr=''.join(f'<tr><td>{html.escape(a)}</td><td>{v:.4f}×</td></tr>' for a,v in sorted(c['paired_geomean'].items(),key=lambda x:-x[1]));tables.append(f'<h2>{html.escape(name)}</h2><p>{c["rows"]:,} observations; {c["groups"]:,} matched groups. Reference: {html.escape(c["baseline"])}</p><table><tr><th>Method</th><th>Speed ratio</th></tr>{rr}</table>')
    (out/'index.html').write_text('<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>Kyoto research execution evidence</title><style>body{font:16px system-ui;margin:2rem auto;padding:0 1rem;max-width:1100px;color:#17212b}table{border-collapse:collapse;width:100%}td,th{border-bottom:1px solid #ddd;text-align:left;padding:.5rem}img{max-width:100%;height:auto}figure{margin:2rem 0}pre{white-space:pre-wrap}a{color:#125d94}</style><h1>Kyoto research execution evidence</h1><p><strong>'+('ALL DECLARED GATES PASSED' if manifest.get('all_listed_gates_passed') else 'INCOMPLETE / BLOCKED GATES REMAIN')+'</strong></p><p>Measured receipts only. Candidate timings, whole-solver costs and reference-safety costs have separate boundaries. A successful campaign does not prove universal performance or safety.</p><p><a href="campaign_manifest.json">Manifest and receipt summaries</a> · <a href="SHA256SUMS">File hashes</a></p><h2>Execution gates</h2><table><tr><th>Stage</th><th>Status</th><th>Seconds</th></tr>'+stage_rows+'</table>'+''.join(tables)+images+'</html>\n',encoding='utf-8')
