# Orbit / 02 — three stars

Orbit / 02 is a working native and browser experience alongside Orbit / 01.
Three stars interact through Newtonian gravity, while an alien planet responds
to all three as a massless test particle. A procedural landscape shows the sky
from the rotating planet; an orbital camera reveals the same system from outside.

```bash
./run.sh --orbit 02
./run.sh --orbit 02 --view orbit
./run.sh --orbit 02 --time 8       # Integrate forward to year 8
```

Press **V** to switch surface/orbital views. Press **1** to return to Orbit / 01,
and **2** to open Orbit / 02 again. Version switching resets the selected version.

| Input | Orbit / 02 action |
| --- | --- |
| Left drag | Look around on the surface / rotate the orbital camera |
| Scroll | Zoom the orbital camera |
| Space / N | Pause or resume / advance 16 physics steps while paused |
| `[` / `]` | Halve / double playback speed |
| V / O / D / S | Switch view / toggle orbital trails / data / terrain shadows |
| R / Escape | Reset the initial world and camera / close |

Displayed **1×** advances **0.08 simulated year per real second**. `--time` uses
years for Orbit / 02 and seconds for Orbit / 01. No options still opens Orbit / 01.
The orbital camera automatically adjusts its scale as the system expands.

For the browser:

```bash
python3 -m http.server 8000 --directory doc
# Open http://localhost:8000/orbit-02.html
```

The browser provides playback, scrubbing, surface/orbital camera selection,
terrain shadows, orbital trails, incident-light history, per-star distances,
stellar separation, and conservation readouts. Jump buttons locate the closest
stellar passage and strongest light directly from the recording. Playback loops
at 36 years; a reduced-motion preference starts paused. URL parameters include
`?view=orbit&time=8&paused=1`. The original viewer remains at `index.html`.

## What masses can we justify?

Netflix's [official episode 3 recap](https://www.netflix.com/tudum/articles/3-body-problem-episode-3-recap)
describes a planet moving among three suns. It supplies no numerical stellar
masses or reproducible initial conditions. No exact mass triplet has been
verified here from the novel or the series. Do not present real astronomical
measurements as canonical fictional parameters.

For a measured reference, use the real Alpha Centauri system:

| Star | Mass in solar masses | Source |
| --- | --- | --- |
| Alpha Centauri A | 1.0788 ± 0.0029 | Akeson et al. (2021), Table 8 |
| Alpha Centauri B | 0.9092 ± 0.0025 | Akeson et al. (2021), Table 8 |
| Proxima Centauri (C) | 0.1221 ± 0.0022 | Faria et al. (2022), Table 1 |

