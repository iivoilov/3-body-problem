# Native application recording

`orbit-01.mp4` is a real 15.000-second capture of a dedicated native OpenGL
window's client area, without the desktop, window border, mouse pointer, or audio.
It uses the circular starting preset, initial simulated time 1000, and displayed
1x playback (four simulated seconds per real second).

- MP4: 1280 × 800, 20 frames/second, 300 frames, H.264.
- GIF: 960 × 600, 225 frames, 15.00-second loop, 128-color palette.
- Source command: `./tools/record-demo.sh` from the repository root.

The last GIF delay is rounded to a centisecond so the animation's total duration
matches the 15-second source clip. The clip is not intended to loop seamlessly.

`orbit-02.mp4` and `orbit-02.gif` join two native alien-world recordings:
**8 seconds of surface view followed by 7 seconds of orbital view**. The surface
segment starts at year 0. The orbital segment starts at integrated year 0.64,
with its trajectory history already populated. Both use 1× playback (0.08
simulated year per real second).

- MP4: 1280 × 800, 20 frames/second, 300 frames, H.264; the cut is at frame 160.
- GIF: 960 × 600, 150 frames, 15.00-second loop, 96-color palette; the cut is at frame 80.
- Source command: `./tools/record-demo.sh 02` from the repository root.

Both versions have the same MP4 and GIF dimensions. Orbit / 02's 1440 × 900
native client area is resized to 1280 × 800 while recording, preserving its
aspect ratio. Its GIF uses 10 frames/second to limit the detailed terrain's
file size. Orbit / 01's GIF uses 15 frames/second.
Capture waits for the app's first completed frame, so startup frames do not
introduce a blank image at the beginning or at the view transition.
