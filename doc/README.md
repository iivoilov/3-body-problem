# WebGL replay viewer

Run from the repository root:

```bash
python3 -m http.server 8000 --directory doc
```

Open http://localhost:8000. The initial view is the circular preset at simulated
time 1000, so trails and plots are already populated. Use Play / Pause, Restart,
the time slider, speed selector, preset selector, and the trail/shadow checkboxes.
Drag the scene to orbit the camera and scroll to zoom. Space toggles playback when
focus is outside a form control. A reduced-motion preference starts the replay paused.

Optional URL parameters: `?preset=resonance&time=0`.

The directory is a self-contained static site. Three.js 0.187.0, OrbitControls,
their license, shaders, and both datasets are local files. No CDN, account, build
step, or backend is needed. Use HTTP instead of opening `index.html` through
`file://`, because browser modules and fetched binary files require a server.

The browser **does not integrate gravity**. It reads native output and linearly
interpolates positions between samples. Telemetry is held at its recorded value,
rather than recomputed or interpolated through undefined angles. Each dataset
ends at 2048 simulated seconds; playback then jumps back to zero. Modifying mass,
G, or initial conditions requires rebuilding and re-exporting the native model.

## Export

```bash
cmake -S . -B build
cmake --build build --target orbit_export
./build/orbit_export doc/data
```

The exporter runs headlessly. CMake configuration still requires the project's
OpenGL / GLFW / GLEW development packages, but exporting does not open a window.
It produces `manifest.json`, `circular.bin`, and `resonance.bin` using the same
`Simulation` class as the desktop application.

## Binary contract, version 1

- Encoding: IEEE-754 float32, **little-endian**, no header inside the binary files.
- 16385 frames, including both endpoints, at 0.125-simulated-second spacing.
- 14 floats (56 bytes) per frame, or 917560 bytes per preset.
- Frame time: `frame_index * sampleInterval` from the manifest.
- NaN marks undefined telemetry; it is never a body position.

| Float offsets | Contents |
| --- | --- |
| 0–2 | Sol position, x/y/z |
| 3–5 | Azure position, x/y/z |
| 6–8 | Ember position, x/y/z |
| 9 | Outer/inner osculating period ratio |
| 10–11 | Azure and Ember eccentricities |
| 12–13 | Azure and Ember unwrapped critical angles, in turns |

Positions are barycentric, with Y up and the orbits in the XZ plane. Manifest
metadata includes G, the fixed physical step, playback scale, masses, radii,
colors, spin rates, field order, and starting-preset descriptions. Positions are
sampled every 32 physical steps. The diagnostic telemetry is measured every
0.5 simulated second and held between its measurements, as in the native app.

`replay.mjs` validates the schema and byte length, samples positions, and preserves
phase gaps. Phase plots rebase each uninterrupted visible segment to its first
sample to keep accumulated drift readable. `viewer.mjs` owns Three.js rendering,
camera and playback controls, trails, and charts. `shaders/body.frag` is adapted
from the native fragment shader; no physics engine was ported to JavaScript.

## Checks

```bash
node --test tests/replay.test.mjs
python3 tools/check-viewer.py --screenshots /tmp/orbit-viewer-check
```

The second command requires Python Playwright, Pillow, and Chromium. It starts
and shuts down its own local server, blocks external requests, checks actual
WebGL pixels and playback controls, compares scrubbed metrics with the native
binary data, and checks desktop and mobile layouts. `CHROMIUM_BIN` can select a
browser executable.

## Interpretation

The circular starting state does not demonstrate spontaneous synchronization.
The alternate candidate shows bounded angles over a finite native test, not a
guarantee of permanent resonance. A near-integer period ratio, a narrow trail,
or a short bounded angle trace is not a universal resonance-strength measure.
See the main README's resonance discussion and the development guide.
