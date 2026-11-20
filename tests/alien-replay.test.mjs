import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { AlienTrajectory, skyDirections } from '../doc/alien-replay.mjs';

const manifest = JSON.parse(await readFile(new URL('../doc/data/orbit-02/manifest.json', import.meta.url), 'utf8'));
const bytes = await readFile(new URL('../doc/data/orbit-02/world.bin', import.meta.url));
const buffer = bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
const trajectory = new AlienTrajectory(manifest, buffer);

test('new recording keeps all four bodies and their native diagnostics', () => {
  const initial = trajectory.sample(0);
  assert.equal(initial.positions.length, 12);
  assert.ok(initial.positions.every(Number.isFinite));
  assert.ok(initial.flux > 1 && initial.flux < 2);
  assert.ok(Math.abs(initial.separation - 4) < 1e-6);
  assert.equal(initial.energyError, 0);
  const last = trajectory.sample(36);
  assert.ok(last.nearest > initial.nearest * 5);
  assert.ok(last.flux < initial.flux / 10);
});
test('replay interpolates positions but holds recorded measurements', () => {
  const a = trajectory.sample(0), b = trajectory.sample(trajectory.interval);
  const middle = trajectory.sample(trajectory.interval / 2);
  for (let i = 0; i < 12; i++) assert.ok(Math.abs(middle.positions[i] - (a.positions[i] + b.positions[i])/2) < 1e-6);
  assert.equal(middle.flux, a.flux);
  assert.deepEqual(trajectory.sample(-1), a);
  assert.deepEqual(trajectory.sample(100), trajectory.sample(36));
});
test('malformed recordings fail and invalid times cannot reach the GPU', () => {
  assert.throws(() => new AlienTrajectory({ ...manifest, version: 2 }, buffer), /Unsupported/);
  assert.throws(() => new AlienTrajectory({ ...manifest, duration: NaN }, buffer), /Unsupported/);
  assert.throws(() => new AlienTrajectory(manifest, buffer.slice(0, -4)), /incomplete/);
  assert.throws(() => trajectory.sample(Infinity), /finite/);
});
test('surface sky follows relative stellar positions and the chosen planetary rotation', () => {
  const positions = trajectory.sample(0).positions;
  const directions = skyDirections(positions, 0);
  assert.equal(directions.filter(direction => direction[1] > 0).length, 3);
  for (const direction of directions) assert.ok(Math.abs(Math.hypot(...direction)-1) < 1e-12);
  const translated = Float32Array.from(positions, (value, index) => value + [2, 3, -4][index%3]);
  const equivalent = skyDirections(translated, 0);
  for (let i = 0; i < 3; i++) for (let axis = 0; axis < 3; axis++)
    assert.ok(Math.abs(equivalent[i][axis] - directions[i][axis]) < 1e-6);
  const rotated = skyDirections(positions, manifest.rotationPeriod / 4);
  assert.ok(Math.hypot(...directions[0].map((value, i) => value - rotated[0][i])) > 0.5);
});

test('numbered detail records use absolute epoch times and interpolate only within their window', async () => {
  for (const id of ['02-02', '02-03']) {
    const base = new URL(`../doc/data/orbit-${id}/year-999936/`, import.meta.url);
    const metadata = JSON.parse(await readFile(new URL('manifest.json', base), 'utf8'));
    const bytes = await readFile(new URL('world.bin', base));
    const track = new AlienTrajectory(metadata, bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset+bytes.byteLength));
    assert.equal(track.startTime, 999936);
    assert.equal(track.endTime, 1000000);
    assert.equal(track.sample(0).time, 999936);
    assert.equal(track.sample(1000001).time, 1000000);
    const a = track.sample(track.startTime), b = track.sample(track.startTime+track.interval);
    const middle = track.sample(track.startTime+track.interval/2);
    for (let i=0;i<12;i++) assert.ok(Math.abs(middle.positions[i]-(a.positions[i]+b.positions[i])/2)<2e-6);
  }
});
test('million-year survey holds computed snapshots without invented interpolation', async () => {
  for (const id of ['02-02', '02-03']) {
    const base = new URL(`../doc/data/orbit-${id}/survey/`, import.meta.url);
    const metadata = JSON.parse(await readFile(new URL('manifest.json', base), 'utf8'));
    const bytes = await readFile(new URL('world.bin', base));
    const track = new AlienTrajectory(metadata, bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset+bytes.byteLength));
    assert.equal(track.duration, 1000000);
    assert.equal(track.interpolate, false);
    assert.equal(track.sample(99).time, 0);
    assert.deepEqual(track.sample(99).positions, track.sample(0).positions);
    assert.equal(track.sample(101).time, 100);
    assert.equal(track.sample(1000000).time, 1000000);
    assert.ok(track.sample(1000000).positions.every(Number.isFinite));
  }
});
