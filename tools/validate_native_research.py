#!/usr/bin/env python3
"""Run cumulative Kyoto research gates independently; preserve failures.

No performance claims are generated. An unavailable optional dependency,
compile failure, missing test, or numerical failure is recorded explicitly.
"""
import argparse
import json
import os
from pathlib import Path
import platform
import subprocess
import time

NATIVE = [
    ("kyoto_linear_candidate_adapter_tests", "indexed_linear_solver_candidate_adapter_oracle"),
    ("kyoto_swept_cell_candidates_tests", "swept_cell_candidate_exhaustive_parity"),
    ("kyoto_static_face_solver_parity_tests", "actual_FEM_static_face_broadphase_physical_parity"),
    ("kyoto_coupled_triangle_normal_impulse_tests", "two_finite_mass_triangle_normal_impulse_balance"),
    ("kyoto_tet_orientation_certificate_tests", "outward_continuous_tet_orientation_certificate"),
    ("kyoto_rounded_relative_triangle_tests", "outward_relative_moving_triangle_surface_certificate"),
    ("kyoto_coupled_fem_contact_tests", "synchronized_two_body_FEM_contact_and_rollback"),
    ("kyoto_nonincident_self_contact_tests", "restricted_nonincident_same_body_contact"),
    ("kyoto_typed_self_contact_audit_tests", "typed_adjacent_face_self_contact_audit"),
    ("kyoto_typed_connected_self_contact_tests", "typed_connected_FEM_self_contact"),
    ("kyoto_coupled_torus_physics_tests", "coupled_toroidal_volumes_physical_parity"),
    ("kyoto_persistent_fem_context_tests", "persistent_typed_FEM_physical_parity"),
    ("kyoto_initial_tet_separation_tests", "outward_initial_tetrahedral_separation"),
    ("kyoto_initial_tet_exact_oracle_tests", "initial_tetrahedral_exact_integer_oracle"),
    ("kyoto_initial_volume_fem_tests", "initial_volume_FEM_containment_rollback"),
    ("kyoto_volume_boundary_topology_tests", "complete_volume_boundary_topology"),
    ("kyoto_volume_boundary_fixture_tests", "standalone_volume_boundary_fixture_patterns"),
    ("kyoto_native_volume_boundary_fixture_tests", "actual_native_volume_boundary_factories"),
]
EXACT = [
    ("kyoto_exact_tet_intersection_tests", "exact_static_tetrahedral_intersection"),
    ("kyoto_exact_swept_tet_certificate_tests", "exact_continuous_tetrahedral_separation"),
    ("kyoto_exact_factored_axis_tests", "exact_factored_axis_coefficient_and_pair_parity"),
    ("kyoto_exact_constant_axis_tests", "exact_constant_axis_polynomial_parity"),
    ("kyoto_exact_axis_heldout_tests", "exact_axis_heldout_pair_parity"),
    ("kyoto_exact_axis_heldout_volume_tests", "exact_axis_heldout_whole_volume_parity"),
    ("kyoto_exact_swept_volume_fixture_tests", "exact_continuous_volume_fixture_patterns"),
    ("kyoto_native_exact_swept_volume_tests", "exact_continuous_native_FEM_leaf_volume"),
    ("kyoto_exact_volume_fixture_tests", "exact_static_volume_fixture_patterns"),
    ("kyoto_native_exact_self_volume_tests", "exact_static_native_FEM_endpoint_self_volume"),
]
IPC = [
    ("kyoto_pass34_connected_ipc_audit", "pinned_IPC_connected_FEM_accepted_and_rejected_CCD"),
    ("kyoto_official_ipc_native_backend_tests", "pinned_IPC_LBVH_inside_native_typed_FEM"),
]
METAL = [("kyoto_pass37_metal_physical_scaling", "physical_Metal_candidate_scaling_smoke")]


def execute(command, log, timeout):
    start = time.monotonic()
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("w", encoding="utf-8") as handle:
        handle.write("command: " + json.dumps(command) + "\n")
        handle.flush()
        try:
            result = subprocess.run(command, stdout=handle, stderr=subprocess.STDOUT,
                                    timeout=timeout, check=False)
            code = result.returncode
        except (OSError, subprocess.TimeoutExpired) as error:
            handle.write("\nrunner error: " + str(error) + "\n")
            code = 124 if isinstance(error, subprocess.TimeoutExpired) else 127
    return {"exit_code": code, "elapsed_seconds": time.monotonic() - start,
            "log": str(log)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path.cwd())
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--parallel", type=int, default=min(4, os.cpu_count() or 1))
    parser.add_argument("--timeout", type=int, default=1200, help="seconds per command")
    parser.add_argument("--exact", action="store_true")
    parser.add_argument("--ipc", action="store_true")
    parser.add_argument("--metal", action="store_true")
    args = parser.parse_args()
    if args.parallel < 1 or args.timeout < 1:
        parser.error("parallel and timeout must be positive")
    source = args.source.resolve()
    output = args.output.resolve()
    # Do not overwrite previous validation receipts.
    output.mkdir(parents=True, exist_ok=False)
    manifest = {"source": str(source), "platform": platform.platform(), "groups": []}
    manifest["revision"] = execute(["git", "-C", str(source), "rev-parse", "HEAD"],
                                   output / "revision.log", args.timeout)
    groups = [("native", NATIVE, [])]
    if args.exact:
        groups.append(("exact", EXACT, ["-DKYOTO_WITH_EXACT_SELF_VOLUME=ON"]))
    if args.ipc:
        groups.append(("ipc", IPC, ["-DKYOTO_WITH_OFFICIAL_IPC=ON", "-DIPC_TOOLKIT_WITH_SIMD=OFF"]))
    if args.metal:
        groups.append(("metal", METAL, []))
    failure = manifest["revision"]["exit_code"] != 0
    for name, targets, options in groups:
        build = output / ("build-" + name)
        group = {"name": name, "targets": []}
        manifest["groups"].append(group)
        group["configure"] = execute(["cmake", "-S", str(source), "-B", str(build),
                                      "-DCMAKE_BUILD_TYPE=Release", *options],
                                     output / name / "configure.log", args.timeout)
        configured = group["configure"]["exit_code"] == 0
        for target, regex in targets:
            row = {"target": target, "ctest_regex": regex}
            group["targets"].append(row)
            if not configured:
                row["status"] = "blocked_configuration"
                failure = True
                continue
            row["build"] = execute(["cmake", "--build", str(build), "--target", target,
                                    "--parallel", str(args.parallel)],
                                   output / name / (target + ".build.log"), args.timeout)
            if row["build"]["exit_code"]:
                row["status"] = "build_failed"
                failure = True
            else:
                row["test"] = execute(["ctest", "--test-dir", str(build), "-R", "^" + regex + "$",
                                       "--no-tests=error", "--output-on-failure", "-V"],
                                      output / name / (target + ".test.log"), args.timeout)
                if row["test"]["exit_code"]:
                    row["status"] = "test_failed"
                    failure = True
                else:
                    log = Path(row["test"]["log"]).read_text(errors="replace")
                    row["status"] = "skipped" if "***Skipped" in log else "passed"
                    # A skip is not a validated hardware result.
                    failure |= row["status"] == "skipped"
            print(f"{name}: {target}: {row['status']}", flush=True)
            (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return int(failure)


if __name__ == "__main__":
    raise SystemExit(main())
