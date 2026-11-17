#!/usr/bin/env python3
"""Reproducible Kyoto AABB broad-phase tournament analysis.

One independent unit = (material, regime, target budget, seed). Identical
runtime repeats are aggregated by the MEDIAN of their per-frame means, never
counted as independent simulation seeds. Bootstrap samples whole seed blocks,
not individual frames or repeated trials. No post-hoc winner is deployable.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import random
import re
from collections import Counter, defaultdict
from pathlib import Path
from statistics import mean, median

STEM = re.compile(
    r"^actual-(cloth|sponge|cushion|soft_ball|spring|mixed)-"
    r"(baseline|impact|shear|compression|rapid|relax)-n([0-9]+)-"
    r"seed([0-9]+)(?:-rep([0-9]+))?-evaluated[.]csv$")
GUARD="global_guarded_exact"
AA="global_guarded_aa_control"
SAP="sap_persistent_coherent"
BOUNDED="guard_sap_bounded_trial"

def geomean(ratios):
    # Materialize generators before validating: consuming a generator with
    # any() otherwise leaves no values for statistics.mean().
    values=list(ratios)
    if not values or any(x <= 0 or not math.isfinite(x) for x in values):
        raise ValueError("Expected positive finite ratios")
    return math.exp(mean(math.log(x) for x in values))

def nearest_percentile(xs, q):
    values=sorted(xs)
    if not values: raise ValueError("No samples")
    return values[min(len(values)-1,max(0,round((len(values)-1)*q)))]

def spearman(a,b):
    if len(a)!=len(b) or len(a)<3: return None
    def ranks(x):
        idx=sorted(range(len(x)),key=x.__getitem__)
        result=[0.]*len(x)
        i=0
        while i<len(idx):
            j=i+1
            while j<len(idx) and x[idx[j]]==x[idx[i]]: j+=1
            v=(i+1+j)/2.
            for k in idx[i:j]: result[k]=v
            i=j
        return result
    ra,rb=ranks(a),ranks(b)
    ma,mb=mean(ra),mean(rb)
    numerator=sum((x-ma)*(y-mb) for x,y in zip(ra,rb))
    denom=math.sqrt(sum((x-ma)**2 for x in ra)*sum((y-mb)**2 for y in rb))
    return numerator/denom if denom else None

def file_measurement(path, warmup):
    m=STEM.match(Path(path).name)
    if not m:
        raise ValueError(f"Unexpected replay basename: {path}")
    material,regime,budget,seed,rep=m.groups()
    samples=defaultdict(list)
    checks=defaultdict(dict)
    fields=defaultdict(lambda: defaultdict(list))
    counted_frames=defaultdict(set)
    material_n=None
    with Path(path).open(newline="",encoding="utf-8") as stream:
        reader=csv.DictReader(stream)
        required={"backend","frame","ms","n","pairs","pair_hash","misses","builds","reuses"}
        if not required.issubset(reader.fieldnames or []):
            raise ValueError(f"Missing replay columns: {path}")
        for row in reader:
            k=row["backend"]
            frame=int(row["frame"])
            n=int(row["n"])
            material_n=n if material_n is None else material_n
            if n!=material_n:
                raise ValueError(f"Inconsistent AABB count: {path}")
            tag=(row["pairs"],row["pair_hash"])
            if frame in checks[k]:
                raise ValueError(f"Duplicate backend/frame: {path}, {k}, {frame}")
            checks[k][frame]=tag
            if int(row["misses"])!=0:
                raise ValueError(f"Oracle misses: {path}, {k}, {frame}")
            duration=float(row["ms"])
            if not math.isfinite(duration) or duration<=0:
                raise ValueError(f"Invalid measured latency: {path}, {k}, {frame}")
            if frame<warmup:
                continue
            samples[k].append(duration)
            counted_frames[k].add(frame)
            for column in ("builds","reuses","selective_repairs","aabb_tests",
                           "field_events","hybrid_switched","hybrid_probed","ml_switched",
                           "f7","f8","f9","f10"):
                if column in row and row[column] not in ("",None):
                    fields[k][column].append(float(row[column]))
    if not checks:
        raise ValueError(f"Empty replay: {path}")
    expected_frames=set(checks.get("sap_rebuild",{}))
    if not expected_frames or expected_frames!=set(range(len(expected_frames))):
        raise ValueError(f"Missing or discontinuous oracle frames: {path}")
    expected_timed=set(range(warmup,len(expected_frames)))
    for policy,tags in checks.items():
        if set(tags)!=expected_frames:
            raise ValueError(f"Missing {policy} frames: {path}")
        for f in expected_frames:
            if tags[f]!=checks["sap_rebuild"][f]:
                raise ValueError(f"Wrong exact AABB pairs: {path}, {policy}, {f}")
        if counted_frames[policy]!=expected_timed:
            raise ValueError(f"Wrong warmup coverage: {path}, {policy}")
    expected_methods=set(checks)
    if {GUARD,AA,SAP,BOUNDED}.difference(expected_methods):
        raise ValueError(f"Missing fairness control/baseline: {path}")
    means={k:mean(v) for k,v in samples.items()}
    p95={k:nearest_percentile(v,.95) for k,v in samples.items()}
    count=lambda k,key:sum(fields[k][key])
    guard_frames=len(samples[GUARD])
    return {
        "path":str(path),"material":material,"regime":regime,
        "budget":int(budget),"seed":int(seed),"repeat":int(rep or 1),
        "n_actual":material_n,"frames_total":len(expected_frames),
        "frames_measured":guard_frames,"policy_count":len(expected_methods),
        "methods":sorted(expected_methods),"mean_ms":means,"p95_ms":p95,
        "guard_rebuild_frequency":count(GUARD,"builds")/guard_frames,
        "guard_reuses":count(GUARD,"reuses"),
        "guard_pair_density":mean(fields[GUARD]["f10"]) if fields[GUARD]["f10"] else 0.,
        "guard_movement":mean(fields[GUARD]["f8"]) if fields[GUARD]["f8"] else 0.,
        "events":{k:count(k,"field_events") for k in expected_methods},
        "repairs":{k:count(k,"selective_repairs") for k in expected_methods},
        "switches":{k:count(k,"hybrid_switched")+count(k,"ml_switched")
                    for k in expected_methods},
        "exact_all_frames":True,
    }

def block_interval(cells, method, bootstrap, seed):
    rng=random.Random(seed)
    seed_ids=sorted(set(x["seed"] for x in cells))
    if len(seed_ids)<2:
        return None
    groups=defaultdict(list)
    for x in cells: groups[x["seed"]].append(x)
    draws=[]
    for _ in range(bootstrap):
        sampled=[c for unused in range(len(seed_ids))
                 for c in groups[rng.choice(seed_ids)]]
        draws.append(geomean(c["mean_ms"][GUARD]/c["mean_ms"][method] for c in sampled))
    return [nearest_percentile(draws,.025),nearest_percentile(draws,.975)]

def grouping(cells, key, methods):
    groups=defaultdict(list)
    for c in cells: groups[str(c[key])].append(c)
    result={}
    for k,rows in sorted(groups.items()):
        ranks=Counter()
        for c in rows:
            ranks[min(methods,key=lambda m:c["mean_ms"][m])]+=1
        speeds={}
        for method in methods:
            speeds[method]={
                "geometric_speedup_vs_guard":geomean(
                    c["mean_ms"][GUARD]/c["mean_ms"][method] for c in rows),
                "scene_wins_vs_guard":sum(
                    c["mean_ms"][method]<c["mean_ms"][GUARD] for c in rows),
                "sum_time_speedup_vs_guard":(
                    sum(c["mean_ms"][GUARD] for c in rows)/
                    sum(c["mean_ms"][method] for c in rows))}
        result[k]={
            "cell_count":len(rows),
            "fastest_hindsight_policy_scene_counts":dict(ranks),
            "lowest_summed_scene_time_policy":min(
                methods,key=lambda m:sum(c["mean_ms"][m] for c in rows)),
            "methods":speeds}
    return result

def analyze(paths, warmup=16, bootstrap=2000, seed=2001):
    if bootstrap<100:raise ValueError("Need >=100 bootstrap resamples")
    repeats=[file_measurement(path,warmup) for path in paths]
    units=defaultdict(list)
    for result in repeats:
        key=(result["material"],result["regime"],result["budget"],result["seed"])
        units[key].append(result)
    cells=[]
    for (material,regime,budget,seed_id),members in sorted(units.items()):
        methods=set(members[0]["methods"])
        if any(set(m["methods"])!=methods for m in members):
            raise ValueError(f"Method roster changed among repeated runs of {material}/{regime}")
        if len({r["repeat"] for r in members})!=len(members):
            raise ValueError("Duplicate repeat ID")
        def group_metric(key,method):
            return median(m[key][method] for m in members)
        means={m:group_metric("mean_ms",m) for m in methods}
        p95={m:group_metric("p95_ms",m) for m in methods}
        cells.append({
            "material":material,"regime":regime,"budget":budget,"seed":seed_id,
            "n_actual":members[0]["n_actual"],
            "repeats":len(members),"measured_frames_each":members[0]["frames_measured"],
            "mean_ms":means,"p95_ms":p95,
            "guard_rebuild_frequency":median(m["guard_rebuild_frequency"] for m in members),
            "guard_movement":median(m["guard_movement"] for m in members),
            "guard_pair_density":median(m["guard_pair_density"] for m in members),
            "field_events":{m:median(x["events"][m] for x in members) for m in methods},
            "repairs":{m:median(x["repairs"][m] for x in members) for m in methods},
            "switches":{m:median(x["switches"][m] for x in members) for m in methods},
            "fastest_hindsight_policy":min(methods,key=lambda m:means[m]),
        })
    if not cells:raise ValueError("No completed trial cells")
    methods=sorted(cells[0]["mean_ms"])
    if any(set(x["mean_ms"])!=set(methods) for x in cells):
        raise ValueError("Inconsistent tournament algorithms across scenes")
    leaderboard=[]
    for method in methods:
        ratios=[c["mean_ms"][GUARD]/c["mean_ms"][method] for c in cells]
        tail=[c["p95_ms"][GUARD]/c["p95_ms"][method] for c in cells]
        leaderboard.append({
            "method":method,"geometric_speedup_vs_guard":geomean(ratios),
            "sum_time_speedup_vs_guard":sum(c["mean_ms"][GUARD] for c in cells)/
                                          sum(c["mean_ms"][method] for c in cells),
            "p95_geometric_speedup_vs_guard":geomean(tail),
            "mean_scene_wins_vs_guard":sum(r>1 for r in ratios),
            "p95_scene_wins_vs_guard":sum(r>1 for r in tail),
            "fastest_scene_count":sum(c["fastest_hindsight_policy"]==method for c in cells),
            "seed_block_bootstrap_95pct":block_interval(cells,method,bootstrap,seed),
        })
    leaderboard.sort(key=lambda v:v["geometric_speedup_vs_guard"],reverse=True)
    crossover=[]
    for label,lo,hi in (("0-2%",0.,.02),("2-10%",.02,.1),
                        ("10-30%",.1,.3),("30-60%",.3,.6),
                        ("60-100%",.6,float("inf"))):
        bucket=[c for c in cells if lo<=c["guard_rebuild_frequency"]<hi]
        if not bucket:continue
        crossover.append({
            "guard_rebuild_frequency_bin":label,"cells":len(bucket),
            "persistent_sap_scene_wins":sum(
                c["mean_ms"][SAP]<c["mean_ms"][GUARD] for c in bucket),
            "persistent_sap_geometric_speedup":geomean(
                c["mean_ms"][GUARD]/c["mean_ms"][SAP] for c in bucket),
        })
    response={
        "protocol":{
            "unit":"unique material x regime x budget x seed; medians across repeats",
            "original_csv_files":len(repeats),"unique_cells":len(cells),
            "seeds":sorted({c["seed"] for c in cells}),
            "repeats_by_cell":dict(Counter(str(c["repeats"]) for c in cells)),
            "no_future_data_used":True,
            "correctness":"every algorithm pair count/hash matches SAP every frame; "
                          "independent brute force checked by C++ replay on sampled small-N frames",
            "limitation":"one machine/process run; input scenes share generator and "
                         "model training distribution; seed-blocked bootstrap conditional "
                         "on generator, not general real-world accuracy; fastest-per-scene is hindsight",
            "method_order":"randomized deterministically per replay, repeated identical snapshots",
        },
        "leaderboard":leaderboard,
        "guard_aa":next(e for e in leaderboard if e["method"]==AA),
        "by_material":grouping(cells,"material",methods),
        "by_regime":grouping(cells,"regime",methods),
        "by_budget":grouping(cells,"budget",methods),
        "guard_rebuild_sap_crossover":crossover,
        "guard_rebuild_sap_spearman":spearman(
            [c["guard_rebuild_frequency"] for c in cells],
            [math.log(c["mean_ms"][GUARD]/c["mean_ms"][SAP]) for c in cells]),
        "cells":cells,
        "raw_repetitions":repeats,
    }
    return response

def write_report(data, output):
    output=Path(output)
    output.mkdir(parents=True,exist_ok=True)
    (output/"RUMBLE_PATTERNS.json").write_text(json.dumps(data,indent=2)+"\n",
                                              encoding="utf-8")
    raw=["# Kyoto exact broad-phase deathmatch — all methods, all evidence","",
         f'Unique independent geometry-seed cells: **{data["protocol"]["unique_cells"]}**. '
         f'Raw CSV replay repetitions: **{data["protocol"]["original_csv_files"]}**.',
         "",
         "**Protocol:** identical snapshots for all policies, deterministic shuffled method "
         "order per replay, independent per-method cache, and all query/switch/maintenance "
         "costs included. Repetitions are combined by median before ranking; "
         "95% confidence intervals resample entire seed blocks.",
         "",
         "| Policy | Geo speedup vs Guard | Summed-time speedup | P95 speedup | "
         "Wins vs Guard | Fastest cells | Seed-block 95% interval |",
         "|---|---:|---:|---:|---:|---:|---|"]
    for e in data["leaderboard"]:
        interval=e["seed_block_bootstrap_95pct"]
        ci="n/a" if interval is None else f'[{interval[0]:.3f}, {interval[1]:.3f}]'
        raw.append(f'| {e["method"]} | {e["geometric_speedup_vs_guard"]:.4f}x | '
                   f'{e["sum_time_speedup_vs_guard"]:.4f}x | '
                   f'{e["p95_geometric_speedup_vs_guard"]:.4f}x | '
                   f'{e["mean_scene_wins_vs_guard"]} | {e["fastest_scene_count"]} | {ci} |')
    raw.extend(["","## Guard-versus-Guard A/A sanity check","",
        f'Geometric ratio: **{data["guard_aa"]["geometric_speedup_vs_guard"]:.4f}×**. '
        "A meaningful departure from 1× warns that benchmark order, CPU state "
        "or measurement noise may be comparable with small algorithm gains.",
        "","## Which policies win by workload?",""])
    for field,label in (("by_material","Material"),("by_regime","Motion regime"),
                        ("by_budget","Target primitive budget")):
        raw.extend([f'### {label}',"",
                    "| Workload | Cells | Lowest total-time method | Most frequent fastest methods |",
                    "|---|---:|---|---|"])
        for name,item in data[field].items():
            most=Counter(item["fastest_hindsight_policy_scene_counts"]).most_common(3)
            raw.append(f'| {name} | {item["cell_count"]} | '
                       f'{item["lowest_summed_scene_time_policy"]} | '
                       f'{", ".join(f"{k} ({n})" for k,n in most)} |')
        raw.append("")
    raw.extend(["## Certificate-rebuild crossover and SAP performance","",
                "| Guard rebuild fraction | Cells | SAP wins | Guard/SAP geometric speedup |",
                "|---|---:|---:|---:|"])
    for c in data["guard_rebuild_sap_crossover"]:
        raw.append(f'| {c["guard_rebuild_frequency_bin"]} | {c["cells"]} | '
                   f'{c["persistent_sap_scene_wins"]} | '
                   f'{c["persistent_sap_geometric_speedup"]:.3f}x |')
    value=data["guard_rebuild_sap_spearman"]
    raw.extend(["",f'Rank correlation between rebuild frequency and SAP speedup: '
                f'**{value:.3f}**' if value is not None else
                'Rank correlation unavailable.',"",
                "## Interpretation limits","",
                "- This is CPU AABB candidate generation, **not** narrow-phase or "
                "fully coupled cloth-contact end-to-end latency.",
                "- Equal algorithm ordering and independent state are controlled at "
                "the scene-repetition level, not by simultaneous synchronized execution.",
                "- The A/A control is a mandatory sanity check; any observed "
                "speedup smaller than its noise should be treated as uncertain.",
                "- The independently fastest per-scene algorithm is a **hindsight oracle**, "
                "not a deployable online policy.",
                "- The signal-guided patch controller adjusts certificate padding "
                "from past/current displacement; it does **not** certify future motion.",
                "- Every algorithm must match the sorted unique AABB pair oracle. "
                "The C++ brute-force oracle is sampled for small scenes.",
                "- Unseen external scenes, memory, multithreading, GPU and full "
                "solver measurements remain separate validation tasks.",""])
    (output/"RUMBLE_PATTERNS.md").write_text("\n".join(raw),encoding="utf-8")
    return output/"RUMBLE_PATTERNS.md"

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("csvs",nargs="+",type=Path)
    p.add_argument("--warmup",type=int,default=16)
    p.add_argument("--bootstrap",type=int,default=2000)
    p.add_argument("--seed",type=int,default=20261118)
    p.add_argument("--output",type=Path,required=True)
    a=p.parse_args()
    result=analyze(a.csvs,a.warmup,a.bootstrap,a.seed)
    print(write_report(result,a.output))

if __name__=="__main__": main()
