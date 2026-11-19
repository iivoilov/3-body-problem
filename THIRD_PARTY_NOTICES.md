# Third-party notices

The WebGL documentation viewer bundles **Three.js 0.187.0**, including
OrbitControls, from the official npm package:

https://registry.npmjs.org/three/-/three-0.187.0.tgz

Three.js is distributed under the MIT License. Its copyright and complete license
are preserved in [doc/vendor/LICENSE.three](doc/vendor/LICENSE.three), and its
source headers are retained. The package version and verified npm integrity hash
are recorded in [doc/vendor/version.json](doc/vendor/version.json).

The native application uses installed GLFW, GLEW, and OpenGL libraries; those
libraries are not bundled in this source tree. FFmpeg is used as an optional
recording/conversion tool and is not bundled.
