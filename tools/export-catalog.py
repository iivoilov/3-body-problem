"""Export numbered worlds and record reproducible source/output hashes."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--record-existing', action='store_true', help='Hash and validate an export already produced by the current build.')
    args = parser.parse_args()
    if not args.record_existing:
        subprocess.run([str(ROOT/'build-headless/orbit_catalog_export'), str(ROOT/'doc/data')], check=True)
    catalog_path=ROOT/'doc/data/orbit-02/catalog.json'
    catalog=json.loads(catalog_path.read_text())
    outputs={str(catalog_path.relative_to(ROOT)): digest(catalog_path)}
    for orbit in catalog['orbits']:
        for record in orbit['records']:
            path=ROOT/'doc'/record['manifest']
            manifest=json.loads(path.read_text())
            binary=path.parent/manifest['file']
            if binary.stat().st_size!=80*manifest['frameCount']:
                raise RuntimeError(f'Incomplete recording: {binary}')
            for item in (path,binary):
                outputs[str(item.relative_to(ROOT))]=digest(item)
        if orbit['id']!='02-01':
            verification=ROOT/f"doc/data/orbit-{orbit['id']}/verification.json"
            result=json.loads(verification.read_text())
            if not result['passed'] or result['integratedYears']!=1_000_000:
                raise RuntimeError('A public million-year export must complete the target.')
            outputs[str(verification.relative_to(ROOT))]=digest(verification)
    sources=('src/alien_catalog.hpp','src/catalog_export.cpp','src/alien_world.hpp','src/physics.hpp','src/stability.hpp','tools/export-catalog.py')
    provenance={'createdUTC': datetime.now(timezone.utc).isoformat(),
                'gitBase': subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
                'compiler': subprocess.check_output(['c++','--version'],text=True).splitlines()[0],
                'command': ['./build-headless/orbit_catalog_export','doc/data'],
                'sourceSHA256': {name:digest(ROOT/name) for name in sources},'outputSHA256':outputs,
                'warning':'Finite integrations and sampled diagnostics; survey gaps are not interpolated. Orbit-02-01 is a separate original 36-year record.'}
    (ROOT/'doc/data/orbit-02/provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
    print('Verified and hashed the numbered catalogue and all replay files.')


if __name__=='__main__':
    main()
