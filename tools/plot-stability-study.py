"""Plot saved stability results and initial relative orbital geometry.

Optional documentation tool: requires NumPy and Matplotlib, not the viewer.
All plotted values come from the tracked results JSON; raw CSVs are unnecessary.
"""
import argparse
import json
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
G = 4*np.pi**2


def ellipse(r, v, mass):
    radius = np.linalg.norm(r)
    h = np.cross(r, v)
    eccentricity_vector = np.cross(v, h)/(G*mass)-r/radius
    e = np.linalg.norm(eccentricity_vector)
    a = -G*mass/(np.dot(v, v)-2*G*mass/radius)
    p = eccentricity_vector/e
    q = np.cross(h/np.linalg.norm(h), p)
    angle = np.linspace(0, 2*np.pi, 500)
    distance = a*(1-e*e)/(1+e*np.cos(angle))
    return distance[:, None]*(np.cos(angle)[:, None]*p+np.sin(angle)[:, None]*q)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--results", type=Path, default=ROOT/"doc/experiments/stability/results.json")
    parser.add_argument("--output", type=Path, default=ROOT/"doc/experiments/stability")
    args = parser.parse_args()
    study = json.loads(args.results.read_text())
    args.output.mkdir(parents=True, exist_ok=True)
    plt.rcParams.update({"font.size": 10, "svg.fonttype": "none", "axes.spines.top": False,
                         "axes.spines.right": False, "savefig.facecolor": "white"})
    names = ("current", "inclined-12", "inclined-16", "inclined-24", "inclined-32",
             "coplanar-24", "high-inclination-24", "solar-triplet-24")
    labels = ("Current demo", "4 / 12 AU, 20°", "4 / 16 AU, 20°", "4 / 24 AU, 20°",
              "4 / 32 AU, 20°", "4 / 24 AU, coplanar", "4 / 24 AU, 60°", "Three solar masses, 4 / 24 AU")
    runs = [next(r for r in study["runs"] if r["preset"] == name and r["trial"] == "baseline") for name in names]
    fig, ax = plt.subplots(figsize=(10, 4.8), layout="constrained")
    for index, run in enumerate(runs):
        ax.barh(index, run["time"]-1, left=1, color="#24806b" if run["passed"] else "#be6740", height=.6)
        if run["passed"]:
            label = "1,000,000 yr ✓"
        elif run["preset"] == "current":
            label = "3 yr: hierarchy envelope"
        elif run["preset"] == "inclined-12":
            label = "678,440 yr: hierarchy envelope"
        else:
            label = "6,881 yr: planet envelope"
        ax.text(run["time"]*1.18, index, label, va="center", fontsize=9)
    ax.set(yticks=range(len(labels)), yticklabels=labels, xscale="log", xlim=(1, 2e7),
           xlabel="Integrated time to first screening failure or target (years)",
           title="Compact-triple study: baseline trials")
    ax.invert_yaxis()
    ax.axvline(1e6, color="#666666", lw=1, ls="--")
    ax.grid(axis="x", alpha=.15)
    fig.savefig(args.output/"screening.svg")
    plt.close(fig)

    run = next(r for r in runs if r["preset"] == "inclined-16")
    bodies = run["initial_bodies"]
    masses = np.array([body["mass"] for body in bodies])
    positions = np.array([body["position"] for body in bodies])
    velocities = np.array([body["velocity"] for body in bodies])
    center = np.average(positions[:2], axis=0, weights=masses[:2])
    drift = np.average(velocities[:2], axis=0, weights=masses[:2])
    inner = ellipse(positions[1]-positions[0], velocities[1]-velocities[0], masses[:2].sum())
    outer = ellipse(positions[2]-center, velocities[2]-drift, masses[:3].sum())
    planet = ellipse(positions[3]-positions[0], velocities[3]-velocities[0], masses[0])
    fig = plt.figure(figsize=(11, 5.6), layout="constrained")
    axes = [fig.add_subplot(1, 2, i+1, projection="3d") for i in range(2)]
    for ax, curves, limit, title in [
        (axes[0], [(inner, "#bc7b21", "B relative to A: 4 AU, 0°"),
                   (outer, "#bd5149", "C relative to AB: 16 AU, 20°")], 18, "Stellar reference orbits"),
        (axes[1], [(planet, "#187f7c", "Planet relative to A: 0.9 AU, 10°")], 1, "Planet reference orbit")]:
        for curve, color, label in curves:
            ax.plot(curve[:, 0], curve[:, 2], curve[:, 1], color=color, label=label, lw=1.8)
        corners = np.array([[-limit, -limit], [limit, -limit], [limit, limit], [-limit, limit], [-limit, -limit]])
        ax.plot(corners[:, 0], corners[:, 1], np.zeros(5), color="#777777", alpha=.5, ls="--", lw=.8)
        ax.scatter([0], [0], [0], s=18, color="#555555")
        ax.set(xlim=(-limit, limit), ylim=(-limit, limit), zlim=(-limit, limit),
               xlabel="Relative X (AU)", ylabel="Relative Z (AU)", zlabel="Relative Y (AU)", title=title)
        ax.set_box_aspect((1, 1, 1))
        ax.view_init(elev=20, azim=-60)
        ax.legend(loc="upper left", fontsize=8)
    fig.suptitle("Inclined-16: initial osculating ellipses, with different relative origins\n"
                 "Dashed outline: inner binary plane. These are reference ellipses, not integrated paths.", fontsize=11)
    fig.savefig(args.output/"geometry.svg")
    fig.savefig(args.output/"geometry.png", dpi=150)
    plt.close(fig)

    for name in ('screening.svg', 'geometry.svg'):
        path = args.output/name
        path.write_text('\n'.join(line.rstrip() for line in path.read_text().splitlines())+'\n')


if __name__ == "__main__":
    main()
