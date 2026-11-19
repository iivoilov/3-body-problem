"""Run reproducible, non-graphical CTL experiments and reduce CSVs to time windows.

Uses only Python's standard library. Raw time series stay in the ignored build
directory; small reports can be kept under doc/experiments/tides.
"""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import subprocess
import time


ROOT = Path(__file__).resolve().parents[1]
FIELDS = ("period_ratio", "a_azure", "a_ember", "e_azure", "e_ember",
          "spin_over_n_azure", "spin_over_n_ember")


def new_phase():
    return {"valid": 0, "segments": {}, "min": math.inf, "max": -math.inf,
            "first": None, "last": None}


def record_phase(phase, value, segment):
    if value is None:
        return
    phase["valid"] += 1
    segment = phase["segments"].setdefault(int(segment), [value,value,value,value])
    segment[0] = min(segment[0], value)
    segment[1] = max(segment[1], value)
    segment[3] = value
    phase["min"] = min(phase["min"], value)
    phase["max"] = max(phase["max"], value)
    if phase["first"] is None:
        phase["first"] = value
    phase["last"] = value


def phase_summary(phase, samples):
    contiguous = phase["valid"] == samples and len(phase["segments"]) == 1
    return {"valid_fraction": phase["valid"]/samples, "segments": len(phase["segments"]),
            "span_turns": phase["max"]-phase["min"] if contiguous else None,
            "net_turns": phase["last"]-phase["first"] if contiguous else None,
            "largest_segment_span_turns": max((s[1]-s[0] for s in phase["segments"].values()), default=None),
            "largest_segment_abs_net_turns": max((abs(s[3]-s[2]) for s in phase["segments"].values()), default=None)}


def new_window():
    return {"samples": 0, "start": None, "end": None,
            "sums": {key: 0.0 for key in FIELDS},
            "mins": {key: math.inf for key in FIELDS},
            "maxs": {key: -math.inf for key in FIELDS},
            "phases": [new_phase() for _ in range(2)]}


def finish_window(window):
    result = {"start": window["start"], "end": window["end"], "samples": window["samples"]}
    for key in FIELDS:
        result[key] = {"mean": window["sums"][key]/window["samples"],
                       "min": window["mins"][key], "max": window["maxs"][key]}
    for name, phase in zip(("azure", "ember"), window["phases"]):
        result[f"phi_{name}"] = phase_summary(phase,window["samples"])
    return result


