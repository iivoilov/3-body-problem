import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { AlienTrajectory, skyDirections } from './alien-replay.mjs';

const $ = id => document.getElementById(id);
let manifest, trajectory, renderer, scene, camera, controls, sky, bodies;
let currentTime = 0, speed = 1, surface = true;
let playing = !matchMedia('(prefers-reduced-motion: reduce)').matches;
let yaw = 2.93, pitch = 0.035, previousTimestamp = 0, lastUiTime = -1, dirty = true, previousExtent = 8;
const shaderText = source => source.replace(/^#version[^\n]*\n/, '');
const labels = [...document.querySelectorAll('.sun-label')];
const forward = new THREE.Vector3(), right = new THREE.Vector3(), up = new THREE.Vector3();
const worldUp = new THREE.Vector3(0, 1, 0), projected = new THREE.Vector3();

async function resource(path, type = 'text') {
  const response = await fetch(new URL(path, import.meta.url));
  if (!response.ok) throw new Error(`Could not load ${path} (${response.status}).`);
  return type === 'json' ? response.json() : type === 'binary' ? response.arrayBuffer() : response.text();
}

function playbackStatus() {
  $('play').textContent = playing ? 'Pause' : 'Play';
  $('play').setAttribute('aria-label', playing ? 'Pause the observation' : 'Play the observation');
  dirty = true;
}

function setView(isSurface) {
  surface = isSurface;
  controls.enabled = !surface;
  $('trails').disabled = surface;
  $('shadows').disabled = !surface;
  $('surface-view').setAttribute('aria-pressed', String(surface));
  $('orbit-view').setAttribute('aria-pressed', String(!surface));
  $('location').textContent = surface ? 'TRISOLARIS / SURFACE OBSERVATION' : 'COMPACT TRIPLE / BARYCENTRIC VIEW';
  $('camera-hint').textContent = surface ? 'Drag to look around' : 'Drag to rotate · Scroll to zoom';
  $('world').setAttribute('aria-label', surface ? 'Alien landscape beneath three moving suns' : 'Three gravitating stars and the alien planet, with recorded trajectories');
  dirty = true;
}

function jump(time) {
  currentTime = Math.max(0, Math.min(trajectory.duration, time));
  playing = false;
  playbackStatus();
}

function createScene(skyVertex, skyFragment, bodyVertex, bodyFragment) {
  renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'high-performance' });
  renderer.setPixelRatio(Math.min(devicePixelRatio, 1.5));
  renderer.setClearColor(0x050a0e);
  $('world').appendChild(renderer.domElement);
  scene = new THREE.Scene();
  camera = new THREE.PerspectiveCamera(45, 1, 0.01, 1000);
  camera.position.set(8, 11, 15);
  controls = new OrbitControls(camera, renderer.domElement);
  controls.enablePan = false;
  controls.enableDamping = true;
  controls.minDistance = 3;
  controls.maxDistance = 1000;
  controls.minPolarAngle = 0.10;
  controls.maxPolarAngle = Math.PI - 0.10;
  controls.enabled = false;
  sky = new THREE.Mesh(new THREE.PlaneGeometry(2, 2), new THREE.RawShaderMaterial({
    glslVersion: THREE.GLSL3,
    vertexShader: `precision highp float;\n${shaderText(skyVertex)}`,
    fragmentShader: `precision highp float;\nprecision highp int;\n${shaderText(skyFragment)}`,
    depthTest: false, depthWrite: false,
    uniforms: {
      viewport: { value: new THREE.Vector2() }, cameraForward: { value: new THREE.Vector3() },
      cameraRight: { value: new THREE.Vector3() }, cameraUp: { value: new THREE.Vector3() },
      sunDirections: { value: [0, 1, 2].map(() => new THREE.Vector3()) },
      sunColors: { value: manifest.stars.map(star => new THREE.Vector3(...star.color)) },
      sunPowers: { value: new THREE.Vector3() }, sunAngles: { value: new THREE.Vector3() },
      time: { value: 0 }, fovScale: { value: 0.62 }, surfaceView: { value: 1 }, shadowsEnabled: { value: 1 },
    },
  }));
  sky.frustumCulled = false;
  sky.renderOrder = -100;
  scene.add(sky);
  const geometry = new THREE.SphereGeometry(1, 64, 40);
  const haloCanvas = document.createElement('canvas');
  haloCanvas.width = haloCanvas.height = 128;
  const context = haloCanvas.getContext('2d');
  const gradient = context.createRadialGradient(64, 64, 0, 64, 64, 64);
  gradient.addColorStop(0, 'rgba(255,255,255,0.65)');
  gradient.addColorStop(0.2, 'rgba(255,255,255,0.25)');
  gradient.addColorStop(0.5, 'rgba(255,255,255,0.055)');
  gradient.addColorStop(1, 'rgba(255,255,255,0)');
  context.fillStyle = gradient;
  context.fillRect(0, 0, 128, 128);
  const haloTexture = new THREE.CanvasTexture(haloCanvas);
  bodies = [...manifest.stars, manifest.planet].map((body, index) => {
    const color = index < 3 ? new THREE.Vector3(...body.color) : new THREE.Vector3(0.26, 0.45, 0.48);
    const material = new THREE.ShaderMaterial({
      glslVersion: THREE.GLSL3, vertexShader: bodyVertex, fragmentShader: shaderText(bodyFragment),
      uniforms: { eye: { value: new THREE.Vector3() }, baseColor: { value: color },
        starPositions: { value: [0, 1, 2].map(() => new THREE.Vector3()) },
        starColors: { value: manifest.stars.map(star => new THREE.Vector3(...star.color)) },
        time: { value: 0 }, starBody: { value: index < 3 ? 1 : 0 } },
    });
    const mesh = new THREE.Mesh(geometry, material);
    mesh.scale.setScalar(body.displayRadius);
    scene.add(mesh);
    const trailPositions = new Float32Array(4100 * 3);
    const trailGeometry = new THREE.BufferGeometry();
    trailGeometry.setAttribute('position', new THREE.BufferAttribute(trailPositions, 3).setUsage(THREE.DynamicDrawUsage));
    trailGeometry.setDrawRange(0, 0);
    const trailColor = index < 3 ? new THREE.Color().setRGB(...body.color).multiplyScalar(0.5) : new THREE.Color(0x6db4ad);
    const trail = new THREE.Line(trailGeometry, new THREE.LineBasicMaterial({ color: trailColor, transparent: true, opacity: 0.75 }));
    trail.frustumCulled = false;
    scene.add(trail);
    let halo;
    if (index < 3) {
      halo = new THREE.Sprite(new THREE.SpriteMaterial({ map: haloTexture,
        color: new THREE.Color().setRGB(...body.color), transparent: true, blending: THREE.AdditiveBlending, depthWrite: false }));
      halo.scale.setScalar(body.displayRadius * 12);
      scene.add(halo);
    }
    return { mesh, halo, trail, trailPositions, trailFrame: -1 };
  });
  new ResizeObserver(() => {
    const { width, height } = $('world').getBoundingClientRect();
    if (!width || !height) return;
    renderer.setSize(width, height, false);
    renderer.getDrawingBufferSize(sky.material.uniforms.viewport.value);
    camera.aspect = width / height;
    camera.fov = THREE.MathUtils.radToDeg(2 * Math.atan(Math.tan(Math.PI / 8) / Math.min(camera.aspect, 1)));
    camera.updateProjectionMatrix();
    dirty = true;
  }).observe($('world'));
  let pointer = null;
  renderer.domElement.addEventListener('pointerdown', event => {
    if (!surface || event.button !== 0) return;
    pointer = { id: event.pointerId, x: event.clientX, y: event.clientY };
    renderer.domElement.setPointerCapture(event.pointerId);
  });
  renderer.domElement.addEventListener('pointermove', event => {
    if (!surface || pointer?.id !== event.pointerId) return;
    yaw -= (event.clientX - pointer.x) * 0.004;
    pitch = Math.max(-0.8, Math.min(1.2, pitch + (event.clientY - pointer.y) * 0.003));
    pointer.x = event.clientX; pointer.y = event.clientY;
  });
  for (const type of ['pointerup', 'pointercancel', 'lostpointercapture']) renderer.domElement.addEventListener(type, () => { pointer = null; });
}

