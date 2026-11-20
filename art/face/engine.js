// The robot's face engine (browser prototype).
//
// Draws the robot's face into a 135x240 framebuffer exactly the way the ESP32 will: every shape is a signed
// distance function evaluated per pixel (anti-aliased, morphable), every moving value is a spring, and the frame
// is quantised to RGB565 before it is shown. It is written in a plain C-like style so it ports line for line to
// C++ (firmware) once the scenes are picked. No DOM work in here except the text and image rasterisers.
(function (G) {
'use strict';

// ---------------------------------------------------------------- math
const PI = Math.PI, TAU = PI * 2;
const clamp = (x, a, b) => (x < a ? a : x > b ? b : x);
const mix = (a, b, t) => a + (b - a) * t;
const len = (x, y) => Math.sqrt(x * x + y * y);
const smooth = (a, b, x) => { const t = clamp((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); };
function hash2(x, y) {
  let h = (Math.imul(x | 0, 374761393) + Math.imul(y | 0, 668265263)) | 0;
  h = Math.imul(h ^ (h >>> 13), 1274126177);
  return ((h ^ (h >>> 16)) >>> 0) / 4294967295;
}
function vnoise(x, y) {
  const xi = Math.floor(x), yi = Math.floor(y), xf = x - xi, yf = y - yi;
  const u = xf * xf * (3 - 2 * xf), v = yf * yf * (3 - 2 * yf);
  return mix(mix(hash2(xi, yi), hash2(xi + 1, yi), u), mix(hash2(xi, yi + 1), hash2(xi + 1, yi + 1), u), v);
}
// Deterministic random for scenes (so a loop replays the same).
function rng(seed) { let s = seed >>> 0 || 1; return () => { s ^= s << 13; s >>>= 0; s ^= s >>> 17; s ^= s << 5; s >>>= 0; return s / 4294967296; }; }

const Ease = {
  outCubic: t => 1 - Math.pow(1 - clamp(t, 0, 1), 3),
  inCubic: t => Math.pow(clamp(t, 0, 1), 3),
  inOut: t => { t = clamp(t, 0, 1); return t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2; },
  outBack: t => { t = clamp(t, 0, 1); const c1 = 1.9, c3 = c1 + 1; return 1 + c3 * Math.pow(t - 1, 3) + c1 * Math.pow(t - 1, 2); },
  outElastic: t => { t = clamp(t, 0, 1); if (t === 0 || t === 1) return t; return Math.pow(2, -10 * t) * Math.sin((t * 10 - 0.75) * (TAU / 3)) + 1; },
  // 0 -> 1 -> 0 bump over [0,1]
  bump: t => { t = clamp(t, 0, 1); return Math.sin(t * PI); },
};
// Progress of t through [a, a+d], 0..1.
const prog = (t, a, d) => clamp((t - a) / d, 0, 1);

// ---------------------------------------------------------------- colour
function hex(h) { const n = parseInt(h.slice(1), 16); return [(n >> 16) & 255, (n >> 8) & 255, n & 255]; }
function mixc(a, b, t) { return [mix(a[0], b[0], t), mix(a[1], b[1], t), mix(a[2], b[2], t)]; }
function scalec(a, k) { return [a[0] * k, a[1] * k, a[2] * k]; }

// ---------------------------------------------------------------- signed distance functions (2-D, y down)
const SD = {
  circle: (x, y, r) => len(x, y) - r,
  // Ellipse, approximate (good to a pixel at these sizes; exact enough for anti-aliasing).
  ellipse: (x, y, a, b) => {
    const k0 = len(x / a, y / b), k1 = len(x / (a * a), y / (b * b));
    return k0 < 1e-5 ? -Math.min(a, b) : (k0 * (k0 - 1)) / k1;
  },
  roundBox: (x, y, hw, hh, r) => {
    r = Math.min(r, hw, hh);
    const qx = Math.abs(x) - hw + r, qy = Math.abs(y) - hh + r;
    return len(Math.max(qx, 0), Math.max(qy, 0)) + Math.min(Math.max(qx, qy), 0) - r;
  },
  segment: (x, y, ax, ay, bx, by) => {
    const pax = x - ax, pay = y - ay, bax = bx - ax, bay = by - ay;
    const h = clamp((pax * bax + pay * bay) / (bax * bax + bay * bay || 1), 0, 1);
    return len(pax - bax * h, pay - bay * h);
  },
  // Heart centred on (0,0), s = about half its height.
  heart: (x, y, s) => {
    s *= 1.8;
    const px = Math.abs(x) / s, py = -y / s + 0.55;
    let d;
    if (py + px > 1) d = len(px - 0.25, py - 0.75) - Math.SQRT2 / 4;
    else {
      const m = 0.5 * Math.max(px + py, 0);
      d = Math.sqrt(Math.min(px * px + (py - 1) * (py - 1), (px - m) * (px - m) + (py - m) * (py - m))) * Math.sign(px - py);
    }
    return d * s;
  },
  // Five-point star, point up. r outer radius, rf inner ratio (0.4 classic, 0.6 chubby).
  star: (x, y, r, rf) => {
    const k1x = 0.809016994375, k1y = -0.587785252292, k2x = -k1x, k2y = k1y;
    let px = Math.abs(x), py = -y;
    let d1 = Math.max(k1x * px + k1y * py, 0); px -= 2 * d1 * k1x; py -= 2 * d1 * k1y;
    let d2 = Math.max(k2x * px + k2y * py, 0); px -= 2 * d2 * k2x; py -= 2 * d2 * k2y;
    px = Math.abs(px); py -= r;
    const bax = rf * -k1y - 0, bay = rf * k1x - 1;
    const h = clamp((px * bax + py * bay) / (bax * bax + bay * bay), 0, r);
    return len(px - bax * h, py - bay * h) * Math.sign(py * bax - px * bay);
  },
  // Arc stroke: centre (0,0), radius ra, half-aperture ang (rad) around straight up, thickness rb. Draws "∩".
  arc: (x, y, ra, ang, rb) => {
    const sx = Math.sin(ang), cy = Math.cos(ang);
    const px = Math.abs(x), py = -y;
    return (cy * px > sx * py ? len(px - sx * ra, py - cy * ra) : Math.abs(len(px, py) - ra)) - rb;
  },
  // Egg, wide end down. ra = bottom radius, rb rounding. Height about ra*(1+0.9).
  egg: (x, y, ra, rb) => {
    const k = Math.sqrt(3);
    const px = Math.abs(x), py = -y;
    const r = ra - rb;
    return (py < 0 ? len(px, py) - r : k * (px + r) < py ? len(px, py - k * r) : len(px + r, py) - 2 * r) - rb;
  },
  // Drop: round end at (0,0) radius r1, tip at (0,-h) radius r2 (tip points up).
  drop: (x, y, r1, r2, h) => {
    const px = Math.abs(x), py = -y;
    const b = (r1 - r2) / h, a = Math.sqrt(1 - b * b);
    const k = -b * px + a * py;
    if (k < 0) return len(px, py) - r1;
    if (k > a * h) return len(px, py - h) - r2;
    return a * px + b * py - r1;
  },
  // Distance to the edge of a hex cell (pointy-top) in a lattice of size s. Negative inside the cell.
  hexCell: (x, y, s) => {
    const rx = 1, ry = 1.7320508;
    const ux = x / s, uy = y / s;
    const ax = ((ux % rx) + rx) % rx - rx / 2, ay = ((uy % ry) + ry) % ry - ry / 2;
    const bx = (((ux - rx / 2) % rx) + rx) % rx - rx / 2, by = (((uy - ry / 2) % ry) + ry) % ry - ry / 2;
    const ga = ax * ax + ay * ay < bx * bx + by * by;
    const gx = Math.abs(ga ? ax : bx), gy = Math.abs(ga ? ay : by);
    const c = Math.max(gx * 0.5 + gy * 0.8660254, gx);
    return (c - 0.5) * s;
  },
};

// ---------------------------------------------------------------- framebuffer
class Screen {
  constructor(w, h) {
    this.w = w; this.h = h;
    this.px = new Float32Array(w * h * 3);
    this.img = null; // ImageData for presenting
    this.dim = 1;    // backlight, 0..1
  }
  fill(c) { const p = this.px; for (let i = 0; i < p.length; i += 3) { p[i] = c[0]; p[i + 1] = c[1]; p[i + 2] = c[2]; } }
  // Vertical gradient with grain (the grain also stops RGB565 banding).
  fillGradient(top, bot, grain) {
    const p = this.px, w = this.w, h = this.h;
    for (let y = 0; y < h; y++) {
      const t = y / (h - 1);
      for (let x = 0; x < w; x++) {
        const n = grain ? (hash2(x, y) - 0.5) * grain : 0;
        const i = (y * w + x) * 3;
        p[i] = mix(top[0], bot[0], t) + n; p[i + 1] = mix(top[1], bot[1], t) + n; p[i + 2] = mix(top[2], bot[2], t) + n;
      }
    }
  }
  blend(x, y, c, a) {
    if (a <= 0 || x < 0 || y < 0 || x >= this.w || y >= this.h) return;
    const i = (y * this.w + x) * 3, p = this.px;
    if (a >= 1) { p[i] = c[0]; p[i + 1] = c[1]; p[i + 2] = c[2]; return; }
    p[i] += (c[0] - p[i]) * a; p[i + 1] += (c[1] - p[i + 1]) * a; p[i + 2] += (c[2] - p[i + 2]) * a;
  }
  add(x, y, c, a) {
    if (a <= 0 || x < 0 || y < 0 || x >= this.w || y >= this.h) return;
    const i = (y * this.w + x) * 3, p = this.px;
    p[i] += c[0] * a; p[i + 1] += c[1] * a; p[i + 2] += c[2] * a;
  }
  // Fill an SDF shape. f(x, y) -> distance at pixel centre. Box limits the work.
  shape(x0, y0, x1, y1, f, c, alpha = 1) {
    x0 = Math.max(0, Math.floor(x0)); y0 = Math.max(0, Math.floor(y0));
    x1 = Math.min(this.w - 1, Math.ceil(x1)); y1 = Math.min(this.h - 1, Math.ceil(y1));
    for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++) {
      const d = f(x + 0.5, y + 0.5);
      if (d < 0.5) this.blend(x, y, c, clamp(0.5 - d, 0, 1) * alpha);
    }
  }
  // Fill with a per-pixel colour: shader(x, y, d) returns [r,g,b] (or null to skip).
  shade(x0, y0, x1, y1, f, shader, alpha = 1) {
    x0 = Math.max(0, Math.floor(x0)); y0 = Math.max(0, Math.floor(y0));
    x1 = Math.min(this.w - 1, Math.ceil(x1)); y1 = Math.min(this.h - 1, Math.ceil(y1));
    for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++) {
      const d = f(x + 0.5, y + 0.5);
      if (d < 0.5) { const c = shader(x + 0.5, y + 0.5, d); if (c) this.blend(x, y, c, clamp(0.5 - d, 0, 1) * alpha); }
    }
  }
  // Soft additive glow around a shape: falls off over `r` pixels outside it.
  glow(x0, y0, x1, y1, f, c, r, strength) {
    x0 = Math.max(0, Math.floor(x0)); y0 = Math.max(0, Math.floor(y0));
    x1 = Math.min(this.w - 1, Math.ceil(x1)); y1 = Math.min(this.h - 1, Math.ceil(y1));
    for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++) {
      const d = f(x + 0.5, y + 0.5);
      if (d > 0 && d < r * 3) this.add(x, y, c, Math.exp(-d / r) * strength);
    }
  }
  // Draw an RGBA bitmap ({w,h,data: Uint8ClampedArray premultiplied? no: straight alpha}) centred on (x,y).
  sprite(bmp, x, y, o = {}) {
    if (!bmp || !bmp.w) return;
    const sx = o.sx ?? o.scale ?? 1, sy = o.sy ?? o.scale ?? 1, rot = o.rot || 0, alpha = o.alpha ?? 1;
    if (alpha <= 0 || sx === 0 || sy === 0) return;
    const ax = o.ax ?? 0.5, ay = o.ay ?? 0.5; // anchor as a fraction of the bitmap
    const tint = o.tint || null, flash = o.flash || 0, clip = o.clip || null;
    const cs = Math.cos(rot), sn = Math.sin(rot);
    const hw = bmp.w * Math.abs(sx), hh = bmp.h * Math.abs(sy);
    const rad = len(hw, hh) + 2;
    const x0 = Math.max(0, Math.floor(x - rad)), x1 = Math.min(this.w - 1, Math.ceil(x + rad));
    const y0 = Math.max(0, Math.floor(y - rad)), y1 = Math.min(this.h - 1, Math.ceil(y + rad));
    const d = bmp.data, bw = bmp.w, bh = bmp.h;
    for (let py = y0; py <= y1; py++) for (let px = x0; px <= x1; px++) {
      if (clip && !clip(px + 0.5, py + 0.5)) continue;
      const dx = px + 0.5 - x, dy = py + 0.5 - y;
      // inverse rotate, inverse scale, into bitmap space
      const u = (cs * dx + sn * dy) / sx + ax * bw - 0.5, v = (-sn * dx + cs * dy) / sy + ay * bh - 0.5;
      if (u < -1 || v < -1 || u > bw || v > bh) continue;
      const iu = Math.floor(u), iv = Math.floor(v), fu = u - iu, fv = v - iv;
      let r = 0, g = 0, b = 0, a = 0;
      for (let k = 0; k < 4; k++) {
        const qx = iu + (k & 1), qy = iv + (k >> 1);
        if (qx < 0 || qy < 0 || qx >= bw || qy >= bh) continue;
        const wgt = ((k & 1) ? fu : 1 - fu) * ((k >> 1) ? fv : 1 - fv);
        const j = (qy * bw + qx) * 4, aa = d[j + 3] / 255 * wgt;
        r += d[j] * aa; g += d[j + 1] * aa; b += d[j + 2] * aa; a += aa;
      }
      if (a <= 0.002) continue;
      let c = [r / a, g / a, b / a];
      if (tint) c = [c[0] * tint[0] / 255, c[1] * tint[1] / 255, c[2] * tint[2] / 255];
      if (flash) c = mixc(c, [255, 255, 255], flash);
      this.blend(px, py, c, a * alpha);
    }
  }
  // Quantise to RGB565 (what the panel can show) and write to a 2-D canvas context at native size.
  present(ctx) {
    if (!this.img) this.img = ctx.createImageData(this.w, this.h);
    const p = this.px, o = this.img.data, dim = this.dim;
    for (let i = 0, j = 0; i < p.length; i += 3, j += 4) {
      const r = clamp(p[i] * dim, 0, 255) | 0, g = clamp(p[i + 1] * dim, 0, 255) | 0, b = clamp(p[i + 2] * dim, 0, 255) | 0;
      o[j] = (r & 0xf8) | (r >> 5); o[j + 1] = (g & 0xfc) | (g >> 6); o[j + 2] = (b & 0xf8) | (b >> 5); o[j + 3] = 255;
    }
    ctx.putImageData(this.img, 0, 0);
  }
}

