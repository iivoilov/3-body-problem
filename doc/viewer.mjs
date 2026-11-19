import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { RecordedTrajectory, phaseChanges } from './replay.mjs';

const $ = id => document.getElementById(id);
const palette = { grid: '#29384c', muted: '#a1b0c5', ratio: '#63dfc3', azure: '#58b9fa', ember: '#f68b62' };
const cache = new Map();
let manifest, trajectory, renderer, scene, camera, controls, bodies, glow;
let currentTime = 1000, speed = 1, playing = !matchMedia('(prefers-reduced-motion: reduce)').matches;
let previousTimestamp = 0, lastUiTime = -1, uiDirty = true;

async function resource(path, type = 'text') {
  const response = await fetch(new URL(path, import.meta.url));
  if (!response.ok) throw new Error(`Could not load ${path} (${response.status}).`);
  return type === 'json' ? response.json() : type === 'binary' ? response.arrayBuffer() : response.text();
}

function status() {
  $('status').textContent = `${playing ? 'Playing' : 'Paused'} · ${speed * manifest.playbackScale} simulated seconds / real second`;
  $('play').textContent = playing ? 'Pause' : 'Play';
  $('play').setAttribute('aria-label', playing ? 'Pause replay' : 'Play replay');
}

function createScene(vertexShader, fragmentShader) {
  renderer = new THREE.WebGLRenderer({ antialias: true });
  renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
  renderer.setClearColor(0x040912);
  $('scene').appendChild(renderer.domElement);
  scene = new THREE.Scene();
  camera = new THREE.PerspectiveCamera(45, 1, 0.1, 200);
  camera.position.set(24 * Math.cos(0.65) * Math.sin(0.25), 24 * Math.sin(0.65), 24 * Math.cos(0.65) * Math.cos(0.25));
  controls = new OrbitControls(camera, renderer.domElement);
  controls.enableDamping = true;
  controls.enablePan = false;
  controls.minDistance = 2.5;
  controls.maxDistance = 48;
  controls.minPolarAngle = 0.12;
  controls.maxPolarAngle = Math.PI - 0.12;

  const geometry = new THREE.SphereGeometry(1, 64, 40);
  bodies = manifest.bodies.map((body, index) => {
    const material = new THREE.ShaderMaterial({
      glslVersion: THREE.GLSL3, vertexShader, fragmentShader,
      uniforms: {
        baseColor: { value: new THREE.Vector3(...body.color) }, eye: { value: new THREE.Vector3() },
        bodyKind: { value: index }, time: { value: 0 }, sunRadius: { value: manifest.bodies[0].radius },
        sunPosition: { value: new THREE.Vector3() }, occluderCenter: { value: new THREE.Vector3() },
        occluderRadius: { value: index === 0 ? 0 : manifest.bodies[3 - index].radius },
        shadowsEnabled: { value: 1 },
      },
    });
    const mesh = new THREE.Mesh(geometry, material);
    mesh.scale.setScalar(body.radius);
    scene.add(mesh);

    const trailCount = Math.ceil(128 / manifest.sampleInterval) + 2;
    const trailPositions = new Float32Array(trailCount * 3);
    const trailGeometry = new THREE.BufferGeometry();
    trailGeometry.setAttribute('position', new THREE.BufferAttribute(trailPositions, 3).setUsage(THREE.DynamicDrawUsage));
    trailGeometry.setDrawRange(0, 0);
    const color = index === 0 ? 0xbb883c : index === 1 ? 0x27658a : 0x873f2b;
    const trail = new THREE.Line(trailGeometry, new THREE.LineBasicMaterial({ color }));
    trail.frustumCulled = false;
    scene.add(trail);
    return { mesh, trail, trailPositions, trailFrame: -1 };
  });

  const halo = document.createElement('canvas');
  halo.width = halo.height = 128;
  const context = halo.getContext('2d');
  const gradient = context.createRadialGradient(64, 64, 0, 64, 64, 64);
  gradient.addColorStop(0, 'rgba(255,190,90,0.7)');
  gradient.addColorStop(0.3, 'rgba(255,150,50,0.25)');
  gradient.addColorStop(1, 'rgba(255,90,20,0)');
  context.fillStyle = gradient;
  context.fillRect(0, 0, 128, 128);
  glow = new THREE.Sprite(new THREE.SpriteMaterial({
    map: new THREE.CanvasTexture(halo), blending: THREE.AdditiveBlending, depthWrite: false, opacity: 0.8,
  }));
  glow.scale.set(6.6, 6.6, 1);
  scene.add(glow);

  let randomState = 42;
  const random = () => { randomState = (1664525 * randomState + 1013904223) >>> 0; return randomState / 4294967296; };
  const stars = new Float32Array(1600 * 3);
  for (let i = 0; i < 1600; i++) {
    const y = random() * 2 - 1, angle = random() * 2 * Math.PI, radius = Math.sqrt(1 - y * y);
    stars.set([90 * radius * Math.cos(angle), 90 * y, 90 * radius * Math.sin(angle)], i * 3);
  }
  const starGeometry = new THREE.BufferGeometry();
  starGeometry.setAttribute('position', new THREE.BufferAttribute(stars, 3));
  scene.add(new THREE.Points(starGeometry, new THREE.PointsMaterial({ color: 0x8293ae, size: 1.5, sizeAttenuation: false })));

  new ResizeObserver(() => {
    const { width, height } = $('scene').getBoundingClientRect();
    if (!width || !height) return;
    renderer.setSize(width, height, false);
    camera.aspect = width / height;
    // Preserve enough horizontal field of view for the whole system on narrow screens.
    camera.fov = THREE.MathUtils.radToDeg(2 * Math.atan(Math.tan(THREE.MathUtils.degToRad(45) / 2) / Math.min(camera.aspect, 1)));
    camera.updateProjectionMatrix();
    uiDirty = true;
  }).observe($('scene'));
  window.addEventListener('resize', () => { uiDirty = true; });
}