function updateTrails(sample) {
  const first = Math.max(0, sample.frame - 4096);
  bodies.forEach((body, i) => {
    body.trail.visible = !surface && $('trails').checked;
    if (!body.trail.visible) return;
    let count = 0;
    if (body.trailFrame !== sample.frame) {
      for (let frame = first; frame <= sample.frame; frame++) {
        for (let axis = 0; axis < 3; axis++) body.trailPositions[count * 3 + axis] = trajectory.value(frame, i*3+axis);
        count++;
      }
      body.trailFrame = sample.frame;
      body.trail.geometry.setDrawRange(0, count + 1);
    } else count = sample.frame - first + 1;
    body.trailPositions.set(sample.positions.subarray(i*3, i*3+3), count*3);
    body.trail.geometry.attributes.position.needsUpdate = true;
  });
}

function updateLabels() {
  const { width, height } = $('world').getBoundingClientRect();
  labels.forEach((label, i) => {
    if (surface) { label.style.display = 'none'; return; }
    projected.copy(bodies[i].mesh.position).project(camera);
    const x = (projected.x + 1) * width / 2, y = (1 - projected.y) * height / 2;
    const visible = projected.z > -1 && projected.z < 1 && x > 12 && x < width-100 && y > 85 && y < height-75;
    label.style.display = visible ? 'block' : 'none';
    label.style.left = `${x+12}px`; label.style.top = `${y-5}px`;
  });
}