// ---------------------------------------------------------------- text and images -> bitmaps
const _txtCache = new Map();
let _tc = null;
// Rasterise text once (white fill, optional outline colour) and keep it. The firmware will use a pre-built
// 4-bit anti-aliased bitmap font of the same face, so what you see here is what the robot draws.
function textBitmap(str, o) {
  const font = o.font || 'Bagel Fat One', size = o.size || 20, weight = o.weight || '400';
  const fill = o.color || '#ffffff', stroke = o.outline || 0, oc = o.oc || '#000000';
  const key = [str, font, size, weight, fill, stroke, oc, o.spacing || 0].join('|');
  let b = _txtCache.get(key);
  if (b) return b;
  if (!_tc) _tc = document.createElement('canvas').getContext('2d', { willReadFrequently: true });
  const ctx = _tc;
  ctx.font = `${weight} ${size}px "${font}"`;
  if ('letterSpacing' in ctx) ctx.letterSpacing = (o.spacing || 0) + 'px';
  const m = ctx.measureText(str);
  const pad = Math.ceil(stroke) + 2;
  const w = Math.ceil(m.width) + pad * 2, h = Math.ceil(size * 1.35) + pad * 2;
  ctx.canvas.width = w; ctx.canvas.height = h;
  ctx.font = `${weight} ${size}px "${font}"`;
  if ('letterSpacing' in ctx) ctx.letterSpacing = (o.spacing || 0) + 'px';
  ctx.textBaseline = 'alphabetic';
  const by = pad + Math.round(size * 1.0);
  if (stroke) { ctx.lineJoin = 'round'; ctx.lineWidth = stroke * 2; ctx.strokeStyle = oc; ctx.strokeText(str, pad, by); }
  ctx.fillStyle = fill; ctx.fillText(str, pad, by);
  const data = ctx.getImageData(0, 0, w, h).data;
  b = { w, h, data: new Uint8ClampedArray(data), base: by };
  _txtCache.set(key, b);
  return b;
}
const _imgCache = new Map();
// Load an image file into a bitmap, optionally resized so its longest side is `max` px.
function loadBitmap(src, max) {
  const key = src + '|' + (max || 0);
  if (_imgCache.has(key)) return _imgCache.get(key);
  const holder = { w: 0, h: 0, data: null, ready: false };
  _imgCache.set(key, holder);
  const im = new Image();
  im.onload = () => {
    let w = im.naturalWidth, h = im.naturalHeight;
    if (max) { const k = max / Math.max(w, h); w = Math.max(1, Math.round(w * k)); h = Math.max(1, Math.round(h * k)); }
    const c = document.createElement('canvas'); c.width = w; c.height = h;
    const x = c.getContext('2d', { willReadFrequently: true });
    x.imageSmoothingQuality = 'high';
    x.drawImage(im, 0, 0, w, h);
    holder.data = x.getImageData(0, 0, w, h).data; holder.w = w; holder.h = h; holder.ready = true;
  };
  im.src = src;
  return holder;
}

