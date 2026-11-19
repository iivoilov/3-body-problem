# Tides and orbital resonance: a headless experiment

This experiment adds a scientific tidal prescription to the existing three-body
initial conditions, then integrates without creating a window. The interactive
OpenGL app and the exported WebGL recordings still use the conservative model.

The question is whether tidal dissipation makes the planets settle into a quieter
2:1 mean-motion resonance. This differs from **tidal locking of a planet's spin**:
a planet can approach its own spin equilibrium while the two orbital periods move
away from 2:1.

## Model choice

We use the **equilibrium tide with constant time lag**, often called the
Mignard–Hut or weak-friction model. A deformed body's bulge responds to forcing
after a small fixed delay, `lag`. The resulting force exchanges spin and orbital
angular momentum and dissipates mechanical energy as heat. The response is
parameterized by physical radius `R`, potential Love number `k2`, time lag, and
moment of inertia `I = c m R²`.

This is a tractable first experiment, not a universal description of planetary
interiors. CTL is used in scientific N-body work; real rheology can depend on
frequency, and the small-lag equilibrium approximation excludes dynamical tides.

Primary references:

- [Hut (1981), Tidal evolution in close binary systems](https://scixplorer.org/abs/1981A%26A....99..126H/abstract).
- [Wisdom & Tian (2015), Early evolution of the Earth–Moon system with a fast-spinning Earth](https://web.mit.edu/wisdom/www/wisdom-tian.pdf),
  equation 3, for the vector CTL force. The authors describe the limitations of
  CTL for the real Moon; their own lunar integrations use another tidal model.
- [Lu et al., Self-Consistent Spin, Tidal and Dynamical Equations of Motion in the REBOUNDx Framework](https://arxiv.org/abs/2303.00006),
  for scientific use of coupled CTL spin/orbit evolution. Our implementation is
  a smaller model, without rotational flattening.
- [Socrates & Katz (2012), Physical Basis for a Constant Lag Time](https://arxiv.org/abs/1209.5723),
  for the assumptions under which this prescription follows from fluid dynamics.

## Forces and bookkeeping

For each planet let `r = position_planet - position_star`, `v` be its relative
velocity, `Omega` its angular velocity vector, `M` the stellar mass, and
`C = 3 G k2 M² R⁵`. The force on the planet is

```text
F_elastic     = -C r / |r|⁸
F_dissipative = -C lag / |r|¹⁰ × [2 r (r·v) + |r|² (v - Omega×r)]
```

The primary receives the opposite force. The planet receives spin torque
`-r×F_dissipative`; its spin is evolved, not held fixed. The elastic bulge has
potential `-C/(6 |r|⁶)`. Instantaneous heat production is nonnegative:

```text
dH/dt = C lag / |r|¹⁰ × [2(r·v)² + |r|² |v - Omega×r|²].
```

Thus the checks are **mechanical energy + accumulated heat**, total **orbital +
spin angular momentum**, and net linear momentum. Orbital energy and orbital
angular momentum separately need not be conserved.

All three objects exert Newtonian gravity. Only tides raised by the star in the
selected planets are included. Stellar tides, planet–planet tides, rotational
quadrupoles, GR, gas migration, evolving radii, and tidal disruption are omitted.
Integration stops at physical surface contact or an unbound osculating orbit.

## Parameters

The original masses `1000, 2, 4`, gravity `0.00856736493150118`, zero initial total
linear momentum, semimajor axes, phases, and initial period ratio `2.05` are
retained. Both initial-condition presets are tested. Planet spins initially use
the existing prograde rates, `0.25` radians per simulated second.

| Quantity | Azure | Ember |
| --- | --- | --- |
| Physical radius | 0.15 | `0.15 cbrt(2)` = 0.1889881575 |
| Potential Love number `k2` | 0.3 | 0.3 |
| Time lag | 0.1 | 0.1 |
| Moment-of-inertia factor `c` | 0.33 | 0.33 |

These are compact toy planets with equal mean density. The star is spherical,
with contact radius 1.0. Drawing radii **0.65 and 0.85 are not used for tides**:
tidal strength scales as `R⁵`, making that substitution a major physical change.
There is no calibration to Earth, Mars, or the real solar system, and no arbitrary
force multiplier to speed up damping.

For a nearly circular isolated orbit and a relaxed spin, CTL predicts

```text
de/dt ≈ -e/tau_e
1/tau_e = (21/2) k2 lag (M/m) (R/a)⁵ n²,   n² = G(M+m)/a³.
```

At the initial semimajor axes, `tau_e` is about **3.80 million** simulated seconds
for Azure and **110.60 million** for Ember. Equal-density planets do not have
equal tidal damping rates: the inner orbit dominates. Eccentricity and resonant
coupling modify these isolated, small-e estimates.

The extra sensitivity runs lower both lags to `0.01`, and separately enlarge
Ember's physical radius to `0.4` at lag `0.1`. The latter boosts its tidal
coefficient by about **42.5** and changes its elastic bulge and spin inertia too;
it is a different planetary structure, not just a faster clock. A further case
combines the larger Ember with Azure's lag reduced to `0.01`, so the outer
planet's damping dominates.

The CSV reports the conservative lag bound `2 lag (|Omega| + instantaneous orbital
frequency)`. It should remain small compared with one for the first-order model.
Large lag values are not a physically neutral way of making a run faster.

## Integrator and diagnostics

The conservative part uses the existing kick–drift–kick leapfrog plus the elastic
tidal force. Dissipation is composed symmetrically before and after that step.
For a single pair at fixed positions, radial relative velocity and tangential
spin/orbit slip decay exponentially. This substep is integrated exactly, including
back reaction, spin torque, and heat. Symmetric pair ordering accounts for the
shared star. The complete method is second order; it is dissipative rather than
globally symplectic or time reversible.

The primary study uses `h = 1/32` simulated second; both-tide runs are repeated at
`h = 1/64`. Changing `h` is numerical refinement, not changing tidal strength.
Each run lasts **4,194,304** simulated seconds, approximately **175,000 initial
Azure orbits**. Osculating diagnostics are sampled every 0.5 simulated second;
raw CSV output in the recorded study is every 16 seconds. The interactive app
retains its original `1/256` step. Statistics use successive
16,384-second windows, keeping the first and final windows separate from the
initial instantaneous ratio.

We retain both 2:1 angles,
`phi_i = 2 lambda_Ember - lambda_Azure - periapsis_i`, along with semimajor axes,
eccentricities, spin/orbit frequency ratios, conservation checks, and heat.
Phase tracking resets when eccentricity is below `1e-4`. A window containing gaps
or multiple segments has no single phase span. Near-zero eccentricity and
finite sampling limit phase interpretation even outside this threshold.

A smaller phase span in one finite window is not proof of capture. The report
also checks the phase span and continuity over the entire recorded run. In particular,
strong eccentricity damping can make angles appear to librate outside a genuine
resonance. See [Delisle et al. (2012)](https://arxiv.org/abs/1207.3171).

## Run it

Only a C++17 compiler, CMake, and Python 3 are needed for the study. OpenGL,
GLFW, GLEW, a display, and Node are not needed by the experiment. Node adds the
optional existing replay tests if installed.

```bash
cmake -S . -B build-headless -DCMAKE_BUILD_TYPE=Release -DORBIT_BUILD_VIEWER=OFF
cmake --build build-headless --parallel 2
ctest --test-dir build-headless --output-on-failure
python3 tools/run-tidal-study.py --interval 16
```

The study runs gravity controls, inner/both-planet tides, a zero-lag elastic
control, three parameter sensitivities, and timestep refinement. Raw time series
and run metadata stay in the ignored `build-headless/tidal-study/` directory.
Compact results are written to [experiments/tides/](experiments/tides/).
`--reuse` reuses completed raw runs only when their options and executable hash
match. The report records source hashes; no automatic Git commits are made.

For one shorter run:

```bash
./build-headless/tidal_experiment --preset resonance --tides both \
  --duration 1048576 --step 0.03125 --interval 16 \
  --output build-headless/resonance-both.csv
```

Use `--tides none|elastic|inner|outer|both`, `--lag-inner`, `--lag-outer`,
`--radius-inner`, and `--radius-outer` to explore the model. `elastic` has both
bulges and zero lag; `none` has no tidal forces. All times and lengths use the
original arbitrary simulation units.

## Interpretation

Planetary eccentricity damping often causes **divergent migration** near 2:1:
energy is lost while total angular momentum is retained. It is not a general
mechanism that attracts any initial period ratio to exactly 2.
[Lithwick & Wu (2012), Resonant Repulsion of Kepler Planet Pairs](https://arxiv.org/abs/1204.2555).

Whether resonant oscillations shrink or grow depends on dissipation in both
planets and the starting state. Tides can even break a resonance.
[Delisle, Laskar & Correia (2014), Resonance breaking due to dissipation in planar planetary systems](https://arxiv.org/abs/1404.4861).

The measured outcomes and numerical refinement checks are recorded in
[the study results](experiments/tides/RESULTS.md).
