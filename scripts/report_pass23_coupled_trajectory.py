#!/usr/bin/env python3
"""Validate and chart ORIGINAL native two-deformable FEM contact trajectory.

CSV input comes ONLY from apps/pass23_coupled_trajectory.cpp.
No synthetic body trajectories or performance measurements are created.
"""
from __future__ import annotations
import argparse
import csv
import json
import math
from pathlib import Path

COLUMNS=(
    "macro_step","time_start_s","time_end_s","integrated_dt_s",
    "accepted_physical_leaves","rejected_moving_ccd_trials",
    "rejected_inverted_tet_trials","mutual_contact_impulse_updates",
    "minimum_accepted_face_gap_m","minimum_tetra_j",
    "impulse_A_x_ns","impulse_A_y_ns","impulse_A_z_ns",
    "impulse_B_x_ns","impulse_B_y_ns","impulse_B_z_ns",
    "maximum_whole_system_momentum_residual_ns",
    "kinetic_energy_j","volumetric_elastic_energy_j",
    "gravity_potential_j","combined_mechanical_energy_j",
)

def load_native(path:Path)->list[dict]:
    with path.open(encoding="utf-8",newline="") as file:
        reader=csv.DictReader(file)
        missing=set(COLUMNS)-set(reader.fieldnames or ())
        if missing:raise ValueError(f"missing original coupled physical columns: {sorted(missing)}")
        rows=list(reader)
    if len(rows)!=120:
        raise ValueError(f"expected 120 physical macro-steps, got {len(rows)}")
    previous=0.
    for n,row in enumerate(rows,1):
        numeric={key:float(row[key]) for key in COLUMNS}
        if any(not math.isfinite(v) for v in numeric.values()):
            raise ValueError(f"nonfinite physical source at step {n}")
        if int(row["macro_step"])!=n:
            raise ValueError(f"physical source step discontinuity at {n}")
        if abs(numeric["time_start_s"]-previous)>1e-11:
            raise ValueError(f"source actual physical time gap at step {n}")
        if abs(numeric["integrated_dt_s"]-.0005)>1e-12:
            raise ValueError(f"source accepted wrong requested physical dt at {n}")
        if abs(numeric["time_end_s"]-numeric["time_start_s"]-
               numeric["integrated_dt_s"])>1e-11:
            raise ValueError(f"accepted coupled source advanced wrong physical time at {n}")
        previous=numeric["time_end_s"]
        if not (numeric["minimum_accepted_face_gap_m"]>.00025 and
                numeric["minimum_tetra_j"]>1e-6):
            raise ValueError(f"accepted colliding/inverted coupled physical mesh at {n}")
        if int(row["accepted_physical_leaves"])<1:
            raise ValueError(f"no truly integrated physical leaf at {n}")
        if any(int(row[k])<0 for k in (
            "rejected_moving_ccd_trials","rejected_inverted_tet_trials",
            "mutual_contact_impulse_updates")):
            raise ValueError(f"negative actual source impulse/rejection count at {n}")
        mismatch=math.sqrt(sum(
            (numeric[f"impulse_A_{axis}_ns"]+
             numeric[f"impulse_B_{axis}_ns"])**2
            for axis in "xyz"))
        if mismatch>1e-8:
            raise ValueError(f"unequal opposite original mutual impulses at {n}")
        if numeric["maximum_whole_system_momentum_residual_ns"]>1e-5:
            raise ValueError(f"measured original momentum residual too large at {n}")
        actual_energy=(numeric["kinetic_energy_j"]+
                       numeric["volumetric_elastic_energy_j"]+
                       numeric["gravity_potential_j"])
        if (abs(actual_energy-numeric["combined_mechanical_energy_j"]) >
                1e-9*max(1.,abs(actual_energy))):
            raise ValueError(f"mechanical-energy source columns conflict at {n}")
    if previous<=0:
        raise ValueError("no elapsed physical source clock")
    if not any(int(row["mutual_contact_impulse_updates"])>0 for row in rows):
        raise ValueError("native source never demonstrated mutual contact")
    return rows