// ---------------------------------------------------------------- springs
class Spring {
  constructor(v, k = 220, z = 0.62) { this.x = v; this.t = v; this.v = 0; this.k = k; this.z = z; }
  to(t, k, z) { this.t = t; if (k) this.k = k; if (z !== undefined) this.z = z; }
  snap(v) { this.x = this.t = v; this.v = 0; }
  kick(dv) { this.v += dv; }
  step(dt) {
    // semi-implicit Euler in small steps (stable for stiff springs)
    const n = Math.max(1, Math.ceil(dt / 0.004)), h = dt / n, c = 2 * this.z * Math.sqrt(this.k);
    for (let i = 0; i < n; i++) { this.v += (-this.k * (this.x - this.t) - c * this.v) * h; this.x += this.v * h; }
  }
}

// ---------------------------------------------------------------- skins (the three routes)
// Each skin is a full look: background, eye material, lids, mouth and effect palette. Every animation runs in
// every skin, so the routes are compared in motion, not as stills.
const SKINS = {
  // A: safe. The category at its best: luminous pills on near-black, no pupils; the whole eye moves.
  glow: {
    name: 'Glow', bg: hex('#06090d'), bg2: hex('#0b1016'), grain: 3,
    eye: hex('#7cf5e6'), eyeHi: hex('#e9fffb'), glow: hex('#2fd6c3'),
    pupils: false, lids: 'squash', rim: 0,
    ink: hex('#7cf5e6'), mouth: hex('#7cf5e6'), mouthOn: false,
    text: '#eafffb', textOutline: '#06090d', accent: hex('#ffcf4a'), heart: hex('#ff5d8f'), blush: hex('#ff5d8f'),
    confetti: ['#7cf5e6', '#ffcf4a', '#ff5d8f', '#9b8cff', '#ffffff'],
  },
  // B: bold. Glossy "marble" eyes (iris, pupil, fixed catchlights) set into the robot's own orange skin, so the
  // screen's edge disappears into the case. Pupils, lids and a tiny mouth carry the acting.
  marble: {
    name: 'Marble', bg: hex('#f59a1b'), bg2: hex('#ec8510'), grain: 5,
    eye: hex('#13b5ae'), eyeDark: hex('#06474d'), eyeLight: hex('#7ff0dc'), pupil: hex('#0b0f14'),
    catch: hex('#ffffff'), pupils: true, lids: 'lids', rim: 1.6, rimC: hex('#4a1f04'),
    ink: hex('#4a1f04'), mouth: hex('#4a1f04'), mouthIn: hex('#9c2a1c'), tongue: hex('#ff7a8a'), mouthOn: true,
    text: '#fff6e5', textOutline: '#4a1f04', accent: hex('#13b5ae'), heart: hex('#ff3b5c'), blush: hex('#ff5a4e'),
    confetti: ['#13b5ae', '#fff6e5', '#ff3b5c', '#4a1f04', '#ffe066'],
  },
  // C: wild. A picture-book creature in boiling ink on warm paper: the outlines shimmer on twos like hand
  // drawing, and every shape change is a liquid morph.
  ink: {
    name: 'Ink', bg: hex('#f4ead5'), bg2: hex('#eadcbf'), grain: 10,
    eye: hex('#1d1a22'), catch: hex('#fffaf0'), pupils: false, lids: 'lids', rim: 0, boil: 0.9,
    ink: hex('#1d1a22'), mouth: hex('#1d1a22'), mouthIn: hex('#1d1a22'), tongue: hex('#e8574d'), mouthOn: true,
    text: '#1d1a22', textOutline: '#f4ead5', accent: hex('#e8574d'), heart: hex('#e8574d'), blush: hex('#f08a74'),
    confetti: ['#e8574d', '#1d1a22', '#2f6fd6', '#f2b632', '#3aa66b'],
  },
};
// Background options for the marble skin (and anyone who wants them).
const BACKS = {
  skin: null, // the skin's own
  night: [hex('#141a33'), hex('#0b0f22')],
  black: [hex('#07080b'), hex('#07080b')],
  cream: [hex('#f4ead5'), hex('#eadcbf')],
};

// ---------------------------------------------------------------- layout
function layoutFor(w, h) {
  if (w < h) return { w, h, cx: w / 2, cy: 104, sep: 63, ew: 26, eh: 33, my: 158, capY: 212, portrait: true };
  return { w, h, cx: w / 2, cy: 58, sep: 104, ew: 38, eh: 42, my: 112, capY: 118, portrait: false };
}

