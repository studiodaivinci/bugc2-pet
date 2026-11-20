// Runs one scene on one screen: timing, scheduled cues, captions, and the draw order
// background -> under() -> face -> particles -> over() -> caption. Firmware gets the same loop.
(function (G) {
'use strict';
const { Screen, Face, FX, text, textBitmap, clamp, Ease, FONTS, hex } = G.RF;

class Player {
  constructor(canvas, scene, opts = {}) {
    this.cv = canvas; this.ctx = canvas.getContext('2d');
    this.scene = scene; this.opts = opts;
    this.speed = opts.speed || 1;
    this.reset();
  }
  reset() {
    const o = this.opts, w = this.cv.width, h = this.cv.height;
    this.scr = new Screen(w, h);
    this.face = new Face(this.scr, { skin: o.skin, back: o.back, mouth: o.mouth, seed: 7 });
    this.fx = new FX(this.scr, this.face);
    const S = this.S = {
      face: this.face, fx: this.fx, scr: this.scr, L: this.face.L, skin: this.face.skin, t: 0, dt: 0,
      timers: [], cap: null, data: {}, assets: G.RF_ASSETS || {}, r: G.RF.rng(1234), player: this,
      at: (t, fn) => { S.timers.push({ t, fn }); S.timers.sort((a, b) => a.t - b.t); },
      after: (d, fn) => S.at(S.t + d, fn),
      say: (str, o2 = {}) => { S.cap = Object.assign({ str, t0: S.t }, o2); if (o2.glance !== false) { this.face.look(0, 0.75, 300); S.after(0.5, () => { if (!o2.keepLook) this.face.look(0, 0, 300); }); } },
      unsay: () => { if (S.cap) { S.cap.out = S.t; } },
      tapped: 0,
    };
    if (this.scene.setup) this.scene.setup(S);
  }
  step(dt) {
    const S = this.S;
    S.dt = dt; S.t += dt;
    while (S.timers.length && S.timers[0].t <= S.t) S.timers.shift().fn(S);
    if (this.scene.frame) this.scene.frame(S, S.t, dt);
    this.face.update(dt);
    this.fx.update(dt);
    if (S.cap && S.cap.out !== undefined && S.t - S.cap.out > 0.25) S.cap = null;
  }
  draw() {
    const S = this.S, sc = this.scene;
    if (sc.background) sc.background(S, S.t); else this.face.drawBackground();
    if (sc.under) sc.under(S, S.t);
    this.face.draw({ noBg: true });
    if (sc.mid) sc.mid(S, S.t);
    this.fx.draw();
    if (sc.over) sc.over(S, S.t);
    if (S.cap) drawCaption(S);
    if (sc.post) sc.post(S, S.t);
    this.scr.present(this.ctx);
  }
  // simulate to time t without drawing, then draw once (poster frames)
  renderAt(t) {
    this.reset();
    const dt = 1 / 30;
    while (this.S.t + dt <= t) this.step(dt);
    this.draw();
  }
  tap() { if (this.scene.tap) this.scene.tap(this.S); }
  button() { if (this.scene.button) this.scene.button(this.S); }
  event(name) { if (this.scene.event) this.scene.event(this.S, name); }
}

// Captions: up to two lines, auto-fit to the width, popped in with overshoot. Shouts use the display face.
function drawCaption(S) {
  const c = S.cap, L = S.L, scr = S.scr, skin = S.skin;
  const age = S.t - c.t0;
  const out = c.out !== undefined ? clamp((S.t - c.out) / 0.25, 0, 1) : 0;
  const lines = c.str.split('\n');
  const font = c.shout ? FONTS.shout : FONTS.say;
  const maxW = (c.width || L.w - 10);
  let size = c.size || (c.shout ? 30 : 19);
  const col = c.color || skin.text, oc = c.oc || skin.textOutline;
  const outline = c.outline ?? (c.shout ? 2.5 : 2);
  for (let k = 0; k < 6; k++) {
    let wmax = 0;
    for (const ln of lines) wmax = Math.max(wmax, textBitmap(ln, { font, size, color: col, outline, oc }).w - outline * 2 - 4);
    if (wmax <= maxW) break;
    size = Math.floor(size * maxW / wmax);
  }
  const lh = size * (c.shout ? 1.0 : 1.08);
  const cy = (c.y ?? L.capY) - (lines.length - 1) * lh / 2;
  const pop = c.noPop ? 1 : Ease.outBack(clamp(age / 0.32, 0, 1));
  for (let i = 0; i < lines.length; i++) {
    const delay = i * 0.12, p = c.noPop ? 1 : Ease.outBack(clamp((age - delay) / 0.32, 0, 1));
    const wob = c.wobble ? Math.sin(S.t * 9 + i) * 0.05 : 0;
    const y = cy + i * lh + (c.bounce ? -Math.abs(Math.sin(S.t * 6 + i)) * 3 : 0);
    text(scr, lines[i], L.cx, y, { font, size, color: col, outline, oc, scale: Math.max(0, p) * (1 - out), rot: wob, alpha: 1 - out });
  }
}

G.RF.Player = Player;
})(typeof window !== 'undefined' ? window : globalThis);
