"""Check short exports and the published catalogue, including honest survey gaps."""
import csv
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='catalog-export-') as temporary:
    out = Path(temporary)
    subprocess.run([sys.argv[1], str(out), '--years', '128'], check=True, capture_output=True)
    catalog = json.loads((out/'orbit-02/catalog.json').read_text())
    assert [entry['id'] for entry in catalog['orbits']] == ['02-01','02-02','02-03']
    for orbit in catalog['orbits'][1:]:
        verification = json.loads((out/f"orbit-{orbit['id']}/verification.json").read_text())
        assert verification['passed'] and verification['integratedYears']==128
        for record in orbit['records']:
            path=out/record['manifest'].removeprefix('data/')
            manifest=json.loads(path.read_text())
            values=(path.parent/manifest['file']).read_bytes()
            assert len(values)==manifest['frameCount']*80
            assert manifest['startTime']+manifest['duration']<=128
            assert manifest['interpolate']==(manifest['kind']=='detail')
            for i in (0,manifest['frameCount']-1):
                frame=struct.unpack_from('<20f',values,i*80)
                assert all(math.isfinite(x) for x in frame)
                distances=[math.dist(frame[j*3:j*3+3],frame[9:12]) for j in range(3)]
                flux=sum(star['luminosity']/r**2 for star,r in zip(manifest['stars'],distances))
                assert math.isclose(frame[12],flux,rel_tol=3e-6)
                assert abs(frame[15])<1e-4
        survey=next(entry for entry in orbit['records'] if 'survey' in entry['manifest'])
        m=json.loads((out/survey['manifest'].removeprefix('data/')).read_text())
        assert not m['interpolate'] and m['duration']==128

catalog=json.loads((ROOT/'doc/data/orbit-02/catalog.json').read_text())
reference=json.loads((ROOT/'doc/experiments/dance/results.json').read_text())
baseline=next(r for r in reference['runs'] if r['trial']=='radius-5-baseline')
for orbit in catalog['orbits'][1:]:
    root=ROOT/'doc'/f"data/orbit-{orbit['id']}"
    v=json.loads((root/'verification.json').read_text())
    assert v['passed'] and v['integratedYears']==1_000_000
    for entry in orbit['records']:
        p=ROOT/'doc'/entry['manifest'];m=json.loads(p.read_text())
        assert len((p.parent/'world.bin').read_bytes())==80*m['frameCount']
    if orbit['id']=='02-02':
        m=json.loads((root/'year-0/manifest.json').read_text())
        assert [star['mass'] for star in m['stars']]==[1,1,1]
        for actual,expected in zip(m['stars'],baseline['initial_bodies']):
            assert actual['position']==expected['position'] and actual['velocity']==expected['velocity']
        data=(root/'year-0/world.bin').read_bytes()
        old=list(csv.DictReader((ROOT/'doc/experiments/dance/preview.csv').open()))
        for index in (0,16,128,1024):
            frame=struct.unpack_from('<12f',data,index*8*80)
            expected=[float(old[index][key]) for key in ('ax','ay','az','bx','by','bz','cx','cy','cz','px','py','pz')]
            assert max(abs(a-b) for a,b in zip(frame,expected))<3e-6
print('Catalogue exports: states match validated research, windows are bounded, survey gaps are explicit, and both million-year records are complete.')
