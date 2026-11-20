// Replay only. The native C++ integrator produces all trajectories and telemetry.
export class AlienTrajectory {
  constructor(manifest, buffer) {
    if (manifest.format !== 'orbit-02-trajectory' || manifest.version !== 1 ||
        manifest.encoding !== 'float32-le' || manifest.fieldsPerFrame !== 20 ||
        !Number.isInteger(manifest.frameCount) || manifest.frameCount < 2 ||
        !Number.isFinite(manifest.sampleInterval) || manifest.sampleInterval <= 0 ||
        !Number.isFinite(manifest.duration) || manifest.duration <= 0 ||
        Math.abs((manifest.frameCount - 1) * manifest.sampleInterval - manifest.duration) > 1e-8) {
      throw new Error('Unsupported Orbit / 02 recording. Regenerate with orbit02_export.');
    }
    if (buffer.byteLength !== manifest.frameCount * 80) throw new Error('Orbit / 02 recording is incomplete.');
    this.view = new DataView(buffer);
    this.manifest = manifest;
    this.count = manifest.frameCount;
    this.interval = manifest.sampleInterval;
    this.duration = manifest.duration;
  }
  value(frame, field) { return this.view.getFloat32((frame * 20 + field) * 4, true); }
  sample(time) {
    if (!Number.isFinite(time)) throw new Error('Time must be finite.');
    const clamped = Math.max(0, Math.min(this.duration, time));
    const frame = Math.min(this.count - 1, Math.floor(clamped / this.interval));
    const next = Math.min(frame + 1, this.count - 1);
    const fraction = (clamped - frame * this.interval) / this.interval;
    const positions = new Float32Array(12);
    for (let field = 0; field < 12; field++) {
      const a = this.value(frame, field);
      positions[field] = a + (this.value(next, field) - a) * fraction;
    }
    return { time: clamped, frame, positions, flux: this.value(frame, 12), nearest: this.value(frame, 13),
      separation: this.value(frame, 14), energyError: this.value(frame, 15), angularError: this.value(frame, 16),
      contributions: [17, 18, 19].map(field => this.value(frame, field)) };
  }
}

export function skyDirections(positions, time, rotationPeriod = 1.6) {
  const angle = 0.57 + 2 * Math.PI * time / rotationPeriod;
  const up = [0.28 * Math.sin(angle), Math.sqrt(1 - 0.28 ** 2), 0.28 * Math.cos(angle)];
  const normalize = v => { const norm = Math.hypot(...v); return v.map(value => value / norm); };
  const north = normalize([-up[0] * up[1], 1 - up[1] ** 2, -up[2] * up[1]]);
  const east = [north[1]*up[2]-north[2]*up[1], north[2]*up[0]-north[0]*up[2], north[0]*up[1]-north[1]*up[0]];
  const dot = (a, b) => a.reduce((sum, value, i) => sum + value*b[i], 0);
  return [0, 1, 2].map(i => {
    const direction = normalize([0, 1, 2].map(axis => positions[i*3+axis] - positions[9+axis]));
    return [dot(direction, east), dot(direction, up), dot(direction, north)];
  });
}
