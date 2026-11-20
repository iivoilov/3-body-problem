"""Run a finite-time compact-triple study; no changes to the interactive preset.

Uses only Python's standard library and the project's stellar_stability target.
Raw checkpoint CSVs stay in the ignored build tree. Small reproducibility
summaries can be saved under doc/experiments/stability.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
PRESETS = ("current", "coplanar-24", "inclined-12", "inclined-16", "inclined-24", "inclined-32",
           "high-inclination-24", "solar-triplet-24")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, default=ROOT / "build-headless/stellar_stability")
    parser.add_argument("--years", type=float, default=1_000_000)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--results", type=Path, default=ROOT / "doc/experiments/stability/results.json")
    parser.add_argument("--raw", type=Path, default=ROOT / "build-headless/stability-runs")
    args = parser.parse_args()
    if args.years <= 0 or not 1 <= args.jobs <= 8:
        parser.error("Duration must be positive and jobs must be between 1 and 8.")
    args.raw.mkdir(parents=True, exist_ok=True)
    executable = args.executable.resolve()
    start = time.monotonic()
    results = []

    def trial(preset, tag, dt, phase=0, tilt=0):
        command = [str(executable), preset, "--years", str(args.years), "--step", str(dt),
                   "--phase", str(phase), "--inclination-offset", str(tilt),
                   "--output", str(args.raw / f"{preset}-{tag}.csv")]
        completed = subprocess.run(command, check=True, capture_output=True, text=True)
        result = json.loads(completed.stdout)
        result["trial"] = tag
        result["command"] = ["./build-headless/stellar_stability", *command[1:-1],
                             f"build-headless/stability-runs/{preset}-{tag}.csv"]
        print(f"{preset:24s} {tag:14s} {'PASS' if result['passed'] else 'FAIL'} "
              f"t={result['time']:.3f} yr  {result['failure']}  "
              f"max |dE/E|={result['max_stellar_energy_error']:.3g}", flush=True)
        return result

    def batch(specifications):
        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            futures = [pool.submit(trial, *specification) for specification in specifications]
            for future in as_completed(futures):
                results.append(future.result())

    batch([(preset, "baseline", 1/512) for preset in PRESETS])
    passed = {result["preset"] for result in results if result["passed"]}
    refinements = []
    for preset in ("inclined-16", "inclined-24", "solar-triplet-24"):
        if preset in passed:
            refinements.append((preset, "half-step", 1/1024))
    refinements.extend([("inclined-12", "half-step", 1/1024),
                        ("high-inclination-24", "half-step", 1/1024),
                        ("current", "viewer-step", 1/16384)])
    for preset in ("inclined-16", "inclined-24"):
        if preset in passed:
            refinements.extend([(preset, "phase-plus", 1/512, 1, 1),
                                (preset, "phase-minus", 1/512, -1, -1)])
    batch(refinements)
    summary = {
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "git_base": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "platform": platform.platform(),
        "compiler": subprocess.check_output(["c++", "--version"], text=True).splitlines()[0],
        "source_sha256": {name: hashlib.sha256((ROOT/name).read_bytes()).hexdigest()
                          for name in ("src/stability.hpp", "src/stability_experiment.cpp", "src/alien_world.hpp", "src/physics.hpp",
                                       "tools/run-stability-study.py")},
        "integrator": "Existing pairwise Newtonian kick-drift-kick leapfrog; independent stars and massless planet",
        "units": {"length": "AU", "mass": "solar mass", "time": "Julian year"},
        "check_interval_years": 0.125,
        "checkpoint_interval_years": 128,
        "limits": {"stellar_span_AU": 100, "semimajor_axis_factor": [0.5, 2], "planet_host_distance_AU": 5,
                   "stellar_energy_error": 1e-4, "stellar_angular_error": 1e-8},
        "warning": "Finite sampled diagnostics, not a stability proof. Trials stop at the first failure; a planet failure does not prove stellar disruption. Conservation checks exclude the massless planet.",
        "wall_seconds": time.monotonic()-start,
        "runs": sorted(results, key=lambda result: (result["preset"], result["trial"])),
    }
    args.results.parent.mkdir(parents=True, exist_ok=True)
    args.results.write_text(json.dumps(summary, indent=2, allow_nan=False)+"\n")
    print(f"Saved {len(results)} trials to {args.results}", flush=True)


if __name__ == "__main__":
    main()