// ---------------------------------------------------------------- the face
const EYE_KEYS = {
  dx: 0, dy: 0,      // offset from the eye's home (px)
  sw: 1, sh: 1,      // size multipliers
  round: 0.82,       // 0 = boxy, 1 = fully round
  open: 1,           // blink: 1 open, 0 shut
  lidT: 0, lidTA: 0, // upper lid: closure 0..1, slope (+ = inner corner down: cross)
  lidB: 0, lidBA: 0, // lower lid: closure 0..1, slope
  lidBC: 0,          // lower lid curve (happy cheek push, + = bulges up)
  lookX: 0, lookY: 0,// gaze -1..1
  pupil: 1,          // pupil size
  morph: 0,          // 0 eye -> 1 target shape
  tilt: 0,           // rotation (rad)
  spin: 0,           // spiral rotation (for dizzy)
};
const FACE_KEYS = {
  x: 0, y: 0, sx: 1, sy: 1, rot: 0,
  mShow: 1, mOpen: 0, mCurve: 0.4, mWide: 1, mY: 0,
  blush: 0, sweat: 0, tears: 0, glowAmt: 1,
};
// Expressions: targets for both eyes (b), each eye (L, R), the face (f) and mouth shape.
const EXPR = {
  neutral: { b: {}, f: { mCurve: 0.35, mOpen: 0 } },
  happy: { b: { lidB: 0.42, lidBC: 0.9, sh: 1.02 }, f: { mCurve: 1, mOpen: 0.35, blush: 0.6 } },
  joy: { b: { morph: 1, sh: 1 }, shape: 'arc', f: { mCurve: 1, mOpen: 0.85, blush: 0.8 } },
  love: { b: { morph: 1, sw: 1.05, sh: 1.05 }, shape: 'heart', f: { mCurve: 1, mOpen: 0.2, blush: 1 } },
  starry: { b: { morph: 1, sw: 1.1, sh: 1.1 }, shape: 'star', f: { mCurve: 1, mOpen: 0.9, blush: 0.4 } },
  sleepy: { b: { lidT: 0.55, lidTA: -0.12, lookY: 0.3, pupil: 0.9 }, f: { mCurve: 0.1, mOpen: 0, blush: 0 } },
  sad: { b: { lidT: 0.32, lidTA: -0.42, lookY: 0.45, sh: 0.95 }, f: { mCurve: -0.8, mOpen: 0, mWide: 0.8 } },
  grumpy: { b: { lidT: 0.38, lidTA: 0.42, lidB: 0.12, sh: 0.95 }, f: { mCurve: -0.6, mOpen: 0, mWide: 0.7 } },
  scared: { b: { sw: 0.9, sh: 1.16, pupil: 0.45, lidB: 0, lidT: 0, lidTA: -0.2 }, f: { mCurve: -0.3, mOpen: 0.35, mWide: 0.7, sweat: 1 } },
  surprised: { b: { sw: 1.08, sh: 1.18, pupil: 0.6 }, f: { mCurve: 0, mOpen: 1, mWide: 0.55 } },
  suspicious: { L: { lidT: 0.45, lidB: 0.3, lidTA: 0.15 }, R: { lidT: 0.15, lidB: 0.1, sh: 1.05 }, b: { lookX: 0.7 }, f: { mCurve: -0.15, mOpen: 0, mWide: 0.6 } },
  proud: { b: { lidT: 0.3, lidB: 0.25, lidBC: 0.7, lookY: -0.3 }, f: { mCurve: 0.9, mOpen: 0, mWide: 0.9, y: -3 } },
  curious: { L: { sh: 1.1, sw: 1.05 }, R: { sh: 0.9, lidT: 0.15 }, b: { lookX: -0.3, lookY: -0.2, tilt: 0.12 }, f: { mCurve: 0.2, mOpen: 0.15, mWide: 0.5, rot: 0.1 } },
  dizzy: { b: { morph: 1 }, shape: 'spiral', f: { mCurve: -0.2, mOpen: 0.25, mWide: 0.8 } },
  xx: { b: { morph: 1 }, shape: 'x', f: { mCurve: -0.5, mOpen: 0.5, mWide: 0.6 } },
  focused: { b: { lidT: 0.28, lidB: 0.22, lidTA: 0.1, pupil: 0.8 }, f: { mCurve: 0, mOpen: 0, mWide: 0.5 } },
  hungry: { b: { lookY: 0.4, sh: 1.05, pupil: 1.25, lidT: 0.1, lidTA: -0.25 }, f: { mCurve: -0.2, mOpen: 0.45, mWide: 0.7 } },
};

class Face {
  constructor(scr, opts = {}) {
    this.scr = scr;
    this.L = layoutFor(scr.w, scr.h);
    this.skin = SKINS[opts.skin || 'marble'];
    this.back = opts.back ? BACKS[opts.back] : null;
    this.mouthOn = opts.mouth ?? this.skin.mouthOn;
    this.t = 0;
    this.eyes = [this._eye(-1), this._eye(1)];
    this.f = {};
    for (const k in FACE_KEYS) this.f[k] = new Spring(FACE_KEYS[k], 160, 0.6);
    this.shape = ['eye', 'eye'];
    this.pupilShape = 'round'; // round | slit | bar | none
    this.eyeColor = null;      // override eye colour (animal powers)
    this.extraEyes = 0;        // spider: extra small eyes
    this.compound = 0;         // bee: hex facets
    this.autoBlink = true; this.autoLook = true;
    this._nextBlink = 1.2; this._nextLook = 0.8; this._blinkT = -1;
    this.visible = 1;          // fade the whole face
    this.hidden = false;
    this.mouthShape = 'smile'; // smile | cat | wave | tongue | chomp
    this.chomp = 0;
    this.googly = false;             // white eyes with rolling pupils (physics)
    this.grav = [0, 1];              // gravity in screen space for googly pupils (tilt), in g
    this.gp = [[0, 0, 0, 0], [0, 0, 0, 0]]; // per eye: x, y, vx, vy (px from eye centre)
    this.eyeDark = null; this.eyeLight = null; this.glowColor = null;
    this.r = rng(opts.seed || 7);
    this.expr = 'neutral';
    this.set('neutral', { snap: true });
  }
  _eye(side) { const e = { side }; for (const k in EYE_KEYS) e[k] = new Spring(EYE_KEYS[k], 260, 0.62); return e; }
  // Set an expression. opts: snap, k (stiffness), z (damping), keep (don't reset unspecified keys)
  set(name, opts = {}) {
    const ex = EXPR[name] || EXPR.neutral;
    this.expr = name;
    const k = opts.k, z = opts.z;
    for (let i = 0; i < 2; i++) {
      const e = this.eyes[i], per = i === 0 ? ex.L : ex.R;
      for (const key in EYE_KEYS) {
        if (key === 'open' || key === 'spin') continue;
        if ((key === 'lookX' || key === 'lookY') && opts.keepLook) continue;
        let v = EYE_KEYS[key];
        if (ex.b && key in ex.b) v = ex.b[key];
        if (per && key in per) v = per[key];
        if (opts.keep && !(ex.b && key in ex.b) && !(per && key in per)) continue;
        if (opts.snap) e[key].snap(v); else e[key].to(v, k, z);
      }
    }
    if (ex.shape) { this.shape = [ex.shape, ex.shape]; }
    else if (!opts.keep) { /* morph returns to 0; shape kept until morph is gone */ }
    for (const key in FACE_KEYS) {
      if (key === 'x' || key === 'y' || key === 'sx' || key === 'sy' || key === 'rot' || key === 'mShow' || key === 'glowAmt') {
        if (ex.f && key in ex.f) { if (opts.snap) this.f[key].snap(ex.f[key]); else this.f[key].to(ex.f[key]); }
        else if (!opts.keep && (key === 'rot' || key === 'y' || key === 'x')) this.f[key].to(0);
        continue;
      }
      let v = FACE_KEYS[key];
      if (ex.f && key in ex.f) v = ex.f[key];
      if (opts.snap) this.f[key].snap(v); else this.f[key].to(v);
    }
    this.mouthShape = (ex.f && ex.f.shape) || this.mouthShape;
    return this;
  }
  eyeSet(key, v, which = 2, k, z) { for (let i = 0; i < 2; i++) if (which === 2 || which === i) this.eyes[i][key].to(v, k, z); return this; }
  eyeSnap(key, v, which = 2) { for (let i = 0; i < 2; i++) if (which === 2 || which === i) this.eyes[i][key].snap(v); return this; }
  faceSet(key, v, k, z) { this.f[key].to(v, k, z); return this; }
  look(x, y, k = 420) { for (const e of this.eyes) { e.lookX.to(x, k, 0.75); e.lookY.to(y, k, 0.75); } return this; }
  blink(dur = 0.16) { this._blinkT = 0; this._blinkD = dur; return this; }
  kick(key, v) { this.f[key].kick(v); return this; }
  // A quick squash-and-stretch of the whole face: amt > 0 stretches up first.
  boing(amt = 0.25) { this.f.sy.kick(amt * 14); this.f.sx.kick(-amt * 10); return this; }
  update(dt) {
    this.t += dt;
    // idle life
    if (this.autoBlink) {
      this._nextBlink -= dt;
      if (this._nextBlink <= 0) { this.blink(); this._nextBlink = 1.8 + this.r() * 3.6; if (this.r() < 0.18) this._nextBlink = 0.32; }
    }
    if (this.autoLook) {
      this._nextLook -= dt;
      if (this._nextLook <= 0) {
        const r = this.r();
        if (r < 0.35) this.look(0, 0); // back to you: eye contact is home
        else this.look((this.r() * 2 - 1) * 0.85, (this.r() * 2 - 1) * 0.55);
        this._nextLook = 0.6 + this.r() * 2.2;
      }
    }
    if (this._blinkT >= 0) {
      this._blinkT += dt;
      const p = this._blinkT / this._blinkD;
      // close fast (35%), open slower
      const o = p < 0.35 ? 1 - Ease.inCubic(p / 0.35) : Ease.outCubic((p - 0.35) / 0.65);
      for (const e of this.eyes) e.open.snap(clamp(o, 0, 1));
      if (p >= 1) { this._blinkT = -1; for (const e of this.eyes) e.open.snap(1); }
    }
    for (const e of this.eyes) for (const k in EYE_KEYS) if (k !== 'open') e[k].step(dt);
    for (const k in this.f) this.f[k].step(dt);
    for (const e of this.eyes) e.spin.x += dt * 5;
    if (this.googly) this._googly(dt);
    // morph back to plain eyes resets the shape once it is gone
    for (let i = 0; i < 2; i++) if (this.eyes[i].morph.t === 0 && this.eyes[i].morph.x < 0.02) this.shape[i] = 'eye';
  }

