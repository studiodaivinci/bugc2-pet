// The animation catalogue. Each scene is a short script for the face engine.
// prio: 'core' = in the gift build (firmware/pet),
//       'should' = the Should list and everyday life, 'explore' = ideas to see if they're fun.
(function (G) {
'use strict';
const RF = G.RF;
const KID_NAME = 'Friend';  // the firmware's PET_KID_NAME (firmware/lib/petconfig/src/pet_config.h)
const { SD, hex, mixc, clamp, mix, Ease, prog, len, PI, TAU, text, FONTS } = RF;

// ------------------------------------------------------------ shared helpers
const A = (S, k) => { const b = S.assets[k]; return b && b.ready ? b : null; };
function sunburst(S, cx, cy, col, alpha, rot, n = 12) {
  const scr = S.scr, R = 300;
  scr.shape(0, 0, scr.w, scr.h, (x, y) => {
    const a = Math.atan2(y - cy, x - cx) + rot, k = ((a / TAU) * n % 1 + 1) % 1;
    return (Math.abs(k - 0.5) - 0.25) * len(x - cx, y - cy) * 0.6;
  }, col, alpha);
}
function bgFill(S, top, bot, grain = 5) { S.scr.fillGradient(top, bot, grain); }
function contrast(S) { return S.skin === RF.SKINS.ink ? hex('#1d1a22') : S.skin === RF.SKINS.glow ? hex('#eafffb') : hex('#fff6e5'); }
function eyePair(S) { return [S.face.eyePos(0), S.face.eyePos(1)]; }
function animalEyes(S, o) {
  const f = S.face;
  f.eyeColor = o.eye ? hex(o.eye) : null; f.eyeDark = o.dark ? hex(o.dark) : null; f.eyeLight = o.light ? hex(o.light) : null;
  f.glowColor = o.glow ? hex(o.glow) : null;
  f.pupilShape = o.pupil || 'round';
  f.compound = o.compound || 0; f.extraEyes = o.extra || 0;
  for (const e of f.eyes) { e.sw.to(o.sw ?? 1, 320, 0.45); e.sh.to(o.sh ?? 1, 320, 0.45); e.round.to(o.round ?? 0.82); e.pupil.to(o.pupilSize ?? 1); }
}
function plainEyes(S) { animalEyes(S, {}); }
// The power card: the toy pops in on a sunburst, the name below; then it zaps into the eyes.
function powerCard(S, t, key, name, burst) {
  const t0 = 0, tz = 1.45;
  if (t > tz + 0.25) return;
  const scr = S.scr, L = S.L;
  const k = Ease.outBack(prog(t, 0.05, 0.45)), out = prog(t, tz, 0.25);
  sunburst(S, L.cx, 100, hex(burst), 0.22 * (1 - out), t * 0.6, 14);
  const toy = A(S, key);
  const sc = k * (1 - Ease.inCubic(out)) * (1 + Math.sin(t * 7) * 0.03);
  if (toy) scr.sprite(toy, L.cx, 100 - out * 6, { scale: sc, rot: Math.sin(t * 3) * 0.06 });
  else scr.shape(L.cx - 40, 60, L.cx + 40, 140, (x, y) => SD.circle(x - L.cx, y - 100, 34 * sc), hex(burst), 1);
  const nk = Ease.outBack(prog(t, 0.3, 0.4)) * (1 - out);
  let ns = 30; while (ns > 12 && RF.textBitmap(name, { font: FONTS.shout, size: ns, color: S.skin.text, outline: 2.5, oc: S.skin.textOutline }).w > L.w - 4) ns -= 2;
  if (nk > 0.01) text(scr, name, L.cx, 190, { font: FONTS.shout, size: ns, color: S.skin.text, outline: 2.5, oc: S.skin.textOutline, scale: nk, rot: Math.sin(t * 5) * 0.04 });
}
function zap(S, col) {
  const [a, b] = eyePair(S);
  for (const p of [a, b]) for (let i = 0; i < 2; i++) S.fx.emit({ type: 'ring', x: p.x, y: p.y, size: 8 + i * 6, grow: 140, life: 0.4, fade: 0.3, color: col, width: 2 });
  S.face.boing(0.45);
}
function cardPhase(S, t) { S.face.hidden = t < 1.45; }
function stars(S, t, cx, cy, rx, ry, n, col) {
  for (let i = 0; i < n; i++) {
    const a = t * 4 + i * TAU / n, x = cx + Math.cos(a) * rx, y = cy + Math.sin(a) * ry, s = 4 + Math.sin(a) * 1.5;
    S.scr.shape(x - 9, y - 9, x + 9, y + 9, (px, py) => SD.star(px - x, py - y, s, 0.5) - 0, col, 1);
  }
}
function textPop(S, str, x, y, o = {}) {
  S.fx.emit({ type: 'text', text: str, x, y, vx: o.vx || 0, vy: o.vy ?? -30, life: o.life || 0.9, fade: 0.3, size: o.size || 14, size0: o.size || 14, fontSize: o.size || 14, font: o.font || FONTS.shout, textColor: o.color || S.skin.text, outline: o.outline ?? 2, oc: o.oc || S.skin.textOutline, rot: o.rot || 0, vr: o.vr || 0, grow: o.grow || 0 });
}
function hearts(S, n, x, y, o = {}) { S.fx.burst('heart', x, y, n, Object.assign({ speed: 70, g: -40, drag: 1.5, size: 5, life: 1.3, color: S.skin.heart, vr: 2, jitter: 10, spread: 2.2 }, o)); }
function confetti(S, x, y, n, angle, o = {}) { S.fx.burst('confetti', x, y, n, Object.assign({ angle, spread: 0.8, speed: 190, g: 160, drag: 1.2, size: 6, life: 2.2, colors: S.skin.confetti, vr: 14 }, o)); }
const shiver = (S, amt, freq = 70) => { S.face.f.x.snap(Math.sin(S.t * freq) * amt); };

// Procedural egg (used when the toy-art egg is missing) and the egg drawn from the AI image.
function eggSD(x, y, cx, cy, s) { return SD.egg((x - cx) / s, (y - cy - 22 * s) / s, 34, 4) * s; }
function drawEgg(S, cx, cy, o = {}) {
  const scr = S.scr, s = o.scale || 1, rot = o.rot || 0, img = A(S, 'egg');
  if (img) scr.sprite(img, cx, cy, { scale: s * (o.sy ? 1 : 1), sx: s * (o.sx || 1), sy: s * (o.sy || 1), rot, clip: o.clip, ay: 0.5 });
  else {
    const cs = Math.cos(rot), sn = Math.sin(rot);
    scr.shade(cx - 60, cy - 80, cx + 60, cy + 70, (x, y) => { const dx = x - cx, dy = y - cy; const u = cs * dx + sn * dy, v = -sn * dx + cs * dy; if (o.clip && !o.clip(x, y)) return 9; return eggSD(u, v / (o.sy || 1), 0, 0, s); },
      (x, y) => { const h = RF.vnoise(x * 0.45, y * 0.45); const base = mixc(hex('#fbf3e2'), hex('#e9dcc2'), clamp((x - cx + 30) / 70, 0, 1)); return h > 0.78 ? hex('#3f8f8a') : h < 0.12 ? hex('#d9772b') : base; });
  }
}
// Crack lines: deterministic jagged paths from the egg's upper middle.
const CRACKS = [
  [[0, -20], [-6, -12], [2, -6], [-4, 2]],
  [[0, -20], [8, -14], [4, -8], [12, -2], [9, 5]],
  [[-4, 2], [-14, 6], [-10, 12], [-22, 16]],
  [[9, 5], [18, 10], [14, 16], [26, 20], [24, 26]],
];
function drawCracks(S, cx, cy, level, s, rot, col) {
  const scr = S.scr, cs = Math.cos(rot), sn = Math.sin(rot);
  for (let i = 0; i < Math.min(level, CRACKS.length); i++) {
    const pts = CRACKS[i].map(([u, v]) => [cx + (cs * u - sn * v) * s * 1.4, cy + (sn * u + cs * v) * s * 1.4 - 8 * s]);
    for (let j = 0; j < pts.length - 1; j++) {
      const [ax, ay] = pts[j], [bx, by] = pts[j + 1];
      scr.shape(Math.min(ax, bx) - 3, Math.min(ay, by) - 3, Math.max(ax, bx) + 3, Math.max(ay, by) + 3, (x, y) => SD.segment(x, y, ax, ay, bx, by) - 1.1, col, 1);
    }
  }
}
function golfBall(S, x, y, r, sh = 1) {
  const scr = S.scr;
  scr.shape(x - r - 3, y + r * 0.6 - 4, x + r + 3, y + r * 1.4, (px, py) => SD.ellipse(px - x, py - y - r * 0.95, r * 0.9, r * 0.3), hex('#000000'), 0.25 * sh);
  scr.shade(x - r - 2, y - r - 2, x + r + 2, y + r + 2, (px, py) => SD.circle(px - x, py - y, r), (px, py) => {
    const u = px - x, v = py - y, d = (Math.round(u / 3) * 3 - u) ** 2 + (Math.round(v / 3 + (Math.round(u / 3) % 2) * 0.5) * 3 - v - (Math.round(u / 3) % 2) * 1.5) ** 2;
    const shade = 1 - clamp((u + v) / (r * 3), -0.1, 0.35);
    const c = d < 0.9 ? [210, 214, 220] : [250, 250, 248];
    return [c[0] * shade, c[1] * shade, c[2] * shade];
  });
}
function battery(S, x, y, level, col) {
  const scr = S.scr, w = 34, h = 16;
  scr.shape(x - w / 2 - 2, y - h / 2 - 2, x + w / 2 + 6, y + h / 2 + 2, (px, py) => Math.min(Math.abs(SD.roundBox(px - x, py - y, w / 2, h / 2, 4)) - 1.4, SD.roundBox(px - x - w / 2 - 2, py - y, 2, 4, 1)), contrast(S), 1);
  const fw = (w - 6) * clamp(level, 0, 1);
  if (fw > 0.5) scr.shape(x - w / 2 + 3, y - h / 2 + 3, x - w / 2 + 3 + fw, y + h / 2 - 3, (px, py) => SD.roundBox(px - (x - w / 2 + 3 + fw / 2), py - y, fw / 2, h / 2 - 3, 1.5), col, 1);
}
function bolt(S, x, y, s, col, a = 1) {
  S.scr.shape(x - 12 * s, y - 14 * s, x + 12 * s, y + 14 * s, (px, py) => {
    const u = (px - x) / s, v = (py - y) / s;
    const d1 = Math.max(-(u + 0.5) + v * 0.55 - 1, (u + 0.5) - v * 0.55 - 6, v - 1, -v - 11);
    const d2 = Math.max(-(u - 0.5) + v * 0.55 + 6 - 6.5, (u - 0.5) - v * 0.55 - 0.5, -v - 1, v - 11);
    return Math.min(d1, d2) * s;
  }, col, a);
}

const C = { teal: '#13b5ae', gold: '#ffd23a', pink: '#ff3b5c', cream: '#fff6e5', brown: '#4a1f04', night: '#141a33', sky: '#7fd6ff' };

// Where the mouth sits on screen (it follows the face's springs).
function mouthPos(S) { const f = S.face, X = f._xf(), L = S.L; return { x: X.cx + Math.sin(f.f.rot.x) * -(L.my - L.cy), y: X.cy + (L.my - L.cy + f.f.mY.x) * X.sy, sx: X.sx }; }
const tri = (u, c, w) => Math.max(0, 1 - Math.abs(u - c) / w);
const smoothBump = (p, c, w) => { const k = clamp(1 - Math.abs(p - c) / w, 0, 1); return k * k * (3 - 2 * k); };
// The heartbeat: a "lub" and a softer "dub" every BEAT seconds from `from` (the robot thumps its buzzer on each lub).
const BEAT = 0.7;
function beatPulse(t, from) { if (from < 0 || t < from) return 0; const p = (t - from) % BEAT; return smoothBump(p, 0.06, 0.07) + 0.6 * smoothBump(p, 0.24, 0.07); }
// A heart that fills like a glass: an empty slot in darker orange, red liquid with a wobbling surface.
function fillHeart(S, x, y, s, fill, t) {
  const scr = S.scr, R = s * 1.2 + 4, top = y - s, bot = y + s;
  scr.shape(x - R, y - R, x + R, y + R, (px, py) => SD.heart(px - x, py - y, s) - 2.2, hex(C.brown), 1);
  const lvl = bot - (bot - top) * fill, wav = fill > 0.02 && fill < 0.98 ? 1.3 : 0;
  const empty = hex('#d0700c'), hi = hex('#ff6f86'), lo = hex('#d81e45');
  scr.shade(x - R, y - R, x + R, y + R, (px, py) => SD.heart(px - x, py - y, s), (px, py) => {
    const k = clamp(py - (lvl + Math.sin(px * 0.45 + t * 9) * wav) + 0.5, 0, 1);
    return mixc(empty, mixc(hi, lo, clamp((py - top) / (bot - top), 0, 1)), k);
  });
  if (fill > 0.6) scr.shape(x - s, y - s, x, y, (px, py) => SD.ellipse(px - (x - s * 0.45), py - (y - s * 0.42), s * 0.22, s * 0.13), hex('#ffffff'), 0.6 * (fill - 0.6) / 0.4);
}
// A heart monitor's line through the heart: each lub (from `from`, none after `until`) sends a blip out both sides.
function ecgLine(S, cx, cy, from, until, alpha) {
  if (alpha <= 0.01) return;
  const scr = S.scr, w = scr.w, ys = S.data.ecg || (S.data.ecg = new Float32Array(w)), V = 95;
  const n = Math.floor((S.t - from) / BEAT);
  for (let x = 0; x < w; x++) {
    const d = Math.abs(x + 0.5 - cx) - 18;
    let y = cy;
    if (d > 0) for (let k = Math.max(0, n - 1); k <= n; k++) {
      const tb = from + k * BEAT;
      if (tb > until) break;
      const run = (S.t - tb) * V, u = d - run, fade = clamp(1 - run / 75, 0, 1);
      y += (-12 * tri(u, 0, 3) + 4 * tri(u, 4.5, 2.5) + 3 * tri(u, -4.5, 2.5)) * fade;
    }
    ys[x] = y;
  }
  const f = (px, py) => { const i = Math.min(w - 1, Math.max(0, Math.floor(px))), sl = (ys[Math.min(w - 1, i + 1)] - ys[Math.max(0, i - 1)]) / 2; return Math.abs(py - ys[i]) / Math.sqrt(1 + sl * sl); };
  scr.shape(0, cy - 17, w, cy + 7, (px, py) => f(px, py) - 2, hex(C.brown), alpha);
  scr.shape(0, cy - 17, w, cy + 7, (px, py) => f(px, py) - 0.8, contrast(S), alpha);
}
// Stickers in the captions' style (cream, brown outline): a chunky down arrow (tip at y + 16k) and a speaker with an X.
function sticker(S, x0, y0, x1, y1, sd) {
  S.scr.shape(x0, y0, x1, y1, (px, py) => sd(px, py) - 2.4, hex(C.brown), 1);
  S.scr.shape(x0, y0, x1, y1, sd, contrast(S), 1);
}
function downArrow(S, x, y, k) {
  if (k <= 0.01) return;
  sticker(S, x - 13 * k - 4, y - 17 * k - 4, x + 13 * k + 4, y + 16 * k + 4, (px, py) => {
    const u = (px - x) / k, v = (py - y) / k;
    return Math.min(SD.roundBox(u, v + 7, 5, 10, 2), Math.max((15 * Math.abs(u) + 13 * (v - 16)) / 19.849, 1 - v)) * k;
  });
}
function speakerOff(S, x, y, k) {
  if (k <= 0.01) return;
  sticker(S, x - 13 * k - 4, y - 11 * k - 4, x + 16 * k + 4, y + 11 * k + 4, (px, py) => {
    const u = (px - x) / k, v = (py - y) / k;
    const box = SD.roundBox(u + 9, v, 3.5, 4.5, 1), cone = Math.max(Math.max(-(u + 6), u - 2), (Math.abs(v) - 4.5 - (u + 6) * 0.75) / 1.25);
    const x1 = SD.segment(u, v, 6, -4.5, 14, 4.5) - 1.7, x2 = SD.segment(u, v, 6, 4.5, 14, -4.5) - 1.7;
    return Math.min(Math.min(box, cone), Math.min(x1, x2)) * k;
  });
}
// A brass zipper across the mouth, closed from the left up to the slider (k = 0 open, 1 shut).
function zipper(S, k) {
  const scr = S.scr, m = mouthPos(S), W = 18 * m.sx, x0 = m.x - W, y = m.y, xe = x0 + 2 * W * k, brass = hex(C.gold), ink = hex(C.brown);
  if (xe > x0 + 0.5) {
    scr.shape(x0 - 4, y - 5, xe + 1, y + 5, (px, py) => Math.max(Math.abs(py - y) - 3.4, Math.max(x0 - px, px - xe)), ink, 1);
    scr.shape(x0 - 1, y - 5, xe + 1, y + 5, (px, py) => {
      const q = (px - x0) / 4, i = Math.floor(q), u = (q - i - 0.5) * 4, v = py - y - (i % 2 ? 1.1 : -1.1);
      return Math.max(SD.roundBox(u, v, 1.4, 1.7, 0.5), Math.max(x0 - px, px - xe));
    }, brass, 1);
  }
  const body = (px, py) => Math.min(SD.roundBox(px - xe, py - y, 3.4, 4, 1.4), SD.roundBox(px - xe, py - y - 8, 2.4, 4.6, 2));
  scr.shape(xe - 6, y - 7, xe + 6, y + 16, (px, py) => body(px, py) - 1, ink, 1);
  scr.shape(xe - 6, y - 7, xe + 6, y + 16, body, brass, 1);
  scr.shape(xe - 3, y + 7, xe + 3, y + 13, (px, py) => SD.circle(px - xe, py - y - 10, 1.1), ink, 1);
}

// ------------------------------------------------------------ the catalogue
const SCENES = [];
const add = (s) => { s.poster = s.poster ?? Math.min(2, (s.dur || 4) * 0.5); SCENES.push(s); return s; };

// ===== FACE: living eyes and moods (core) =====
add({ id: 'idle', group: 'Face', prio: 'core', title: 'Living eyes', dur: 13, poster: 4.6,
  when: 'Always on: blinks, glances around, and comes back to you. Small moods flicker through.',
  setup(S) {
    const f = S.face;
    S.at(4.2, () => f.set('curious', { keepLook: true }));
    S.at(5.8, () => f.set('neutral', { keepLook: true }));
    S.at(8.0, () => { f.set('happy', { keepLook: true }); f.boing(0.15); });
    S.at(9.2, () => f.set('neutral', { keepLook: true }));
    S.at(11.0, () => { f.autoBlink = false; f.blink(0.7); });
    S.at(11.8, () => { f.autoBlink = true; });
  } });

add({ id: 'googly', group: 'Face', prio: 'core', title: 'Googly eyes', dur: 9, poster: 5,
  when: 'Held in the hand: tilt it and the pupils roll, shake it and they rattle, spin it and they whirl. Drag on the screen to tilt.',
  setup(S) { const f = S.face; f.googly = true; f.autoLook = false; f.set('neutral'); f.gp[0][0] = 0; },
  frame(S, t) {
    const f = S.face; let g;
    if (S.data.drag) g = S.data.drag;
    else if (t < 2.4) g = [Math.sin(t * 2.2) * 0.7, 0.8];
    else if (t < 4) g = [t < 3.2 ? -1.1 : 1.1, 0.3];
    else if (t < 5.6) { g = [(S.r() - 0.5) * 6, (S.r() - 0.5) * 6]; f.f.x.snap(Math.sin(t * 50) * 3); f.faceSet('mOpen', 0.8); f.faceSet('mCurve', 0); }
    else if (t < 7.6) { g = [Math.cos(t * 11) * 1.4, Math.sin(t * 11) * 1.4]; f.faceSet('rot', Math.sin(t * 2) * 0.05); }
    else { g = [0, 1]; f.faceSet('mOpen', 0); f.faceSet('mCurve', 0.4); f.faceSet('rot', 0); }
    f.grav = g;
  } });

const MOOD = (id, title, when, expr, extra = {}) => add(Object.assign({ id: 'mood-' + id, group: 'Moods', prio: 'core', title, dur: 4.4, poster: 2.2, when,
  setup(S) {
    const f = S.face;
    S.at(0.25, () => f.blink());
    S.at(0.7, () => { f.set(expr); if (extra.go) extra.go(S); });
    S.at(3.6, () => { f.set('neutral'); f.blink(); if (extra.end) extra.end(S); });
  } }, extra.def || {}));

MOOD('happy', 'Happy', 'A treat, a pat, a win.', 'happy', { go(S) { S.face.boing(0.3); S.after(0.45, () => S.face.boing(0.3)); S.after(0.15, () => S.face.kick('y', -90)); S.after(0.6, () => S.face.kick('y', -90)); } });
MOOD('love', 'Love', 'Someone says hi, or picks it up gently.', 'love', { def: {
  frame(S, t) {
    if (t > 0.9 && t < 3.5) {
      const ph = (t - 0.9) % 0.9;
      const beat = ph < 0.12 || (ph > 0.24 && ph < 0.34);
      for (const e of S.face.eyes) { e.sw.to(beat ? 1.22 : 1.04, 900, 0.4); e.sh.to(beat ? 1.22 : 1.04, 900, 0.4); }
      if (Math.floor(t * 3) !== Math.floor((t - S.dt) * 3)) hearts(S, 1, S.L.cx + (S.r() - 0.5) * 60, S.L.cy - 20, { speed: 30, angle: -PI / 2, spread: 0.6 });
    }
  } } });
MOOD('sleepy', 'Sleepy', 'Left alone for a while: droops, yawns, nods off.', 'sleepy', { def: {
  frame(S, t) {
    const f = S.face;
    if (t > 1.0 && t < 1.9) { f.faceSet('mOpen', 1.1); f.faceSet('mCurve', 0); f.faceSet('mWide', 0.7); f.eyeSet('lidT', 0.85); f.faceSet('sy', 1.08); }
    else if (t > 1.9 && t < 3.5) { f.faceSet('mOpen', 0); f.faceSet('sy', 1); f.eyeSet('lidT', 0.55 + 0.35 * prog(t, 1.9, 1.2)); f.faceSet('y', 10 * prog(t, 2.0, 1.3)); }
    if (t > 3.2 && t < 3.25) { f.kick('y', -200); f.set('surprised'); }
    if (t > 2.1 && Math.floor(t * 1.6) !== Math.floor((t - S.dt) * 1.6) && t < 3.2) textPop(S, 'z', S.L.cx + 40, S.L.cy - 30, { size: 12 + (t - 2) * 6, vy: -25, vx: 12, life: 1.2 });
  } } });
MOOD('scared', 'Scared', 'A sudden lunge, a loud noise, a hand too fast.', 'scared', { def: {
  frame(S, t) {
    const f = S.face;
    if (t > 0.7 && t < 3.5) { shiver(S, 1.3, 75); f.mouthShape = 'wave'; if (Math.floor(t * 4) !== Math.floor((t - S.dt) * 4)) f.look(S.r() < 0.5 ? -0.9 : 0.9, -0.1, 900); }
    else { f.mouthShape = 'smile'; f.f.x.to(0); }
  } } });
MOOD('dizzy', 'Dizzy', 'After a long spin.', 'dizzy', { def: {
  frame(S, t) {
    const f = S.face;
    if (t > 0.7 && t < 3.5) { f.faceSet('x', Math.cos(t * 4.5) * 6); f.faceSet('y', Math.sin(t * 4.5) * 3); f.faceSet('rot', Math.sin(t * 3) * 0.13); f.mouthShape = 'wave'; }
    else f.mouthShape = 'smile';
  },
  over(S, t) { if (t > 0.9 && t < 3.5) stars(S, t, S.L.cx + S.face.f.x.x, S.L.cy - 50, 38, 7, 3, hex(C.gold)); } } });
MOOD('suspicious', 'Suspicious', 'Someone else picks it up. "Hmm... you\'re not ' + KID_NAME + '."', 'suspicious', { def: {
  frame(S, t) { if (t > 0.9 && t < 3.4) { const x = Math.sin((t - 0.9) * 1.6) > 0 ? 0.8 : -0.8; S.face.look(x, 0, 120); } },
  setup2: 1 }, go(S) { S.after(0.4, () => S.say('Hmm...', { glance: false })); }, end(S) { S.unsay(); } });
MOOD('sad', 'Sad', 'Loses a game, or gets a "no" in training.', 'sad', { go(S) { S.after(0.6, () => S.face.faceSet('tears', 1)); S.after(1.2, () => S.face.kick('y', -60)); S.after(1.9, () => S.face.kick('y', -60)); }, end(S) { S.face.faceSet('tears', 0); } });
MOOD('grumpy', 'Grumpy', 'Woken up from a nap, or teased.', 'grumpy', { def: {
  frame(S, t) {
    if (t > 1.0 && t < 3.4 && Math.floor(t * 5) !== Math.floor((t - S.dt) * 5)) for (const side of [-1, 1]) S.fx.emit({ type: 'puff', x: S.L.cx + side * 52, y: S.L.cy - 44, vx: side * 18, vy: -50, size: 3, grow: 10, life: 0.7, fade: 0.4, color: S.skin === RF.SKINS.glow ? hex('#c9fff8') : hex('#ffffff'), alpha: 0.85 });
    if (t > 1.2 && t < 1.7) shiver(S, 1.5, 60);
  } }, go(S) { S.after(0.5, () => S.say('Hmph!', { shout: true, glance: false })); }, end(S) { S.unsay(); } });
MOOD('surprised', 'Surprised', 'Something new appears in front of its sensor.', 'surprised', { go(S) { S.face.boing(0.7); S.face.kick('y', -120); }, def: {
  over(S, t) { if (t > 0.75 && t < 3.5) { const k = Ease.outBack(prog(t, 0.75, 0.3)); text(S.scr, '!', S.L.cx, S.L.cy - 64 + S.face.f.y.x, { font: FONTS.shout, size: 34, color: S.skin.text, outline: 2.5, oc: S.skin.textOutline, scale: k, rot: Math.sin(t * 12) * 0.12 * (1 - prog(t, 0.9, 0.6)) }); } } } });
MOOD('proud', 'Proud', 'Learned a new name, or beat a record.', 'proud', { go(S) { S.after(0.5, () => { const [a] = eyePair(S); S.fx.emit({ type: 'spark', x: a.x - 12, y: a.y - 18, size: 7, life: 0.7, fade: 0.4, color: hex('#ffffff'), vr: 3 }); }); } });

// ===== HANDLING reactions (core) =====
add({ id: 'wheee', group: 'Handling', prio: 'core', title: 'Wheee! (picked up)', dur: 4.2, poster: 1.4,
  when: 'Lifted off the floor. Motors are already off: it never drives while held.',
  setup(S) {
    const f = S.face;
    S.at(0.2, () => { f.set('surprised'); f.boing(0.8); f.kick('y', -160); });
    S.at(0.55, () => { f.set('joy'); f.faceSet('mOpen', 1); S.say('Wheee!', { shout: true, bounce: true, glance: false }); });
    S.at(3.3, () => { S.unsay(); f.set('happy'); });
  },
  frame(S, t) {
    const f = S.face;
    if (t > 0.55 && t < 3.3) {
      f.faceSet('rot', Math.sin(t * 5) * 0.2); f.faceSet('y', -8 + Math.sin(t * 10) * 2);
      if (Math.floor(t * 6) !== Math.floor((t - S.dt) * 6)) S.fx.emit({ type: S.r() < 0.5 ? 'spark' : 'heart', x: S.L.cx + (S.r() - 0.5) * 120, y: 175, vy: -120 - S.r() * 60, vx: (S.r() - 0.5) * 40, size: 4 + S.r() * 2, life: 0.9, fade: 0.3, color: S.r() < 0.5 ? S.skin.heart : hex('#ffffff'), vr: 4 });
    } else f.faceSet('rot', 0);
  } });

add({ id: 'phew', group: 'Handling', prio: 'core', title: 'Phew (put down)', dur: 4.2, poster: 1.6,
  when: 'Set back on the floor.',
  setup(S) {
    const f = S.face;
    f.f.y.snap(-26); f.set('scared', { snap: true }); f.f.sweat.snap(1);
    S.at(0.35, () => f.faceSet('y', 0, 500, 0.5));
    S.at(0.55, () => { f.kick('sy', -9); f.kick('sx', 7); for (const side of [-1, 1]) S.fx.burst('puff', S.L.cx + side * 40, S.L.cy + 62, 3, { angle: side < 0 ? PI : 0, spread: 0.6, speed: 60, drag: 4, size: 4, grow: 8, life: 0.6, color: S.skin === RF.SKINS.glow ? hex('#44706a') : hex('#ffffff') }); });
    S.at(0.9, () => { f.set('happy'); f.eyeSet('lidT', 0.75); f.faceSet('mOpen', 0.5); f.faceSet('mCurve', 0); f.faceSet('mWide', 0.5); S.say('phew...', { glance: false }); });
    S.at(1.3, () => { f.faceSet('sweat', 0); const x = S.L.cx + (S.L.sep / 2 + S.L.ew * 0.85), y = S.L.cy - S.L.eh * 0.9; S.fx.emit({ type: 'drop', x, y, vy: 20, g: 500, size: 7, life: 0.9, color: hex('#bfeaff') }); });
    S.at(2.0, () => { f.faceSet('mOpen', 0); f.faceSet('mCurve', 0.8); f.faceSet('mWide', 1); });
    S.at(3.4, () => { S.unsay(); f.set('neutral'); });
  } });

add({ id: 'shaken', group: 'Handling', prio: 'core', title: 'Dizzy (shaken)', dur: 6.2, poster: 2.6,
  when: 'Shaken hard: the pupils rattle, then spiral eyes, stars, and hiccups.',
  setup(S) {
    const f = S.face; f.googly = true; f.autoLook = false;
    S.at(1.7, () => { f.googly = false; f.set('dizzy'); });
    S.at(3.4, () => { f.kick('y', -150); f.boing(0.3); textPop(S, 'hic!', S.L.cx + 34, S.L.cy - 52, { size: 14, vy: -20 }); });
    S.at(4.3, () => { f.kick('y', -150); f.boing(0.3); textPop(S, 'hic!', S.L.cx - 34, S.L.cy - 52, { size: 14, vy: -20 }); });
    S.at(5.1, () => f.set('neutral'));
  },
  frame(S, t) {
    const f = S.face;
    if (t < 1.7) { f.grav = [(S.r() - 0.5) * 7, (S.r() - 0.5) * 7]; f.f.x.snap(Math.sin(t * 55) * 4); f.f.y.snap(Math.cos(t * 41) * 3); f.faceSet('mOpen', 0.9); f.faceSet('mCurve', 0); }
    else if (t < 5.1) { f.faceSet('x', Math.cos(t * 4.5) * 5); f.faceSet('rot', Math.sin(t * 3) * 0.12); f.mouthShape = 'wave'; }
    else { f.mouthShape = 'smile'; f.faceSet('x', 0); f.faceSet('rot', 0); }
  },
  over(S, t) { if (t > 1.8 && t < 5.1) stars(S, t, S.L.cx + S.face.f.x.x, S.L.cy - 50, 38, 7, 3, hex(C.gold)); } });

add({ id: 'turtle', group: 'Handling', prio: 'core', title: 'Turtle (upside down)', dur: 5.2, poster: 1.8,
  when: 'Flipped on its back. Help! I\'m stuck! until someone turns it over.',
  setup(S) {
    const f = S.face; f.autoLook = false;
    S.at(0.2, () => { f.set('scared'); f.mouthShape = 'wave'; S.say('Help!\nI\'m stuck!', { glance: false, wobble: true }); });
    S.at(3.5, () => { S.unsay(); f.set('joy'); f.mouthShape = 'smile'; f.boing(0.6); f.faceSet('rot', 0); f.faceSet('x', 0); });
    S.at(3.8, () => S.say('Thanks!', { shout: true, glance: false }));
    S.at(4.8, () => { S.unsay(); f.set('happy'); });
  },
  frame(S, t) { const f = S.face; if (t > 0.2 && t < 3.5) { f.faceSet('rot', Math.sin(t * 16) * 0.12, 900, 0.3); if (Math.floor(t * 5) !== Math.floor((t - S.dt) * 5)) f.look((S.r() - 0.5) * 2, (S.r() - 0.5) * 1.2, 900); } } });

const patOnce = (S, n) => {
  const f = S.face;
  f.kick('sy', -10); f.kick('sx', 6); f.set(n >= 3 ? 'love' : 'joy');
  f.f.blush.to(1);
  S.data.gig = S.t;
  textPop(S, n >= 3 ? 'hee hee!' : 'hee!', S.L.cx + (n % 2 ? 30 : -30), S.L.cy - 52, { size: 13, vy: -35, rot: n % 2 ? 0.2 : -0.2 });
  hearts(S, n >= 3 ? 8 : 2, S.L.cx, S.L.cy - 40);
  S.after(n >= 3 ? 1.4 : 0.6, () => { if (S.t - S.data.gig >= (n >= 3 ? 1.35 : 0.55)) f.set('happy'); });
};
add({ id: 'pats', group: 'Handling', prio: 'core', title: 'Pats', dur: 5, poster: 2.9,
  when: 'Tap its head: a giggle and hearts. Three pats in a row and it melts. Click the screen to pat.',
  setup(S) { S.data.n = 0; for (const t of [0.6, 1.5, 2.4]) S.at(t, () => { S.data.n++; patOnce(S, S.data.n); }); S.at(4.4, () => S.face.set('neutral')); },
  frame(S, t) { if (S.data.gig !== undefined && t - S.data.gig < 0.45) S.face.f.y.snap(Math.sin(t * 60) * 1.6); },
  tap(S) { S.data.n++; patOnce(S, S.data.n); } });

// ===== HATCHING (core) =====
add({ id: 'egg-sleep', group: 'Hatching', prio: 'core', title: 'The egg', dur: 6, poster: 2,
  when: 'Unhatched, in the box. It breathes, wobbles, and snores.',
  setup(S) { S.face.hidden = true; S.at(0.6, () => S.say('Tap me to help\nme hatch!', { glance: false })); },
  under(S, t) {
    const wob = Math.sin(t * 1.4) * 0.05 + (t > 3 && t < 3.8 ? Math.sin((t - 3) * 30) * 0.08 * (1 - prog(t, 3, 0.8)) : 0);
    drawEgg(S, S.L.cx, 106 + Math.sin(t * 1.8) * 1, { scale: 1, sy: 1 + Math.sin(t * 1.8) * 0.015, rot: wob });
    if (Math.floor(t / 1.3) !== Math.floor((t - S.dt) / 1.3)) textPop(S, 'z', S.L.cx + 30, 52, { size: 13, vx: 14, vy: -18, life: 1.4 });
  } });

add({ id: 'egg-crack', group: 'Hatching', prio: 'core', title: 'Cracking', dur: 6.6, poster: 4.4,
  when: 'Every tap or shake cracks it more. Something peeks out. Click the screen to tap.',
  setup(S) { S.face.hidden = true; S.data.lv = 0; S.data.kick = -9; for (const t of [0.8, 2.0, 3.2, 4.4]) S.at(t, () => S.player.tap()); },
  tap(S) {
    const lv = ++S.data.lv; S.data.kick = S.t;
    S.fx.burst('shard', S.L.cx, 80, 3 + lv, { speed: 90, g: 300, size: 3, life: 0.8, colors: ['#fbf3e2', '#e9dcc2', '#3f8f8a'], vr: 12, jitter: 20 });
    S.say(['Again!', 'Harder!', 'Almost!', 'Almost...!', 'Almost...!!'][Math.min(lv - 1, 4)], { shout: true, glance: false });
  },
  under(S, t) {
    const k = t - S.data.kick, lv = S.data.lv;
    const rot = k < 0.6 ? Math.sin(k * 40) * 0.12 * (1 - k / 0.6) : Math.sin(t * 1.4) * 0.03;
    const cx = S.L.cx, cy = 106;
    drawEgg(S, cx, cy, { rot, sy: 1 - (k < 0.15 ? 0.05 : 0) });
    drawCracks(S, cx, cy, lv, 1, rot, hex('#3a2410'));
    if (lv >= 3) {
      // a dark hole in the shell, and two eyes blinking inside it
      const hx = cx + Math.cos(rot) * -3 - Math.sin(rot) * -14, hy = cy - 14 + Math.sin(rot) * -3;
      const open = (Math.floor(t * 1.7) % 4 === 0) ? 0.2 : 1;
      S.scr.shape(hx - 18, hy - 12, hx + 18, hy + 12, (x, y) => SD.ellipse(x - hx, y - hy, 15, 8 + (lv - 3) * 2), hex('#120a04'), 1);
      for (const s of [-1, 1]) S.scr.shape(hx + s * 6 - 4, hy - 5, hx + s * 6 + 4, hy + 5, (x, y) => SD.ellipse(x - hx - s * 6 - Math.sin(t) * 2, y - hy, 2.4, 3.4 * open), S.skin === RF.SKINS.ink ? hex('#fffaf0') : hex('#7ff0dc'), 1);
    }
  } });

add({ id: 'egg-hatch', group: 'Hatching', prio: 'core', title: 'Hatch!', dur: 7.4, poster: 4.2,
  when: 'The last tap: it bursts, and the eyes open for the very first time.',
  setup(S) {
    const f = S.face; f.hidden = true; f.autoBlink = false; f.autoLook = false; S.data.burst = 0.9;
    for (const e of f.eyes) e.open.snap(0);
    S.at(0.9, () => {
      S.fx.flash = 1.2; f.hidden = false; f.f.sx.snap(0.6); f.f.sy.snap(0.6); f.faceSet('sx', 1, 260, 0.35); f.faceSet('sy', 1, 260, 0.35);
      S.fx.burst('shard', S.L.cx, 70, 12, { speed: 170, g: 420, size: 5, life: 1.4, colors: ['#fbf3e2', '#e9dcc2', '#3f8f8a', '#d9772b'], vr: 14, jitter: 30, angle: -PI / 2, spread: 2.6 });
      S.fx.burst('spark', S.L.cx, 90, 8, { speed: 120, drag: 2, size: 5, life: 0.8, color: '#ffffff' });
    });
    S.at(4.0, () => f.blink(0.2)); S.at(4.35, () => f.blink(0.2));
    S.at(4.9, () => f.look(-0.9, 0.1, 300)); S.at(5.5, () => f.look(0.9, 0.1, 300));
    S.at(6.1, () => { f.look(0, 0, 300); f.set('curious', { keepLook: true }); });
    S.at(6.6, () => textPop(S, '?', S.L.cx + 46, S.L.cy - 48, { size: 22, vy: -12, life: 0.9 }));
  },
  frame(S, t) {
    const f = S.face;
    if (t < 0.9) f.f.x.snap(0);
    if (t > 1.6 && t < 3.6) { const o = Ease.inOut(prog(t, 1.9, 1.5)); for (const e of f.eyes) e.open.snap(o < 0.98 ? o * (1 - 0.15 * Math.sin(t * 9) * (1 - o)) : 1); }
    if (t > 3.6 && f._blinkT < 0) for (const e of f.eyes) e.open.snap(1);
  },
  under(S, t) {
    const cx = S.L.cx;
    if (t < 0.9) {
      const sh = Math.sin(t * 60) * 0.06 * prog(t, 0, 0.8);
      drawEgg(S, cx, 106, { rot: sh }); drawCracks(S, cx, 106, 4, 1, sh, hex('#3a2410'));
    }
  },
  mid(S, t) {
    // the bottom of the shell stays on like a cup, then falls away
    if (t >= 0.9 && t < 6.0) {
      const drop = t > 5.2 ? Ease.inCubic(prog(t, 5.2, 0.7)) * 160 : 0;
      const cy = 214 + drop, zig = (x) => 172 + drop + Math.abs(((x * 0.16) % 2) - 1) * 9;
      drawEgg(S, S.L.cx, cy, { scale: 1.65, clip: (x, y) => y > zig(x) });
    }
  } });

add({ id: 'hello-kid', group: 'Hatching', prio: 'core', title: 'Are you... ' + KID_NAME + '?', dur: 7, poster: 2.2,
  when: 'Right after hatching. The kid presses the big button and it imprints on them. Press A (or click) to answer.',
  setup(S) {
    const f = S.face;
    S.at(0.3, () => { f.set('curious'); S.say('Are you...', { glance: false }); });
    S.at(1.5, () => { f.set('neutral'); for (const e of f.eyes) { e.pupil.to(1.35); e.sh.to(1.1); } f.faceSet('mOpen', 0.3); f.faceSet('mCurve', 0); f.faceSet('mWide', 0.5); f.kick('y', -60); S.say('...' + KID_NAME + '?', { shout: true, glance: false }); });
    S.at(3.3, () => S.player.button());
  },
  button(S) {
    if (S.data.done) return; S.data.done = 1;
    const f = S.face; f.set('love'); f.boing(0.7); hearts(S, 14, S.L.cx, S.L.cy, { speed: 140, spread: TAU });
    S.say('You\'re my\nperson!', { glance: false });
    S.after(2.6, () => f.set('happy'));
  },
  tap(S) { this.button(S); } });

const HEART_Y = 34;
add({ id: 'hold-button', group: 'Hatching', prio: 'core', title: 'Hold my button!', dur: 6.4, poster: 2.3,
  when: 'Right after "Are you... ' + KID_NAME + '?". While they hold the big button the heart fills and beats with the buzzer, then hearts: "You\'re my person!" The robot sends "hold" on the press and "done" 1.4 s later ("release" if they let go). Press A.',
  setup(S) {
    const f = S.face, d = S.data;
    f.autoLook = false;
    d.fill = 0; d.holding = false; d.holdAt = -1; d.doneAt = -1; d.until = 1e9; d.beatN = -1; d.wobble = false;
    S.at(0.2, () => {
      S.say('Hold my\nbutton!', { glance: false });
      if (d.holdAt < 0 && d.doneAt < 0) { for (const e of f.eyes) e.pupil.to(1.25); f.look(0, 0.85, 300); }  // looking down at its button
    });
    S.at(1.3, () => S.player.event('hold'));
    S.at(2.7, () => S.player.event('done'));
  },
  frame(S, t) {
    const d = S.data, f = S.face;
    if (d.holding) {
      d.fill = Math.min(1, d.fill + S.dt / 1.3);
      const n = Math.floor((S.t - d.holdAt) / BEAT);
      if (n !== d.beatN) {
        d.beatN = n;
        S.fx.emit({ type: 'ring', x: S.L.cx, y: HEART_Y, size: 15, grow: 70, life: 0.5, fade: 0.45, color: S.skin.heart, width: 1.8 });
        for (const e of f.eyes) e.pupil.kick(4);
        f.kick('sx', 0.8); f.kick('sy', 0.9);
      }
    } else if (d.doneAt < 0) d.fill = Math.max(0, d.fill - S.dt * 2);
  },
  over(S, t) {
    const d = S.data;
    let sc = 1 + 0.2 * (d.holding ? beatPulse(S.t, d.holdAt) : 0);
    if (d.doneAt >= 0) { const k = prog(t, d.doneAt, 0.4); sc = 1 + Math.sin(k * PI) * 0.4 + Math.sin(t * 3) * 0.04 * k; }
    if (d.holdAt >= 0) ecgLine(S, S.L.cx, HEART_Y, d.holdAt, d.until, prog(t, d.holdAt, 0.25) * (d.doneAt >= 0 ? 1 - prog(t, d.doneAt + 0.5, 0.5) : 1));
    fillHeart(S, S.L.cx, HEART_Y, 16 * sc * Ease.outBack(prog(t, 0.05, 0.4)), d.fill, t);
  },
  event(S, name) {
    const f = S.face, d = S.data;
    if (name === 'hold' && !d.holding && d.doneAt < 0) {
      d.holding = true; d.holdAt = S.t; d.until = 1e9; d.beatN = -1;
      f.set('happy'); f.look(0, 0, 300); for (const e of f.eyes) e.pupil.to(1.3);
      if (d.wobble) { d.wobble = false; S.say('Hold my\nbutton!', { glance: false }); }
    } else if (name === 'release' && d.holding) {
      d.holding = false; d.until = S.t; d.wobble = true;
      f.set('neutral'); for (const e of f.eyes) e.pupil.to(1.25); f.look(0, 0.85, 300);
      S.say('Hold my\nbutton!', { glance: false, wobble: true });
    } else if (name === 'done' && d.doneAt < 0) {
      d.doneAt = S.t; d.holding = false; d.until = Math.min(d.until, S.t); d.fill = 1;
      f.set('love'); f.boing(0.7);
      hearts(S, 14, S.L.cx, S.L.cy, { speed: 140, spread: TAU });
      hearts(S, 6, S.L.cx, HEART_Y, { speed: 90, spread: TAU });
      S.say('You\'re my\nperson!', { glance: false });
      S.after(2.6, () => f.set('happy'));
    }
  },
  button(S) { const d = S.data; if (d.holding || d.doneAt >= 0) return; this.event(S, 'hold'); S.after(1.4, () => this.event(S, 'done')); },
  tap(S) { this.button(S); } });

add({ id: 'say-hi', group: 'Hatching', prio: 'core', title: 'Say hi (listening)', dur: 6.4, poster: 2.8,
  when: 'Hold the button and talk. It listens with the motors stopped (the mic shares a pin with them).',
  setup(S) {
    const f = S.face;
    S.at(0.3, () => S.say('Hold my button\nand say hi!'));
    S.at(1.9, () => { S.unsay(); f.set('surprised'); f.eyeSet('lookY', -0.25); f.faceSet('mOpen', 0); S.data.listen = S.t; });
    S.at(4.3, () => { S.data.listen = 0; f.set('love'); f.boing(0.5); hearts(S, 10, S.L.cx, S.L.cy, { speed: 120, spread: TAU }); S.say('Hi ' + KID_NAME + '!', { shout: true, glance: false }); });
    S.at(5.8, () => { S.unsay(); f.set('happy'); });
  },
  over(S, t) {
    if (!S.data.listen) return;
    const lt = t - S.data.listen, scr = S.scr, L = S.L, col = contrast(S);
    // sound rings out of both sides, like ears
    for (let i = 0; i < 3; i++) {
      const r = ((lt * 30 + i * 12) % 36) + 6, a = 1 - r / 42;
      for (const s of [-1, 1]) { const x = L.cx + s * 58, y = L.cy; scr.shape(x - r - 2, y - r - 2, x + r + 2, y + r + 2, (px, py) => Math.max(Math.abs(SD.circle(px - x, py - y, r)) - 1.2, s * (x - px) + 2), col, a * 0.8); }
    }
    // level meter: 9 bars driven by a voice-like envelope
    const env = (Math.sin(lt * 9) * 0.5 + 0.5) * (Math.sin(lt * 2.3) * 0.4 + 0.6);
    for (let i = 0; i < 9; i++) {
      const x = L.cx + (i - 4) * 10, hgt = 4 + 26 * env * (0.4 + 0.6 * RF.vnoise(i * 3.1, lt * 7));
      scr.shape(x - 4, 212 - hgt / 2 - 1, x + 4, 212 + hgt / 2 + 1, (px, py) => SD.roundBox(px - x, py - 212, 3, hgt / 2, 3), i % 2 ? S.skin.accent || col : col, 1);
    }
  } });

add({ id: 'birthday', group: 'Hatching', prio: 'core', title: 'Happy Birthday!', dur: 10, poster: 6.2,
  when: '"It\'s your birthday? It\'s MY birthday too!" Plays Happy Birthday with the kid\'s name.',
  setup(S) {
    const f = S.face;
    S.data.flames = [[-27.2, -50.5], [-18.2, -53.4], [-13.5, -44.4], [-4.5, -54.3], [12.4, -52.9], [23.4, -47.2]];
    S.at(0.3, () => { f.set('surprised'); f.boing(0.5); S.say('It\'s your\nbirthday?', { glance: false }); });
    S.at(1.9, () => { f.set('starry'); f.boing(0.8); S.say('It\'s MY birthday\ntoo!', { glance: false }); confetti(S, 0, 240, 26, -1.2); confetti(S, S.L.w, 240, 26, -PI + 1.2); });
    S.at(3.7, () => { S.unsay(); f.faceSet('y', 6); f.faceSet('sx', 0.78); f.faceSet('sy', 0.78); f.set('joy', { keep: true }); });
    S.at(4.1, () => S.say('Happy Birthday\n' + KID_NAME + '!', { shout: true, glance: false, y: 32, size: 24, bounce: true }));
  },
  frame(S, t) {
    if (t > 4.0) {
      if (Math.floor(t * 9) !== Math.floor((t - S.dt) * 9)) S.fx.emit({ type: 'confetti', x: S.r() * S.L.w, y: -6, vx: (S.r() - 0.5) * 30, vy: 40 + S.r() * 30, g: 20, size: 6, life: 6, rot: S.r() * TAU, vr: 8, color: S.skin.confetti[(S.r() * 5) | 0] });
      if (Math.floor(t * 2) !== Math.floor((t - S.dt) * 2)) { S.face.kick('y', -70); S.fx.emit({ type: 'note', x: S.L.cx + (S.r() - 0.5) * 100, y: 140, vy: -40, vx: (S.r() - 0.5) * 20, size: 8, life: 1.4, color: contrast(S), rot: (S.r() - 0.5) * 0.4 }); }
    }
  },
  over(S, t) {
    if (t < 3.7) return;
    const cake = A(S, 'cake'), k = Ease.outBack(prog(t, 3.7, 0.5)), y = 240 - 52 * k;
    if (cake) S.scr.sprite(cake, S.L.cx, y, { scale: 1 });
    // candle flames, flickering (drawn on top of the toy's unlit candles)
    const fl = S.data.flames || [];
    for (const [fx, fy] of fl) {
      const x = S.L.cx + fx, yy = y + fy, h = 4.5 + Math.sin(t * 17 + fx) * 1.2;
      S.scr.glow(x - 10, yy - 14, x + 10, yy + 8, (px, py) => SD.drop(px - x, py - yy, 2, 0.2, h), hex('#ffb340'), 3, 0.5);
      S.scr.shape(x - 4, yy - 10, x + 4, yy + 4, (px, py) => SD.drop(px - x, py - yy, 2, 0.2, h), hex('#ffd76a'), 1);
      S.scr.shape(x - 2, yy - 4, x + 2, yy + 2, (px, py) => SD.drop(px - x, py - yy + 0.5, 0.9, 0.1, h * 0.45), hex('#ffffff'), 1);
    }
  } });

add({ id: 'floor-ask', group: 'Hatching', prio: 'core', title: 'Put me on the floor!', dur: 6, poster: 2,
  when: 'After the song (step 5 of the hatching): it asks to go down for its first tricks, hopping and pointing at the floor. The robot sends "putdown" once it is level and still on the floor. Try Put down.',
  setup(S) {
    const f = S.face;
    f.autoLook = false; S.data.down = -1;
    S.at(0.2, () => { if (S.data.down >= 0) return; f.set('neutral'); f.faceSet('mCurve', 1); f.faceSet('mOpen', 0.45); f.faceSet('blush', 0.5); for (const e of f.eyes) e.pupil.to(1.2); f.look(0, 0.9, 300); S.say('Put me on\nthe floor!', { glance: false, y: 40 }); });
    S.at(3.4, () => S.player.event('putdown'));
  },
  frame(S, t) { if (S.data.down < 0 && t > 0.2 && Math.floor(t * 1.1) !== Math.floor((t - S.dt) * 1.1)) { S.face.kick('y', -70); S.face.boing(0.15); } },
  over(S, t) {
    const d = S.data, k = d.down < 0 ? Ease.outBack(prog(t, 0.3, 0.35)) : 1 - Ease.inCubic(prog(t, d.down, 0.25));
    downArrow(S, S.L.cx, 194 + Math.abs(Math.sin(t * 5.5)) * 5, k);
  },
  event(S, name) {
    if ((name !== 'putdown' && name !== 'floor') || S.data.down >= 0) return;
    const f = S.face;
    S.data.down = S.t;
    S.unsay(); f.set('starry'); f.boing(0.8); f.look(0, 0, 300);
    S.fx.burst('spark', S.L.cx, S.L.cy, 10, { speed: 130, spread: TAU, size: 5, life: 0.8, color: C.gold });
    S.after(0.3, () => S.say('Watch\nthis!', { shout: true, glance: false }));
    S.after(1.8, () => f.set('happy'));
  } });

// ===== ANIMAL POWERS (core) =====
// In the hand: the card (toy + name), then the eyes change and the true fact shows, big (22 px, a few words a
// screen, three short lines at most; the big button skips ahead). Then "Put me down!" until the robot is on the floor
// ("putdown" or "floor"; it may come early), and the trick plays. Hand powers (o.hand) don't ask.
// The trick (frame, under, over, o.trick's cues) runs on its own clock, trickT, which reads 1.45 as the trick starts,
// so its timings are the ones it had when it followed the card directly. The animal's look (background, mid, look)
// runs on the scene clock from the zap on. o.end is the trick's end on its clock.
const FACT_Y = 201, FACT_SIZE = 22, FACT_READ = 2.6, ASK_DEMO = 1.6, GO_DELAY = 0.4;
const trickT = (S) => S.t - S.data.t0 + 1.45;
function powerGo(S) { const d = S.data; d.ph = 'go'; d.goAt = S.t + GO_DELAY; S.unsay(); S.face.look(0, 0, 300); }
const POWER = (id, name, burst, eyes, o) => {
  const pre = 1.95 + o.facts.length * FACT_READ + (o.hand ? 0 : ASK_DEMO) + GO_DELAY;  // the demo's trick start, near enough
  add(Object.assign({ id: 'power-' + id, group: 'Animal powers', prio: 'core', title: name[0] + name.slice(1).toLowerCase(),
    dur: Math.ceil((pre + o.end - 1.45 + 0.2) * 10) / 10, poster: Math.round((pre + (o.posterT || 3.2) - 1.45) * 10) / 10,
  setup(S) {
    const d = S.data;
    S.face.autoLook = false;
    Object.assign(d, { ph: 'card', fi: -1, next: 0, ready: false, askAt: -1, downAt: -1, goAt: 0, t0: 0 });
    S.at(1.45, () => { animalEyes(S, eyes); zap(S, hex(burst)); d.ph = 'fact'; d.next = S.t + 0.45; });
  },
  frame(S, t) {
    const d = S.data, f = S.face;
    f.hidden = t < 1.45;
    if (d.ph === 'fact' && S.t >= d.next) {
      d.fi++;
      if (d.fi < o.facts.length) { S.say(o.facts[d.fi], { y: FACT_Y, size: FACT_SIZE }); f.boing(0.2); d.next = S.t + FACT_READ; }
      else if (o.hand || d.ready) powerGo(S);
      else { d.ph = 'ask'; d.askAt = S.t; S.say('Put me\ndown!', { glance: false, y: 40 }); f.look(0, 0.9, 300); S.after(ASK_DEMO, () => S.player.event('putdown')); }
    }
    if (d.ph === 'go' && S.t >= d.goAt) { d.ph = 'trick'; d.t0 = S.t; if (o.trick) o.trick(S); }
    if (o.look && t > 1.45) o.look(S, t);
    if (d.ph === 'trick' && o.frame) o.frame(S, trickT(S));
  },
  background: o.background,
  under(S) { if (S.data.ph === 'trick' && o.under) o.under(S, trickT(S)); },
  mid(S, t) { if (o.mid && t > 1.45) o.mid(S, t); },
  over(S, t) {
    const d = S.data;
    powerCard(S, t, id, name, burst);
    if (d.askAt >= 0) downArrow(S, S.L.cx, 194 + Math.abs(Math.sin(t * 5.5)) * 5, d.downAt < 0 ? Ease.outBack(prog(t, d.askAt + 0.1, 0.35)) : 1 - Ease.inCubic(prog(t, d.downAt, 0.25)));
    if (d.ph === 'trick' && o.over) o.over(S, trickT(S));
  },
  button(S) { const d = S.data; if (d.ph === 'fact' && d.fi >= 0) d.next = S.t; },
  event(S, name) {
    const d = S.data;
    if (name !== 'putdown' && name !== 'floor') return;
    d.ready = true;
    if (d.ph === 'ask') { d.downAt = S.t; S.face.boing(0.4); powerGo(S); }
  },
}, { when: o.when + (o.hand ? ' First, in the hand: the fact, big (Press A skips a screen). Then the trick, still in the hand.' : ' First, in the hand: the fact, big (Press A skips a screen), then "Put me down!" until the robot is on the floor (Put down). Then the trick.') }));
};

POWER('cheetah', 'CHEETAH', C.gold, { eye: '#f0a81c', dark: '#7a3d00', light: '#ffe27a', glow: '#ffb627' }, {
  when: 'Spins flat out, stops, pants. The eyes go amber with a cheetah\'s black tear stripes.',
  facts: ['The fastest\nanimal\non land!'], end: 6.1,
  frame(S, t) {
    const f = S.face;
    if (t < 4.0) { f.set('focused', { keep: true }); f.faceSet('rot', Math.sin(t * 30) * 0.03); f.f.x.snap(Math.sin(t * 47) * 1.2); }
    else { f.mouthShape = 'tongue'; f.faceSet('mOpen', 0.9 + Math.sin(t * 22) * 0.25); f.faceSet('mCurve', 0.3); f.f.y.snap(Math.sin(t * 22) * 2.4); f.eyeSet('lidT', 0.3); f.eyeSet('lidB', 0.15); f.f.sweat.to(1); }
  },
  under(S, t) {
    if (t > 4.0) return;
    // whoosh: arcs racing round the screen centre (the robot is spinning)
    const scr = S.scr, cx = S.L.cx, cy = S.L.cy + 10, k = prog(t, 1.5, 0.4) * (1 - prog(t, 3.6, 0.4));
    for (let i = 0; i < 7; i++) {
      const r = 30 + i * 14, a0 = -t * (9 + i) + i * 2.1, span = 0.9;
      scr.shape(cx - r - 3, cy - r - 3, cx + r + 3, cy + r + 3, (x, y) => {
        let a = Math.atan2(y - cy, x - cx) - a0; a = ((a % TAU) + TAU) % TAU;
        return a < span ? Math.abs(len(x - cx, y - cy) - r) - 1.4 * (a / span) : 9;
      }, contrast(S), 0.5 * k);
    }
  },
  mid(S, t) {
    // tear stripes from the inner corners of the eyes down to the mouth
    const [a, b] = eyePair(S), my = S.L.my + S.face.f.y.x, col = S.skin === RF.SKINS.glow ? hex('#ffb627') : S.skin === RF.SKINS.ink ? hex('#1d1a22') : hex('#2a1204');
    for (const [p, s] of [[a, 1], [b, -1]]) {
      const x0 = p.x + s * S.L.ew * 0.55, y0 = p.y + S.L.eh * 0.72, x1 = S.L.cx - s * 13, y1 = my + 1;
      // tapered and bowed outwards a little, like the real marks
      const mx = (x0 + x1) / 2 + s * -4, mY = (y0 + y1) / 2;
      const seg = (x, y) => Math.min(SD.segment(x, y, x0, y0, mx, mY), SD.segment(x, y, mx, mY, x1, y1));
      S.scr.shape(Math.min(x0, x1, mx) - 5, y0 - 5, Math.max(x0, x1, mx) + 5, y1 + 5, (x, y) => seg(x, y) - (3.2 - 1.8 * clamp((y - y0) / (y1 - y0 + 1), 0, 1)), col, 1);
    }
  } });

POWER('honeybee', 'HONEYBEE', C.gold, { eye: '#3b2a12', dark: '#0e0904', light: '#b08a2a', glow: '#ffd23a', pupil: 'none', compound: 1, sw: 1.05, sh: 1.08 }, {
  when: 'The waggle dance: wiggles along a figure 8 to a tune. Compound eyes and antennae.',
  facts: ['Bees dance\nto show', 'where\nflowers are!'], end: 6.3,
  frame(S, t) {
    const f = S.face;
    if (t < 5.2) {
      const p = (t - 1.6) * 1.3, x = Math.sin(p) * 22, y = Math.sin(2 * p) * 10;
      const waggle = Math.cos(p) > 0.3 ? Math.sin(t * 40) * 3 : 0;
      f.faceSet('x', x + waggle, 600, 0.5); f.faceSet('y', y - 8, 600, 0.5); f.faceSet('rot', waggle * 0.03);
      f.set('happy', { keep: true });
      if (Math.floor(t * 2.5) !== Math.floor((t - S.dt) * 2.5)) textPop(S, 'bzz', S.L.cx + (S.r() - 0.5) * 90, 186, { size: 12, vy: -20, rot: (S.r() - 0.5) * 0.4 });
    } else { f.faceSet('x', 0); f.faceSet('y', 0); f.faceSet('rot', 0); }
  },
  under(S, t) {
    // the dance path, dotted, appearing as it is danced
    const scr = S.scr, col = contrast(S), upto = (t - 1.6) * 1.3;
    for (let p = 0; p < Math.min(upto, TAU * 1.5); p += 0.16) {
      const x = S.L.cx + Math.sin(p) * 22 * 1.6, y = S.L.cy + 4 + Math.sin(2 * p) * 10 * 2.2;
      scr.shape(x - 3, y - 3, x + 3, y + 3, (px, py) => SD.circle(px - x, py - y, 1.3), col, 0.55);
    }
    const fl = A(S, 'flower');
    if (fl && t > 3) S.scr.sprite(fl, S.L.cx + 50, 180, { scale: 0.42 * Ease.outBack(prog(t, 3, 0.4)), rot: Math.sin(t * 2) * 0.1 });
  },
  mid(S, t) {
    // antennae with ball tips, bobbing
    const [a, b] = eyePair(S), col = S.skin === RF.SKINS.glow ? hex('#ffd23a') : S.skin === RF.SKINS.ink ? hex('#1d1a22') : hex('#2a1204');
    for (const [p, s] of [[a, -1], [b, 1]]) {
      const x0 = p.x + s * 4, y0 = p.y - S.L.eh * 0.9, x1 = x0 + s * 14 + Math.sin(t * 7 + s) * 3, y1 = y0 - 22 + Math.cos(t * 7) * 2;
      S.scr.shape(Math.min(x0, x1) - 6, y1 - 6, Math.max(x0, x1) + 6, y0 + 3, (x, y) => Math.min(SD.segment(x, y, x0, y0, x1, y1) - 1.4, SD.circle(x - x1, y - y1, 3.6)), col, 1);
    }
  } });

POWER('hummingbird', 'HUMMINGBIRD', '#36d17c', { eye: '#121418', dark: '#000000', light: '#3a4440', glow: '#7dffb0', pupil: 'none', sw: 0.48, sh: 0.48, round: 1 }, {
  when: 'Hovers in place with a hum (the light buzz). It grows a beak and its wings blur.',
  facts: ['Its wings\nbeat about', '50 times\na second!'], end: 6.1,
  background(S, t) {
    const k = prog(t, 1.45, 0.4);
    if (k <= 0) { S.face.drawBackground(); return; }
    const sh = Math.sin(t * 1.5) * 0.5 + 0.5;
    const top = mixc(hex('#b8f5d8'), hex('#ffd1ec'), sh), bot = mixc(hex('#5fe0a0'), hex('#ff7ab8'), 1 - sh);
    S.scr.fillGradient(mixc(S.skin.bg, top, k), mixc(S.skin.bg2, bot, k), 6);
  },
  look(S, t) { S.face.f.y.snap(Math.sin(t * 3) * 3); S.face.f.x.snap(Math.sin(t * 90) * 0.6); },
  frame(S, t) { if (t > 5.2) S.face.f.blush.to(0.8); },
  mid(S, t) {
    const scr = S.scr, L = S.L, y = L.cy + S.face.f.y.x;
    // wing blur: two fans that flicker
    for (const s of [-1, 1]) {
      // a blurred wing: three ghost positions of a wing fanning up and down beside the face
      const x0 = L.cx + s * 50, y0 = y + 4;
      for (let k = 0; k < 3; k++) {
        const ang = -0.9 + k * 0.55 + Math.sin(t * 60 + k) * 0.15, cs = Math.cos(ang), sn = Math.sin(ang);
        scr.shape(x0 - 50, y0 - 50, x0 + 50, y0 + 50, (x, yy) => { const dx = (x - x0) * s, dy = yy - y0, u = cs * dx + sn * dy, v = -sn * dx + cs * dy; return SD.ellipse(u - 20, v, 22, 7); }, hex('#2e8a5e'), 0.3);
      }
    }
    // beak
    const bx = L.cx, by = y + 14;
    scr.shape(bx - 6, by - 4, bx + 6, by + 46, (x, yy) => Math.max(Math.abs(x - bx) - 3.4 * (1 - (yy - by) / 42), by - yy, yy - by - 40), hex('#121418'), 1);
    S.face.mouthOn = false;
  },
  over(S, t) { if (Math.floor(t * 3) !== Math.floor((t - S.dt) * 3)) textPop(S, 'hmmm', S.L.cx + (S.r() < 0.5 ? -38 : 38), 46, { size: 11, vy: -14, life: 0.8 }); } });

POWER('spider', 'SPIDER', '#9b8cff', { eye: '#17171c', dark: '#000000', light: '#5a5a70', glow: '#c9b8ff', pupil: 'none', extra: 1, sw: 1.08, sh: 1.08 }, {
  when: 'A scared little spider: eight eyes, shivering.',
  facts: ['Most spiders\nhave 8 eyes!'], end: 6.1,
  frame(S, t) { const f = S.face; if (t < 5.0) { f.set('scared', { keep: true }); shiver(S, 1.4, 80); f.mouthShape = 'wave'; } else { f.mouthShape = 'smile'; f.f.x.to(0); f.set('happy', { keep: true }); } },
  over(S, t) { if (t > 2.2 && t < 2.3) textPop(S, 'eek!', S.L.cx - 40, S.L.cy - 56, { size: 14 }); } });

POWER('owl', 'OWL', '#ff9b1f', { eye: '#ff9b1f', dark: '#9a3f00', light: '#ffe08a', glow: '#ffb627', pupilSize: 1.45, sw: 1.14, sh: 0.98, round: 1 }, {
  when: 'Turns three-quarters round, looks, turns back. Huge orange owl eyes and ear tufts.',
  facts: ['Owls can\nturn their\nheads 270°!', 'That\'s most\nof the way\nround!'], end: 5.0, posterT: 2.6,
  frame(S, t) {
    const f = S.face;
    if (t > 2.3 && t < 3.2) f.faceSet('x', -150 * Ease.inCubic(prog(t, 2.3, 0.5)), 900, 0.9);
    else if (t >= 3.2 && t < 3.25) f.f.x.snap(150);
    else if (t >= 3.25) f.faceSet('x', 0, 160, 0.55);
    if (t > 1.9 && t < 2.1) f.eyes[0].open.snap(0.1); else if (t > 2.1 && f._blinkT < 0) f.eyes[0].open.snap(1);
  },
  background(S, t) {
    S.face.drawBackground();
    if (t <= 1.45) return;
    // pale facial discs behind the eyes
    const [a, b] = eyePair(S), col = S.skin === RF.SKINS.glow ? hex('#3b2a12') : S.skin === RF.SKINS.ink ? hex('#e9dcc2') : hex('#ffe6c4');
    for (const p of [a, b]) S.scr.shape(p.x - 46, p.y - 46, p.x + 46, p.y + 46, (x, y) => SD.circle(x - p.x, y - p.y, 41), col, 1);
  },
  mid(S, t) {
    const L = S.L, x0 = L.cx + S.face.f.x.x, y0 = L.cy + S.face.f.y.x - L.eh - 12, col = S.skin === RF.SKINS.glow ? hex('#ff9b1f') : S.skin === RF.SKINS.ink ? hex('#1d1a22') : hex('#6b3410');
    for (const s of [-1, 1]) { const x = x0 + s * 46; S.scr.shape(x - 14, y0 - 22, x + 14, y0 + 8, (px, py) => Math.max(SD.segment(px, py, x - s * 8, y0 + 4, x + s * 5, y0 - 16) - 4.5, py - y0 - 4), col, 1); }
  },
  over(S, t) { if (t > 3.4 && t < 3.5) textPop(S, 'hoo!', S.L.cx + 40, S.L.cy - 60, { size: 16 }); } });

POWER('dolphin', 'DOLPHIN', C.sky, { eye: '#24486e', dark: '#0c1a2c', light: '#6fa3d6', glow: '#7fd6ff', pupilSize: 1.1 }, {
  when: 'Spins exactly 1, 2 or 3 times, counted by the gyro.',
  facts: ['Spinner\ndolphins spin', 'up to 7 times\nin one jump!'], end: 6.5,
  background(S, t) {
    const k = prog(t, 1.45, 0.5);
    S.scr.fillGradient(mixc(S.skin.bg, hex('#3fb6e0'), k), mixc(S.skin.bg2, hex('#0a3f78'), k), 5);
  },
  trick(S) { for (const [t, n] of [[2.2, '1!'], [3.1, '2!'], [4.0, '3!']]) S.after(t - 1.45, () => { S.data.spin = trickT(S); S.data.n = n; S.fx.burst('drop', S.L.cx, 200, 9, { angle: -PI / 2, spread: 1.6, speed: 160, g: 400, size: 5, life: 1, color: '#d9f6ff' }); S.face.set('joy', { keep: true }); }); },
  frame(S, t) {
    const f = S.face;
    if (S.data.spin && t - S.data.spin < 0.6) f.f.rot.snap(Ease.inOut((t - S.data.spin) / 0.6) * TAU); else f.f.rot.snap(0);
    if (t > 4.8) f.set('happy', { keep: true });
    if (Math.floor(t * 3) !== Math.floor((t - S.dt) * 3)) S.fx.emit({ type: 'ring', x: S.r() * S.L.w, y: 240, vy: -60, size: 2 + S.r() * 2, life: 3, color: hex('#d9f6ff'), width: 0.9 });
  },
  over(S, t) { if (S.data.n && t - S.data.spin < 0.85) text(S.scr, S.data.n, S.L.cx, 196, { font: FONTS.shout, size: 40, color: '#ffffff', outline: 2.5, oc: '#0a3f78', scale: Ease.outBack(prog(t - S.data.spin, 0, 0.3)) }); } });

POWER('bat', 'BAT', '#b28cff', { eye: '#2a1a14', dark: '#000000', light: '#6a4a3a', glow: '#c9b8ff', sw: 0.62, sh: 0.62, round: 1, pupilSize: 0.8 }, {
  when: 'Held in the hand: sonar. It beeps faster as your hand comes closer.',
  hand: true, facts: ['Many bats\nfind bugs', 'by listening\nfor echoes!'], end: 6.8,
  background(S, t) { const k = prog(t, 1.45, 0.5); S.scr.fillGradient(mixc(S.skin.bg, hex('#241a4a'), k), mixc(S.skin.bg2, hex('#0c0a1e'), k), 5); },
  frame(S, t) {
    const d = Math.max(4, 60 - (t - 1.6) * 14), rate = 1 / (0.08 + d / 90);
    S.data.dist = d;
    if (t < 5.6 && Math.floor(t * rate) !== Math.floor((t - S.dt) * rate)) S.fx.emit({ type: 'ring', x: S.L.cx, y: 30, size: 4, grow: 160, life: 0.7, fade: 0.6, color: hex('#c9b8ff'), width: 1.6 });
    if (t > 4.6 && t < 5.6) { S.face.set('surprised', { keep: true }); for (const e of S.face.eyes) { e.sw.to(0.9); e.sh.to(0.9); } }
    if (t > 5.6) S.face.set('happy', { keep: true });
  },
  mid(S, t) {
    const L = S.L, y0 = L.cy + S.face.f.y.x - L.eh * 0.55, col = S.skin === RF.SKINS.glow ? hex('#b28cff') : S.skin === RF.SKINS.ink ? hex('#1d1a22') : hex('#8a5a3a');
    for (const s of [-1, 1]) {
      const x = L.cx + s * 40, wig = Math.sin(t * 8 + s) * 0.08;
      S.scr.shape(x - 30, y0 - 56, x + 30, y0 + 10, (px, py) => { const u = (px - x) * Math.cos(wig) - (py - y0) * Math.sin(wig), v = (px - x) * Math.sin(wig) + (py - y0) * Math.cos(wig); return Math.max(Math.abs(u + s * v * 0.25) - 14 * (1 + v / 48), -v - 48, v); }, col, 1);
    }
  },
  over(S, t) { if (t < 5.6 && S.data.dist) text(S.scr, Math.round(S.data.dist) + ' cm', S.L.cx, 212, { font: FONTS.say, size: 18, color: '#ffffff', outline: 2, oc: '#0c0a1e' }); } });

POWER('chameleon', 'CHAMELEON', '#3aa66b', { eye: '#3aa66b', dark: '#0d4a2a', light: '#c7ff8a', glow: '#7dffb0', pupilSize: 0.7 }, {
  when: 'Changes colour on the screen (and the base LEDs). Its eyes look two ways at once.',
  hand: true, facts: ['Chameleons\ncan look', 'two ways\nat once!'], end: 6.1,
  background(S, t) {
    const k = prog(t, 1.45, 0.5); if (k <= 0) { S.face.drawBackground(); return; }
    const h = (t * 0.25) % 1, c = hsl(h, 0.65, 0.55), c2 = hsl((h + 0.08) % 1, 0.7, 0.42);
    S.scr.fillGradient(mixc(S.skin.bg, c, k), mixc(S.skin.bg2, c2, k), 5);
  },
  look(S, t) { const f = S.face, h = (t * 0.25 + 0.5) % 1; f.eyeColor = hsl(h, 0.7, 0.45); f.eyeLight = hsl(h, 0.8, 0.75); f.eyeDark = hsl(h, 0.6, 0.18); },
  frame(S, t) {
    const f = S.face;
    if (t < 5) { f.eyes[0].lookX.to(Math.sin(t * 2.1) * 0.9, 300, 0.7); f.eyes[0].lookY.to(Math.cos(t * 1.3) * 0.6, 300, 0.7); f.eyes[1].lookX.to(Math.sin(t * 1.4 + 2) * 0.9, 300, 0.7); f.eyes[1].lookY.to(Math.sin(t * 2.6) * 0.6, 300, 0.7); }
    else f.look(0, 0);
  } });

function hsl(h, s, l) { const a = s * Math.min(l, 1 - l), f = (n) => { const k = (n + h * 12) % 12; return l - a * Math.max(-1, Math.min(k - 3, 9 - k, 1)); }; return [f(0) * 255, f(8) * 255, f(4) * 255]; }

// ===== GAMES (core) =====
add({ id: 'hot-potato', group: 'Games', prio: 'core', title: 'Hot potato', dur: 8.4, poster: 4.6,
  when: 'Passed round the room, ticking faster and glowing redder, until BOOM.',
  background(S, t) {
    const k = clamp((t - 0.3) / 5.5, 0, 1), boom = t > 6.0;
    if (boom) { S.scr.fillGradient(hex('#3a3236'), hex('#1c181a'), 8); return; }
    const red = mixc(hex('#ff6a3a'), hex('#d4142a'), k);
    S.scr.fillGradient(mixc(S.skin.bg, red, k * 0.9), mixc(S.skin.bg2, hex('#8a0a1a'), k * 0.9), 6);
  },
  setup(S) {
    const f = S.face; f.autoLook = false;
    S.at(0.4, () => { f.set('scared'); S.data.tick = 0; });
    S.at(6.0, () => {
      S.fx.flash = 1.4; S.fx.burst('puff', S.L.cx, S.L.cy, 18, { speed: 130, drag: 3, size: 8, grow: 18, life: 1.4, colors: ['#5a5254', '#2e2a2c', '#777072'], spread: TAU });
      S.fx.burst('spark', S.L.cx, S.L.cy, 12, { speed: 200, drag: 2, size: 6, life: 0.6, colors: ['#ffd23a', '#ffffff'], spread: TAU });
      f.set('dizzy'); f.boing(1); f.f.sweat.snap(0); f.mouthShape = 'wave'; S.data.boom = 1;
      S.say('BOOM!', { shout: true, glance: false, color: '#ffd23a', oc: '#2e2a2c' });
    });
    S.at(7.4, () => { S.unsay(); f.mouthShape = 'smile'; f.set('xx'); S.fx.emit({ type: 'puff', x: S.L.cx, y: S.L.my, vy: -40, size: 4, grow: 14, life: 0.8, color: hex('#8a8486') }); textPop(S, 'cough', S.L.cx + 30, S.L.my - 10, { size: 12, color: '#ffffff', oc: '#2e2a2c' }); });
  },
  frame(S, t) {
    const f = S.face;
    if (t > 0.4 && t < 6.0) {
      const p = (t - 0.4) / 5.6, rate = 1.4 + p * p * 9;
      S.data.tick += S.dt * rate;
      if (S.data.tick >= 1) { S.data.tick -= 1; textPop(S, 'tick', S.L.cx + (S.r() - 0.5) * 80, 200, { size: 12 + p * 6, vy: -30, life: 0.5, color: '#ffffff', oc: '#8a0a1a' }); f.kick('sy', 2 + p * 6); }
      shiver(S, p * 3, 60);
      if (Math.floor(t * 4) !== Math.floor((t - S.dt) * 4)) f.look((S.r() - 0.5) * 1.8, 0, 900);
    }
  },
  mid(S, t) {
    if (!S.data.boom) return;
    // soot smudges on the face
    for (const [dx, dy, r] of [[-30, 30, 9], [28, -36, 7], [42, 24, 6], [-48, -20, 5]]) S.scr.shape(S.L.cx + dx - r - 4, S.L.cy + dy - r - 4, S.L.cx + dx + r + 4, S.L.cy + dy + r + 4, (x, y) => SD.circle(x - S.L.cx - dx, y - S.L.cy - dy, r) + RF.vnoise(x * 0.5, y * 0.5) * 4, hex('#151214'), 0.55);
  } });

add({ id: 'quick-draw', group: 'Games', prio: 'core', title: 'Quick draw', dur: 7, poster: 3.1,
  when: '"Wait... wait... GO!" Tap it first. It times you and keeps the records.',
  setup(S) {
    const f = S.face;
    S.at(0.2, () => { f.set('focused'); S.say('Wait...', { glance: false }); });
    S.at(1.4, () => S.say('Wait...', { glance: false, size: 22 }));
    S.at(2.6, () => { S.fx.flash = 1; f.set('surprised'); f.boing(0.9); S.say('GO!', { shout: true, glance: false, size: 54, y: 196 }); S.fx.burst('spark', S.L.cx, 196, 14, { speed: 180, drag: 3, size: 6, life: 0.6, colors: [C.gold, '#ffffff'], spread: TAU }); });
    S.at(3.5, () => { f.set('starry'); f.boing(0.4); S.say('0.31 s', { shout: true, glance: false, size: 36 }); });
    S.at(4.8, () => { S.say('NEW RECORD!', { shout: true, glance: false, bounce: true }); confetti(S, 0, 240, 22, -1.2); confetti(S, S.L.w, 240, 22, -PI + 1.2); });
    S.at(6.4, () => { S.unsay(); f.set('happy'); });
  },
  frame(S, t) { if (t > 0.2 && t < 2.6) { const z = 1 + prog(t, 0.2, 2.4) * 0.12; S.face.faceSet('sx', z); S.face.faceSet('sy', z); } else if (t > 2.6 && t < 2.7) { S.face.faceSet('sx', 1); S.face.faceSet('sy', 1); } } });

add({ id: 'quick-early', group: 'Games', prio: 'core', title: 'Quick draw: too soon!', dur: 5, poster: 2.2,
  when: 'Somebody tapped before GO. It laughs at them.',
  setup(S) {
    const f = S.face;
    S.at(0.2, () => { f.set('focused'); S.say('Wait...', { glance: false }); });
    S.at(1.3, () => { f.set('joy'); f.faceSet('mOpen', 1); S.say('Too soon!\nHee hee!', { glance: false, wobble: true }); S.data.laugh = 1; });
    S.at(4.2, () => { S.unsay(); S.data.laugh = 0; f.set('happy'); });
  },
  frame(S, t) { if (S.data.laugh) { S.face.f.y.snap(Math.abs(Math.sin(t * 18)) * -4); S.face.faceSet('rot', Math.sin(t * 9) * 0.06); } } });

add({ id: 'hole-in-one', group: 'Games', prio: 'core', title: 'Hallway golf: hole in one', dur: 6.6, poster: 2.4,
  when: 'It sits at the end of the hallway as the hole. A ball hits it: celebration, and it counts the hits.',
  setup(S) {
    const f = S.face; f.autoLook = false;
    S.at(0, () => f.look(0, 0.9, 200));
    S.at(1.0, () => { f.kick('sy', -12); f.kick('sx', 9); f.set('surprised'); S.fx.burst('spark', S.L.cx, 196, 8, { speed: 100, drag: 3, size: 5, life: 0.5, color: '#ffffff', spread: PI, angle: -PI / 2 }); });
    S.at(1.4, () => { f.set('starry'); f.boing(0.7); S.say('HOLE\nIN ONE!', { shout: true, glance: false, bounce: true, y: 200 }); });
    S.at(4.4, () => { S.unsay(); f.set('happy'); S.say('Hits today: 3', { glance: false }); });
    S.at(6.0, () => S.unsay());
  },
  frame(S, t) {
    if (t > 1.5 && t < 4.4 && Math.floor(t * 2.2) !== Math.floor((t - S.dt) * 2.2)) { const x = 20 + S.r() * 95, y = 22 + S.r() * 40; S.fx.burst('spark', x, y, 10, { speed: 70, drag: 2.5, g: 40, size: 4, life: 0.9, colors: S.skin.confetti, spread: TAU }); }
  },
  under(S, t) {
    // The robot is the hole, so its pin springs up between the eyes when the ball drops in, sways, and sinks at the end.
    const fg = A(S, 'golfflag'); if (!fg || t < 1.05) return;
    const up = Ease.outBack(prog(t, 1.05, 0.45)) * (1 - Ease.inCubic(prog(t, 6.0, 0.35))), w = t - 1.05;
    S.scr.sprite(fg, S.L.cx + 4, 66, { sx: 0.6, sy: 0.6 * up, ay: 0.97, rot: Math.sin(w * 7) * 0.14 * Math.exp(-w * 1.1) });
  },
  over(S, t) {
    if (t < 1.05) { const k = Ease.outCubic(t / 1.0), y = 250 - k * 64, r = 9 - k * 2.5; golfBall(S, S.L.cx + Math.sin(k * 3) * 6, y, r); }
  } });

add({ id: 'who-next', group: 'Games', prio: 'should', title: 'Who\'s next?', dur: 5, poster: 3.6,
  when: 'Spins on the floor and stops pointing at someone. The eyes whirl, then lock on.',
  setup(S) { const f = S.face; f.autoLook = false; S.at(0.2, () => f.set('dizzy')); S.at(3.0, () => { f.set('surprised'); f.boing(0.5); f.look(0, -0.8, 500); }); S.at(3.4, () => S.say('YOU!', { shout: true, glance: false, size: 40 })); S.at(4.6, () => S.unsay()); },
  frame(S, t) { if (t < 3) S.face.f.rot.snap(Ease.outCubic(t / 3) * TAU * 3); else S.face.f.rot.snap(0); } });

// ===== PUPPY TRAINING (core) =====
add({ id: 'train-guess', group: 'Training', prio: 'core', title: 'Guessing a power', dur: 6.4, poster: 2.4,
  when: 'The kid calls a power. It thinks, guesses, and tries it. Right or wrong is part of the game.',
  setup(S) {
    const f = S.face; f.autoLook = false;
    S.at(0.2, () => { f.set('curious'); f.look(-0.7, -0.8, 300); textPop(S, '?', S.L.cx - 40, S.L.cy - 54, { size: 22, vy: -10, life: 1.4 }); });
    S.at(0.6, () => { S.data.slot = S.t; });
    S.at(3.2, () => { S.data.slotEnd = 1; f.look(0, -0.9, 300); f.set('surprised', { keepLook: true }); f.boing(0.4); });
    S.at(4.0, () => { animalEyes(S, { eye: '#f0a81c', dark: '#7a3d00', light: '#ffe27a', glow: '#ffb627' }); zap(S, hex(C.gold)); f.look(0, 0); S.say('Cheetah?', { shout: true, glance: false }); });
  },
  over(S, t) {
    if (!S.data.slot || t > 4.0) return;
    const keys = ['owl', 'honeybee', 'dolphin', 'cheetah', 'spider', 'bat', 'hummingbird'];
    const st = t - S.data.slot, speed = S.data.slotEnd ? 0 : Math.max(1.2, 12 - st * 4.2);
    S.data.pos = (S.data.pos || 0) + S.dt * speed;
    if (S.data.slotEnd && !S.data.locked) { S.data.locked = 1; S.data.pos = 3; }
    const i = Math.floor(S.data.pos) % keys.length, fr = S.data.slotEnd ? 0 : S.data.pos % 1;
    const scr = S.scr, bx = S.L.cx, by = 200;
    scr.shape(bx - 34, by - 30, bx + 34, by + 30, (x, y) => Math.abs(SD.roundBox(x - bx, y - by, 30, 26, 9)) - 1.6, contrast(S), 1);
    for (const k of [0, 1]) {
      const toy = A(S, keys[(i + k) % keys.length]), y = by + (k - fr) * 44;
      if (toy) scr.sprite(toy, bx, y, { scale: 0.42, clip: (x, yy) => Math.abs(yy - by) < 24 && Math.abs(x - bx) < 28 });
    }
  } });

add({ id: 'train-yes', group: 'Training', prio: 'core', title: 'Treat! (got it right)', dur: 5.6, poster: 2.2,
  when: 'A pat or the big button means "yes". It gobbles the treat and remembers the name.',
  setup(S) {
    const f = S.face; f.autoLook = false;
    S.at(0.1, () => { f.look(0, -0.9, 300); f.set('surprised', { keepLook: true }); });
    S.at(0.9, () => { f.look(0, 0.3, 400); f.set('joy'); f.mouthShape = 'chomp'; S.data.chomp = S.t; });
    S.at(2.6, () => { S.data.chomp = 0; f.mouthShape = 'smile'; f.set('love'); hearts(S, 8, S.L.cx, S.L.cy); S.say('Yum!', { shout: true, glance: false }); });
    S.at(3.8, () => { f.set('proud'); S.say('Cheetah:\n3 of 5', { glance: false }); });
    S.at(5.2, () => S.unsay());
  },
  frame(S, t) {
    const f = S.face;
    if (S.data.chomp) { f.chomp = Math.abs(Math.sin((t - S.data.chomp) * 11)); f.f.y.snap(f.chomp * 2); if (Math.floor(t * 6) !== Math.floor((t - S.dt) * 6)) S.fx.burst('circle', S.L.cx, S.L.my + 4, 2, { speed: 70, g: 300, size: 1.6, life: 0.6, color: '#c98a3a', angle: PI / 2, spread: 2.4 }); }
  },
  over(S, t) {
    const ck = A(S, 'treat');
    if (t < 0.95) { const k = Ease.inCubic(prog(t, 0.2, 0.75)), y = -20 + k * (S.L.my + 30), sc = 0.42 * (1 - k * 0.6); if (ck) S.scr.sprite(ck, S.L.cx, y, { scale: sc, rot: t * 4 }); }
    if (t > 3.8 && t < 5.4) { for (let i = 0; i < 5; i++) { const x = S.L.cx - 36 + i * 18, y = 166, on = i < 3; S.scr.shape(x - 9, y - 9, x + 9, y + 9, (px, py) => SD.star(px - x, py - y, 7, 0.5) - (on ? 0 : 0), on ? hex(C.gold) : mixc(contrast(S), S.skin.bg, 0.6), on ? 1 : 0.8); } }
  } });

add({ id: 'train-no', group: 'Training', prio: 'core', title: 'Oops (got it wrong)', dur: 5, poster: 1.5,
  when: 'The kid says "no" (the robot picks how; not the power button, and the side button is hard to find). A little sad, then it tries again.',
  setup(S) {
    const f = S.face;
    S.at(0.2, () => { f.set('sad'); f.kick('y', 60); S.say('Oops!', { shout: true, glance: false }); });
    S.at(2.2, () => { f.set('focused'); f.kick('y', -60); S.say('I\'ll try\nagain!', { glance: false }); });
    S.at(4.4, () => { S.unsay(); f.set('neutral'); });
  } });

// ===== EVERYDAY (should) =====
add({ id: 'nap', group: 'Everyday', prio: 'should', title: 'Nap', dur: 9, poster: 4.5,
  when: 'Left alone: dozes, snores a bubble, dims the screen to save battery. Picking it up wakes it.',
  setup(S) {
    const f = S.face; f.autoLook = false;
    S.at(0.2, () => f.set('sleepy'));
    S.at(1.8, () => { f.autoBlink = false; for (const e of f.eyes) e.lidT.to(1, 80, 0.9); f.faceSet('mOpen', 0); f.faceSet('y', 8, 40, 0.9); S.data.sleep = S.t; });
    S.at(7.0, () => { S.data.sleep = 0; S.data.pop = S.t; f.autoBlink = true; f.set('surprised'); f.boing(0.9); f.kick('y', -200); });
    S.at(8.0, () => f.set('grumpy'));
  },
  frame(S, t) {
    const f = S.face;
    S.scr.dim = S.data.sleep ? 1 - 0.6 * prog(t, S.data.sleep + 0.5, 2) : 1;
    if (S.data.sleep && Math.floor(t / 1.6) !== Math.floor((t - S.dt) / 1.6)) textPop(S, 'z', S.L.cx + 44, S.L.cy - 40, { size: 13, vy: -18, vx: 10, life: 1.6 });
  },
  over(S, t) {
    if (S.data.sleep) {
      const br = (Math.sin((t - S.data.sleep) * 1.9) * 0.5 + 0.5), r = 3 + br * 9, x = S.L.cx + 6 + r * 0.4, y = S.L.my + 6 + S.face.f.y.x + r * 0.6;
      const c = S.skin === RF.SKINS.glow ? hex('#9fd8ff') : hex('#d6f2ff');
      S.scr.shape(x - r - 2, y - r - 2, x + r + 2, y + r + 2, (px, py) => Math.abs(SD.circle(px - x, py - y, r)) - 0.9, c, 0.9);
      S.scr.shape(x - r, y - r, x + r, y + r, (px, py) => SD.circle(px - x + r * 0.4, py - y + r * 0.4, r * 0.22), hex('#ffffff'), 0.9);
    }
    if (S.data.pop && t - S.data.pop < 0.3) S.fx.burst('circle', S.L.cx + 10, S.L.my + 10, 6, { speed: 80, size: 1.5, life: 0.3, color: '#d6f2ff', spread: TAU });
  } });

add({ id: 'hungry', group: 'Everyday', prio: 'should', title: 'Hungry, then dinner', dur: 8, poster: 1.8,
  when: 'Low battery: a hungry face and a tummy rumble. On the charger: nom nom.',
  setup(S) {
    const f = S.face; f.autoLook = false;
    S.at(0.2, () => { f.set('hungry'); S.say('I\'m hungry...', { glance: false }); });
    S.at(1.8, () => { S.data.rumble = S.t; textPop(S, 'grrrumble', S.L.cx, 178, { size: 12, vy: -10, life: 1 }); });
    S.at(3.6, () => { S.unsay(); S.data.charge = S.t; f.set('joy'); f.mouthShape = 'chomp'; });
    S.at(6.6, () => { f.mouthShape = 'smile'; f.set('happy'); S.say('Full!', { shout: true, glance: false }); });
  },
  frame(S, t) {
    const f = S.face;
    if (S.data.rumble && t - S.data.rumble < 0.8) shiver(S, 2, 50); else if (!S.data.charge) f.f.x.to(0);
    if (S.data.charge && t < 6.6) { f.chomp = Math.abs(Math.sin(t * 10)); if (Math.floor(t * 2) !== Math.floor((t - S.dt) * 2)) textPop(S, 'nom', S.L.cx + (S.r() < 0.5 ? -38 : 38), S.L.my, { size: 13, vy: -30 }); }
  },
  over(S, t) {
    const lv = S.data.charge ? 0.1 + 0.9 * prog(t, S.data.charge, 3) : 0.1, blink = !S.data.charge && Math.floor(t * 3) % 2;
    battery(S, S.L.cx, 22, lv, lv < 0.25 ? hex('#ff3b30') : hex('#34d17a'));
    if (blink) S.scr.shape(S.L.cx - 22, 10, S.L.cx + 24, 34, () => -1, S.skin.bg, 0.6);
    if (S.data.charge && t < 6.6) bolt(S, S.L.cx + 34, 22, 0.8 + Math.sin(t * 12) * 0.1, hex(C.gold));
  } });

add({ id: 'charging', group: 'Everyday', prio: 'core', title: 'Charging (dinner)', dur: 6.4, poster: 1.6,
  when: 'On the charger: munching while the battery fills, then content. The robot shows its real battery level and sends "full" when it is charged.',
  setup(S) {
    const f = S.face;
    f.autoLook = false; S.data.full = -1;
    S.at(0.2, () => { f.set('joy'); f.mouthShape = 'chomp'; f.boing(0.4); S.say('Yum!', { shout: true, glance: false }); });
    S.at(1.6, () => S.unsay());
    S.at(3.6, () => { if (S.data.full >= 0) return; f.mouthShape = 'smile'; f.set('happy'); });
    S.at(4.4, () => S.player.event('full'));
  },
  frame(S, t) {
    const f = S.face;
    if (t > 0.2 && t < 3.6 && S.data.full < 0) {
      f.chomp = Math.abs(Math.sin(t * 10));
      if (Math.floor(t * 2) !== Math.floor((t - S.dt) * 2)) textPop(S, 'nom', S.L.cx + (S.r() < 0.5 ? -38 : 38), S.L.my, { size: 13, vy: -30 });
    }
  },
  over(S, t) {
    const full = S.data.full >= 0, lv = full ? 1 : 0.15 + 0.85 * prog(t, 0.2, 3.8);
    battery(S, S.L.cx, 22, lv, lv < 0.25 ? hex('#ff3b30') : hex('#34d17a'));
    if (!full) bolt(S, S.L.cx + 34, 22, 0.8 + Math.sin(t * 12) * 0.1, hex(C.gold));
  },
  event(S, name) {
    if (name !== 'full' || S.data.full >= 0) return;
    const f = S.face;
    S.data.full = S.t;
    f.mouthShape = 'smile'; f.set('happy'); f.boing(0.5);
    S.say('Full!', { shout: true, glance: false });
    S.fx.burst('spark', S.L.cx, 22, 8, { speed: 90, spread: TAU, size: 4, life: 0.7, color: C.gold });
  } });

add({ id: 'muted', group: 'Everyday', prio: 'core', title: 'Quiet mode (muted)', dur: 4.6, poster: 2.6,
  when: 'Sound switched off: "shh!", a wink, and it zips its mouth shut. The robot sends "unmute" to unzip it.',
  setup(S) {
    const f = S.face;
    S.data.zip = -1; S.data.unzip = -1;
    S.at(0.2, () => { f.faceSet('mOpen', 0.5); f.faceSet('mWide', 0.4); f.faceSet('mCurve', 0); f.look(0, 0, 300); textPop(S, 'shh!', S.L.cx + 36, S.L.my - 8, { size: 15, vy: -24 }); });
    S.at(0.7, () => { f.shape[0] = 'arc'; f.eyeSet('morph', 1, 0, 600, 0.8); f.faceSet('mOpen', 0); f.faceSet('mCurve', 0.6); });  // a wink
    S.at(1.1, () => { f.eyeSet('morph', 0, 0); S.data.zip = S.t; f.faceSet('mShow', 0, 200, 0.9); textPop(S, 'zip!', S.L.cx - 32, S.L.my + 4, { size: 13, vy: -26 }); });
    S.at(1.9, () => { f.set('happy'); f.boing(0.3); S.say('Quiet mode', { glance: false }); });
    S.at(3.9, () => S.unsay());
  },
  mid(S, t) {
    const d = S.data;
    if (d.zip < 0 || (d.unzip >= 0 && t - d.unzip > 0.45)) return;
    zipper(S, d.unzip >= 0 ? 1 - Ease.inOut(prog(t, d.unzip, 0.45)) : Ease.inOut(prog(t, d.zip, 0.55)));
  },
  over(S, t) {
    const d = S.data;
    speakerOff(S, S.L.cx, 26, d.unzip >= 0 ? 1 - Ease.inCubic(prog(t, d.unzip, 0.25)) : Ease.outBack(prog(t, 1.9, 0.35)));
  },
  event(S, name) {
    const d = S.data;
    if (name !== 'unmute' || d.zip < 0 || d.unzip >= 0) return;
    const f = S.face;
    d.unzip = S.t; S.unsay();
    S.after(0.4, () => {
      f.faceSet('mShow', 1, 300, 0.6); f.set('joy'); f.boing(0.6);
      S.say('La la la!', { shout: true, glance: false });
      for (let i = 0; i < 3; i++) S.fx.emit({ type: 'note', x: S.L.cx - 30 + i * 30, y: S.L.my, vx: (i - 1) * 15, vy: -50, size: 8, life: 1.2, color: contrast(S), rot: (i - 1) * 0.2 });
    });
  } });

add({ id: 'missed-you', group: 'Everyday', prio: 'should', title: 'I missed you!', dur: 6.4, poster: 3.4,
  when: 'Woken after days apart. It knows how long from its clock.',
  setup(S) {
    const f = S.face;
    S.at(0.3, () => { f.set('surprised'); f.boing(0.6); S.say('You were gone\n3 days!', { glance: false }); });
    S.at(2.2, () => { f.set('love'); f.faceSet('tears', 1); S.say('I missed you!', { shout: true, glance: false, bounce: true }); hearts(S, 10, S.L.cx, S.L.cy, { speed: 120, spread: TAU }); });
    S.at(5.4, () => { S.unsay(); f.faceSet('tears', 0); f.set('happy'); });
  } });

add({ id: 'peekaboo', group: 'Everyday', prio: 'should', title: 'Peekaboo', dur: 6, poster: 1.8,
  when: 'Cover its sensor: "Where\'d everyone go?" Uncover it: "Peekaboo!"',
  setup(S) {
    const f = S.face; f.autoLook = false;
    S.at(0.2, () => { S.data.dark = S.t; f.set('scared'); S.say('Where\'d everyone\ngo?', { glance: false }); });
    S.at(3.0, () => { S.data.dark = 0; S.fx.flash = 0.9; f.set('joy'); f.faceSet('mOpen', 1); f.boing(0.9); S.say('Peekaboo!', { shout: true, glance: false, bounce: true }); });
    S.at(5.2, () => { S.unsay(); f.set('happy'); });
  },
  frame(S, t) { if (S.data.dark && Math.floor(t * 1.5) !== Math.floor((t - S.dt) * 1.5)) S.face.look((S.r() - 0.5) * 1.8, (S.r() - 0.5), 300); },
  post(S, t) { if (S.data.dark) { const k = prog(t, S.data.dark, 0.3); const p = S.scr.px; for (let i = 0; i < p.length; i++) p[i] *= 1 - 0.72 * k; } } });

add({ id: 'crystal-ball', group: 'Everyday', prio: 'should', title: 'Fortune ball', dur: 7.4, poster: 4.6,
  when: 'Ask a question, shake it, and it answers with suspense (the magic 8-ball idea).',
  setup(S) {
    const f = S.face;
    S.at(0.2, () => { f.set('curious'); S.say('Ask me a\nquestion!', { glance: false }); });
    S.at(1.7, () => { S.unsay(); f.googly = true; f.autoLook = false; S.data.shake = S.t; });
    S.at(2.8, () => { S.data.shake = 0; f.googly = false; S.data.ball = S.t; });
    S.at(6.6, () => { S.data.ball = 0; f.set('happy'); });
  },
  frame(S, t) { if (S.data.shake) { S.face.grav = [(S.r() - 0.5) * 7, (S.r() - 0.5) * 7]; shiver(S, 4, 40); } else S.face.f.x.to(0); },
  over(S, t) {
    if (!S.data.ball) return;
    const bt = t - S.data.ball, scr = S.scr, cx = S.L.cx, cy = 116, R = 58 * Ease.outBack(prog(bt, 0, 0.35));
    scr.shade(cx - R - 3, cy - R - 3, cx + R + 3, cy + R + 3, (x, y) => SD.circle(x - cx, y - cy, R), (x, y) => mixc(hex('#2a3bb8'), hex('#070b2a'), clamp(len(x - cx + 10, y - cy + 14) / R, 0, 1)));
    scr.shape(cx - R - 4, cy - R - 4, cx + R + 4, cy + R + 4, (x, y) => Math.abs(SD.circle(x - cx, y - cy, R)) - 2.2, hex('#0b0f14'), 1);
    const k = Ease.outCubic(prog(bt, 0.6, 1.6)), sz = 16 + 26 * k, rot = (1 - k) * 0.9 + Math.sin(bt * 2) * 0.05, ty = cy + 6 + (1 - k) * 20;
    const cs = Math.cos(rot), sn = Math.sin(rot);
    scr.shape(cx - 50, cy - 50, cx + 50, cy + 50, (x, y) => { const u = cs * (x - cx) + sn * (y - ty), v = -sn * (x - cx) + cs * (y - ty); return Math.max(-v - sz * 0.75, v * 0.5 + Math.abs(u) * 0.866 - sz * 0.4) + 0; }, hex('#3b5cff'), 0.25 + 0.75 * k);
    if (k > 0.5) text(scr, 'YES!', cx, ty - 6, { font: FONTS.shout, size: 18, color: '#ffffff', alpha: (k - 0.5) * 2, rot: rot * 0.5 });
    scr.shape(cx - R, cy - R, cx + R, cy + R, (x, y) => SD.ellipse(x - cx + R * 0.38, y - cy + R * 0.48, R * 0.24, R * 0.13), hex('#ffffff'), 0.6);
  } });

add({ id: 'dice', group: 'Everyday', prio: 'should', title: 'Dice', dur: 5.4, poster: 3.6,
  when: 'Shake to roll, for board games.',
  setup(S) { const f = S.face; f.autoLook = false; f.hidden = true; S.at(0.2, () => S.data.roll = S.t); S.at(2.6, () => { S.data.landed = S.t; }); S.at(3.2, () => { f.hidden = false; f.f.y.snap(-30); f.f.sx.snap(0.5); f.f.sy.snap(0.5); f.faceSet('sx', 0.62); f.faceSet('sy', 0.62); f.faceSet('y', -52); f.look(0, 0.9, 300); f.set('surprised', { keepLook: true }); }); S.at(3.8, () => { f.set('joy'); S.say('Five!', { shout: true, glance: false }); }); },
  over(S, t) {
    if (!S.data.roll) return;
    const rt = t - S.data.roll, land = S.data.landed ? Ease.outBack(prog(t, S.data.landed, 0.4)) : 0;
    const rot = S.data.landed ? (1 - land) * 0.4 : rt * 9, y = S.data.landed ? 150 : 150 - Math.abs(Math.sin(rt * 6)) * 40 * (1 - rt / 2.6);
    const n = S.data.landed ? 5 : 1 + (Math.floor(rt * 10) % 6), scr = S.scr, cx = S.L.cx, s = 34, cs = Math.cos(rot), sn = Math.sin(rot);
    const loc = (x, yy) => [cs * (x - cx) + sn * (yy - y), -sn * (x - cx) + cs * (yy - y)];
    scr.shape(cx - 56, y - 56, cx + 56, y + 56, (x, yy) => { const q = loc(x, yy); return SD.roundBox(q[0], q[1], s, s, 10) - 2; }, hex('#2a1204'), 1);
    scr.shade(cx - 56, y - 56, cx + 56, y + 56, (x, yy) => { const q = loc(x, yy); return SD.roundBox(q[0], q[1], s, s, 10); }, (x, yy) => mixc(hex('#ffffff'), hex('#e6ded0'), clamp((loc(x, yy)[1] + s) / (2 * s), 0, 1)));
    const P = { 1: [[0, 0]], 2: [[-1, -1], [1, 1]], 3: [[-1, -1], [0, 0], [1, 1]], 4: [[-1, -1], [1, -1], [-1, 1], [1, 1]], 5: [[-1, -1], [1, -1], [0, 0], [-1, 1], [1, 1]], 6: [[-1, -1], [1, -1], [-1, 0], [1, 0], [-1, 1], [1, 1]] }[n];
    for (const [a, b] of P) scr.shape(cx - 56, y - 56, cx + 56, y + 56, (x, yy) => { const q = loc(x, yy); return SD.circle(q[0] - a * 17, q[1] - b * 17, 6); }, hex('#1d1a22'), 1);
  } });

add({ id: 'wake-up', group: 'Everyday', prio: 'should', title: 'Good morning', dur: 6.4, poster: 3.2,
  when: 'Switched on. The eyes open from a line, it yawns and stretches (the stretch doubles as gyro calibration).',
  setup(S) {
    const f = S.face; f.autoBlink = false; f.autoLook = false; for (const e of f.eyes) e.open.snap(0); S.scr.dim = 0;
    S.at(2.2, () => { f.faceSet('mOpen', 1.1); f.faceSet('mCurve', 0); f.faceSet('mWide', 0.7); f.faceSet('sy', 1.12, 60, 0.5); f.faceSet('sx', 0.92, 60, 0.5); });
    S.at(3.4, () => { f.faceSet('mOpen', 0); f.faceSet('sy', 1); f.faceSet('sx', 1); f.boing(0.3); });
    S.at(3.8, () => { f.autoBlink = true; f.blink(); });
    S.at(4.4, () => { f.set('happy'); S.say('Good morning!', { glance: false }); });
    S.at(6.0, () => S.unsay());
  },
  frame(S, t) {
    const f = S.face;
    S.scr.dim = Ease.outCubic(prog(t, 0.1, 1.2));
    if (t < 2.2) { const o = Ease.inOut(prog(t, 0.6, 1.4)) * 0.45; for (const e of f.eyes) e.open.snap(o); }
    else if (t < 3.4) for (const e of f.eyes) e.open.snap(0.05);
    else if (t < 3.8) for (const e of f.eyes) e.open.snap(Ease.outCubic(prog(t, 3.4, 0.3)));
  } });

add({ id: 'listening', group: 'Everyday', prio: 'should', title: 'Listening (button held)', dur: 4, poster: 1.6,
  when: 'Every voice command: the face while the button is held. Motors are stopped first.',
  setup(S) { const f = S.face; f.autoLook = false; S.at(0.2, () => { f.set('surprised'); f.look(0, -0.3); S.data.listen = S.t; }); S.at(3.4, () => { S.data.listen = 0; f.set('happy'); f.look(0, 0); textPop(S, 'Got it!', S.L.cx, 200, { size: 18, font: FONTS.say, vy: -10 }); }); },
  over(S, t) { SCENES.find(s => s.id === 'say-hi').over(S, t); } });

// ===== EXPLORE =====
add({ id: 'boop', group: 'Explore', prio: 'explore', title: 'Boop', dur: 4, poster: 1.3,
  when: 'Tap the middle of the screen: it goes cross-eyed looking at its own nose.',
  setup(S) {
    const f = S.face; f.autoLook = false;
    S.at(0.8, () => { f.eyes[0].lookX.to(0.85, 600, 0.5); f.eyes[1].lookX.to(-0.85, 600, 0.5); f.eyeSet('lookY', 0.7, 2, 600, 0.5); f.kick('sy', -6); f.faceSet('mOpen', 0.4); f.faceSet('mCurve', 0); f.faceSet('mWide', 0.5); textPop(S, 'boop!', S.L.cx, S.L.cy + 6, { size: 16, vy: -40 }); });
    S.at(2.4, () => { f.look(0, 0); f.set('joy'); });
    S.at(3.4, () => f.set('neutral'));
  } });

add({ id: 'sneeze', group: 'Explore', prio: 'explore', title: 'Achoo!', dur: 5, poster: 2.25,
  when: 'A random idle gag, maybe when it\'s dusty (the base LEDs could flash).',
  setup(S) {
    const f = S.face;
    S.at(0.3, () => { f.eyeSet('lidT', 0.5); f.eyeSet('lidB', 0.3); f.faceSet('mOpen', 0.4); f.faceSet('mCurve', 0); f.faceSet('mWide', 0.6); textPop(S, 'ah...', S.L.cx + 30, S.L.cy - 50, { size: 13 }); });
    S.at(1.0, () => { f.faceSet('y', -10, 30, 0.7); f.faceSet('sy', 1.08, 30, 0.7); textPop(S, 'ahh...', S.L.cx - 30, S.L.cy - 56, { size: 15 }); });
    S.at(2.0, () => { S.data.sn = 1; f.set('joy'); f.faceSet('mOpen', 1); f.f.y.snap(12); f.kick('sy', -14); S.fx.flash = 0.5; S.say('ACHOO!', { shout: true, glance: false, size: 36 }); S.fx.burst('circle', S.L.cx, S.L.my, 16, { angle: PI / 2, spread: 1.8, speed: 160, g: 100, size: 1.6, life: 0.7, color: '#bfeaff' }); });
    S.at(3.4, () => { S.unsay(); f.set('happy'); f.faceSet('y', 0); f.faceSet('sy', 1); textPop(S, 'excuse me', S.L.cx, 200, { size: 13, font: FONTS.say }); });
  } });

add({ id: 'shy-peek', group: 'Explore', prio: 'explore', title: 'Learning to be brave', dur: 9, poster: 2.6,
  when: 'Bravery: shy at first, it peeks over the edge of the screen. Gentle approaches bring it out; a lunge sends it back.',
  setup(S) {
    const f = S.face; f.autoLook = false; f.f.y.snap(150); f.set('scared', { snap: true }); f.f.sweat.snap(0);
    S.at(0.4, () => f.faceSet('y', 96, 60, 0.8));
    S.at(1.4, () => f.look(-0.9, -0.6, 200)); S.at(2.2, () => f.look(0.9, -0.6, 200)); S.at(3.0, () => f.look(0, -0.7, 200));
    S.at(3.6, () => { f.faceSet('y', 40, 50, 0.8); f.set('curious', { keepLook: true }); });
    S.at(5.0, () => { f.faceSet('y', 0, 60, 0.7); f.set('happy'); f.look(0, 0); S.say('Hi...', { glance: false }); });
    S.at(7.0, () => { S.unsay(); S.fx.flash = 0.4; f.set('scared'); f.faceSet('y', 150, 500, 0.7); });
  } });

add({ id: 'freeze-dance', group: 'Explore', prio: 'explore', title: 'Freeze dance', dur: 8, poster: 1.7,
  when: 'It plays music and dances; when the music stops, everyone freezes. Including the robot.',
  background(S, t) { const beat = Math.floor(t * 2.4); const frozen = t > 4.2 && t < 6.4; const c = frozen ? S.skin.bg : mixc(S.skin.bg, hsl((beat * 0.17) % 1, 0.7, 0.55), 0.55); S.scr.fillGradient(c, mixc(c, [0, 0, 0], 0.2), 6); },
  setup(S) { const f = S.face; f.set('joy'); S.at(4.2, () => { S.data.frozen = 1; f.set('surprised'); S.say('FREEZE!', { shout: true, glance: false, size: 34 }); }); S.at(6.4, () => { S.data.frozen = 0; S.unsay(); f.set('joy'); }); },
  frame(S, t) {
    const f = S.face;
    if (S.data.frozen) { f.autoBlink = false; return; }
    f.autoBlink = true;
    const b = t * 2.4 * PI; f.faceSet('rot', Math.sin(b) * 0.18, 500, 0.5); f.faceSet('y', -Math.abs(Math.sin(b)) * 10, 500, 0.5); f.faceSet('x', Math.sin(b / 2) * 10, 500, 0.5);
    if (Math.floor(t * 2.4) !== Math.floor((t - S.dt) * 2.4)) S.fx.emit({ type: 'note', x: S.L.cx + (S.r() - 0.5) * 110, y: 200, vy: -60, size: 8, life: 1.2, color: contrast(S), rot: (S.r() - 0.5) * 0.5 });
  } });

add({ id: 'spin-art', group: 'Explore', prio: 'explore', title: 'Spin art', dur: 6, poster: 2,
  when: 'Floor spins put the screen at the centre of the spin. Patterns made for spinning blend into new colours (it spins here to show it).',
  background(S, t) {
    const scr = S.scr, cx = scr.w / 2, cy = scr.h / 2, rot = t * 18;
    const p = scr.px;
    for (let y = 0; y < scr.h; y++) for (let x = 0; x < scr.w; x++) {
      const dx = x - cx, dy = y - cy, r = len(dx, dy), a = Math.atan2(dy, dx) + rot;
      const k = (((a / TAU) * 6 + r * 0.02) % 1 + 1) % 1;
      const c = hsl((k + r * 0.004) % 1, 0.85, 0.55), i = (y * scr.w + x) * 3;
      p[i] = c[0]; p[i + 1] = c[1]; p[i + 2] = c[2];
    }
  },
  setup(S) { S.face.hidden = true; } });

// ===== PLAY: the living face you can poke =====
add({ id: 'play', group: 'Play', prio: 'core', title: 'Poke it', dur: 0, poster: 1,
  when: 'The idle face, live. Use the buttons under the screen: pick up, put down, shake, pat, flip, press A. Drag on the screen to tilt.',
  setup(S) { S.data.mode = 'idle'; },
  frame(S, t) {
    const f = S.face;
    if (S.data.mode === 'shake' && t - S.data.at < 1.4) { f.googly = true; f.grav = [(S.r() - 0.5) * 7, (S.r() - 0.5) * 7]; f.f.x.snap(Math.sin(t * 55) * 4); }
    else if (S.data.mode === 'shake' && !S.data.dizzy) { S.data.dizzy = 1; f.googly = false; f.set('dizzy'); S.after(2.4, () => { f.set('neutral'); S.data.mode = 'idle'; S.data.dizzy = 0; }); }
    if (S.data.mode === 'shake' && S.data.dizzy) { f.faceSet('x', Math.cos(t * 4.5) * 5); f.faceSet('rot', Math.sin(t * 3) * 0.12); }
    if (S.data.mode === 'lifted') { f.faceSet('rot', Math.sin(t * 5) * 0.16); }
    if (S.data.mode === 'flipped') { f.faceSet('rot', Math.sin(t * 16) * 0.12, 900, 0.3); }
    if (S.data.drag) { f.googly = true; f.grav = S.data.drag; S.data.dragged = 1; } else if (S.data.dragged && S.data.mode === 'idle') { S.data.dragged = 0; f.googly = false; }
    if (S.data.gig !== undefined && t - S.data.gig < 0.45) f.f.y.snap(Math.sin(t * 60) * 1.6);
  },
  over(S, t) { if (S.data.mode === 'shake' && S.data.dizzy) stars(S, t, S.L.cx + S.face.f.x.x, S.L.cy - 50, 38, 7, 3, hex(C.gold)); },
  event(S, name) {
    const f = S.face;
    S.unsay(); f.faceSet('rot', 0); f.faceSet('x', 0); f.mouthShape = 'smile'; f.googly = false;
    if (name === 'pickup') { S.data.mode = 'lifted'; f.set('surprised'); f.boing(0.8); f.kick('y', -160); S.after(0.35, () => { f.set('joy'); S.say('Wheee!', { shout: true, bounce: true, glance: false }); }); S.after(2.6, () => { if (S.data.mode === 'lifted') S.unsay(); }); }
    if (name === 'putdown') { S.data.mode = 'idle'; f.kick('sy', -9); f.kick('sx', 7); f.set('happy'); f.eyeSet('lidT', 0.75); S.say('phew...', { glance: false }); S.after(1.6, () => { S.unsay(); f.set('neutral'); }); }
    if (name === 'shake') { S.data.mode = 'shake'; S.data.at = S.t; S.data.dizzy = 0; f.autoLook = false; f.faceSet('mOpen', 0.9); f.faceSet('mCurve', 0); S.after(4, () => { f.autoLook = true; }); }
    if (name === 'pat') { S.data.n = (S.data.n || 0) + 1; patOnce(S, S.data.n); if (S.data.n >= 3) S.data.n = 0; }
    if (name === 'flip') { S.data.mode = 'flipped'; f.set('scared'); f.mouthShape = 'wave'; S.say('Help!\nI\'m stuck!', { glance: false, wobble: true }); }
    if (name === 'unflip') { S.data.mode = 'idle'; f.set('joy'); f.boing(0.6); S.say('Thanks!', { shout: true, glance: false }); S.after(1.4, () => { S.unsay(); f.set('neutral'); }); }
    if (name === 'button') { f.set('love'); f.boing(0.5); hearts(S, 8, S.L.cx, S.L.cy, { speed: 120, spread: TAU }); S.after(1.6, () => f.set('neutral')); }
  },
  tap(S) { this.event(S, 'pat'); } });

G.RF_SCENES = SCENES;
})(typeof window !== 'undefined' ? window : globalThis);
