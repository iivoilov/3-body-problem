# 3-body-problem — Orbit / 01 development guide

This is the longer development and learning guide. Run its commands from the
repository root. See [the main README](../README.md) for the publication overview,
recorded demo, and browser replay.

A small native C++ / OpenGL learning project: one sun and two fictional planets,
with Newtonian three-body gravity, trajectory trails, eclipse shadows, a star field,
and a camera you can rotate around the scene.
No downloaded models, textures, or fonts are needed.

## Start

On a Linux graphical desktop, from this directory:

```bash
./run.sh
```

The default is a circular starting preset with an initial outer/inner period
ratio of **2.05**, running at **1x**. Each displayed 1x now advances **four simulated
seconds per real second**; orbital motion and surface spins both run four times
faster than the former 1x setting.

For a tested starting state with bounded 2:1 critical angles, use:

```bash
./run.sh --resonance
```

To fast-forward the selected preset and start with a populated measurement history:

```bash
./run.sh --resonance --time 1000
```

`--time` numerically integrates forward before displaying the scene; it does not
load a saved window or camera. The requested time is rounded down to the fixed
physics-step grid. `--speed` selects the initial playback factor, from 1/64 to 16;
the default is 1, and 16x advances 64 simulated seconds per real second. Fast-forward
replays the currently selected initial conditions, so changing presets or parameters
produces a different trajectory at the same requested time.

The launcher defaults to the NVIDIA adapter and clears inherited forced software
rendering. The longer `env -u LIBGL_ALWAYS_SOFTWARE ...` command is unnecessary.
To select another adapter explicitly:

```bash
MESA_D3D12_DEFAULT_ADAPTER_NAME=Intel ./run.sh
```

The launcher builds the project and tests the selected renderer before opening the
window. If that test fails, it tries Mesa software rendering. This handles the
black-frame problem observed with this machine's Intel WSL / D3D12 graphics path;
the NVIDIA RTX 4080 path passes the graphics tests.
To choose software rendering explicitly after building, run the executable directly:

```bash
LIBGL_ALWAYS_SOFTWARE=1 ./build/three_body_problem
```

The dependencies are a C++17 compiler, CMake, OpenGL development files, GLFW 3.3+,
and GLEW. They were already installed in the development environment. On
Debian/Ubuntu, install missing dependencies with:

```bash
sudo apt install build-essential cmake libgl1-mesa-dev libglfw3-dev libglew-dev
```

