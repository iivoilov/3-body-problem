"""Verify the public recording, reproducibility, units, and native telemetry."""
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="orbit02-export-") as temporary:
    root = Path(temporary)
    for name in ("first", "second"):
        subprocess.run([sys.argv[1], str(root / name)], check=True, capture_output=True, text=True)
    manifest = json.loads((root / "first/manifest.json").read_text())
    assert manifest["format"] == "orbit-02-trajectory" and manifest["version"] == 1
    assert manifest["units"] == {"length": "AU", "mass": "solar mass", "time": "Julian year", "flux": "Earth incident flux"}
    assert manifest["planet"]["mass"] == 0 and manifest["planet"]["testParticle"]
    assert [star["mass"] for star in manifest["stars"]] == [1.0788, 0.9092, 0.1221]
    assert manifest["frameCount"] == 18433 and manifest["sampleInterval"] == 1 / 512
    assert manifest["physicsStep"] == 1 / 16384
    data = (root / "first/world.bin").read_bytes()
    assert len(data) == manifest["frameCount"] * 80
    assert hashlib.sha256(data).digest() == hashlib.sha256((root / "second/world.bin").read_bytes()).digest()
    bundled = Path(__file__).resolve().parents[1] / "doc/data/orbit-02"
    assert data == (bundled / "world.bin").read_bytes(), "bundled browser data must match the native solver"
    assert manifest == json.loads((bundled / "manifest.json").read_text())
    for frame in (0, 256, 4096, 12000, manifest["frameCount"] - 1):
        values = struct.unpack_from("<20f", data, frame * 80)
        assert all(math.isfinite(value) for value in values)
        barycentre = [sum(manifest["stars"][i]["mass"] * values[i*3+axis] for i in range(3)) /
                      sum(star["mass"] for star in manifest["stars"]) for axis in range(3)]
        assert math.hypot(*barycentre) < 2e-6
        distances = [math.dist(values[i*3:i*3+3], values[9:12]) for i in range(3)]
        flux = sum(star["luminosity"] / distance**2 for star, distance in zip(manifest["stars"], distances))
        assert math.isclose(flux, values[12], rel_tol=2e-6)
        assert math.isclose(min(distances), values[13], rel_tol=2e-6)
        assert math.isclose(sum(values[17:20]), values[12], rel_tol=2e-6)
        assert abs(values[15]) < 2e-5
    print("Orbit / 02 export passed: units, initial state, binary layout, native/bundled equality, flux, conservation, determinism.")