async function loadPreset(id) {
  const preset = manifest.presets.find(entry => entry.id === id);
  if (!preset) throw new Error('Unknown starting preset.');
  $('preset').disabled = true;
  $('status').textContent = 'Loading recorded trajectories…';
  if (!cache.has(id)) cache.set(id, new RecordedTrajectory(manifest, await resource(`data/${preset.file}`, 'binary')));
  trajectory = cache.get(id);
  currentTime = Math.max(0, Math.min(trajectory.duration, currentTime));
  $('preset').value = id;
  $('preset-description').textContent = preset.description;
  $('time').max = trajectory.duration;
  $('time').step = trajectory.interval;
  $('data-duration').textContent = trajectory.duration;
  for (const body of bodies) body.trailFrame = -1;
  for (const id of ['preset', 'play', 'restart', 'time']) $(id).disabled = false;
  uiDirty = true;
  status();
}

function updateTrails(sample) {
  const first = Math.max(0, sample.frame - Math.floor(128 / trajectory.interval));
  for (let bodyIndex = 0; bodyIndex < 3; bodyIndex++) {
    const body = bodies[bodyIndex];
    body.trail.visible = $('trails').checked;
    if (!body.trail.visible) continue;
    let count = 0;
    if (body.trailFrame !== sample.frame) {
      for (let frame = first; frame <= sample.frame; frame++) {
        for (let axis = 0; axis < 3; axis++) body.trailPositions[count * 3 + axis] = trajectory.value(frame, bodyIndex * 3 + axis);
        count++;
      }
      body.trailFrame = sample.frame;
      body.trail.geometry.setDrawRange(0, count + 1);
    } else {
      count = sample.frame - first + 1;
    }
    body.trailPositions.set(sample.positions.subarray(bodyIndex * 3, bodyIndex * 3 + 3), count * 3);
    body.trail.geometry.attributes.position.needsUpdate = true;
  }
}