To build and run directly, without the launcher's renderer check:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
./build/three_body_problem
```

Keep the source directory available: the executable loads readable GLSL files
from `shaders/` using the source path recorded by CMake. It can run from any
working directory; reconfigure CMake if you move the source directory.

## Controls

| Input | Action |
| --- | --- |
| Left-drag inside the scene | Rotate the camera around its current target |
| Scroll | Zoom in or out |
| Space | Pause / resume all animation |
| `[` / `]` | Halve / double speed, from 1/64x to 16x; starts at 1x |
| N | Advance 0.125 simulated second while paused |
| W | Show the triangles as a wireframe |
| O | Toggle actual trajectory trails |
| D | Show / hide orbital measurement plots |
| E | Integrate forward to the next alignment, pause, and focus on Ember |
| S | Toggle eclipse shadows for comparison |
| R | Reset the selected starting preset, time, speed, camera, and display options |
| Escape | Close the window |

The camera still works while the simulation is paused. Playback starts at **1x**:
one real second advances four simulated seconds. The initial approximate periods
are 24.0 simulated seconds for Azure and 49.2 for Ember (about **6.0 and 12.3 real
seconds** at the starting playback speed). These are initial two-body estimates,
not imposed periods; actual motion evolves through mutual gravitational forces.

The sidebar displays simulated time, playback speed, masses, G, the fixed physics
step, net linear momentum, and relative energy/angular-momentum errors. The time
line also reports the actual simulated seconds per real second (`S/S`). The energy readout is
`(E - E_initial) / abs(E_initial)`; angular-momentum error is the magnitude of its
change divided by its initial magnitude. These are fractions, not percentages.
The net-momentum readout is `length(sum(mass * velocity))` in simulation units;
it starts at zero to floating-point precision. Individual bodies have nonzero
momentum, and the total angular momentum is nonzero for this orbiting system.

## Measuring orbit shape and synchronization

The bottom panel tracks three measurements. Blue / **A** is Azure; orange / **B**
is Ember. Samples are recorded every **0.5 simulated second**, independently of
render frame rate, playback speed, or whether the panel is visible. Each plot
shows the most recent **1024 simulated seconds**; early in a run it shows only the
data collected so far. Axes scale automatically. Press D to restore the larger
scene view; data continues to accumulate while the plots are hidden.

1. **Period ratio:** `T_Ember / T_Azure`, with a dashed reference line at 2:1.
   `2:1 OFFSET = 100 * (ratio / 2 - 1)` expresses the relative difference in percent.
   Zero means the instantaneous period estimates match 2:1; this is a detuning
   measurement, **not a resonance probability or strength**.
2. **Eccentricity:** each orbit's physical elongation. Zero is circular, between
   zero and one is elliptic, and one is the parabolic limit. This is independent
   of how the camera projects the orbital plane onto the screen. A circular orbit
   can appear elliptical when viewed obliquely.
3. **2:1 phase change:** two critical angles, one for each periapsis:

   ```text
   phi_A = 2 * lambda_Ember - lambda_Azure - periapsis_Azure
   phi_B = 2 * lambda_Ember - lambda_Azure - periapsis_Ember
   ```

   Here lambda is mean longitude, not the instantaneous polar angle of the planet.
   The angles are unwrapped to remove artificial jumps at +/-180 degrees. Curves
   show change in turns from the first reliable sample of each contiguous segment
   in the visible window. This keeps long-run drift readable. Repeated full turns
   reveal phase circulation; bounded oscillation is evidence to investigate for
   libration. The two angles need not behave alike.

These are **sun-relative osculating two-body elements**, calculated from the
current relative position and velocity using `mu = G * (M_Sol + M_planet)`.
The other planet perturbs those elements, so period ratio and eccentricity vary.
`src/orbit_diagnostics.hpp` contains the calculations and phase tracking.

At eccentricity below `1e-4`, periapsis is too poorly defined for this display's
angle diagnostic: the affected curve has a gap, shows `--`, and starts a new
segment when reliable again. The phase formulas apply to the current planar,
prograde setup; unsupported orbital orientations also leave gaps. An unbound
orbit has no elliptic period, so its ratio/phase measurements are left undefined.

A short bounded phase trace does not prove resonance; near-circular forced motion
can also produce apparently bounded angles. The panel deliberately makes no
automatic resonance declaration. It monitors **2:1 specifically**, rather than
testing every possible resonance. Longer observation and mean-frequency analysis
would be needed for a stronger conclusion. Increasing eccentricity alone is not
a measure of resonance.

Press **E**, then **S** to compare the eclipsed and unobstructed surface at exactly
the same time and camera position. Drag to inspect the shadow from another angle.
Press Space to watch the shadow move away; the camera follows Ember in this view.
Press R to return to the full-system view and reset the selected preset.
Alignment timing depends on the starting state. Alignments are found by stepping a copy of the actual
simulation, not from a circular-orbit formula. The search is limited to four
initial outer periods; if no alignment is found, the original state is unchanged.
The same shadows also appear during normal playback.

The eclipse readout reports the fraction of the sun blocked at Ember's **day-side
centre** (the subsolar point), not the fraction of the entire planet in shadow.
It reports geometric coverage even when S disables the shadow rendering.

## A more promising resonance starting state

`--resonance` selects initial eccentricities **0.30 and 0.12**, aligned periapsides,
and mean longitudes **0.35 and 0.175 radians**. Both critical angles start at zero.
The initial semimajor axes still give an osculating period ratio of **2.05**.
Both presets start with zero total linear momentum in the barycentric frame.

This candidate was selected by scanning initial eccentricities and relative
phases. The automated long test integrates it for **16384 simulated seconds** and
checks that neither critical angle makes a full turn, with no low-eccentricity
gaps hiding a circulation. Their spans are approximately **0.20 and 0.45 turns**.
This is useful evidence of a bounded 2:1 phase relationship over that run, not a
proof of indefinite stability. Its instantaneous period ratio can vary and need
not remain exactly 2; the periapsides also precess.

The circular 2.05 starting state still exhibits phase drift in the tested runs.
An isolated conservative system does not automatically relax into a synchronized
attractor. The optional preset chooses a promising initial configuration rather
than adding artificial damping, migration, or constraints to force a lock. R
retains whichever preset was selected at launch.

## What this version simulates

Every body has a mass, position, and velocity. All three pairs interact, including
the planets with each other, and the sun responds to the planets. Acceleration is
Newton's gravitational law:

```text
acceleration_i = sum over j != i:
    G * mass_j * (position_j - position_i) / distance_ij^3