function drawChart() {
  const canvas = $('flux-chart'), dpr = Math.min(devicePixelRatio, 2);
  const width = canvas.clientWidth, height = canvas.clientHeight;
  canvas.width = Math.round(width*dpr); canvas.height = Math.round(height*dpr);
  const ctx = canvas.getContext('2d');
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  const left = 1, right = width-1, top = 5, bottom = height-8;
  const x = time => left+(right-left)*time/trajectory.duration;
  const y = value => bottom-(Math.log10(Math.max(value, 0.001))+3)/5*(bottom-top);
  ctx.strokeStyle = '#343737'; ctx.lineWidth = 1; ctx.setLineDash([3, 5]);
  ctx.beginPath(); ctx.moveTo(left,y(1)); ctx.lineTo(right,y(1)); ctx.stroke(); ctx.setLineDash([]);
  ctx.beginPath();
  for (let frame = 0; frame < trajectory.count; frame += 24) {
    const px = x(frame*trajectory.interval), py = y(trajectory.value(frame,12));
    if (!frame) ctx.moveTo(px,py); else ctx.lineTo(px,py);
  }
  ctx.strokeStyle = '#deb281'; ctx.lineWidth = 1.25; ctx.stroke();
  ctx.lineTo(right,bottom); ctx.lineTo(left,bottom); ctx.closePath();
  const fill = ctx.createLinearGradient(0,top,0,bottom);
  fill.addColorStop(0,'#d5a56c38'); fill.addColorStop(1,'#d5a56c00');
  ctx.fillStyle=fill; ctx.fill();
  const sample = trajectory.sample(currentTime);
  ctx.strokeStyle = '#eee6d566'; ctx.beginPath(); ctx.moveTo(x(currentTime),top); ctx.lineTo(x(currentTime),bottom); ctx.stroke();
  ctx.fillStyle = '#efd5b1'; ctx.beginPath(); ctx.arc(x(currentTime),y(sample.flux),2.5,0,2*Math.PI); ctx.fill();
}

function updateUi(sample, directions) {
  $('time').value = currentTime;
  $('year').textContent = currentTime.toFixed(3);
  $('time-label').textContent = `${currentTime.toFixed(2)} / ${trajectory.duration.toFixed(2)} yr`;
  $('flux').textContent = sample.flux.toFixed(3);
  $('nearest').textContent = sample.nearest.toFixed(3);
  $('separation').textContent = sample.separation.toFixed(3);
  $('energy').textContent = sample.energyError.toExponential(2);
  $('angular').textContent = sample.angularError.toExponential(2);
  for (let i = 0; i < 3; i++) {
    const distance = Math.hypot(...[0,1,2].map(axis => sample.positions[i*3+axis]-sample.positions[9+axis]));
    $(`distance-${i}`).textContent = `${distance.toFixed(3)} AU`;
  }
  const visible = directions.filter(direction => direction[1] > 0).length;
  $('sky-caption').textContent = surface ? (currentTime < 0.01 ? 'THE FIRST DAWN' : 'NO TWO DAWNS ALIKE') : 'GRAVITY IN MOTION';
  $('sky-state').textContent = surface ? ['A long night beneath unfamiliar stars.', 'One sun above the horizon.', 'Two suns share the sky.', 'Three suns above the horizon.'][visible] : 'Three interacting stars. One wandering world.';
  drawChart();
}

