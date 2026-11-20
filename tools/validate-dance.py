"""Optional independent eight-year DOP853 check of the dance experiment."""
import json
from pathlib import Path
import subprocess

import numpy as np
import scipy
from scipy.integrate import solve_ivp

ROOT = Path(__file__).resolve().parents[1]


def main():
    executable = str(ROOT/'build-headless/stellar_dance')
    runs = [json.loads(subprocess.check_output([executable, '--years', '8', '--radius', '5',
                                               '--step', str(1/n)], text=True)) for n in (512, 1024, 2048)]
    initial = np.array([b['position']+b['velocity'] for b in runs[0]['initial_bodies']])
    masses = np.array([1, 1, 1, 0])

    def field(_time, values):
        state = values.reshape(4, 6)
        delta = state[None, :, :3]-state[:, None, :3]
        distance = np.linalg.norm(delta, axis=2)
        np.fill_diagonal(distance, np.inf)
        acceleration = 4*np.pi**2*np.sum(delta*masses[None, :, None]/distance[:, :, None]**3, axis=1)
        return np.concatenate((state[:, 3:], acceleration), axis=1).ravel()

    solution = solve_ivp(field, (0, 8), initial.ravel(), method='DOP853', rtol=2e-13, atol=1e-14, max_step=.005)
    if not solution.success:
        raise RuntimeError(solution.message)
    reference = solution.y[:, -1].reshape(4, 6)
    comparisons = []
    for run in runs:
        final = np.array([b['position']+b['velocity'] for b in run['final_bodies']])
        errors = []
        for group in (slice(0, 3), slice(3, 4)):
            error = np.linalg.norm((final[group]-reference[group])/np.array([4, 4, 4, np.pi, np.pi, np.pi]))
            errors.append(float(error))
        comparisons.append({'dt': run['dt'], 'scaled_stellar_state_error': errors[0], 'scaled_planet_state_error': errors[1]})
    ratios = {key: [comparisons[i+1][key]/comparisons[i][key] for i in (0, 1)]
              for key in ('scaled_stellar_state_error', 'scaled_planet_state_error')}
    assert all(.02 < r < .15 for values in ratios.values() for r in values), ratios
    output = {'years': 8, 'method': 'DOP853', 'rtol': 2e-13, 'atol': 1e-14,
              'max_step_years': .005, 'function_evaluations': solution.nfev,
              'numpy_version': np.__version__, 'scipy_version': scipy.__version__,
              'reference_final_state': reference.tolist(), 'comparisons': comparisons, 'halving_error_ratios': ratios,
              'warning': 'Short independent comparison; no independent million-year integration.'}
    path = ROOT/'doc/experiments/dance/independent-validation.json'
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(output, indent=2)+'\n')
    print(ratios)


if __name__ == '__main__':
    main()