```

The parameters use consistent, arbitrary simulation units, not SI metres or kilograms:

| Parameter | Value |
| --- | --- |
| G | 0.00856736493150118 |
| Sol mass | 1000 |
| Azure mass | 2 |
| Ember mass | 4 |
| Initial semimajor axes | 5 and approximately 8.07411 |
| Initial outer/inner period ratio | 2.05 |
| Base playback rate at displayed 1x | 4 simulated seconds per real second |
| Fixed physics step | 1/256 simulated second |

G was chosen so a negligible-mass body at radius 5 around Sol would orbit in
24 simulated seconds: `G * M_sol = 4*pi^2 * 5^3 / 24^2`. The default circular
starting speeds use `sqrt(G * (M_sol + M_planet) / distance)`. Ember's initial
semimajor axis is derived from `initialPeriodRatio` in `src/scene.hpp`, including
the different planet masses. In the eccentric preset, Kepler's equation supplies
initial positions and velocities at the same semimajor axes. Both configurations
shift all positions and velocities into the centre-of-mass
frame. The barycentre begins at the origin with zero net momentum; **the sun is
free to move**. Its lighting and halo move with it.

`src/physics.hpp` implements pairwise accelerations and double-precision
kick-drift-kick leapfrog integration. `src/scene.hpp` manages the initial state,
fixed-step accumulator, playback controls, diagnostics, and trail history. Rendering
converts positions to floats only when supplying OpenGL. Changing playback speed
changes the number of fixed steps performed, not the size of each step. Large
desktop stalls are capped at 0.1 real second per frame to avoid a sudden time jump.

The former circular guides have been replaced by the **actual barycentric paths**
of all three bodies, sampled every 1/16 simulated second and retained for the last
128 simulated seconds. Trails grow as time advances; they do not predict an orbit.

The initial system stays close to the previous scene while allowing perturbations.
Three-body motion can be regular or chaotic; mutual gravity does **not** guarantee
an unstable system. Since all initial positions and velocities lie in the XZ plane,
the motion remains planar. Y is still the up direction. Visual radii, colours, and
surface rotation are chosen for illustration rather than astronomical realism.
Sol and both planets spin in the same prograde sense as the initial planetary
orbits: azimuth increases from +X toward +Z, about the -Y axis in our coordinate
system. Spin rates are 0.075, 0.25, and 0.25 radians per simulated second. Each
body's own angle is wrapped independently, so Sol's slower spin stays continuous.
Spin is prescribed visual rotation; the gravitational solver does not integrate
spin torques or tidal locking.

This version models Newtonian translation without relativity, tides, or spin-orbit
coupling. At surface contact the simulation pauses at the last valid step; it
does not merge or bounce bodies. There is no force softening. This avoids continuing
the point-mass model through an overlap or singular encounter.

### Integration between frames

The physics uses a **fixed** 1/256-simulated-second step, not proximity-based
adaptive stepping. Each frame's elapsed wall time is multiplied by playback speed
and the base playback scale of 4, then added to an accumulator. The solver consumes whole fixed steps and retains the
fractional remainder for the next frame. At 60 rendered frames per second and
1x playback, this means about 17 physics steps per frame. At 16x it is about
273 or 274 steps per frame. The physical step size itself is unchanged.

Each step is kick-drift-kick leapfrog:

```text
a_old   = gravity(all current positions)
v_half  = v_old + a_old * dt/2
r_new   = r_old + v_half * dt
a_new   = gravity(all new positions)
v_new   = v_half + a_new * dt/2
```

The renderer draws the last completed physical state; there is currently no
interpolation between adjacent physics states. Trail samples are stored every
16 steps (1/16 simulated second) and drawn as straight connected segments.

Keeping dt fixed preserves this leapfrog method's symplectic structure and gives
reproducible trajectories at different render rates. Much closer encounters can
need smaller steps or a different adaptive/hybrid integrator; the current solver
does not automatically improve its time resolution near another body.

Lighting uses the direction toward the sun's centre for diffuse shading and a
small highlight. For each planet fragment, `shaders/body.frag` checks how much of
the sun's apparent disk is covered by the other planet. The apparent angular
sizes depend on the actual radii and distances. Disk overlap gives a dark umbra
(total eclipse) and a smoothly varying penumbra (partial eclipse), rather than an
arbitrary fixed-width blur. A body behind the receiver or beyond the sun does not
block its light. The sun itself remains emissive.

This angular-disk overlap is a practical approximation, not a full integration of
light over the sun's surface. The sun is treated as uniformly bright; there is no
limb darkening or atmospheric scattering. Ambient light remains visible even in
total shadow, so the eclipse is dark rather than perfectly black.
The halo is a softly blended rectangle facing the camera, not a bloom effect.

## How the code draws a frame

Start reading `src/main.cpp` at `run()`, then follow the numbered rendering steps.

1. **Create a window and context.** GLFW manages the operating-system window,
   input, and timing. The context stores OpenGL state. GLEW loads the OpenGL
   functions once that context exists.
2. **Build a sphere from triangles.** `makeSphere()` produces positions, surface
   normals, and triangle indices. `Mesh` in `src/gpu.hpp` uploads them to GPU buffers.
   All three bodies reuse this one sphere mesh.
3. **Advance physics and position the bodies.** The CPU integrates mutual gravity
   in fixed steps. A model matrix scales, rotates, and translates the unit sphere
   to each body's current position. The view matrix describes the camera; the
   projection matrix gives perspective. The vertex shader applies these transforms.
4. **Shade their surfaces.** `shaders/body.frag` computes surface colour and
   lighting. For planets, the angle between the normal and the direction toward
   the sun determines the diffuse illumination. The other planet's centre and
   radius are supplied as uniforms; the visible fraction of the sun attenuates
   direct lighting and highlights. `src/eclipse.hpp` uses the same angular-disk
   model on the CPU for the readout and geometry tests.
5. **Compose the image.** Depth testing keeps hidden surfaces out of view. Stars
   and trajectory trails use a simpler shader. The halo uses additive blending. A final
   2D pass draws the sidebar with tiny built-in bitmap letters.
6. **Present and repeat.** Swap the finished frame onto the screen, then draw the
   next one with updated simulation time.

The CPU handles time, camera input, and gravitational dynamics. The GPU runs the
vertex and fragment shaders over the geometry being drawn. OpenGL connects them.

The small matrix functions in `src/math.hpp` are written out so the mathematics
is inspectable. They cover only what this scene needs; a larger project would
normally use a library such as GLM.

## First experiments

Make one change at a time, rebuild with `./run.sh`, and observe the result:

1. Pause and press **W**. Rotate the camera. Identify the triangles forming each sphere.
2. Pause and press **N** to advance 1/8 simulated second at a time. Change playback
   speed with brackets and observe that speed changes the pacing rather than the paths.
3. In `src/scene.hpp`, increase a planet's `mass` to strengthen its perturbation of
   the other bodies. Restart and inspect the trails and energy error. Change
   `initialPeriodRatio` to tune the starting period ratio. `orbitRadius` means
   initial semimajor axis, not a constraint on future motion. `radius` controls
   its visible size, eclipse geometry, and surface-contact detection.
4. In `shaders/body.frag`, replace the planet's final `color` with `surface`.
   Observe what removing lighting does to the sense of depth.
5. Change `rings` and `segments` in `makeSphere()` to `8` and `12`.
   Inspect the result in shaded and wireframe modes.

Shader edits take effect when the application restarts; they do not require a C++
rebuild. Compiler and shader errors are printed in the terminal.

## Verification

```bash
ctest --test-dir build --output-on-failure
./build/physics_tests
./build/diagnostics_tests
./build/resonance_tests
LIBGL_ALWAYS_SOFTWARE=1 ./build/three_body_problem --smoke-test
LIBGL_ALWAYS_SOFTWARE=1 ./build/three_body_problem --capture /tmp/three-body-problem.ppm
./run.sh --resonance --time 1000 --capture /tmp/orbit-data.ppm
env -u LIBGL_ALWAYS_SOFTWARE MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA ./build/three_body_problem --eclipse
```

The scene tests cover playback controls, camera limits, model/view matrices,
physical eclipse seeking, total and partial coverage, and a moving sun. They do
not need a display.

The physics tests cover inverse-square forces, planet-planet coupling, solar
motion, barycentric initialization, pause/single-step, frame-rate and playback-speed
independence, second-order convergence, time reversibility, bounded trail history,
and contact stopping. They also evolve the default system for 100 initial inner
periods, checking energy, linear/angular momentum, and barycentre conservation.
In the checked circular-preset run, maximum relative energy error was about `4e-8`,
and Azure's distance from Sol varied from about 4.41 to 5.47. This finite test does not establish
indefinite stability.

The diagnostics tests recover known analytic orbital elements and compare
fixed-phase and circulating-angle fixtures, test angle unwrapping, circular and
unbound cases, history limits/reset, and prove that collecting measurements
leaves the computed gravitational trajectory unchanged.

The resonance-preset test checks the initial 2.05 ratio, eccentricities, critical
angles, zero net momentum, and preset-preserving reset, then observes both critical
angles for 16384 simulated seconds. The checked maximum relative energy error
was about `1.9e-7`.

The graphics smoke test renders five frames into an offscreen framebuffer using
a hidden GLFW window. It exercises shaded and wireframe rendering, camera rotation,
zoom, and the orbit toggle; it checks OpenGL errors and visible sun/blue-planet
pixels with the plot area excluded. It exercises the scene with plots visible
and hidden, then renders the same eclipse close-up with shadows on and off and
requires a measurable darkening of the planet's surface. It still requires a
working graphical display. Add `--eclipse --capture /tmp/eclipse.ppm` to capture
the paused eclipse view. The PPM capture can be
opened with an image viewer or converted to PNG.

## Project direction

Orbit / 01 remains the one-star/two-planet experiment described here.
[Orbit / 02](ORBIT_02.md) adds a separate three-star simulation and alien-world
surface view inspired by *The Three-Body Problem*, preserving both versions. Gas-in-a-box work has its own
sibling project directory, `../gas-in-a-box/` relative to the repository root.

## References

- [GLFW getting started](https://www.glfw.org/docs/3.3/quick.html)
- [GLEW initialization](https://glew.sourceforge.net/basic.html)
- [Khronos rendering pipeline overview](https://wikis.khronos.org/opengl/Rendering_Pipeline_Overview)
- [LearnOpenGL](https://learnopengl.com/)
- [NASA eclipse geometry](https://science.nasa.gov/eclipses/geometry/)
- [NASA orbits and Kepler's laws](https://science.nasa.gov/solar-system/orbits-and-keplers-laws/)
- [REBOUND leapfrog integrator](https://rebound.readthedocs.io/en/latest/integrators/#leapfrog)
- [Mean-motion resonance and critical angles](https://academic.oup.com/mnras/article/413/1/554/1064122)