function animate(timestamp) {
  const dt = previousTimestamp ? Math.min((timestamp-previousTimestamp)/1000,0.1) : 0;
  previousTimestamp = timestamp;
  if (playing) currentTime = (currentTime+dt*speed*manifest.playbackScale)%trajectory.duration;
  const sample = trajectory.sample(currentTime);
  const directions = skyDirections(sample.positions,currentTime,manifest.rotationPeriod);
  let extent = 8;
  bodies.forEach((body,i) => {
    body.mesh.position.fromArray(sample.positions,i*3);
    body.mesh.visible = !surface;
    body.mesh.rotation.y = -currentTime*0.35;
    extent = Math.max(extent,body.mesh.position.length());
    const uniforms = body.mesh.material.uniforms;
    uniforms.time.value = currentTime;
    uniforms.eye.value.copy(camera.position);
    for (let star = 0; star < 3; star++) uniforms.starPositions.value[star].fromArray(sample.positions,star*3);
    if (body.halo) { body.halo.position.copy(body.mesh.position); body.halo.visible = !surface; }
  });
  if (extent !== previousExtent) camera.position.multiplyScalar(extent/previousExtent);
  previousExtent = extent;
  controls.update();
  if (surface) forward.set(Math.sin(yaw)*Math.cos(pitch),Math.sin(pitch),Math.cos(yaw)*Math.cos(pitch));
  else camera.getWorldDirection(forward);
  right.crossVectors(forward,worldUp).normalize(); up.crossVectors(right,forward);
  const uniforms = sky.material.uniforms;
  uniforms.cameraForward.value.copy(forward); uniforms.cameraRight.value.copy(right); uniforms.cameraUp.value.copy(up);
  uniforms.fovScale.value = surface ? 0.62 / Math.min(camera.aspect/1.65,1) : Math.tan(THREE.MathUtils.degToRad(camera.fov)/2);
  uniforms.surfaceView.value = surface ? 1 : 0;
  uniforms.shadowsEnabled.value = $('shadows').checked ? 1 : 0;
  uniforms.time.value = currentTime;
  uniforms.sunPowers.value.fromArray(sample.contributions);
  const angles = manifest.stars.map((star,i) => {
    const distance = bodies[i].mesh.position.distanceTo(bodies[3].mesh.position);
    return Math.max(0.009,Math.min(0.065,star.radius*14/distance));
  });
  uniforms.sunAngles.value.fromArray(angles);
  directions.forEach((direction,i) => uniforms.sunDirections.value[i].fromArray(direction));
  updateTrails(sample);
  renderer.render(scene,camera);
  updateLabels();
  if (dirty || Math.abs(currentTime-lastUiTime)>0.015) { updateUi(sample,directions); lastUiTime=currentTime; dirty=false; }
  requestAnimationFrame(animate);
}

function fail(error) {
  $('status').classList.add('error');
  $('status').textContent = `Observation unavailable: ${error.message} Serve doc/ over HTTP and enable WebGL 2.`;
  console.error(error);
}

async function initialize() {
  const [data,binary,skyVertex,skyFragment,bodyVertex,bodyFragment] = await Promise.all([
    resource('data/orbit-02/manifest.json','json'),resource('data/orbit-02/world.bin','binary'),
    resource('shaders/alien-screen.vert'),resource('shaders/alien-sky.frag'),
    resource('shaders/body.vert'),resource('shaders/alien-body.frag'),
  ]);
  manifest=data; trajectory=new AlienTrajectory(manifest,binary);
  const params=new URLSearchParams(location.search);
  const requestedTime=Number(params.get('time')??0);
  currentTime=Number.isFinite(requestedTime) ? Math.max(0,Math.min(trajectory.duration,requestedTime)) : 0;
  if (params.get('paused')==='1') playing=false;
  createScene(skyVertex,skyFragment,bodyVertex,bodyFragment);
  setView(params.get('view')!=='orbit');
  let encounterFrame=0,brightestFrame=0;
  for (let frame=0;frame<trajectory.count;frame++) {
    if(trajectory.value(frame,14)<trajectory.value(encounterFrame,14)) encounterFrame=frame;
    if(trajectory.value(frame,12)>trajectory.value(brightestFrame,12)) brightestFrame=frame;
  }
  $('play').addEventListener('click',()=>{playing=!playing;playbackStatus();});
  $('restart').addEventListener('click',()=>{currentTime=0;yaw=2.93;pitch=0.035;dirty=true;});
  $('time').addEventListener('input',()=>jump(Number($('time').value)));
  $('speed').addEventListener('change',()=>{speed=Number($('speed').value);playbackStatus();});
  $('surface-view').addEventListener('click',()=>setView(true));
  $('orbit-view').addEventListener('click',()=>setView(false));
  $('first-dawn').addEventListener('click',()=>jump(0));
  $('encounter').addEventListener('click',()=>jump(encounterFrame*trajectory.interval));
  $('brightest').addEventListener('click',()=>jump(brightestFrame*trajectory.interval));
  $('remote').addEventListener('click',()=>jump(trajectory.duration));
  document.addEventListener('keydown',event=>{
    if (['INPUT','SELECT','BUTTON','TEXTAREA','SUMMARY','A'].includes(document.activeElement?.tagName)) return;
    if(event.code==='Space'){event.preventDefault();playing=!playing;playbackStatus();}
    if(event.code==='KeyV') setView(!surface);
    if(event.code==='KeyR'){jump(0);yaw=2.93;pitch=0.035;}
  });
  for(const id of ['play','restart','time']) $(id).disabled=false;
  $('status').textContent='';
  playbackStatus();
  $('world').dataset.ready='true';
  requestAnimationFrame(animate);
}
initialize().catch(fail);
