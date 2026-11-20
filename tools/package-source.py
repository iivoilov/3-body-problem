"""Create a publication bundle from an explicit list of source and public-asset paths."""
from pathlib import Path
import zipfile

root = Path(__file__).resolve().parents[1]
destination = root / "dist/3-body-problem-source.zip"
files = [root / name for name in ("README.md", "CMakeLists.txt", ".gitignore", "run.sh", "LICENSE", "THIRD_PARTY_NOTICES.md")]
for directory in ("src", "shaders", "tests", "tools", "doc"):
    for path in (root / directory).rglob("*"):
        if path.is_file() and not path.is_symlink() and "__pycache__" not in path.parts and path.suffix != ".pyc":
            files.append(path)
destination.parent.mkdir(exist_ok=True)
with zipfile.ZipFile(destination, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    for path in sorted(files):
        archive.write(path, Path("3-body-problem") / path.relative_to(root))
    assert archive.testzip() is None
print(f"Prepared {destination.name}: {len(files)} files, {destination.stat().st_size} bytes")
