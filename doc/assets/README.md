# Native application recording

`orbit-lab.mp4` is a real 15.000-second capture of a dedicated native OpenGL
window's client area, without the desktop, window border, mouse pointer, or audio.
It uses the circular starting preset, initial simulated time 1000, and displayed
1x playback (four simulated seconds per real second).

- MP4: 1280 × 800, 20 frames/second, 300 frames, H.264.
- GIF: 960 × 600, 225 frames, 15.00-second loop, 128-color palette.
- Source command: `./tools/record-demo.sh` from the repository root.

The last GIF delay is rounded to a centisecond so the animation's total duration
matches the 15-second source clip. The clip is not intended to loop seamlessly.
