// Playback of recorded C++ output only. No gravitational forces are evaluated here.
export const FIELD = Object.freeze({ ratio: 9, azureE: 10, emberE: 11, azurePhase: 12, emberPhase: 13 });

export class RecordedTrajectory {
  constructor(manifest, buffer) {
    if (manifest.format !== 'orbit-lab-trajectory' || manifest.version !== 1 ||
        manifest.encoding !== 'float32-le' || manifest.fieldsPerFrame !== 14 ||
        !Number.isInteger(manifest.frameCount) || manifest.frameCount < 2 ||
        !Number.isFinite(manifest.sampleInterval) || manifest.sampleInterval <= 0 ||
        !Number.isFinite(manifest.duration) || manifest.duration <= 0 ||
        Math.abs((manifest.frameCount - 1) * manifest.sampleInterval - manifest.duration) > 1e-7) {
      throw new Error('Unsupported trajectory format. Regenerate the browser data with orbit_export.');
    }
    if (buffer.byteLength !== manifest.frameCount * manifest.fieldsPerFrame * 4) {
      throw new Error('The trajectory file is incomplete or does not match its manifest.');
    }
    this.manifest = manifest;
    this.view = new DataView(buffer);
    this.duration = manifest.duration;
    this.interval = manifest.sampleInterval;
    this.count = manifest.frameCount;
    this.stride = manifest.fieldsPerFrame;
  }

  value(frame, field) {
    return this.view.getFloat32((frame * this.stride + field) * 4, true);
  }

  sample(time) {
    if (!Number.isFinite(time)) throw new Error('Playback time must be finite.');
    const clamped = Math.max(0, Math.min(this.duration, time));
    const frame = Math.min(this.count - 1, Math.floor(clamped / this.interval));
    const next = Math.min(this.count - 1, frame + 1);
    const fraction = (clamped - frame * this.interval) / this.interval;
    const positions = new Float32Array(9);
    for (let field = 0; field < 9; field++) {
      const a = this.value(frame, field), b = this.value(next, field);
      positions[field] = a + (b - a) * fraction;
    }
    return {
      time: clamped, frame, positions,
      ratio: this.value(frame, FIELD.ratio),
      eccentricities: [this.value(frame, FIELD.azureE), this.value(frame, FIELD.emberE)],
      phases: [this.value(frame, FIELD.azurePhase), this.value(frame, FIELD.emberPhase)],
    };
  }

  history(time, span = 1024) {
    if (!Number.isFinite(time)) throw new Error('Playback time must be finite.');
    const clamped = Math.max(0, Math.min(time, this.duration));
    const last = Math.min(this.count - 1, Math.floor(clamped / this.interval));
    const stride = Math.max(1, Math.round(this.manifest.telemetryInterval / this.interval));
    const first = Math.max(0, Math.ceil((clamped - span) / this.interval / stride) * stride);
    const result = [];
    for (let frame = first; frame <= last; frame += stride) {
      result.push({
        time: frame * this.interval,
        ratio: this.value(frame, FIELD.ratio),
        eccentricities: [this.value(frame, FIELD.azureE), this.value(frame, FIELD.emberE)],
        phases: [this.value(frame, FIELD.azurePhase), this.value(frame, FIELD.emberPhase)],
      });
    }
    return result;
  }
}

// Rebase each uninterrupted phase segment to keep long-running circulation readable.
export function phaseChanges(samples, series) {
  let origin = null;
  return samples.map(sample => {
    const value = sample.phases[series];
    if (!Number.isFinite(value)) { origin = null; return NaN; }
    if (origin === null) origin = value;
    return value - origin;
  });
}
