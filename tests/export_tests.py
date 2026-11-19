"""Check the public binary contract and deterministic native export using only the stdlib."""
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="orbit-export-") as temporary:
    root = Path(temporary)
    for name in ("first", "second"):
        subprocess.run([sys.argv[1], str(root / name)], check=True, capture_output=True, text=True)
    manifest = json.loads((root / "first/manifest.json").read_text())
    assert manifest["format"] == "orbit-lab-trajectory"
    assert manifest["encoding"] == "float32-le" and manifest["fieldsPerFrame"] == 14
    assert (manifest["frameCount"] - 1) * manifest["sampleInterval"] == manifest["duration"]
    assert manifest["sampleInterval"] / manifest["physicsStep"] == 32
    masses = [body["mass"] for body in manifest["bodies"]]
    for preset in manifest["presets"]:
        data = (root / "first" / preset["file"]).read_bytes()
        assert len(data) == manifest["frameCount"] * 14 * 4
        assert hashlib.sha256(data).digest() == hashlib.sha256((root / "second" / preset["file"]).read_bytes()).digest()
        first = struct.unpack_from("<14f", data)
        assert abs(first[9] - manifest["initialPeriodRatio"]) < 1e-6
        assert math.isnan(first[12]) if preset["id"] == "circular" else abs(first[12]) < 1e-6
        for index in (0, 8, 8000, manifest["frameCount"] - 1):
            frame = struct.unpack_from("<14f", data, index * 14 * 4)
            assert all(math.isfinite(value) for value in frame[:12])
            assert all(frame[body * 3 + 1] == 0 for body in range(3))
            for axis in range(3):
                barycentre = sum(masses[body] * frame[body * 3 + axis] for body in range(3)) / sum(masses)
                assert abs(barycentre) < 1e-6
    print("Export contract passed: sizes, little-endian frames, initial ratio, undefined phases, barycentre, determinism.")