  _googly(dt) {
    const L = this.L;
    for (let i = 0; i < 2; i++) {
      const e = this.eyes[i], g = this.gp[i];
      const w = L.ew * e.sw.x, h = L.eh * e.sh.x, pr = Math.min(w, h) * 0.56;
      const Rx = w * 0.95 - pr, Ry = h * 0.95 - pr;
      const n = Math.max(1, Math.ceil(dt / 0.004)), hs = dt / n;
      for (let k = 0; k < n; k++) {
        g[2] += this.grav[0] * 900 * hs; g[3] += this.grav[1] * 900 * hs;
        const damp = Math.exp(-2.2 * hs); g[2] *= damp; g[3] *= damp;
        g[0] += g[2] * hs; g[1] += g[3] * hs;
        // keep inside the eye (an ellipse), bounce off the rim with a rattle
        const ex = g[0] / Math.max(Rx, 1), ey = g[1] / Math.max(Ry, 1), r = Math.sqrt(ex * ex + ey * ey);
        if (r > 1) {
          g[0] /= r; g[1] /= r;
          const nx = ex / r, ny = ey / r, vn = g[2] * nx + g[3] * ny;
          if (vn > 0) { g[2] -= 1.45 * vn * nx; g[3] -= 1.45 * vn * ny; g[2] *= 0.96; g[3] *= 0.96; }
        }
      }
    }
  }
  // ------------------------------------------------ drawing
  drawBackground() {
    const s = this.skin, b = this.back;
    const top = b ? b[0] : s.bg, bot = b ? b[1] : s.bg2;
    this.scr.fillGradient(top, bot, s.grain);
  }
  // face space -> screen: rotation and scale about the face centre
  _xf() {
    const f = this.f, L = this.L;
    return { cx: L.cx + f.x.x, cy: L.cy + f.y.x, sx: f.sx.x, sy: f.sy.x, cs: Math.cos(f.rot.x), sn: Math.sin(f.rot.x) };
  }
  // where an eye's centre lands on screen
  eyePos(i) {
    const e = this.eyes[i], L = this.L, X = this._xf();
    const lx = e.side * L.sep / 2 + e.dx.x + e.lookX.x * (this.skin.pupils ? 3 : 9);
    const ly = e.dy.x + e.lookY.x * (this.skin.pupils ? 2.5 : 7);
    return { x: X.cx + (X.cs * lx * X.sx - X.sn * ly * X.sy), y: X.cy + (X.sn * lx * X.sx + X.cs * ly * X.sy) };
  }
  draw(opts = {}) {
    if (!opts.noBg) this.drawBackground();
    if (this.hidden || this.visible <= 0) return;
    const f = this.f;
    if (f.blush.x > 0.01) this._drawBlush();
    for (let i = 0; i < 2; i++) this._drawEye(i);
    if (this.extraEyes > 0.01) this._drawExtraEyes();
    if (this.mouthOn && f.mShow.x > 0.02) this._drawMouth();
    if (f.sweat.x > 0.02) this._drawSweat();
    if (f.tears.x > 0.02) this._drawTears();
  }
  _boil(x, y) {
    const b = this.skin.boil; if (!b) return 0;
    const fr = Math.floor(this.t * 8); // on twos at 16 fps -> 8 new drawings a second
    return (vnoise(x * 0.18 + fr * 13.1, y * 0.18 + fr * 7.7) - 0.5) * 2 * b;
  }
  _drawEye(i) {
    const s = this.skin, scr = this.scr, L = this.L, X = this._xf();
    const e = this.eyes[i], side = e.side;
    const P = this.eyePos(i);
    const sx = X.sx, sy = X.sy, rot = this.f.rot.x + e.tilt.x * side;
    const cs = Math.cos(rot), sn = Math.sin(rot);
    const open = e.open.x;
    const w = L.ew * e.sw.x * sx, h0 = L.eh * e.sh.x * sy;
    const morph = clamp(e.morph.x, 0, 1.2), shape = this.shape[i];
    const squash = s.lids === 'squash';
    // blink: glow skin squashes the eye; lidded skins close the lids
    const h = squash ? Math.max(h0 * Math.max(open, 0.07), 2.2) : h0;
    const lidT = squash ? e.lidT.x : 1 - (1 - e.lidT.x) * (open < 1 ? Math.pow(open, 0.9) : 1);
    const lidB = e.lidB.x * (squash ? 1 : Math.max(open, 0.3));
    const r = Math.min(w, h) * e.round.x;
    e._lidT = clamp(lidT, 0, 1); e._lidB = clamp(lidB, 0, 1);
    const lookX = e.lookX.x, lookY = e.lookY.x;
    const self = this;
    const shapeSD = (u, v) => {
      // eye body
      let d = SD.roundBox(u, v, w, h, r);
      // lids (lines with slope and a curve)
      const slT = e.lidTA.x * -side, slB = e.lidBA.x * side;
      const yT = -h + 2 * h * clamp(lidT, 0, 1) + slT * u;
      const dT = (yT - v) / Math.sqrt(1 + slT * slT);
      const curve = e.lidBC.x * (h * 0.6) * (1 - (u * u) / (w * w));
      const yB = h - 2 * h * clamp(lidB, 0, 1) + slB * u - curve;
      const dB = (v - yB) / Math.sqrt(1 + slB * slB);
      d = Math.max(d, dT, dB);
      if (morph > 0.001 && shape !== 'eye' && shape !== 'spiral') {
        let t;
        const S = Math.max(w, h0) ;
        if (shape === 'heart') t = SD.heart(u, v + S * 0.02, S * 0.92);
        else if (shape === 'star') t = SD.star(u, v + S * 0.08, S * 1.08, 0.5);
        else if (shape === 'arc') t = SD.arc(u, v + S * 0.35, S * 0.62, 1.05, S * 0.15);
        else if (shape === 'x') t = Math.min(SD.segment(u, v, -S * 0.55, -S * 0.55, S * 0.55, S * 0.55), SD.segment(u, v, -S * 0.55, S * 0.55, S * 0.55, -S * 0.55)) - S * 0.15;
        else if (shape === 'dot') t = SD.circle(u, v, S * 0.28);
        else if (shape === 'line') t = SD.roundBox(u, v, S * 0.75, S * 0.1, S * 0.1);
        else if (shape === 'ring') t = Math.abs(SD.circle(u, v, S * 0.62)) - S * 0.16;
        else if (shape === 'cross') t = Math.min(SD.roundBox(u, v, S * 0.16, S * 0.7, S * 0.16), SD.roundBox(u, v, S * 0.7, S * 0.16, S * 0.16));
        else t = d;
        d = mix(d, t, clamp(morph, 0, 1));
      }
      return d + self._boil(u * 3 + i * 40, v * 3);
    };
    // pixel -> eye local
    const toLocal = (x, y) => { const dx = x - P.x, dy = y - P.y; return [cs * dx + sn * dy, -sn * dx + cs * dy]; };
    const R = Math.max(w, h0) * 1.45 + 4;
    const x0 = P.x - R, x1 = P.x + R, y0 = P.y - R, y1 = P.y + R;
    const f = (x, y) => { const q = toLocal(x, y); return shapeSD(q[0], q[1]); };
    let base = this.eyeColor || s.eye;
    let colOverride = null;
    if (morph > 0.3 && (shape === 'heart')) colOverride = s.heart;
    if (morph > 0.3 && (shape === 'star')) colOverride = s.accent && s === SKINS.marble ? hex('#ffd23a') : (s.accent || base);
    const mt = smooth(0.2, 0.8, morph);

    if (shape === 'spiral' && morph > 0.01) {
      // dizzy: shrink the eye away, grow a turning spiral
      const k = clamp(morph, 0, 1);
      if (k < 0.99) this._eyeBody(i, f, x0, y0, x1, y1, base, P, w, h, lookX, lookY, toLocal, 1 - k, open);
      const S = Math.max(w, h0) * 0.95 * k, b = S / (2.6 * TAU), sw = Math.max(1.6, S * 0.12);
      const spin = e.spin.x * side;
      const sc = s.pupils ? s.pupil : base;
      scr.shape(P.x - S - 3, P.y - S - 3, P.x + S + 3, P.y + S + 3, (x, y) => {
        const dx = x - P.x, dy = y - P.y, rr = len(dx, dy);
        let ph = Math.atan2(dy, dx) * side + spin;
        ph = ((ph % TAU) + TAU) % TAU;
        const n = Math.round((rr / b - ph) / TAU);
        const dd = Math.abs(rr - b * (ph + TAU * n));
        return Math.max(dd - sw / 2, rr - S) + this._boil(dx * 3, dy * 3);
      }, s === SKINS.marble ? s.pupil : base, 1);
      if (s === SKINS.marble) { /* white disc under the spiral reads better on orange */ }
      return;
    }
    const col = colOverride ? mixc(base, colOverride, mt) : base;
    this._eyeBody(i, f, x0, y0, x1, y1, col, P, w, h, lookX, lookY, toLocal, 1, open, morph, shape, shapeSD);
  }
  _eyeBody(i, f, x0, y0, x1, y1, col, P, w, h, lookX, lookY, toLocal, alpha, open, morph = 0, shape = 'eye', shapeSD) {
    const s = this.skin, scr = this.scr, e = this.eyes[i];
    const plain = morph < 0.5 || shape === 'eye';
    const glowAmt = this.f.glowAmt.x;
    if (this.googly && plain) {
      const white = s === SKINS.glow ? hex('#eafffb') : s === SKINS.ink ? hex('#fffaf0') : hex('#fffdf6');
      const ink = s === SKINS.glow ? hex('#06090d') : s === SKINS.ink ? s.ink : s.pupil;
      if (s === SKINS.glow) scr.glow(x0 - 10, y0 - 10, x1 + 10, y1 + 10, f, s.glow, 5, 0.3 * alpha);
      scr.shape(x0, y0, x1, y1, (x, y) => f(x, y) - (s === SKINS.glow ? 0 : 1.8), s === SKINS.glow ? white : (s.rimC || s.ink), alpha);
      scr.shape(x0, y0, x1, y1, f, white, alpha);
      const g = this.gp[i], pr = Math.min(w, h) * 0.56;
      const pcx = P.x + g[0], pcy = P.y + g[1];
      scr.shape(pcx - pr - 2, pcy - pr - 2, pcx + pr + 2, pcy + pr + 2, (x, y) => Math.max(SD.circle(x - pcx, y - pcy, pr), f(x, y)), ink, alpha);
      scr.shape(pcx - pr, pcy - pr, pcx + pr, pcy + pr, (x, y) => SD.circle(x - pcx + pr * 0.35, y - pcy + pr * 0.38, pr * 0.24), white, alpha);
      this._maybeLash(i, P, w, h, open, toLocal);
      return;
    }
    if (s === SKINS.glow) {
      scr.glow(x0 - 10, y0 - 10, x1 + 10, y1 + 10, f, this.glowColor || s.glow, 5.5, 0.33 * alpha * glowAmt);
      scr.shade(x0, y0, x1, y1, f, (x, y, d) => {
        // a faint scanline and a hot core
        const q = toLocal(x, y);
        const core = 1 - smooth(-h * 0.2, h * 1.1, -d);
        const c = mixc(s.eyeHi, col, clamp(0.55 + core * 0.45, 0, 1));
        return (Math.floor(y) % 3 === 0) ? scalec(c, 0.93) : c;
      }, alpha);
      return;
    }
    if (s.rim && this.eyeColor !== 'none') {
      // dark painted rim, a touch outside the eye
      scr.shape(x0, y0, x1, y1, (x, y) => f(x, y) - s.rim, s.rimC, alpha);
    }
    if (s === SKINS.marble) {
      const pr = Math.min(w, h) * 0.44 * e.pupil.x;
      const px = lookX * (w - pr) * 0.62, py = lookY * (h - pr) * 0.5;
      const dark = this.eyeDark || s.eyeDark, light = this.eyeLight || s.eyeLight;
      const ps = this.pupilShape;
      const comp = this.compound;
      scr.shade(x0, y0, x1, y1, f, (x, y, d) => {
        const q = toLocal(x, y), u = q[0], v = q[1];
        if (!plain) return col;
        // iris: lighter around the pupil, darker towards the edge, and fibres
        const rx = u - px, ry = v - py, rr = len(rx, ry) / Math.max(w, h);
        const ang = Math.atan2(ry, rx);
        const fib = (vnoise(ang * 9 + i * 5, rr * 3) - 0.5) * 0.35;
        let c = mixc(light, col, smooth(0.0, 0.55, rr + fib * 0.4));
        c = mixc(c, dark, smooth(0.45, 1.05, rr) * 0.85 + smooth(-3, 0, d) * 0.5);
        // lower half catches light through the glass
        c = mixc(c, light, smooth(0.2, 0.95, v / h) * 0.18 * (1 - smooth(-2, 0, d)));
        if (comp > 0) {
          const hd = SD.hexCell(u + 40, v + 40, 5.2);
          c = mixc(c, scalec(c, 0.25), smooth(-1.1, 0.3, hd) * comp);
          c = mixc(c, light, smooth(-3.2, -2.2, hd) * (1 - smooth(-2.2, -1.2, hd)) * 0.55 * comp * smooth(-0.6, 0.2, -v / h));
          c = mixc(c, light, smooth(-2.6, -1.4, hd) * (1 - smooth(-1.4, -0.8, hd)) * 0.0);
        }
        return c;
      }, alpha);
      // pupil
      if (plain && ps !== 'none') {
        const pd = (x, y) => {
          const q = toLocal(x, y), u = q[0] - px, v = q[1] - py;
          let d;
          if (ps === 'slit') d = SD.ellipse(u, v, pr * 0.32, pr * 1.55);
          else if (ps === 'bar') d = SD.roundBox(u, v, pr * 1.35, pr * 0.42, pr * 0.4);
          else d = SD.circle(u, v, pr);
          return Math.max(d, f(x, y));
        };
        scr.shape(x0, y0, x1, y1, pd, s.pupil, alpha);
      }
      // catchlights: fixed to the light, they don't follow the gaze. That is what makes it glass.
      const cl = (x, y) => { const q = toLocal(x, y); return Math.max(SD.ellipse(q[0] + w * 0.36, q[1] + h * 0.42, w * 0.2, h * 0.15), f(x, y)); };
      const cl2 = (x, y) => { const q = toLocal(x, y); return Math.max(SD.circle(q[0] - w * 0.3, q[1] - h * 0.32, Math.min(w, h) * 0.09), f(x, y)); };
      const ca = plain ? 1 : 0.85;
      scr.shape(x0, y0, x1, y1, cl, s.catch, alpha * ca * (open > 0.35 ? 1 : 0));
      scr.shape(x0, y0, x1, y1, cl2, s.catch, alpha * ca * 0.9 * (open > 0.35 ? 1 : 0));
      // closed: a lash line where the lids meet
      this._maybeLash(i, P, w, h, open, toLocal);
      return;
    }
    if (s === SKINS.ink) {
      scr.shape(x0, y0, x1, y1, f, col, alpha);
      if (plain && this.eyeColor && this.pupilShape !== 'none') {
        // an animal's coloured eye gets an ink pupil
        const pr = Math.min(w, h) * 0.42 * e.pupil.x, px = lookX * (w - pr) * 0.6, py = lookY * (h - pr) * 0.5;
        scr.shape(x0, y0, x1, y1, (x, y) => { const q = toLocal(x, y); return Math.max(SD.circle(q[0] - px, q[1] - py, pr), f(x, y) + 1); }, s.ink, alpha);
        scr.shape(x0, y0, x1, y1, (x, y) => Math.abs(f(x, y) + 0.8) - 1.1, s.ink, alpha);
      }
      if (plain) {
        const cl = (x, y) => { const q = toLocal(x, y); return Math.max(SD.circle(q[0] + w * 0.3 - lookX * w * 0.25, q[1] + h * 0.38 - lookY * h * 0.2, Math.min(w, h) * 0.24 * e.pupil.x), f(x, y) + 1.5); };
        scr.shape(x0, y0, x1, y1, cl, s.catch, alpha * (open > 0.35 ? 1 : 0));
      }
      this._maybeLash(i, P, w, h, open, toLocal);
    }
  }
  _maybeLash(i, P, w, h, open, toLocal) {
    const e = this.eyes[i], meet = e._lidT + e._lidB;
    const a = Math.max(open < 0.3 ? 1 - open / 0.3 : 0, clamp((meet - 0.82) / 0.16, 0, 1));
    if (a > 0.01) this._lashLine(i, P, w, h, a, toLocal);
  }
  _lashLine(i, P, w, h, a, toLocal) {
    const s = this.skin, e = this.eyes[i];
    // closed eye: a "‿" lid line where the lids meet, low in the eye
    const meetY = clamp(h * ((e._lidT ?? 1) - (e._lidB ?? 0)), -h * 0.6, h * 0.8);
    const ra = w * 0.9, bottom = meetY + 2, c = bottom - ra;
    const col = s === SKINS.glow ? (this.eyeColor || s.eye) : (s.rimC || s.ink);
    this.scr.shape(P.x - w - 4, P.y - h - 4, P.x + w + 4, P.y + h + 4, (x, y) => {
      const q = toLocal(x, y);
      return SD.arc(q[0], -(q[1] - c), ra, 0.95, 1.5) + this._boil(q[0] * 3, q[1] * 3);
    }, col, a);
  }
  _drawExtraEyes() {
    // spider: two pairs of small eyes above the big ones
    const s = this.skin, X = this._xf(), L = this.L, k = this.extraEyes;
    const spots = [[-1.25, -1.35, 0.34], [1.25, -1.35, 0.34], [-0.45, -1.62, 0.26], [0.45, -1.62, 0.26], [-1.6, -0.3, 0.22], [1.6, -0.3, 0.22]];
    for (const [ux, uy, rr] of spots) {
      const x = X.cx + ux * L.sep / 2, y = X.cy + uy * L.eh, r = rr * L.ew * k;
      if (r < 0.5) continue;
      if (s.rim) this.scr.shape(x - r - 3, y - r - 3, x + r + 3, y + r + 3, (px, py) => SD.circle(px - x, py - y, r + s.rim), s.rimC);
      this.scr.shape(x - r - 2, y - r - 2, x + r + 2, y + r + 2, (px, py) => SD.circle(px - x, py - y, r), s.pupils ? s.pupil : s.eye);
      this.scr.shape(x - r, y - r, x + r, y + r, (px, py) => SD.circle(px - x + r * 0.35, py - y + r * 0.35, r * 0.3), s.catch || [255, 255, 255]);
    }
  }
  _drawBlush() {
    const s = this.skin, L = this.L, X = this._xf(), a = this.f.blush.x;
    for (const side of [-1, 1]) {
      const x = X.cx + side * (L.sep / 2 + 6) * X.sx, y = X.cy + L.eh * 1.05 * X.sy;
      const rx = L.ew * 0.62, ry = L.ew * 0.3;
      if (s === SKINS.marble) {
        this.scr.shape(x - rx - 4, y - ry - 4, x + rx + 4, y + ry + 4, (px, py) => SD.ellipse(px - x, py - y, rx, ry) - 2, s.blush, 0.55 * a);
      } else {
        // three little hatch strokes: comic-book blush (glows on the dark skin, inks on paper)
        for (let k = -1; k <= 1; k++) {
          const bx = x + k * 5.5, f = (px, py) => SD.segment(px, py, bx - 1.8, y + 3, bx + 1.8, y - 3) - 1 + this._boil(px * 3, py * 3);
          if (s === SKINS.glow) this.scr.glow(bx - 8, y - 8, bx + 8, y + 8, f, s.blush, 2, 0.5 * a);
          this.scr.shape(bx - 4, y - 5, bx + 4, y + 5, f, s.blush, a);
        }
      }
    }
  }
  _drawMouth() {
    const s = this.skin, L = this.L, X = this._xf(), f = this.f;
    const k = f.mShow.x;
    const mx = X.cx + Math.sin(f.rot.x) * -(L.my - L.cy), my = X.cy + (L.my - L.cy + f.mY.x) * X.sy;
    const W = 11 * f.mWide.x * X.sx * k, curve = f.mCurve.x, open = clamp(f.mOpen.x, 0, 1.2) * k;
    const lw = 1.7;
    const shape = this.mouthShape;
    const scr = this.scr, ink = s.mouth;
    if (shape === 'cat') {
      // "w"
      const f2 = (x, y) => { const u = x - mx, v = y - my; return Math.min(SD.arc(u + W * 0.48, -(v - W * 0.05), W * 0.5, 1.2, lw * 0.6), SD.arc(u - W * 0.48, -(v - W * 0.05), W * 0.5, 1.2, lw * 0.6)); };
      scr.shape(mx - W * 1.2, my - W, mx + W * 1.2, my + W, f2, ink);
      return;
    }
    if (shape === 'wave') {
      const f2 = (x, y) => { const u = x - mx, v = y - my; if (Math.abs(u) > W) return len(Math.abs(u) - W, v); return Math.abs(v - Math.sin(u / W * PI * 2 + this.t * 14) * 1.6) - lw * 0.6; };
      scr.shape(mx - W - 2, my - 5, mx + W + 2, my + 5, f2, ink);
      return;
    }
    if (shape === 'chomp') {
      const c = this.chomp; // 0 shut, 1 wide
      const hh = 2 + c * 8, ww = W * (1.05 - c * 0.15);
      const f2 = (x, y) => SD.roundBox(x - mx, y - my - hh * 0.3, ww, hh, Math.min(hh, 5));
      scr.shape(mx - W - 3, my - 12, mx + W + 3, my + 16, (x, y) => f2(x, y) - lw * 0.7, ink);
      if (s.mouthIn && c > 0.2) scr.shape(mx - W, my - 10, mx + W, my + 14, (x, y) => f2(x, y) + 0.5, s.mouthIn);
      return;
    }
    // parametric smile / frown / open mouth. y(u) is the lip line: + curve lifts the corners (smile).
    const bend = curve * 4.2;
    const q = (u) => 1 - (u * u) / (W * W);
    const yLine = (u) => my + bend * q(u) - bend * 0.5;
    const up = clamp(5 - 4 * curve, 1, 9), down = clamp(5 + 4 * curve, 1, 9);
    const qe = (u) => Math.sqrt(Math.max(0, q(u))); // elliptical profile: D and O shapes
    const yT = (u) => yLine(u) - open * up * qe(u), yB = (u) => yLine(u) + open * down * qe(u);
    const ends = (x, y) => { const u = x - mx, su = Math.sign(u) || 1; return len(Math.abs(u) - W, y - yLine(su * W)); };
    if (open < 0.08) {
      const f2 = (x, y) => {
        const u = x - mx;
        if (Math.abs(u) > W) return ends(x, y) - lw * 0.65;
        const sl = -2 * bend * u / (W * W);
        return Math.abs(y - yLine(u)) / Math.sqrt(1 + sl * sl) - lw * 0.65;
      };
      scr.shape(mx - W - 3, my - 9, mx + W + 3, my + 9, f2, ink);
    } else {
      const inside = (x, y) => { const u = x - mx; if (Math.abs(u) >= W) return ends(x, y); return Math.max(yT(u) - y, y - yB(u)); };
      scr.shape(mx - W - 3, my - 14, mx + W + 3, my + 14, (x, y) => inside(x, y) - lw * 0.7, ink);
      if (s.mouthIn && open > 0.3) scr.shape(mx - W, my - 12, mx + W, my + 12, (x, y) => inside(x, y) + 0.6, s.mouthIn);
      if (shape === 'tongue' && open > 0.2) {
        const ty = yB(0) - 2.5;
        scr.shape(mx - 8, ty - 8, mx + 8, ty + 8, (x, y) => Math.max(SD.ellipse(x - mx, y - ty, W * 0.5, 3.4 + open * 1.5), inside(x, y) + 0.3), s.tongue);
      }
    }
  }
  _drawSweat() {
    const s = this.skin, L = this.L, X = this._xf(), a = this.f.sweat.x;
    const x = X.cx + (L.sep / 2 + L.ew * 0.85) * X.sx, y = X.cy - L.eh * 0.9 + Math.sin(this.t * 3) * 1.2;
    const c = s === SKINS.glow ? hex('#9fd8ff') : s === SKINS.ink ? hex('#2f6fd6') : hex('#bfeaff');
    this.scr.shape(x - 8, y - 12, x + 8, y + 8, (px, py) => SD.drop(px - x, py - y, 3.6 * a, 0.4, 7 * a), c, a);
    this.scr.shape(x - 4, y - 4, x + 4, y + 4, (px, py) => SD.circle(px - x + 1.2, py - y + 0.5, 0.9 * a), [255, 255, 255], a * 0.9);
  }
  _drawTears() {
    const s = this.skin, a = this.f.tears.x;
    const c = s === SKINS.glow ? hex('#9fd8ff') : s === SKINS.ink ? hex('#2f6fd6') : hex('#bfeaff');
    for (let i = 0; i < 2; i++) {
      const P = this.eyePos(i), h = this.L.eh * this.eyes[i].sh.x;
      const x = P.x + this.eyes[i].side * this.L.ew * 0.45, y0 = P.y + h * 0.55;
      const L = 30 * a;
      this.scr.shape(x - 4, y0 - 2, x + 4, y0 + L + 4, (px, py) => SD.segment(px, py, x, y0, x + Math.sin(py * 0.3 + this.t * 6) * 0.6, y0 + L) - 2.2, c, 0.85);
    }
  }
}

