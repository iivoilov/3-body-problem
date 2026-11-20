"""Optional independent short-run check using installed NumPy/SciPy DOP853.

This supplements the million-year leapfrog timestep comparisons; it is not a
second million-year integration. The regular build/study needs no Python packages.
"""
import argparse
import csv
import json
from pathlib import Path
import subprocess
import tempfile

import numpy as np
import scipy
from scipy.integrate import solve_ivp

ROOT = Path(__file__).resolve().parents[1]
G = 4 * np.pi**2


def orbit(r, v, mass):
    mu = G * mass
    distance = np.linalg.norm(r)
    energy = np.dot(v, v)/2 - mu/distance
    h = np.cross(r, v)
    return -mu/(2*energy), np.linalg.norm(np.cross(v,h)/mu-r/distance), h


def diagnostics(state, masses):
    positions, velocities = state[:, :3], state[:, 3:]
    inner = orbit(positions[1]-positions[0], velocities[1]-velocities[0], masses[0]+masses[1])
    center = np.average(positions[:2], axis=0, weights=masses[:2])
    drift = np.average(velocities[:2], axis=0, weights=masses[:2])
    outer = orbit(positions[2]-center, velocities[2]-drift, masses[:3].sum())
    planet = orbit(positions[3]-positions[0], velocities[3]-velocities[0], masses[0])
    return np.array([inner[0], inner[1], outer[0], outer[1], planet[0], planet[1]])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--results", type=Path, default=ROOT/"doc/experiments/stability/results.json")
    parser.add_argument("--output", type=Path, default=ROOT/"doc/experiments/stability/independent-validation.json")
    args = parser.parse_args()
    study = json.loads(args.results.read_text())
    cases = []
    for name in ("inclined-16", "inclined-24", "solar-triplet-24"):
        run = next(row for row in study["runs"] if row["preset"] == name and row["trial"] == "baseline")
        masses = np.array([body["mass"] for body in run["initial_bodies"]])
        initial = np.array([body["position"]+body["velocity"] for body in run["initial_bodies"]])

        def acceleration(_time, values):
            state = values.reshape(4,6)
            delta = state[None,:,:3]-state[:,None,:3]
            radii = np.linalg.norm(delta,axis=2)
            np.fill_diagonal(radii,np.inf)
            field = G*np.sum(delta*masses[None,:,None]/radii[:,:,None]**3,axis=1)
            return np.concatenate((state[:,3:],field),axis=1).ravel()

        solution = solve_ivp(acceleration,(0,128),initial.ravel(),method="DOP853",rtol=2e-12,atol=1e-13,
                             max_step=0.05,t_eval=np.arange(0,128.125,0.125))
        if not solution.success:
            raise RuntimeError(solution.message)
        reference = diagnostics(solution.y[:,-1].reshape(4,6),masses)
        comparisons = []
        with tempfile.TemporaryDirectory(prefix="stability-reference-") as temporary:
            for denominator in (256,512,1024):
                output = Path(temporary)/"trajectory.csv"
                subprocess.run([str(ROOT/"build-headless/stellar_stability"),name,"--years","128",
                                "--step",str(1/denominator),"--output",str(output)],
                               check=True,capture_output=True,text=True)
                rows = list(csv.DictReader(output.open()))
                last = rows[-1]
                value = np.array([float(last[key]) for key in ("inner_a","inner_e","outer_a","outer_e","planet_a","planet_e")])
                error = float(np.linalg.norm((value-reference)/np.maximum(np.abs(reference),1)))
                comparisons.append({"dt":1/denominator,"relative_scaled_element_error":error,"elements":value.tolist()})
        ratios = [comparisons[i+1]["relative_scaled_element_error"]/comparisons[i]["relative_scaled_element_error"] for i in (0,1)]
        assert all(0.12 < ratio < 0.40 for ratio in ratios), (name,ratios)
        cases.append({"preset":name,"years":128,"method":"DOP853","rtol":2e-12,"atol":1e-13,
                      "function_evaluations":solution.nfev,"reference_elements":reference.tolist(),
                      "leapfrog_comparisons":comparisons,"halving_error_ratios":ratios})
        print(name,"independent element errors:",[row["relative_scaled_element_error"] for row in comparisons],
              "halving ratios:",ratios,flush=True)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps({"numpy_version":np.__version__,"scipy_version":scipy.__version__,
                                     "element_order":["inner_a","inner_e","outer_a","outer_e","planet_a","planet_e"],
                                     "warning":"Independent validation covers 128 years, not one million years.","runs":cases},indent=2)+"\n")


if __name__ == "__main__":
    main()
