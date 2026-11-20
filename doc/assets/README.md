# Native application recording

`orbit-01.gif` is a real 15.000-second capture of a dedicated native OpenGL
window's client area, without the desktop, window border, mouse pointer, or audio.
It uses the circular starting preset, initial simulated time 1000, and displayed
1x playback (four simulated seconds per real second).

- GIF: 960 × 600, 225 frames, 15.00-second loop, 128-color palette.
- Source command: `./tools/record-demo.sh` from the repository root.

The last GIF delay is rounded to a centisecond so the animation's total duration
matches the 15-second source clip. The clip is not intended to loop seamlessly.

Each numbered alien world has its own native recording:

| GIF | Simulated years / real second | Orbital segment starts at year |
| --- | --- | --- |
| `orbit-02-01.gif` | 0.08 | 0.64 |
| `orbit-02-02.gif` | 2 | 16 |
| `orbit-02-03.gif` | 0.8 | 6.4 |

Each joins **8 seconds of terrain followed by 7 seconds of orbital view**.
The surface application is launched at year 0. The orbital application starts at eight real
seconds of that preset's playback, with its integrated history already populated.
Use `./tools/record-demo.sh 02-01`, `02-02`, or `02-03`; `02` aliases `02-01`.

All numbered-world GIFs are **960 × 600, 150 frames, exactly 15.00 seconds**,
with a 96-colour palette and the cut at frame 80. The 1440 × 900 native client
area is captured without window borders, resized while preserving aspect ratio,
and encoded at 10 fps. Orbit / 01 remains the same width and height at 15 fps.
Intermediate video files stay in a temporary directory and are deleted when
recording finishes. Capture waits for the app's first completed frame.
