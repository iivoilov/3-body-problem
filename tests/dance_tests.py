"""Fast external checks for published-orbit units and the research CLI."""
import json
import math
import subprocess
import sys

executable = sys.argv[1]


def run(*arguments):
    return json.loads(subprocess.check_output([executable, *map(str, arguments)], text=True))


large = run('--years', 8, '--scale', 4, '--radius', 5, '--step', 1/512)
small = run('--years', 1, '--scale', 1, '--radius', 5, '--step', 1/4096)
assert large['passed'] and small['passed']
assert large['steps'] == small['steps']
assert abs(large['reference_stellar_period_years']/small['reference_stellar_period_years']-8) < 1e-12
for a, b in zip(large['final_bodies'], small['final_bodies']):
    for x, y in zip(a['position'], b['position']):
        assert abs(x/4-y) < 1e-10
    for x, y in zip(a['velocity'], b['velocity']):
        assert abs(x*2-y) < 1e-10
for axis in range(3):
    assert abs(sum(b['position'][axis] for b in large['initial_bodies'][:3])) < 1e-14
    assert abs(sum(b['velocity'][axis] for b in large['initial_bodies'][:3])) < 1e-14
assert large['max_star_span_AU'] < 8.001
assert large['min_star_separation_AU'] > .4
period = large['reference_stellar_period_years']
returned = run('--years', period, '--step', period/16384, '--radius', 5)
assert returned['passed']
assert returned['final_scaled_position_return_error'] < 1e-5, returned['final_scaled_position_return_error']
invalid = subprocess.run([executable, '--step', '-1'], capture_output=True)
assert invalid.returncode != 0
print('Dance CLI: Newtonian scaling, barycentre, momentum, published periodic return, and invalid-input checks passed.')
