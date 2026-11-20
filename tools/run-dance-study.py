"""Research-only million-year tests of a published spatial stellar dance.

No graphics dependencies; leaves the application preset unchanged.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import platform
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--years', type=float, default=1_000_000)
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    if args.years <= 0 or not 1 <= args.jobs <= 8:
        parser.error('Positive years and 1–8 jobs required.')
    output = ROOT/'doc/experiments/dance'
    raw = ROOT/'build-headless/dance-runs'
    output.mkdir(parents=True, exist_ok=True)
    raw.mkdir(parents=True, exist_ok=True)
    specifications = [('radius-4-control', 4, 1/512, 0, 0),
                      ('radius-5-baseline', 5, 1/512, 0, 0),
                      ('radius-5-half-step', 5, 1/1024, 0, 0),
                      ('radius-5-perturbed-stars', 5, 1/512, 0, 1e-6),
                      ('radius-5-phase-90', 5, 1/512, 90, 0),
                      ('radius-6-baseline', 6, 1/512, 0, 0)]

    def trial(spec):
        name, radius, step, phase, perturb = spec
        command = ['./build-headless/stellar_dance', '--years', str(args.years), '--scale', '4',
                   '--radius', str(radius), '--step', str(step), '--phase', str(phase),
                   '--perturb', str(perturb), '--output', f'build-headless/dance-runs/{name}.csv']
        result = json.loads(subprocess.check_output(command, cwd=ROOT, text=True))
        result.update(trial=name, command=command)
        print(name, 'PASS' if result['passed'] else 'FAIL', result['time'], result['failure'], flush=True)
        return result

    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        runs = [future.result() for future in as_completed([pool.submit(trial, spec) for spec in specifications])]
    summary = {'created_utc': datetime.now(timezone.utc).isoformat(),
               'git_base': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
               'source': 'https://arxiv.org/html/2508.08568v1#Sx1.T4',
               'source_table': 'Supplementary Table S4: Li & Liao O3(1.0)',
               'source_sha256': {name: hashlib.sha256((ROOT/name).read_bytes()).hexdigest()
                                 for name in ('src/stellar_dance.cpp', 'src/stability.hpp', 'src/alien_world.hpp',
                                              'src/physics.hpp', 'tools/run-dance-study.py')},
               'platform': platform.platform(),
               'compiler': subprocess.check_output(['c++', '--version'], text=True).splitlines()[0],
               'units': {'length': 'AU', 'mass': 'solar mass', 'time': 'Julian year'},
               'limits': {'stellar_span_AU': 100, 'planet_barycentre_distance_AU': 100,
                          'stellar_relative_energy_error': 1e-4, 'stellar_relative_angular_error': 1e-8},
               'check_interval_years': .0625,
               'luminosities_solar': [1, 1, 1],
               'warning': 'Finite numerical retention, not a proof or a climate model. Planet orbits the whole triple. Contact uses viewer reference radii. Preview records first 64 years; no periodic reset or replay is used in long integration.',
               'runs': sorted(runs, key=lambda r: r['trial'])}
    (output/'results.json').write_text(json.dumps(summary, indent=2, allow_nan=False)+'\n')
    # A short record for independently reproducible scientific figures.
    baseline = raw/'radius-5-baseline.csv'
    (output/'preview.csv').write_bytes(baseline.read_bytes())


if __name__ == '__main__':
    main()