function drawChart(canvas, samples, series, options = {}) {
  const width = canvas.clientWidth, height = canvas.clientHeight, dpr = Math.min(devicePixelRatio, 2);
  if (canvas.width !== Math.round(width * dpr) || canvas.height !== Math.round(height * dpr)) {
    canvas.width = Math.round(width * dpr); canvas.height = Math.round(height * dpr);
  }
  const context = canvas.getContext('2d');
  context.setTransform(dpr, 0, 0, dpr, 0, 0);
  context.clearRect(0, 0, width, height);
  if (!samples.length) return;
  let low = options.zero ? 0 : Infinity, high = options.minimumHigh ?? -Infinity;
  for (const entry of series) for (const value of entry.values) {
    if (!Number.isFinite(value)) continue;
    low = Math.min(low, value); high = Math.max(high, value);
  }
  if (options.reference !== undefined) { low = Math.min(low, options.reference); high = Math.max(high, options.reference); }
  if (!Number.isFinite(low) || !Number.isFinite(high)) { low = -0.25; high = 0.25; }
  const pad = Math.max(options.minimumPad ?? 0.01, (high - low) * 0.12);
  if (!options.zero) low -= pad;
  high += pad;
  const left = 44, right = width - 6, top = 8, bottom = height - 24;
  const y = value => bottom - (value - low) / (high - low) * (bottom - top);
  const start = samples[0].time, end = samples.at(-1).time;
  const x = time => left + (time - start) / Math.max(end - start, trajectory.interval) * (right - left);
  context.lineWidth = 1;
  context.strokeStyle = palette.grid;
  context.fillStyle = palette.muted;
  context.font = '12px ui-monospace, monospace';
  for (let row = 0; row < 3; row++) {
    const value = low + (high - low) * row / 2;
    context.beginPath(); context.moveTo(left, y(value)); context.lineTo(right, y(value)); context.stroke();
    context.fillText(value.toFixed(2), 0, y(value) + 4);
  }
  context.fillText(start.toFixed(0), left, height - 4);
  context.textAlign = 'right'; context.fillText(`${end.toFixed(0)} s`, right, height - 4); context.textAlign = 'left';
  context.save(); context.beginPath(); context.rect(left, top, right - left, bottom - top); context.clip();
  if (options.reference !== undefined) {
    context.setLineDash([5, 5]); context.strokeStyle = palette.ratio;
    context.beginPath(); context.moveTo(left, y(options.reference)); context.lineTo(right, y(options.reference)); context.stroke();
    context.setLineDash([]);
  }
  for (const entry of series) {
    context.strokeStyle = entry.color; context.lineWidth = 1.6; context.beginPath();
    let connected = false;
    entry.values.forEach((value, index) => {
      if (!Number.isFinite(value)) { connected = false; return; }
      if (connected) context.lineTo(x(samples[index].time), y(value));
      else context.moveTo(x(samples[index].time), y(value));
      connected = true;
    });
    context.stroke();
  }
  context.restore();
}

function updateUi(sample) {
  $('time').value = currentTime;
  $('time-label').textContent = `${currentTime.toFixed(1)} / ${trajectory.duration} simulated seconds`;
  $('ratio').textContent = Number.isFinite(sample.ratio) ? sample.ratio.toFixed(5) : 'Undefined';
  const offset = (sample.ratio / 2 - 1) * 100;
  $('offset').textContent = Number.isFinite(offset) ? `${offset >= 0 ? '+' : ''}${offset.toFixed(2)}% from 2:1 · not a resonance score` : 'No bound two-body period';
  $('azure-e').textContent = `A ${sample.eccentricities[0].toFixed(4)}`;
  $('ember-e').textContent = `B ${sample.eccentricities[1].toFixed(4)}`;
  const samples = trajectory.history(currentTime);
  const phases = [phaseChanges(samples, 0), phaseChanges(samples, 1)];
  for (let i = 0; i < 2; i++) {
    const value = phases[i].at(-1);
    $(i === 0 ? 'azure-phase' : 'ember-phase').textContent = `${i === 0 ? 'A' : 'B'} ${Number.isFinite(value) ? `${value >= 0 ? '+' : ''}${value.toFixed(2)}` : '—'}`;
  }
  drawChart($('ratio-chart'), samples, [{ values: samples.map(value => value.ratio), color: palette.ratio }], { reference: 2 });
  drawChart($('eccentricity-chart'), samples, [0, 1].map(i => ({ values: samples.map(value => value.eccentricities[i]), color: i === 0 ? palette.azure : palette.ember })), { zero: true, minimumHigh: 0.1 });
  drawChart($('phase-chart'), samples, phases.map((values, i) => ({ values, color: i === 0 ? palette.azure : palette.ember })), { minimumPad: 0.25 });
  $('history-label').textContent = `Recorded measurements over the last ${samples.length ? (samples.at(-1).time - samples[0].time).toFixed(1) : 0} simulated seconds. Phase gaps mark unreliable periapsis directions.`;
}