def summarize(rows:list[dict])->dict:
    first=next(row for row in rows if int(row["mutual_contact_impulse_updates"])>0)
    min_gap=min(float(r["minimum_accepted_face_gap_m"]) for r in rows)
    min_j=min(float(r["minimum_tetra_j"]) for r in rows)
    contact=sum(int(r["mutual_contact_impulse_updates"]) for r in rows)
    return {
        "experiment":"Original physically coupled 3D neo-Hookean two-deformable FEM",
        "source_kind":"actual C++23 solver state; two finite-mass bodies exchange contact impulses",
        "macro_steps":len(rows),
        "integrated_physical_time_s":float(rows[-1]["time_end_s"]),
        "accepted_physical_leaves":sum(int(r["accepted_physical_leaves"]) for r in rows),
        "rejected_continuous_motion_trials":sum(int(r["rejected_moving_ccd_trials"]) for r in rows),
        "rejected_inverted_tetra_trials":sum(int(r["rejected_inverted_tet_trials"]) for r in rows),
        "mutual_face_impulse_updates":contact,
        "contact_bearing_macro_steps":sum(
            int(r["mutual_contact_impulse_updates"])>0 for r in rows),
        "first_contact_step":int(first["macro_step"]),
        "first_contact_time_s":float(first["time_end_s"]),
        "minimum_accepted_face_gap_m":min_gap,
        "minimum_accepted_tetra_deformation_j":min_j,
        "maximum_momentum_residual_kg_m_per_s":max(
            float(r["maximum_whole_system_momentum_residual_ns"]) for r in rows),
        "initial_mechanical_energy_j":float(rows[0]["combined_mechanical_energy_j"]),
        "last_mechanical_energy_j":float(rows[-1]["combined_mechanical_energy_j"]),
        "warnings":[
            "Original explicit local barycentric impulse solver is not independent continuum physical calibration",
            "Continuous moving-moving separation uses a conservative floating guard, not directed-rounding universal CCD",
            "No self-contact, closed-surface inside/outside classification, implicit KKT/IPC barrier or source-scene generality",
            "Momentum identity assumes matching gravity/damping; kinetic, elastic and gravity energy not globally conserved under damping/discretization",
            "Full CCD event equivalence and broadphase-vs-pinned IPC performance still need independent source tests",
        ],
    }

def main()->None:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--input",required=True,type=Path)
    p.add_argument("--output",required=True,type=Path)
    args=p.parse_args()
    rows=load_native(args.input)
    summary=summarize(rows)
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/"pass23_real_coupled_contact_summary.json").write_text(
        json.dumps(summary,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print(json.dumps(summary,indent=2))
        return
    t=[float(r["time_end_s"]) for r in rows]
    gap=[1000*float(r["minimum_accepted_face_gap_m"]) for r in rows]
    fig,ax=plt.subplots(figsize=(10,5))
    ax.plot(t,gap,label="Accepted true finite triangle face gap")
    ax.axhline(.25,ls="--",label="Half-skin safety threshold (mm)")
    ax.set(xlabel="Actual fully integrated time (s)",
           ylabel="Minimum source finite face distance (mm)",
           title="Two original deformable FEM bodies: accepted nonpenetration")
    ax.legend();ax.grid(alpha=.25);fig.tight_layout()
    fig.savefig(args.output/"23a_original_coupled_contact_gap.png",dpi=190)
    plt.close(fig)

    fig,ax=plt.subplots(figsize=(10,5))
    counts=[int(r["mutual_contact_impulse_updates"]) for r in rows]
    ax.bar(t,counts,width=.00035,label="Original symmetric contact updates")
    ax.set(xlabel="Actual fully integrated time (s)",
           ylabel="Original finite-face impulse updates",
           title="Actual two-deformable coupling activation")
    ax.grid(axis="y",alpha=.25);ax.legend();fig.tight_layout()
    fig.savefig(args.output/"23b_original_mutual_impulses.png",dpi=190)
    plt.close(fig)

    fig,ax=plt.subplots(figsize=(10,5))
    for field,label in [
        ("kinetic_energy_j","Kinetic energy"),
        ("volumetric_elastic_energy_j","Elastic strain energy"),
        ("gravity_potential_j","Gravity potential"),
        ("combined_mechanical_energy_j","Combined nonconservative mechanical energy")]:
        ax.plot(t,[float(r[field]) for r in rows],label=label)
    ax.set(xlabel="Actual fully integrated time (s)",ylabel="Energy (J)",
           title="Original two-body physical energy account (damped explicit solver)")
    ax.grid(alpha=.25);ax.legend();fig.tight_layout()
    fig.savefig(args.output/"23c_original_energy_account.png",dpi=190)
    plt.close(fig)
    print(json.dumps(summary,indent=2))

if __name__=="__main__":main()