def reduce_csv(path, window_size):
    windows = []
    maxima = {key: 0.0 for key in ("energy_budget_error", "angular_momentum_error", "net_momentum", "lag_phase_bound")}
    initial = final = None
    all_phases = [new_phase() for _ in range(2)]
    samples = 0
    with path.open() as stream:
        for row in csv.DictReader(stream):
            values = {key: float(value) if value else None for key, value in row.items()}
            if initial is None:
                initial = values
            final = values
            samples += 1
            index = max(0, math.ceil(values["time"]/window_size)-1)
            while len(windows) <= index:
                windows.append(new_window())
            window = windows[index]
            window["samples"] += 1
            if window["start"] is None:
                window["start"] = values["time"]
            window["end"] = values["time"]
            for key in FIELDS:
                value = values[key]
                window["sums"][key] += value
                window["mins"][key] = min(window["mins"][key], value)
                window["maxs"][key] = max(window["maxs"][key], value)
            for i, name in enumerate(("azure", "ember")):
                value = values[f"phi_{name}_turns"]
                segment = values[f"segment_{name}"]
                record_phase(window["phases"][i],value,segment)
                record_phase(all_phases[i],value,segment)
            for key in maxima:
                maxima[key] = max(maxima[key], abs(values[key]))
    return {"initial": initial, "final": final, "sampled_maxima": maxima,
            "overall_phases": {name: phase_summary(phase,samples)
                               for name, phase in zip(("azure", "ember"),all_phases)},
            "dissipated_fraction": final["heat"]/abs(initial["mechanical_energy"]),
            "windows": [finish_window(window) for window in windows if window["samples"]]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, default=ROOT/"build-headless/tidal_experiment")
    parser.add_argument("--output", type=Path, default=ROOT/"build-headless/tidal-study")
    parser.add_argument("--report", type=Path, default=ROOT/"doc/experiments/tides")
    parser.add_argument("--duration", type=float, default=4194304)
    parser.add_argument("--step", type=float, default=1/32)
    parser.add_argument("--interval", type=float, default=8)
    parser.add_argument("--window", type=float, default=16384)
    parser.add_argument("--reuse", action="store_true", help="Reuse completed runs with identical configuration")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    args.report.mkdir(parents=True, exist_ok=True)
    configurations = []
    for preset in ("circular", "resonance"):
        for mode in ("none", "inner", "both"):
            configurations.append(dict(name=f"{preset}-{mode}", preset=preset, tides=mode))
    configurations += [
        dict(name="resonance-elastic", preset="resonance", tides="elastic"),
        dict(name="resonance-both-weak", preset="resonance", tides="both", lag_inner=0.01, lag_outer=0.01),
        dict(name="resonance-both-large-outer", preset="resonance", tides="both", radius_outer=0.4),
        dict(name="resonance-outer-dominated", preset="resonance", tides="both", lag_inner=0.01, radius_outer=0.4),
        dict(name="circular-both-fine", preset="circular", tides="both", step=args.step/2),
        dict(name="resonance-both-fine", preset="resonance", tides="both", step=args.step/2),
        dict(name="resonance-outer-dominated-fine", preset="resonance", tides="both", lag_inner=0.01,
             radius_outer=0.4, step=args.step/2),
    ]
    reports = []
    started = time.monotonic()
    for config in configurations:
        path = args.output/f"{config['name']}.csv"
        settings = dict(preset=config["preset"], tides=config["tides"], duration=args.duration,
                        step=config.get("step", args.step), interval=args.interval,
                        lag_inner=config.get("lag_inner", 0.1), lag_outer=config.get("lag_outer", 0.1),
                        radius_inner=0.15, radius_outer=config.get("radius_outer", 0.15*2**(1/3)))
        metadata = path.with_suffix(".json")
        cached = json.loads(metadata.read_text()) if metadata.exists() else None
        # Include the executable hash so a changed solver invalidates old results.
        executable_hash = hashlib.sha256(args.executable.read_bytes()).hexdigest()
        if args.reuse and path.exists() and cached and cached["settings"] == settings and cached["executable_sha256"] == executable_hash:
            console = cached["console"]
            print(f"Reusing {config['name']}", flush=True)
        else:
            metadata.unlink(missing_ok=True)
            command = [str(args.executable.resolve())]
            for key, value in settings.items():
                command += ["--"+key.replace("_", "-"), str(value)]
            command += ["--output", str(path.resolve())]
            run = subprocess.run(command, check=True, text=True, capture_output=True)
            console = run.stdout.strip()
            metadata.write_text(json.dumps(dict(settings=settings, executable_sha256=executable_hash,
                                               console=console), indent=2)+"\n")
            print(console, flush=True)
        report = reduce_csv(path, args.window)
        if not math.isclose(report["final"]["time"], settings["duration"], rel_tol=0, abs_tol=1e-8):
            raise RuntimeError(f"Incomplete time series: {path}; rerun without --reuse")
        report.update(name=config["name"], settings=settings, console=console.split("; CSV ")[0])
        reports.append(report)
        late = report["windows"][-1]
        print(f"  Late mean ratio {late['period_ratio']['mean']:.6f}; e "
              f"{late['e_azure']['mean']:.6f}, {late['e_ember']['mean']:.6f}; "
              f"phase spans {late['phi_azure']['span_turns']}, {late['phi_ember']['span_turns']}; "
              f"elapsed {time.monotonic()-started:.1f}s", flush=True)
        # Save progress after every complete run.
        summary = dict(model="planetary equilibrium tide, constant time lag, with evolving spins",
                       gravity=0.00856736493150118, masses=[1000,2,4], love_number=0.3,
                       inertia_factor=0.33, primary_contact_radius=1.0,
                       executable_sha256=executable_hash,
                       diagnostic_interval=0.5, window_seconds=args.window,
                       sources_sha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                                       for p in (ROOT/"src/physics.hpp", ROOT/"src/scene.hpp", ROOT/"src/math.hpp",
                                                 ROOT/"src/orbit_diagnostics.hpp", ROOT/"src/tides.hpp",
                                                 ROOT/"src/tidal_experiment.cpp", Path(__file__).resolve())},
                       runs=[dict(**{key: value for key, value in report.items() if key != "windows"},
                                  window_count=len(report["windows"]), first_window=report["windows"][0],
                                  last_window=report["windows"][-1]) for report in reports])
        (args.report/"summary.json").write_text(json.dumps(summary, indent=2, allow_nan=False)+"\n")
    columns = ["run", "start", "end", "mean_period_ratio", "mean_e_azure", "mean_e_ember",
               "phi_azure_span_turns", "phi_ember_span_turns", "phi_azure_net_turns", "phi_ember_net_turns"]
    with (args.report/"windows.csv").open("w", newline="") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(columns)
        for report in reports:
            for window in report["windows"]:
                writer.writerow([report["name"], window["start"], window["end"], window["period_ratio"]["mean"],
                                 window["e_azure"]["mean"], window["e_ember"]["mean"],
                                 window["phi_azure"]["span_turns"], window["phi_ember"]["span_turns"],
                                 window["phi_azure"]["net_turns"], window["phi_ember"]["net_turns"]])
    print(f"Reports: {args.report}")


if __name__ == "__main__":
    main()