function animate(timestamp) {
  const dt = previousTimestamp ? Math.min((timestamp - previousTimestamp) / 1000, 0.1) : 0;
  previousTimestamp = timestamp;
  if (trajectory) {
    if (playing) currentTime = (currentTime + dt * speed * manifest.playbackScale) % trajectory.duration;
    const sample = trajectory.sample(currentTime);
    controls.update();
    for (let index = 0; index < bodies.length; index++) {
      const mesh = bodies[index].mesh, uniforms = mesh.material.uniforms;
      mesh.position.fromArray(sample.positions, index * 3);
      mesh.rotation.y = -(currentTime * manifest.bodies[index].spinRate) % (2 * Math.PI);
      uniforms.eye.value.copy(camera.position);
      uniforms.time.value = currentTime % 1000;
      uniforms.sunPosition.value.fromArray(sample.positions, 0);
      if (index > 0) uniforms.occluderCenter.value.fromArray(sample.positions, (3 - index) * 3);
      uniforms.shadowsEnabled.value = $('shadows').checked ? 1 : 0;
    }
    glow.position.fromArray(sample.positions, 0);
    updateTrails(sample);
    renderer.render(scene, camera);
    if (uiDirty || Math.abs(currentTime - lastUiTime) >= 0.4) {
      updateUi(sample); lastUiTime = currentTime; uiDirty = false;
    }
  }
  requestAnimationFrame(animate);
}

function fail(error) {
  $('status').classList.add('error');
  $('status').textContent = `Viewer unavailable: ${error.message} Serve the doc directory over HTTP and check WebGL 2 support.`;
  console.error(error);
}

async function initialize() {
  const [data, vertexShader, fragmentShader] = await Promise.all([
    resource('data/manifest.json', 'json'), resource('shaders/body.vert'), resource('shaders/body.frag'),
  ]);
  manifest = data;
  const params = new URLSearchParams(location.search);
  const requestedTime = Number(params.get('time') ?? 1000);
  currentTime = Number.isFinite(requestedTime) ? requestedTime : 1000;
  createScene(vertexShader, fragmentShader);
  await loadPreset(params.get('preset') === 'resonance' ? 'resonance' : 'circular');
  $('scene').dataset.ready = 'true';
  $('play').addEventListener('click', () => { playing = !playing; uiDirty = true; status(); });
  $('restart').addEventListener('click', () => { currentTime = 0; uiDirty = true; });
  $('time').addEventListener('input', () => { currentTime = Number($('time').value); playing = false; uiDirty = true; status(); });
  $('speed').addEventListener('change', () => { speed = Number($('speed').value); status(); });
  $('preset').addEventListener('change', () => { loadPreset($('preset').value).catch(fail); });
  $('trails').addEventListener('change', () => { for (const body of bodies) body.trailFrame = -1; });
  document.addEventListener('keydown', event => {
    if (event.code === 'Space' && !['INPUT', 'SELECT', 'BUTTON', 'TEXTAREA'].includes(document.activeElement?.tagName)) {
      event.preventDefault(); playing = !playing; uiDirty = true; status();
    }
  });
  requestAnimationFrame(animate);
}

initialize().catch(fail);
