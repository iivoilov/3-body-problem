# CTL tidal study — 2026-11-20

**The standard planet structure does not settle into a quiet 2:1 resonance under
tides. An outer-dominated dissipation case retains bounded angles and has modestly
smaller late oscillations.** The outcome depends on where energy is dissipated.

Thirteen non-graphical integrations were completed, each lasting **4,194,304
simulated seconds**, approximately **174,937 initial Azure orbits**. See
[the model and commands](../../TIDAL_EXPERIMENTS.md), [machine-readable summaries](summary.json),
and [all window statistics](windows.csv). Raw 16-second CSV records and individual
run metadata are in the ignored `build-headless/tidal-study/` directory.

## Primary comparisons

Values below are means over the final 16,384-second window, rather than a single
orbital snapshot. All runs start with the same instantaneous osculating period
ratio **2.05**; the early window means differ because mutual perturbations act
immediately. `e` is osculating eccentricity. The primary timestep is `1/32`.

| Starting preset | Tidal setting | Mean outer/inner period | Mean Azure e | Mean Ember e | Late angular behavior |
| --- | --- | ---: | ---: | ---: | --- |
| Circular | None | 2.098677 | 0.070428 | 0.006898 | Phase gaps; no demonstrated capture |
| Circular | Azure only | 2.153783 | 0.040234 | 0.005899 | Azure angle bounded in this window; Ember has gaps |
| Circular | Both planets | 2.154145 | 0.040145 | 0.005494 | Azure angle bounded in this window; Ember has gaps |
| Eccentric candidate | None | 2.014263 | 0.299883 | 0.096547 | Both angles bounded throughout recorded run |
| Eccentric candidate | Azure only | 2.200786 | 0.080512 | 0.015279 | Both angles circulate |
| Eccentric candidate | Both planets | 2.175603 | 0.070684 | 0.050472 | Both angles circulate |

With both standard planets dissipating, the candidate's early mean Azure
eccentricity is 0.2992 and the late mean is 0.0707. However, the two critical
angles make about **59.8 and 56.6 full turns** in the final window. The system
loses energy and moves away from resonance as it becomes less eccentric.

For comparison, the gravity-only candidate's two angles span only **0.199 and
0.450 turns over the entire recorded run**, with no phase gaps or tracker resets.
Adding conservative bulges without lag also keeps both angles bounded. The
observed escape therefore requires dissipation in these experiments.

The circular preset illustrates a diagnostic trap: Azure's late angle is bounded
even at period ratio 2.154, while Ember's periapsis becomes unreliable at intervals.
A single bounded angle, especially as eccentricities shrink, is insufficient to
declare resonant capture. We keep phase segments separate; gaps are never joined
into a false full-run libration or a spurious number of complete turns.

## Sensitivity to planetary structure and dissipation

These four cases all use the eccentric starting candidate and both planetary
bulges. Unspecified parameters retain the standard values.

| Change | Mean period ratio | Mean Azure e | Mean Ember e | Late angle spans, Azure / Ember (turns) |
| --- | ---: | ---: | ---: | --- |
| Zero lag, elastic bulges only | 2.014460 | 0.301664 | 0.093156 | 0.193 / 0.445 |
| Both lags 0.01 | 2.021229 | 0.221342 | 0.090016 | 0.231 / 4.331 |
| Ember radius 0.4; both lags 0.1 | 2.153026 | 0.061829 | 0.011893 | 52.753 / gaps |
| Ember radius 0.4; Azure lag 0.01, Ember lag 0.1 | 2.021771 | 0.216443 | 0.028173 | 0.168 / 0.378 |

The last two cases have identical masses, radii, Love numbers, moments of inertia,
initial conditions, and conservative bulges. Changing Azure's lag from 0.1 to
0.01 changes the relative damping enough to retain the bounded-angle pattern.
Using the isolated, small-e estimates, the last case has eccentricity damping
times of about **38.0 million** seconds for Azure and **2.60 million** for Ember.

In that outer-dominated case, the two angles remain defined and bounded throughout
the recorded run, spanning **0.197 and 0.454 turns** in total. Their early-window
spans are **0.195 and 0.452 turns**, compared with **0.168 and 0.378** in the final
window: reductions of about **14% and 16%**. The changes are not monotonic between
windows, and neither angle settles to a constant.

This is retention of bounded motion from the existing eccentric candidate.
Testing capture from the circular preset with this planetary structure requires
a separate experiment. A finite run with smaller oscillations establishes
neither a permanent attractor nor indefinite stability. Period ratios alone
also need care: the resonance angles include periapsis evolution, and the periods
in these tables are instantaneous two-body osculating quantities before averaging.

## Numerical refinement

Three both-tide cases were repeated at half the timestep. Late mean period ratios:

| Case | Step 1/32 | Step 1/64 | Absolute difference |
| --- | ---: | ---: | ---: |
| Circular, standard structure | 2.154145 | 2.154170 | 0.000025 |
| Eccentric candidate, standard structure | 2.175603 | 2.175507 | 0.000096 |
| Eccentric candidate, outer-dominated | 2.021771 | 2.021944 | 0.000172 |

The refined standard candidate still has both angles circulating; their final
window spans are **59.84 and 56.59 turns**. The refined outer-dominated case keeps
both angles bounded over the whole run. Its early-to-late window spans change
from **0.196 to 0.168** and **0.451 to 0.372 turns**, supporting the same modest
reduction observed at the coarser step.

The standard candidate's maximum sampled relative mechanical-plus-heat budget
error falls from **1.23e-5 to 3.07e-6** when the timestep is halved, consistent with
second-order convergence. Actual heat production is about **4.05% of the initial
mechanical binding energy** in this case, much larger than this numerical error.
All runs retain relative total orbital-plus-spin angular momentum errors below
**3e-12**, and net momentum below **4e-11** in simulation units.

Maximum errors printed by the executable are checked every 0.5 simulated second;
the structured report's `sampled_maxima` fields use the saved 16-second records.
These are sampled diagnostics, not rigorous global error bounds. Individual
orbital phases and detailed escape timing are less accurate than the rounded
long-window outcomes above.

## Validation and limits

Both the regular build and the graphics-free build passed **8/8 CTest suites**.
The new tests check heat balance, stellar back reaction, total angular momentum,
spin-to-orbit transfer, vanishing dissipation at circular corotation, zero-lag
behavior, exact agreement with the existing solver when tides are disabled,
second-order convergence, and safe interpretation of phase gaps and long-term
phase drift.

An independent isolated-binary check at `e = 0.05` measures an eccentricity damping
rate of **2.67315e-7**, versus the small-e CTL prediction **2.62840e-7** per
simulated second, a difference of about **1.7%** including finite-e corrections.

The largest recorded conservative lag bound is about **0.20**, and about **0.083**
for the outer-dominated case. The underlying CTL prescription is an approximation,
not a calibrated interior model. Radii, lags, and inertias here describe hypothetical
planets; time cannot be relabeled as physical years without a consistent unit and
material calibration. Stellar tides, rotational flattening, disk migration,
planetary rheology changes, and tidal disruption are outside this experiment.

These outcomes are consistent with published work on
[resonant repulsion](https://arxiv.org/abs/1204.2555) and on
[the dependence of resonant damping or escape on dissipation in both planets](https://arxiv.org/abs/1404.4861).
Tidal dissipation supplies energy loss; its location and the starting resonant
state determine whether that loss quiets the motion or destroys the resonance.
