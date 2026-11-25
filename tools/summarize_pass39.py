#!/usr/bin/env python3
"""Validate paid audit receipts and plot timing versus work, never FEM speedup."""
import argparse
import csv
import statistics
from collections import defaultdict
from pathlib import Path


def summarize(reference, inputs):
    with reference.open(newline='') as f:
        refs = {(r['fixture'], int(r['regime'])): r for r in csv.DictReader(f)}
    if len(refs) != 9:
        raise ValueError('expected nine distinct fixture cases')
    output = []
    blocks = []
    for compiler, path in inputs:
        data = defaultdict(dict)
        with path.open(newline='') as f:
            for r in csv.DictReader(f):
                key = (r['fixture'], int(r['regime']))
                block, slot = int(r['block']), int(r['slot'])
                if key not in refs or not 0 <= block < 5 or not 0 <= slot < 4:
                    raise ValueError('unexpected fixture, block or slot')
                group = data[key, block]
                if slot in group:
                    raise ValueError('duplicate timed slot')
                expected = 'exhaustive' if (slot in (0, 3)) == bool(block % 2) else 'sweep'
                t = int(r['elapsed_ns'])
                ref = refs[key]
                if (r['arm'] != expected or t <= 0 or r['cells'] != ref['cells'] or
                        r['pairs'] != ref['pairs'] or
                        r['overlap_tests'] != ref[expected + '_overlap_tests']):
                    raise ValueError('timing or matched work receipt mismatch')
                group[slot] = (expected, t, int(r['polynomials']))
        for key in refs:
            ratios = []
            for block in range(5):
                group = data[key, block]
                if set(group) != set(range(4)) or len({r[2] for r in group.values()}) != 1:
                    raise ValueError('missing slot or unmatched exact certificate work')
                ex = sum(r[1] for r in group.values() if r[0] == 'exhaustive')
                sw = sum(r[1] for r in group.values() if r[0] == 'sweep')
                ratios.append(ex / sw)
                blocks.append(dict(compiler=compiler, fixture=key[0], regime=key[1], block=block,
                                   exhaustive_ns=ex, sweep_ns=sw))
            ref = refs[key]
            output.append(dict(compiler=compiler, fixture=key[0], regime=key[1], blocks=5,
                               geomean=statistics.geometric_mean(ratios), minimum=min(ratios), maximum=max(ratios),
                               overlap_reduction=1-int(ref['sweep_overlap_tests'])/int(ref['exhaustive_overlap_tests'])))
    return output, blocks


def write_csv(path, rows):
    with path.open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def plots(rows, out):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    regimes = ['Static', 'Shear', 'Nonuniform']
    compilers = list(dict.fromkeys(r['compiler'] for r in rows))
    cases = [(r['fixture'], r['regime']) for r in rows if r['compiler'] == compilers[0]]
    labels = [f'{f} / {regimes[r]}' for f, r in cases]
    fig, ax = plt.subplots(figsize=(9, 5.8), layout='constrained')
    for i, compiler in enumerate(compilers):
        group = [r for r in rows if r['compiler'] == compiler]
        y = [j + (i-(len(compilers)-1)/2)*.22 for j in range(len(cases))]
        v = [r['geomean'] for r in group]
        ax.errorbar(v, y, xerr=[[r['geomean']-r['minimum'] for r in group],
                               [r['maximum']-r['geomean'] for r in group]],
                    fmt='o', capsize=3, label=compiler)
    ax.axvline(1, linestyle='--', color='gray', linewidth=1)
    ax.set_yticks(range(len(labels)), labels); ax.invert_yaxis()
    ax.set_xlabel('Exhaustive / sweep whole-audit time (higher favors sweep)')
    ax.set_title('Whole exact-volume audit: five paired blocks per case')
    ax.legend(); ax.grid(axis='x', alpha=.2)
    fig.savefig(out/'whole_audit_timing.png', dpi=180); plt.close(fig)
    fig, ax = plt.subplots(figsize=(9, 5.8), layout='constrained')
    group = [r for r in rows if r['compiler'] == compilers[0]]
    ax.barh(labels, [100*r['overlap_reduction'] for r in group]); ax.invert_yaxis()
    ax.set_xlim(0, 100); ax.set_xlabel('Reduction in three-axis overlap tests (%)')
    ax.set_title('Candidate work reduction, not elapsed-time speedup')
    ax.grid(axis='x', alpha=.2)
    fig.savefig(out/'overlap_work.png', dpi=180); plt.close(fig)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('reference', type=Path); p.add_argument('gcc', type=Path); p.add_argument('clang', type=Path)
    p.add_argument('output', type=Path); p.add_argument('--plots', action='store_true')
    a = p.parse_args(); a.output.mkdir(parents=True, exist_ok=True)
    rows, blocks = summarize(a.reference, [('GCC', a.gcc), ('Clang', a.clang)])
    write_csv(a.output/'case_summary.csv', rows); write_csv(a.output/'paired_blocks.csv', blocks)
    if a.plots:
        plots(rows, a.output)
    for compiler in ('GCC', 'Clang'):
        print(compiler, statistics.geometric_mean(r['geomean'] for r in rows if r['compiler'] == compiler))


if __name__ == '__main__':
    main()