[Akeson et al., *Precision Millimeter Astrometry of the α Centauri AB System*](https://doi.org/10.3847/1538-3881/abfaff)
reports the A/B measurements.
[Faria et al., *A candidate short-period sub-Earth orbiting Proxima Centauri*](https://www.eso.org/public/archives/releases/sciencepapers/eso2202/eso2202a.pdf)
compiles Proxima's mass from the literature. These are sourced reference values,
not a claim to have selected the latest or uniquely correct estimates.

Real Alpha Centauri is hierarchical: A and B form a close pair, while Proxima
travels much farther out. [Kervella et al. (2017)](https://arxiv.org/abs/1611.03495)
estimate Proxima's orbital period at roughly 550,000 years. Copying the masses
alone does not recreate the fictional system or determine its trajectories.

The implemented preset is a **fiction-inspired compact triple** using those
measured mass ratios. Its initial coordinates and velocities are chosen, not
observations of Alpha Centauri or parameters verified from the novel. An
observational Alpha Centauri orbit reconstruction would be a different preset.

## Numerical model and chosen initial conditions

Units are **AU, solar masses, and Julian years**, with **G = 4π²**. The solver
reuses the existing pairwise force calculation, scaling G to these units. A
fixed **1/16384-year** kick–drift–kick leapfrog step advances the three stars and
the planet together. The planet receives the stars' accelerations at both ends
of each step, with no back reaction. Stellar energy and momentum diagnostics
therefore refer only to the three massive bodies.

The starting A/B separation is 4 AU, with circular two-body relative velocity.
The third star is initially at `(-1.5, 0.65, 6.0)` AU, with velocity
`(-3.1, -0.15, -0.7)` AU/year before subtracting the stellar barycentre and drift.
The planet starts at apoapsis of an osculating `a = 0.90 AU`, `e = 0.16` orbit
around A. Every initial position and velocity is written explicitly into
[`data/orbit-02/manifest.json`](data/orbit-02/manifest.json).

The reference run passes through a stellar separation of about **0.178 AU**.
Over 36 years, the measured incident flux spans roughly **0.003–37 times Earth's**;
the planet subsequently travels far outside the inner system. This is one
chosen numerical trajectory, not a proof of chaos or indefinite stability.
Maximum relative stellar energy error is about **1.32 × 10⁻⁵** at the selected
step. The tests check second-order short-interval trajectory convergence,
linear/angular momentum, barycentre drift, contact handling, reproducibility,
and invariance under render-frame partitioning. Long chaotic trajectories may
diverge between numerical resolutions even when conservation checks are good.

The simulation pauses at stellar or planetary surface contact, checking the
initial state and the end of the drift before evaluating the next force.
Contact uses physical radii, separately from enlarged display radii. No
softening, star merging, tidal force, relativity, or climate model is added.

## The world and its light

Surface sun directions come from the star-to-planet vectors transformed into a
rotating observer's local sky. The invented slow rotation period is **1.6 years**;
the observer is at high latitude (about 74°). Initial conditions put all three
suns above the horizon. Their apparent sizes are enlarged for readability.

The GPU builds a local height-field landscape with sand, basalt slopes, strata,
terrain shadows, atmospheric haze, stellar disks, and coronas. Its exposure,
colours, scattering, and terrain are an artistic interpretation. The native app
and browser share the same landscape and stellar fragment shaders in
`doc/shaders/alien-*.{vert,frag}`. They require no downloaded textures or models.
Noise uses integer cell hashing to avoid visible seams between GPU noise cells.
Terrain intersections are refined, grazing rays have a bounded fallback, and
fine material details fade with their pixel footprint. Rock layers are broad
and irregular rather than tightly repeated stripes. Atmospheric fog excludes
sharp stars and stellar disks to avoid stretching them into surface streaks.

The numerical **incident flux** is `sum(L_i / distance_i²)` in Earth units,
using stellar luminosities of 1.5059, 0.4981, and 0.0016 solar luminosities from
the sources above. It is a planet-level inverse-square measurement, before
local surface orientation, terrain, eclipses, or an atmosphere; it is not a
temperature prediction or an automatic stable/chaotic-era classifier.

## Export, verification, and recording

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
./build/orbit02_export doc/data/orbit-02
ctest --test-dir build --output-on-failure
./run.sh --orbit 02 --smoke-test
python3 tools/check-orbit02.py --screenshots /tmp/orbit-02-check
# Include the native night-sky regression on the selected graphics adapter:
MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA python3 tools/check-orbit02.py --native ./build/three_body_problem
./tools/record-demo.sh 02
```

The browser check needs Python Playwright, Pillow, and Chromium, as does
Orbit / 01's optional browser QA. A headless build also includes `orbit02_export`
and all numerical/export/replay tests: configure with `-DORBIT_BUILD_VIEWER=OFF`.
The native [GIF demo](assets/orbit-02.gif) captures the actual app's client area.
The 15-second edit shows eight seconds of the surface followed by seven seconds
of orbital view. Both Orbit / 01 and Orbit / 02 GIFs are 960 × 600.

The separate `orbit-02-trajectory` version 1 contract has **18433 frames**,
including endpoints, every **1/512 year**. Each little-endian float32 frame
contains **20 fields** (80 bytes):

| Field offsets | Contents |
| --- | --- |
| 0–8 | A, B, C barycentric positions, x/y/z |
| 9–11 | Planet position, x/y/z |
| 12 | Total incident flux, Earth = 1 |
| 13 | Distance to the planet's nearest star, AU |
| 14 | Separation of the closest stellar pair, AU |
| 15–16 | Relative stellar energy and angular-momentum errors |
| 17–19 | Individual stellar flux contributions |

`world.bin` is **1,474,640 bytes**. Positions interpolate between recorded
frames; diagnostics hold their recorded values. The browser evaluates no
gravitational forces. Orbit / 01's existing files and format are unchanged.
Gas-in-a-box work remains in its separate sibling project.
