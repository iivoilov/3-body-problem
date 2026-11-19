import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { RecordedTrajectory, phaseChanges } from '../doc/replay.mjs';

function fixture() {
  const manifest = { format: 'orbit-lab-trajectory', version: 1, encoding: 'float32-le', fieldsPerFrame: 14,
    frameCount: 17, duration: 2, sampleInterval: 0.125, telemetryInterval: 0.5 };
  const buffer = new ArrayBuffer(manifest.frameCount * 14 * 4), view = new DataView(buffer);
  for (let frame = 0; frame < 17; frame++) for (let field = 0; field < 14; field++) {
    view.setFloat32((frame * 14 + field) * 4, field < 9 ? frame + field : field === 9 ? 2.05 : NaN, true);
  }
  return { manifest, buffer };
}

test('positions interpolate between exported frames and clamp to the recording', () => {
  const { manifest, buffer } = fixture(), trajectory = new RecordedTrajectory(manifest, buffer);
  assert.equal(trajectory.sample(0.0625).positions[0], 0.5);
  assert.equal(trajectory.sample(0.0625).positions[8], 8.5);
  assert.equal(trajectory.sample(-20).positions[0], 0);
  assert.equal(trajectory.sample(100).positions[0], 16);
  assert.ok(Math.abs(trajectory.sample(1).ratio - 2.05) < 1e-6);
  assert.ok(Number.isNaN(trajectory.sample(1).phases[0]));
  assert.throws(() => trajectory.sample(NaN), /finite/);
});

test('measurement history follows simulated time and native telemetry spacing', () => {
  const { manifest, buffer } = fixture(), trajectory = new RecordedTrajectory(manifest, buffer);
  assert.deepEqual(trajectory.history(2, 1).map(sample => sample.time), [1, 1.5, 2]);
  assert.deepEqual(trajectory.history(100, 1).map(sample => sample.time), [1, 1.5, 2]);
  assert.deepEqual(trajectory.history(0).map(sample => sample.time), [0]);
});

test('invalid format and truncated data fail explicitly', () => {
  const { manifest, buffer } = fixture();
  assert.throws(() => new RecordedTrajectory({ ...manifest, version: 2 }, buffer), /Unsupported/);
  assert.throws(() => new RecordedTrajectory(manifest, buffer.slice(0, -1)), /incomplete/);
});

test('phase gaps start a fresh visible segment instead of inventing continuity', () => {
  const result = phaseChanges([5, 5.25, NaN, 9, 9.5].map(value => ({ phases: [value, value] })), 0);
  assert.deepEqual(result.slice(0, 2), [0, 0.25]);
  assert.ok(Number.isNaN(result[2]));
  assert.deepEqual(result.slice(3), [0, 0.5]);
});

test('both bundled native exports obey the replay contract', async () => {
  const manifest = JSON.parse(await readFile(new URL('../doc/data/manifest.json', import.meta.url), 'utf8'));
  for (const preset of manifest.presets) {
    const bytes = await readFile(new URL(`../doc/data/${preset.file}`, import.meta.url));
    const buffer = bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
    const trajectory = new RecordedTrajectory(manifest, buffer), first = trajectory.sample(0);
    assert.ok(Math.abs(first.ratio - 2.05) < 1e-6);
    assert.equal(first.positions.length, 9);
    assert.ok(first.positions.every(Number.isFinite));
    assert.ok(trajectory.sample(1000).positions.some((value, index) => Math.abs(value - first.positions[index]) > 0.1));
    if (preset.id === 'circular') assert.ok(Number.isNaN(first.phases[0]));
    else {
      assert.ok(Math.abs(first.eccentricities[0] - 0.30) < 1e-6);
      assert.ok(Math.abs(first.eccentricities[1] - 0.12) < 1e-6);
      assert.ok(Math.abs(first.phases[0]) < 1e-6);
    }
  }
});
