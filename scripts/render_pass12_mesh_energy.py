#!/usr/bin/env python3
"""Three honest Pass-12 figures from ORIGINAL native mesh FEM measurements.

Requires matplotlib, and audit_pass12_mesh_energy.py to have generated
mechanical_energy_steps.csv. Charts label energy changes as observations,
not friction-only dissipation or speedup evidence.
"""
import argparse
import csv
from pathlib import Path

def run():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--audit", type=Path, required=True,
                   help="Folder containing mechanical_energy_steps.csv")
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    with (args.audit / "mechanical_energy_steps.csv").open(
        encoding="utf-8", newline=""
    ) as stream:
        rows = list(csv.DictReader(stream))
    if not rows:
        raise ValueError("No source-accounted FEM steps found")
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError as exc:
        raise SystemExit("Install matplotlib to render source-data figures") from exc
    args.output.mkdir(parents=True, exist_ok=True)
    x = [float(r["time_s"]) for r in rows]
    energies = [float(r["cumulative_delta_mechanical_j"]) for r in rows]
    contact = [int(r["face_impulse_updates"]) for r in rows]
    delta = [float(r["delta_mechanical_j"]) for r in rows]
    regimes = [r["regime"] for r in rows]

    fig, ax = plt.subplots(figsize=(10, 5))
    ax.plot(x, energies, lw=1.8, label="Source discrete mechanical-energy change")
    ax.set(xlabel="Physical elapsed time (s)", ylabel="E(t) - E(0) (J)",
           title="Observed 3D FEM mechanical-energy evolution")
    ax.grid(alpha=0.25)
    ax.legend()
    fig.tight_layout()
    fig.savefig(args.output / "12a_mechanical_energy_observed.png", dpi=190)
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(10, 5))
    ax.plot(x, contact, lw=0.9, label="Actual accepted face impulse updates")
    ax.set(xlabel="Physical elapsed time (s)", ylabel="Impulse updates per macro-step",
           title="Real finite-triangle contact activity")
    ax.grid(alpha=0.25)
    ax.legend()
    fig.tight_layout()
    fig.savefig(args.output / "12b_actual_contact_activity.png", dpi=190)
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(10, 5))
    for regime, marker in (("contact", "o"), ("no_contact", "x")):
        indices = [i for i, r in enumerate(regimes) if r == regime]
        ax.scatter([x[i] for i in indices], [delta[i] for i in indices],
                   marker=marker, s=13, alpha=0.55, label=regime.replace("_", " "))
    ax.axhline(0.0, lw=0.8, color="gray")
    ax.set(xlabel="Physical elapsed time (s)", ylabel="Stepwise delta mechanical energy (J)",
           title="Mechanical energy changes grouped by actual contact receipts")
    ax.grid(alpha=0.25)
    ax.legend()
    fig.tight_layout()
    fig.savefig(args.output / "12c_energy_contact_correlation_not_causation.png", dpi=190)
    plt.close(fig)
    print(f"Three source-backed energy/contact figures: {args.output}")


if __name__ == "__main__":
    run()