// ---------------------------------------------------------------- particles
class FX {
  constructor(scr, face) { this.scr = scr; this.face = face; this.ps = []; this.r = rng(99); this.flash = 0; this.shakeT = 0; this.shakeA = 0; }
  emit(p) {
    const q = Object.assign({ x: 0, y: 0, vx: 0, vy: 0, g: 0, drag: 0, life: 1, age: 0, size: 6, rot: 0, vr: 0, color: [255, 255, 255], type: 'circle', fade: 0.3, grow: 0 }, p);
    if (typeof q.color === 'string') q.color = hex(q.color);
    this.ps.push(q); return q;
  }
  burst(type, x, y, n, o = {}) {
    for (let i = 0; i < n; i++) {
      const a = (o.angle ?? -PI / 2) + (this.r() - 0.5) * (o.spread ?? TAU), sp = (o.speed ?? 80) * (0.5 + this.r() * 0.8);
      const cols = o.colors || [o.color || '#ffffff'];
      this.emit(Object.assign({}, o, { type, x: x + (this.r() - 0.5) * (o.jitter || 0), y: y + (this.r() - 0.5) * (o.jitter || 0), vx: Math.cos(a) * sp, vy: Math.sin(a) * sp, rot: this.r() * TAU, vr: (this.r() - 0.5) * (o.vr ?? 10), size: (o.size || 6) * (0.7 + this.r() * 0.6), color: cols[(this.r() * cols.length) | 0], life: (o.life || 1) * (0.7 + this.r() * 0.6) }));
    }
  }
  update(dt) {
    for (const p of this.ps) {
      p.age += dt; p.vy += p.g * dt; const k = Math.exp(-p.drag * dt); p.vx *= k; p.vy *= k;
      p.x += p.vx * dt; p.y += p.vy * dt; p.rot += p.vr * dt; p.size += p.grow * dt;
    }
    this.ps = this.ps.filter(p => p.age < p.life);
    this.flash = Math.max(0, this.flash - dt * 3);
  }
  draw() {
    const scr = this.scr;
    for (const p of this.ps) {
      const lifeLeft = p.life - p.age;
      const a = clamp(lifeLeft / p.fade, 0, 1) * clamp(p.age / 0.05, 0, 1) * (p.alpha ?? 1);
      const s = p.size, x = p.x, y = p.y, c = p.color, cs = Math.cos(p.rot), sn = Math.sin(p.rot);
      const R = s * 1.6 + 3;
      const loc = (fn) => (px, py) => { const dx = px - x, dy = py - y; return fn(cs * dx + sn * dy, -sn * dx + cs * dy); };
      switch (p.type) {
        case 'heart': scr.shape(x - R, y - R, x + R, y + R, loc((u, v) => SD.heart(u, v, s)), c, a); break;
        case 'star': scr.shape(x - R, y - R, x + R, y + R, loc((u, v) => SD.star(u, v, s, 0.5)), c, a); break;
        case 'spark': // four-point twinkle
          scr.shape(x - R, y - R, x + R, y + R, loc((u, v) => Math.min(SD.ellipse(u, v, s, s * 0.22), SD.ellipse(u, v, s * 0.22, s))), c, a); break;
        case 'confetti': scr.shape(x - R, y - R, x + R, y + R, loc((u, v) => SD.roundBox(u, v * (0.35 + 0.65 * Math.abs(Math.cos(p.age * 9 + p.vr))), s * 0.55, s * 0.32, 0.6)), c, a); break;
        case 'circle': scr.shape(x - R, y - R, x + R, y + R, (px, py) => SD.circle(px - x, py - y, s), c, a); break;
        case 'ring': scr.shape(x - R, y - R, x + R, y + R, (px, py) => Math.abs(SD.circle(px - x, py - y, s)) - (p.width || 1.4), c, a); break;
        case 'puff': scr.shape(x - R, y - R, x + R, y + R, (px, py) => SD.circle(px - x, py - y, s) + vnoise(px * 0.3, py * 0.3) * 2, c, a * 0.9); break;
        case 'drop': scr.shape(x - R, y - R, x + R, y + R, loc((u, v) => SD.drop(u, v, s * 0.5, s * 0.06, s * 1.1)), c, a); break;
        case 'shard': // eggshell piece: a jagged triangle
          scr.shape(x - R, y - R, x + R, y + R, loc((u, v) => { const d1 = SD.segment(u, v, -s, s * 0.5, s * 0.8, s * 0.7); const tri = Math.max(-v - s * 0.6 + Math.abs(u) * 0.9, v - s * 0.7); return Math.min(tri, d1 - 1); }), c, a); break;
        case 'line': scr.shape(x - R * 2, y - R * 2, x + R * 2, y + R * 2, loc((u, v) => SD.segment(u, v, -s, 0, s, 0) - (p.width || 1.2)), c, a); break;
        case 'note': {
          scr.shape(x - R, y - R * 1.5, x + R, y + R, loc((u, v) => Math.min(SD.ellipse(u + s * 0.25, v - s * 0.55, s * 0.42, s * 0.32), SD.roundBox(u - s * 0.12, v + s * 0.1, s * 0.09, s * 0.7, 0.3), SD.roundBox(u + s * 0.08, v + s * 0.7, s * 0.28, s * 0.12, 0.3))), c, a); break;
        }
        case 'text': {
          const b = textBitmap(p.text, { font: p.font, size: p.fontSize || 14, color: p.textColor || '#fff', outline: p.outline || 0, oc: p.oc });
          scr.sprite(b, x, y, { rot: p.rot, alpha: a, scale: s / (p.size0 || s) });
          break;
        }
        case 'sprite': scr.sprite(p.bmp, x, y, { rot: p.rot, alpha: a, scale: s }); break;
      }
    }
    if (this.flash > 0) { const p = scr.px; const k = Math.min(1, this.flash); for (let i = 0; i < p.length; i++) p[i] += (255 - p[i]) * k; }
  }
  clear() { this.ps = []; }
}

// ---------------------------------------------------------------- helpers for scenes
// Draw text at native resolution. y is the vertical centre.
function text(scr, str, x, y, o = {}) {
  const b = textBitmap(str, o);
  scr.sprite(b, x, y, { scale: o.scale ?? 1, sx: o.sx, sy: o.sy, rot: o.rot || 0, alpha: o.alpha ?? 1, ax: o.ax ?? 0.5, ay: o.ay ?? (b.base - (o.size || 20) * 0.36) / b.h });
}

function loadFonts(list) {
  if (typeof document === 'undefined' || !document.fonts) return Promise.resolve();
  return Promise.all(list.map(f => document.fonts.load(f).catch(() => null))).then(() => document.fonts.ready);
}
const FONTS = { shout: 'Bagel Fat One', say: 'Lilita One' };

G.RF = { loadFonts, FONTS, PI, TAU, clamp, mix, len, smooth, hash2, vnoise, rng, Ease, prog, hex, mixc, scalec, SD, Screen, Spring, Face, FX, SKINS, BACKS, EXPR, layoutFor, text, textBitmap, loadBitmap };
})(typeof window !== 'undefined' ? window : globalThis);
