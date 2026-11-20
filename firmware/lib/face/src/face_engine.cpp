#include "face_engine.h"

namespace face {

const Skin MARBLE = {
    hex("#f59a1b"), hex("#ec8510"), 5,
    hex("#13b5ae"), hex("#06474d"), hex("#7ff0dc"), hex("#0b0f14"), hex("#ffffff"),
    1.6f,
    hex("#4a1f04"), hex("#4a1f04"), hex("#4a1f04"), hex("#9c2a1c"), hex("#ff7a8a"), hex("#fff6e5"), hex("#4a1f04"),
    hex("#13b5ae"), hex("#ff3b5c"), hex("#ff5a4e"),
    {hex("#13b5ae"), hex("#fff6e5"), hex("#ff3b5c"), hex("#4a1f04"), hex("#ffe066")},
};

// ---------------------------------------------------------------- expressions
namespace {
constexpr float UNSET = -1e9f;
const float EYE_DEF[E_N] = {0, 0, 1, 1, 0.82f, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0};
const float FACE_DEF[F_N] = {0, 0, 1, 1, 0, 1, 0, 0.4f, 1, 0, 0, 0, 0, 1};

struct ExprDef {
  float b[E_N], L[E_N], R[E_N], f[F_N];
  Shape shape;
};
ExprDef EX[EX_N];
bool exReady = false;

void initExpr() {
  for (auto& e : EX) {
    for (int k = 0; k < E_N; k++) e.b[k] = e.L[k] = e.R[k] = UNSET;
    for (int k = 0; k < F_N; k++) e.f[k] = UNSET;
    e.shape = SH_NONE;
  }
  { auto& e = EX[NEUTRAL]; e.f[F_MCURVE] = 0.35f; e.f[F_MOPEN] = 0; }
  { auto& e = EX[HAPPY]; e.b[E_LIDB] = 0.42f; e.b[E_LIDBC] = 0.9f; e.b[E_SH] = 1.02f;
    e.f[F_MCURVE] = 1; e.f[F_MOPEN] = 0.35f; e.f[F_BLUSH] = 0.6f; }
  { auto& e = EX[JOY]; e.b[E_MORPH] = 1; e.b[E_SH] = 1; e.shape = SH_ARC;
    e.f[F_MCURVE] = 1; e.f[F_MOPEN] = 0.85f; e.f[F_BLUSH] = 0.8f; }
  { auto& e = EX[LOVE]; e.b[E_MORPH] = 1; e.b[E_SW] = 1.05f; e.b[E_SH] = 1.05f; e.shape = SH_HEART;
    e.f[F_MCURVE] = 1; e.f[F_MOPEN] = 0.2f; e.f[F_BLUSH] = 1; }
  { auto& e = EX[STARRY]; e.b[E_MORPH] = 1; e.b[E_SW] = 1.1f; e.b[E_SH] = 1.1f; e.shape = SH_STAR;
    e.f[F_MCURVE] = 1; e.f[F_MOPEN] = 0.9f; e.f[F_BLUSH] = 0.4f; }
  { auto& e = EX[SLEEPY]; e.b[E_LIDT] = 0.55f; e.b[E_LIDTA] = -0.12f; e.b[E_LOOKY] = 0.3f; e.b[E_PUPIL] = 0.9f;
    e.f[F_MCURVE] = 0.1f; e.f[F_MOPEN] = 0; e.f[F_BLUSH] = 0; }
  { auto& e = EX[SAD]; e.b[E_LIDT] = 0.32f; e.b[E_LIDTA] = -0.42f; e.b[E_LOOKY] = 0.45f; e.b[E_SH] = 0.95f;
    e.f[F_MCURVE] = -0.8f; e.f[F_MOPEN] = 0; e.f[F_MWIDE] = 0.8f; }
  { auto& e = EX[GRUMPY]; e.b[E_LIDT] = 0.38f; e.b[E_LIDTA] = 0.42f; e.b[E_LIDB] = 0.12f; e.b[E_SH] = 0.95f;
    e.f[F_MCURVE] = -0.6f; e.f[F_MOPEN] = 0; e.f[F_MWIDE] = 0.7f; }
  { auto& e = EX[SCARED]; e.b[E_SW] = 0.9f; e.b[E_SH] = 1.16f; e.b[E_PUPIL] = 0.45f; e.b[E_LIDB] = 0; e.b[E_LIDT] = 0;
    e.b[E_LIDTA] = -0.2f; e.f[F_MCURVE] = -0.3f; e.f[F_MOPEN] = 0.35f; e.f[F_MWIDE] = 0.7f; e.f[F_SWEAT] = 1; }
  { auto& e = EX[SURPRISED]; e.b[E_SW] = 1.08f; e.b[E_SH] = 1.18f; e.b[E_PUPIL] = 0.6f;
    e.f[F_MCURVE] = 0; e.f[F_MOPEN] = 1; e.f[F_MWIDE] = 0.55f; }
  { auto& e = EX[SUSPICIOUS]; e.L[E_LIDT] = 0.45f; e.L[E_LIDB] = 0.3f; e.L[E_LIDTA] = 0.15f;
    e.R[E_LIDT] = 0.15f; e.R[E_LIDB] = 0.1f; e.R[E_SH] = 1.05f; e.b[E_LOOKX] = 0.7f;
    e.f[F_MCURVE] = -0.15f; e.f[F_MOPEN] = 0; e.f[F_MWIDE] = 0.6f; }
  { auto& e = EX[PROUD]; e.b[E_LIDT] = 0.3f; e.b[E_LIDB] = 0.25f; e.b[E_LIDBC] = 0.7f; e.b[E_LOOKY] = -0.3f;
    e.f[F_MCURVE] = 0.9f; e.f[F_MOPEN] = 0; e.f[F_MWIDE] = 0.9f; e.f[F_Y] = -3; }
  { auto& e = EX[CURIOUS]; e.L[E_SH] = 1.1f; e.L[E_SW] = 1.05f; e.R[E_SH] = 0.9f; e.R[E_LIDT] = 0.15f;
    e.b[E_LOOKX] = -0.3f; e.b[E_LOOKY] = -0.2f; e.b[E_TILT] = 0.12f;
    e.f[F_MCURVE] = 0.2f; e.f[F_MOPEN] = 0.15f; e.f[F_MWIDE] = 0.5f; e.f[F_ROT] = 0.1f; }
  { auto& e = EX[DIZZY]; e.b[E_MORPH] = 1; e.shape = SH_SPIRAL;
    e.f[F_MCURVE] = -0.2f; e.f[F_MOPEN] = 0.25f; e.f[F_MWIDE] = 0.8f; }
  { auto& e = EX[XX]; e.b[E_MORPH] = 1; e.shape = SH_X; e.f[F_MCURVE] = -0.5f; e.f[F_MOPEN] = 0.5f; e.f[F_MWIDE] = 0.6f; }
  { auto& e = EX[FOCUSED]; e.b[E_LIDT] = 0.28f; e.b[E_LIDB] = 0.22f; e.b[E_LIDTA] = 0.1f; e.b[E_PUPIL] = 0.8f;
    e.f[F_MCURVE] = 0; e.f[F_MOPEN] = 0; e.f[F_MWIDE] = 0.5f; }
  { auto& e = EX[HUNGRY]; e.b[E_LOOKY] = 0.4f; e.b[E_SH] = 1.05f; e.b[E_PUPIL] = 1.25f; e.b[E_LIDT] = 0.1f;
    e.b[E_LIDTA] = -0.25f; e.f[F_MCURVE] = -0.2f; e.f[F_MOPEN] = 0.45f; e.f[F_MWIDE] = 0.7f; }
  exReady = true;
}
}  // namespace

// ---------------------------------------------------------------- face
uint8_t* Face::iris_[2] = {nullptr, nullptr};

void Face::buildIris() {
  const float step = 2 * IRIS_R / IRIS_N;
  for (int i = 0; i < 2; i++) {
    uint8_t* t = static_cast<uint8_t*>(bigAlloc(IRIS_N * IRIS_N * 2));
    if (!t) return;
    for (int ky = 0; ky < IRIS_N; ky++)
      for (int kx = 0; kx < IRIS_N; kx++) {
        const float nx = -IRIS_R + (kx + 0.5f) * step, ny = -IRIS_R + (ky + 0.5f) * step;
        const float rr = len(nx, ny), ang = atan2f(ny, nx);
        const float fib = (vnoise(ang * 9 + i * 5, rr * 3) - 0.5f) * 0.35f;
        uint8_t* c = t + (ky * IRIS_N + kx) * 2;
        c[0] = (uint8_t)(smooth(0.0f, 0.55f, rr + fib * 0.4f) * 255 + 0.5f);
        c[1] = (uint8_t)(smooth(0.45f, 1.05f, rr) * 0.85f * 255 + 0.5f);
      }
    iris_[i] = t;
  }
}

void Face::begin(Screen* s, uint32_t seed) {
  if (!exReady) initExpr();
  if (!iris_[1]) buildIris();
  scr = s;
  L = PORTRAIT;
  skin = &MARBLE;
  mouthOn = true;
  t = 0;
  for (int i = 0; i < 2; i++) {
    eyes[i].side = i == 0 ? -1 : 1;
    for (int k = 0; k < E_N; k++) eyes[i][k].init(EYE_DEF[k], 260, 0.62f);
  }
  for (int k = 0; k < F_N; k++) f[k].init(FACE_DEF[k], 160, 0.6f);
  shapes[0] = shapes[1] = SH_EYE;
  pupilShape = P_ROUND;
  hasEyeColor = hasEyeDark = hasEyeLight = false;
  extraEyes = compound = 0;
  autoBlink = autoLook = true;
  nextBlink_ = 1.2;
  nextLook_ = 0.8;
  blinkT_ = -1;
  visible = 1;
  hidden = false;
  mouthShape = M_SMILE;
  chomp = 0;
  googly = false;
  grav[0] = 0;
  grav[1] = 1;
  for (auto& g : gp)
    for (float& v : g) v = 0;
  r_ = Rng(seed);
  expr = NEUTRAL;
  set(NEUTRAL, SNAP);
}

Face& Face::set(Ex name, int flags) {
  const ExprDef& ex = EX[name];
  expr = name;
  const bool snap = flags & SNAP, keep = flags & KEEP, keepLook = flags & KEEPLOOK;
  for (int i = 0; i < 2; i++) {
    Eye& e = eyes[i];
    const float* per = i == 0 ? ex.L : ex.R;
    for (int key = 0; key < E_N; key++) {
      if (key == E_OPEN || key == E_SPIN) continue;
      if ((key == E_LOOKX || key == E_LOOKY) && keepLook) continue;
      float v = EYE_DEF[key];
      const bool inB = ex.b[key] != UNSET, inP = per[key] != UNSET;
      if (inB) v = ex.b[key];
      if (inP) v = per[key];
      if (keep && !inB && !inP) continue;
      if (snap) e[key].snap(v);
      else e[key].to(v);
    }
  }
  if (ex.shape != SH_NONE) shapes[0] = shapes[1] = ex.shape;
  for (int key = 0; key < F_N; key++) {
    const bool has = ex.f[key] != UNSET;
    if (key == F_X || key == F_Y || key == F_SX || key == F_SY || key == F_ROT || key == F_MSHOW || key == F_GLOW) {
      if (has) {
        if (snap) f[key].snap(ex.f[key]);
        else f[key].to(ex.f[key]);
      } else if (!keep && (key == F_ROT || key == F_Y || key == F_X)) {
        f[key].to(0);
      }
      continue;
    }
    const float v = has ? ex.f[key] : FACE_DEF[key];
    if (snap) f[key].snap(v);
    else f[key].to(v);
  }
  return *this;
}

Face& Face::eyeSet(int key, float v, int which, float k, float z) {
  for (int i = 0; i < 2; i++)
    if (which == 2 || which == i) eyes[i][key].to(v, k, z);
  return *this;
}
Face& Face::eyeSnap(int key, float v, int which) {
  for (int i = 0; i < 2; i++)
    if (which == 2 || which == i) eyes[i][key].snap(v);
  return *this;
}
Face& Face::look(float x, float y, float k) {
  for (auto& e : eyes) {
    e[E_LOOKX].to(x, k, 0.75f);
    e[E_LOOKY].to(y, k, 0.75f);
  }
  return *this;
}
void Face::setEyeColors(const C3* eye, const C3* dark, const C3* light) {
  hasEyeColor = eye != nullptr;
  hasEyeDark = dark != nullptr;
  hasEyeLight = light != nullptr;
  if (eye) eyeColor = *eye;
  if (dark) eyeDark = *dark;
  if (light) eyeLight = *light;
}

void Face::update(double dt) {
  t += dt;
  if (autoBlink) {
    nextBlink_ -= dt;
    if (nextBlink_ <= 0) {
      blink();
      nextBlink_ = 1.8 + r_() * 3.6;
      if (r_() < 0.18f) nextBlink_ = 0.32;
    }
  }
  if (autoLook) {
    nextLook_ -= dt;
    if (nextLook_ <= 0) {
      const float r = r_();
      if (r < 0.35f) {
        look(0, 0);
      } else {
        const float a = (r_() * 2 - 1) * 0.85f;
        const float b = (r_() * 2 - 1) * 0.55f;
        look(a, b);
      }
      nextLook_ = 0.6 + r_() * 2.2;
    }
  }
  if (blinkT_ >= 0) {
    blinkT_ += dt;
    const float p = (float)(blinkT_ / blinkD_);
    const float o = p < 0.35f ? 1 - Ease::inCubic(p / 0.35f) : Ease::outCubic((p - 0.35f) / 0.65f);
    for (auto& e : eyes) e[E_OPEN].snap(clampf(o, 0, 1));
    if (p >= 1) {
      blinkT_ = -1;
      for (auto& e : eyes) e[E_OPEN].snap(1);
    }
  }
  for (auto& e : eyes)
    for (int k = 0; k < E_N; k++)
      if (k != E_OPEN) e[k].step(dt);
  for (auto& s : f) s.step(dt);
  for (auto& e : eyes) e[E_SPIN].x += dt * 5;
  if (googly) googlyStep(dt);
  for (int i = 0; i < 2; i++)
    if (eyes[i][E_MORPH].t == 0 && eyes[i][E_MORPH].x < 0.02f) shapes[i] = SH_EYE;
}

void Face::googlyStep(float dt) {
  for (int i = 0; i < 2; i++) {
    Eye& e = eyes[i];
    float* g = gp[i];
    const float w = L.ew * e[E_SW].x, h = L.eh * e[E_SH].x, pr = minf(w, h) * 0.56f;
    const float Rx = w * 0.95f - pr, Ry = h * 0.95f - pr;
    int n = (int)ceilf(dt / 0.004f);
    if (n < 1) n = 1;
    const float hs = dt / n;
    for (int k = 0; k < n; k++) {
      g[2] += grav[0] * 900 * hs;
      g[3] += grav[1] * 900 * hs;
      const float damp = expf(-2.2f * hs);
      g[2] *= damp;
      g[3] *= damp;
      g[0] += g[2] * hs;
      g[1] += g[3] * hs;
      const float ex = g[0] / maxf(Rx, 1), ey = g[1] / maxf(Ry, 1), r = sqrtf(ex * ex + ey * ey);
      if (r > 1) {
        g[0] /= r;
        g[1] /= r;
        const float nx = ex / r, ny = ey / r, vn = g[2] * nx + g[3] * ny;
        if (vn > 0) {
          g[2] -= 1.45f * vn * nx;
          g[3] -= 1.45f * vn * ny;
          g[2] *= 0.96f;
          g[3] *= 0.96f;
        }
      }
    }
  }
}

void Face::drawBackground() { scr->fillGradient(skin->bg, skin->bg2, skin->grain); }

XF Face::xf() const {
  return XF{L.cx + f[F_X].x, L.cy + f[F_Y].x, f[F_SX].x, f[F_SY].x, cosf(f[F_ROT].x), sinf(f[F_ROT].x)};
}

P2 Face::eyePos(int i) const {
  const Eye& e = eyes[i];
  const XF X = xf();
  const float lx = e.side * L.sep / 2 + e[E_DX].x + e[E_LOOKX].x * 3;
  const float ly = e[E_DY].x + e[E_LOOKY].x * 2.5f;
  return P2{X.cx + (X.cs * lx * X.sx - X.sn * ly * X.sy), X.cy + (X.sn * lx * X.sx + X.cs * ly * X.sy)};
}

void Face::draw(bool noBg) {
  if (!noBg) drawBackground();
  if (hidden || visible <= 0) return;
  if (f[F_BLUSH].x > 0.01f) drawBlush();
  ProfMark pm;  // eyes and mouth are counted inside PR_FACE too
  for (int i = 0; i < 2; i++) drawEye(i);
  if (extraEyes > 0.01f) drawExtraEyes();
  pm.to(PR_EYES);
  if (mouthOn && f[F_MSHOW].x > 0.02f) drawMouth();
  pm.to(PR_MOUTH);
  if (f[F_SWEAT].x > 0.02f) drawSweat();
  if (f[F_TEARS].x > 0.02f) drawTears();
}

void Face::drawEye(int i) {
  const Skin& s = *skin;
  const XF X = xf();
  Eye& e = eyes[i];
  const int side = e.side;
  const P2 P = eyePos(i);
  const float rot = f[F_ROT].x + e[E_TILT].x * side;
  const float cs = cosf(rot), sn = sinf(rot);
  const float open = e[E_OPEN].x;
  const float w = L.ew * e[E_SW].x * X.sx, h0 = L.eh * e[E_SH].x * X.sy;
  const float morph = clampf(e[E_MORPH].x, 0, 1.2f);
  const Shape shape = shapes[i];
  const float h = h0;
  const float lidT = 1 - (1 - e[E_LIDT].x) * (open < 1 ? powf(maxf(open, 0), 0.9f) : 1);
  const float lidB = e[E_LIDB].x * maxf(open, 0.3f);
  const float r = minf(w, h) * e[E_ROUND].x;
  e.lidT_ = clampf(lidT, 0, 1);
  e.lidB_ = clampf(lidB, 0, 1);
  const float lookX = e[E_LOOKX].x, lookY = e[E_LOOKY].x;
  const float slT = e[E_LIDTA].x * -side, slB = e[E_LIDBA].x * side;
  const float inT = 1 / sqrtf(1 + slT * slT), inB = 1 / sqrtf(1 + slB * slB);
  const float lidTc = e.lidT_, lidBc = e.lidB_, bulge = e[E_LIDBC].x * (h * 0.6f), iww = 1 / (w * w);
  const float S = maxf(w, h0);
  const bool morphing = morph > 0.001f && shape != SH_EYE && shape != SH_SPIRAL;
  const float mk = clampf(morph, 0, 1);
  auto shapeSD = [&](float u, float v) {
    float d = SD::roundBox(u, v, w, h, r);
    const float dT = (-h + 2 * h * lidTc + slT * u - v) * inT;
    const float yB = h - 2 * h * lidBc + slB * u - bulge * (1 - u * u * iww);
    const float dB = (v - yB) * inB;
    d = max3(d, dT, dB);
    if (morphing) {
      float tt;
      switch (shape) {
        case SH_HEART: tt = SD::heart(u, v + S * 0.02f, S * 0.92f); break;
        case SH_STAR: tt = SD::star(u, v + S * 0.08f, S * 1.08f, 0.5f); break;
        case SH_ARC: tt = SD::arc(u, v + S * 0.35f, S * 0.62f, 1.05f, S * 0.15f); break;
        case SH_X:
          tt = minf(SD::segment(u, v, -S * 0.55f, -S * 0.55f, S * 0.55f, S * 0.55f),
                    SD::segment(u, v, -S * 0.55f, S * 0.55f, S * 0.55f, -S * 0.55f)) - S * 0.15f;
          break;
        case SH_DOT: tt = SD::circle(u, v, S * 0.28f); break;
        case SH_LINE: tt = SD::roundBox(u, v, S * 0.75f, S * 0.1f, S * 0.1f); break;
        case SH_RING: tt = fabsf(SD::circle(u, v, S * 0.62f)) - S * 0.16f; break;
        case SH_CROSS:
          tt = minf(SD::roundBox(u, v, S * 0.16f, S * 0.7f, S * 0.16f), SD::roundBox(u, v, S * 0.7f, S * 0.16f, S * 0.16f));
          break;
        default: tt = d;
      }
      d = mixf(d, tt, mk);
    }
    return d;
  };
  auto toLocal = [&](float x, float y) {
    const float dx = x - P.x, dy = y - P.y;
    return P2{cs * dx + sn * dy, -sn * dx + cs * dy};
  };
  auto fe = [&](float x, float y) {
    const P2 q = toLocal(x, y);
    return shapeSD(q.x, q.y);
  };
  // The work box: the rotated eye, or room for a morph shape (the review page used a looser 1.45x box).
  float bx, by;
  if (morphing) {
    bx = by = S * 1.12f + 5;
  } else {
    bx = fabsf(cs) * w + fabsf(sn) * h + 4;
    by = fabsf(sn) * w + fabsf(cs) * h + 4;
  }
  const float x0 = P.x - bx, x1 = P.x + bx, y0 = P.y - by, y1 = P.y + by;
  const C3 base = hasEyeColor ? eyeColor : s.eye;
  const float mt = smooth(0.2f, 0.8f, morph);

  if (shape == SH_SPIRAL && morph > 0.01f) {
    // dizzy: shrink the eye away, grow a turning spiral
    const float k = clampf(morph, 0, 1);
    if (k < 0.99f) eyeBody(i, fe, x0, y0, x1, y1, base, P, w, h, lookX, lookY, toLocal, 1 - k, open, 0, SH_EYE, cs, sn);
    const float S2 = maxf(w, h0) * 0.95f * k, b = S2 / (2.6f * TAU), swd = maxf(1.6f, S2 * 0.12f);
    const float spin = e[E_SPIN].x * side;
    scr->shape(P.x - S2 - 3, P.y - S2 - 3, P.x + S2 + 3, P.y + S2 + 3, [&](float x, float y) {
      const float dx = x - P.x, dy = y - P.y, rr = len(dx, dy);
      float ph = fastAtan2(dy, dx) * side + spin;
      ph = wrapf(ph, TAU);
      const float n = floorf((rr / b - ph) / TAU + 0.5f);
      const float dd = fabsf(rr - b * (ph + TAU * n));
      return maxf(dd - swd / 2, rr - S2);
    }, s.pupil, 1);
    return;
  }
  C3 col = base;
  if (morph > 0.3f && shape == SH_HEART) col = mixc(base, s.heart, mt);
  else if (morph > 0.3f && shape == SH_STAR) col = mixc(base, hex("#ffd23a"), mt);
  eyeBody(i, fe, x0, y0, x1, y1, col, P, w, h, lookX, lookY, toLocal, 1, open, morph, shape, cs, sn);
}

template <class F, class TL>
void Face::eyeBody(int i, F& f, float x0, float y0, float x1, float y1, C3 col, P2 P, float w, float h, float lookX,
                   float lookY, TL& toLocal, float alpha, float open, float morph, Shape shape, float cs, float sn) {
  const Skin& s = *skin;
  Eye& e = eyes[i];
  const bool plain = morph < 0.5f || shape == SH_EYE;
  // screen position of an eye-local point
  auto toScreen = [&](float u, float v) { return P2{P.x + cs * u - sn * v, P.y + sn * u + cs * v}; };
  if (googly && plain) {
    const C3 white = hex("#fffdf6"), ink = s.pupil;
    scr->rimShade(x0, y0, x1, y1, f, 1.8f, s.rimC, [&](float, float, float) { return white; }, alpha);
    const float* g = gp[i];
    const float pr = minf(w, h) * 0.56f;
    const float pcx = P.x + g[0], pcy = P.y + g[1];
    scr->shape(pcx - pr - 2, pcy - pr - 2, pcx + pr + 2, pcy + pr + 2,
               [&](float x, float y) {
                 const float d = SD::circle(x - pcx, y - pcy, pr);
                 return d >= 0.5f ? d : maxf(d, f(x, y));
               }, ink, alpha);
    scr->shape(pcx - pr, pcy - pr, pcx + pr, pcy + pr,
               [&](float x, float y) { return SD::circle(x - pcx + pr * 0.35f, y - pcy + pr * 0.38f, pr * 0.24f); },
               white, alpha);
    maybeLash(i, P, w, h, open, toLocal);
    return;
  }
  // dark painted rim, a touch outside the eye
  const float pr = minf(w, h) * 0.44f * e[E_PUPIL].x;
  const float pxo = lookX * (w - pr) * 0.62f, pyo = lookY * (h - pr) * 0.5f;
  const C3 dark = hasEyeDark ? eyeDark : s.eyeDark, light = hasEyeLight ? eyeLight : s.eyeLight;
  const float comp = compound, iwh = 1 / maxf(w, h), ih = 1 / h;
  const uint8_t* iris = iris_[i];
  if (!plain || !iris) {  // a shape (or no memory for the iris table): one flat colour
    scr->rimShade(x0, y0, x1, y1, f, s.rim, s.rimC, [&](float, float, float) { return col; }, alpha);
  } else {
    const float tsc = iwh * (IRIS_N / (2 * IRIS_R)), half = IRIS_N / 2.f;
    scr->rimShade(x0, y0, x1, y1, f, s.rim, s.rimC, [&](float x, float y, float d) {
      const P2 q = toLocal(x, y);
      const float u = q.x, v = q.y;
      // iris: lighter around the pupil, darker towards the edge, and fibres (from the table)
      int tx = (int)((u - pxo) * tsc + half), ty = (int)((v - pyo) * tsc + half);
      tx = tx < 0 ? 0 : tx >= IRIS_N ? IRIS_N - 1 : tx;
      ty = ty < 0 ? 0 : ty >= IRIS_N ? IRIS_N - 1 : ty;
      const uint8_t* cell = iris + (ty * IRIS_N + tx) * 2;
      const float edge3 = d > -3 ? smooth(-3, 0, d) : 0, edge2 = d > -2 ? smooth(-2, 0, d) : 0;
      C3 c = mixc(light, col, cell[0] * (1.f / 255));
      c = mixc(c, dark, cell[1] * (1.f / 255) + edge3 * 0.5f);
      // lower half catches light through the glass
      c = mixc(c, light, smooth(0.2f, 0.95f, v * ih) * 0.18f * (1 - edge2));
      if (comp > 0) {
        const float hd = SD::hexCell(u + 40, v + 40, 5.2f);
        c = mixc(c, scalec(c, 0.25f), smooth(-1.1f, 0.3f, hd) * comp);
        c = mixc(c, light, smooth(-3.2f, -2.2f, hd) * (1 - smooth(-2.2f, -1.2f, hd)) * 0.55f * comp *
                               smooth(-0.6f, 0.2f, -v * ih));
      }
      return c;
    }, alpha);
  }
  // pupil
  if (plain && pupilShape != P_NONE) {
    const P2 pc = toScreen(pxo, pyo);
    const float pe = pr * 1.6f + 2;
    scr->shape(pc.x - pe, pc.y - pe, pc.x + pe, pc.y + pe, [&](float x, float y) {
      const P2 q = toLocal(x, y);
      const float u = q.x - pxo, v = q.y - pyo;
      float d;
      if (pupilShape == P_SLIT) d = SD::ellipse(u, v, pr * 0.32f, pr * 1.55f);
      else if (pupilShape == P_BAR) d = SD::roundBox(u, v, pr * 1.35f, pr * 0.42f, pr * 0.4f);
      else d = SD::circle(u, v, pr);
      return d >= 0.5f ? d : maxf(d, f(x, y));  // same result: max(d, ...) >= 0.5 draws nothing
    }, s.pupil, alpha);
  }
  // catchlights: fixed to the light, they don't follow the gaze. That is what makes it glass.
  if (open > 0.35f) {
    const float ca = plain ? 1 : 0.85f;
    const P2 c1 = toScreen(-w * 0.36f, -h * 0.42f), c2 = toScreen(w * 0.3f, h * 0.32f);
    const float e1 = maxf(w * 0.2f, h * 0.15f) + 2, r2 = minf(w, h) * 0.09f, e2 = r2 + 2;
    scr->shape(c1.x - e1, c1.y - e1, c1.x + e1, c1.y + e1, [&](float x, float y) {
      const P2 q = toLocal(x, y);
      const float d = SD::ellipse(q.x + w * 0.36f, q.y + h * 0.42f, w * 0.2f, h * 0.15f);
      return d >= 0.5f ? d : maxf(d, f(x, y));
    }, s.catchC, alpha * ca);
    scr->shape(c2.x - e2, c2.y - e2, c2.x + e2, c2.y + e2, [&](float x, float y) {
      const P2 q = toLocal(x, y);
      const float d = SD::circle(q.x - w * 0.3f, q.y - h * 0.32f, r2);
      return d >= 0.5f ? d : maxf(d, f(x, y));
    }, s.catchC, alpha * ca * 0.9f);
  }
  // closed: a lash line where the lids meet
  maybeLash(i, P, w, h, open, toLocal);
}

template <class TL>
void Face::maybeLash(int i, P2 P, float w, float h, float open, TL& toLocal) {
  Eye& e = eyes[i];
  const float meet = e.lidT_ + e.lidB_;
  const float a = maxf(open < 0.3f ? 1 - open / 0.3f : 0, clampf((meet - 0.82f) / 0.16f, 0, 1));
  if (a <= 0.01f) return;
  // closed eye: a lid line where the lids meet, low in the eye
  const float meetY = clampf(h * (e.lidT_ - e.lidB_), -h * 0.6f, h * 0.8f);
  const float ra = w * 0.9f, bottom = meetY + 2, c = bottom - ra;
  scr->shape(P.x - w - 4, P.y - h - 4, P.x + w + 4, P.y + h + 4, [&](float x, float y) {
    const P2 q = toLocal(x, y);
    return SD::arc(q.x, -(q.y - c), ra, 0.95f, 1.5f);
  }, skin->rimC, a);
}

void Face::drawExtraEyes() {
  // spider: two pairs of small eyes above the big ones, and one more pair at the sides
  static const float spots[6][3] = {{-1.25f, -1.35f, 0.34f}, {1.25f, -1.35f, 0.34f}, {-0.45f, -1.62f, 0.26f},
                                    {0.45f, -1.62f, 0.26f},  {-1.6f, -0.3f, 0.22f},  {1.6f, -0.3f, 0.22f}};
  const Skin& s = *skin;
  const XF X = xf();
  for (const auto& sp : spots) {
    const float x = X.cx + sp[0] * L.sep / 2, y = X.cy + sp[1] * L.eh, r = sp[2] * L.ew * extraEyes;
    if (r < 0.5f) continue;
    scr->shape(x - r - 3, y - r - 3, x + r + 3, y + r + 3,
               [&](float px, float py) { return SD::circle(px - x, py - y, r + s.rim); }, s.rimC);
    scr->shape(x - r - 2, y - r - 2, x + r + 2, y + r + 2,
               [&](float px, float py) { return SD::circle(px - x, py - y, r); }, s.pupil);
    scr->shape(x - r, y - r, x + r, y + r,
               [&](float px, float py) { return SD::circle(px - x + r * 0.35f, py - y + r * 0.35f, r * 0.3f); },
               s.catchC);
  }
}

void Face::drawBlush() {
  const Skin& s = *skin;
  const XF X = xf();
  const float a = f[F_BLUSH].x;
  for (int side = -1; side <= 1; side += 2) {
    const float x = X.cx + side * (L.sep / 2 + 6) * X.sx, y = X.cy + L.eh * 1.05f * X.sy;
    const float rx = L.ew * 0.62f, ry = L.ew * 0.3f;
    scr->shape(x - rx - 4, y - ry - 4, x + rx + 4, y + ry + 4,
               [&](float px, float py) { return SD::ellipse(px - x, py - y, rx, ry) - 2; }, s.blush, 0.55f * a);
  }
}

void Face::drawMouth() {
  const Skin& s = *skin;
  const XF X = xf();
  const float k = f[F_MSHOW].x;
  const float mx = X.cx + sinf(f[F_ROT].x) * -(L.my - L.cy), my = X.cy + (L.my - L.cy + f[F_MY].x) * X.sy;
  const float W = 11 * f[F_MWIDE].x * X.sx * k, curve = f[F_MCURVE].x, open = clampf(f[F_MOPEN].x, 0, 1.2f) * k;
  const float lw = 1.7f;
  const C3 ink = s.mouth;
  if (mouthShape == M_CAT) {
    scr->shape(mx - W * 1.2f, my - W, mx + W * 1.2f, my + W, [&](float x, float y) {
      const float u = x - mx, v = y - my;
      return minf(SD::arc(u + W * 0.48f, -(v - W * 0.05f), W * 0.5f, 1.2f, lw * 0.6f),
                  SD::arc(u - W * 0.48f, -(v - W * 0.05f), W * 0.5f, 1.2f, lw * 0.6f));
    }, ink);
    return;
  }
  if (mouthShape == M_WAVE) {
    const float ph = t * 14;
    scr->shape(mx - W - 2, my - 5, mx + W + 2, my + 5, [&](float x, float y) {
      const float u = x - mx, v = y - my;
      if (fabsf(u) > W) return len(fabsf(u) - W, v);
      return fabsf(v - sinf(u / W * PIF * 2 + ph) * 1.6f) - lw * 0.6f;
    }, ink);
    return;
  }
  if (mouthShape == M_CHOMP) {
    const float c = chomp;
    const float hh = 2 + c * 8, ww = W * (1.05f - c * 0.15f);
    auto f2 = [&](float x, float y) { return SD::roundBox(x - mx, y - my - hh * 0.3f, ww, hh, minf(hh, 5)); };
    scr->shape(mx - W - 3, my - 12, mx + W + 3, my + 16, [&](float x, float y) { return f2(x, y) - lw * 0.7f; }, ink);
    if (c > 0.2f)
      scr->shape(mx - W, my - 10, mx + W, my + 14, [&](float x, float y) { return f2(x, y) + 0.5f; }, s.mouthIn);
    return;
  }
  // parametric smile / frown / open mouth. y(u) is the lip line: + curve lifts the corners (smile).
  const float bend = curve * 4.2f, iW2 = 1 / (W * W);
  auto q = [&](float u) { return 1 - u * u * iW2; };
  auto yLine = [&](float u) { return my + bend * q(u) - bend * 0.5f; };
  const float up = clampf(5 - 4 * curve, 1, 9), down = clampf(5 + 4 * curve, 1, 9);
  auto qe = [&](float u) { return sqrtf(maxf(0, q(u))); };
  auto yT = [&](float u) { return yLine(u) - open * up * qe(u); };
  auto yB = [&](float u) { return yLine(u) + open * down * qe(u); };
  const float yEnd = yLine(W);
  auto ends = [&](float x, float y) { return len(fabsf(x - mx) - W, y - yEnd); };
  if (open < 0.08f) {
    scr->shape(mx - W - 3, my - 9, mx + W + 3, my + 9, [&](float x, float y) {
      const float u = x - mx;
      if (fabsf(u) > W) return ends(x, y) - lw * 0.65f;
      const float sl = -2 * bend * u * iW2;
      return fabsf(y - yLine(u)) / sqrtf(1 + sl * sl) - lw * 0.65f;
    }, ink);
  } else {
    auto inside = [&](float x, float y) {
      const float u = x - mx;
      if (fabsf(u) >= W) return ends(x, y);
      return maxf(yT(u) - y, y - yB(u));
    };
    scr->shape(mx - W - 3, my - 14, mx + W + 3, my + 14, [&](float x, float y) { return inside(x, y) - lw * 0.7f; }, ink);
    if (open > 0.3f)
      scr->shape(mx - W, my - 12, mx + W, my + 12, [&](float x, float y) { return inside(x, y) + 0.6f; }, s.mouthIn);
    if (mouthShape == M_TONGUE && open > 0.2f) {
      const float ty = yB(0) - 2.5f;
      scr->shape(mx - 8, ty - 8, mx + 8, ty + 8, [&](float x, float y) {
        return maxf(SD::ellipse(x - mx, y - ty, W * 0.5f, 3.4f + open * 1.5f), inside(x, y) + 0.3f);
      }, s.tongue);
    }
  }
}

void Face::drawSweat() {
  const XF X = xf();
  const float a = f[F_SWEAT].x;
  const float x = X.cx + (L.sep / 2 + L.ew * 0.85f) * X.sx, y = X.cy - L.eh * 0.9f + sinf(t * 3) * 1.2f;
  const C3 c = hex("#bfeaff");
  scr->shape(x - 8, y - 12, x + 8, y + 8, [&](float px, float py) { return SD::drop(px - x, py - y, 3.6f * a, 0.4f, 7 * a); }, c, a);
  scr->shape(x - 4, y - 4, x + 4, y + 4, [&](float px, float py) { return SD::circle(px - x + 1.2f, py - y + 0.5f, 0.9f * a); },
             WHITE, a * 0.9f);
}

void Face::drawTears() {
  const float a = f[F_TEARS].x;
  const C3 c = hex("#bfeaff");
  for (int i = 0; i < 2; i++) {
    const P2 P = eyePos(i);
    const float h = L.eh * eyes[i][E_SH].x;
    const float x = P.x + eyes[i].side * L.ew * 0.45f, y0 = P.y + h * 0.55f;
    const float Lt = 30 * a, ph = t * 6;
    scr->shape(x - 4, y0 - 2, x + 4, y0 + Lt + 4, [&](float px, float py) {
      return SD::segment(px, py, x, y0, x + sinf(py * 0.3f + ph) * 0.6f, y0 + Lt) - 2.2f;
    }, c, 0.85f);
  }
}

// ---------------------------------------------------------------- particles
void FX::begin(Screen* s) {
  scr = s;
  if (!ps_) ps_ = static_cast<Pt*>(bigAlloc(sizeof(Pt) * MAX));
  n_ = 0;
  flash = 0;
  r_ = Rng(99);
}

Pt* FX::emit(const Pt& p) {
  if (!ps_ || n_ >= MAX) return nullptr;
  ps_[n_] = p;
  ps_[n_].age = 0;
  return &ps_[n_++];
}

void FX::burst(PType type, float x, float y, int n, const Pt& o) {
  for (int i = 0; i < n; i++) {
    const float a = o.angle + (r_() - 0.5f) * o.spread;
    const float sp = o.speed * (0.5f + r_() * 0.8f);
    Pt p = o;
    p.type = type;
    p.x = x + (r_() - 0.5f) * o.jitter;
    p.y = y + (r_() - 0.5f) * o.jitter;
    p.vx = cosf(a) * sp;
    p.vy = sinf(a) * sp;
    p.rot = r_() * TAU;
    p.vr = (r_() - 0.5f) * o.vrRange;
    p.size = o.size * (0.7f + r_() * 0.6f);
    const float rc = r_();
    if (o.colors && o.ncolors > 0) p.color = o.colors[(int)(rc * o.ncolors)];
    p.life = o.life * (0.7f + r_() * 0.6f);
    emit(p);
  }
}

void FX::update(float dt) {
  int j = 0;
  for (int i = 0; i < n_; i++) {
    Pt& p = ps_[i];
    p.age += dt;
    p.vy += p.g * dt;
    const float k = expf(-p.drag * dt);
    p.vx *= k;
    p.vy *= k;
    p.x += p.vx * dt;
    p.y += p.vy * dt;
    p.rot += p.vr * dt;
    p.size += p.grow * dt;
    if (p.age < p.life) {
      if (j != i) ps_[j] = p;
      j++;
    }
  }
  n_ = j;
  flash = maxf(0, flash - dt * 3);
}

void FX::draw() {
  Screen& S = *scr;
  for (int i = 0; i < n_; i++) {
    const Pt& p = ps_[i];
    const float lifeLeft = p.life - p.age;
    const float a = clampf(lifeLeft / p.fade, 0, 1) * clampf(p.age / 0.05f, 0, 1) * p.alpha;
    const float s = p.size, x = p.x, y = p.y, cs = cosf(p.rot), sn = sinf(p.rot);
    const C3 c = p.color;
    const float R = s * 1.6f + 3;
    auto loc = [&](float px, float py, float& u, float& v) {
      const float dx = px - x, dy = py - y;
      u = cs * dx + sn * dy;
      v = -sn * dx + cs * dy;
    };
    switch (p.type) {
      case PT_HEART:
        S.shape(x - R, y - R, x + R, y + R, [&](float px, float py) { float u, v; loc(px, py, u, v); return SD::heart(u, v, s); }, c, a);
        break;
      case PT_STAR:
        S.shape(x - R, y - R, x + R, y + R, [&](float px, float py) { float u, v; loc(px, py, u, v); return SD::star(u, v, s, 0.5f); }, c, a);
        break;
      case PT_SPARK:  // four-point twinkle
        S.shape(x - R, y - R, x + R, y + R, [&](float px, float py) {
          float u, v; loc(px, py, u, v);
          return minf(SD::ellipse(u, v, s, s * 0.22f), SD::ellipse(u, v, s * 0.22f, s));
        }, c, a);
        break;
      case PT_CONFETTI: {
        const float sq = 0.35f + 0.65f * fabsf(cosf(p.age * 9 + p.vr));
        // every pixel it covers is within the rectangle's half sizes (+0.5 for the anti-aliased edge)
        const float hu = s * 0.55f + 0.5f, hv = (s * 0.32f + 0.5f) / sq;
        const float bx = fabsf(cs) * hu + fabsf(sn) * hv + 1, by = fabsf(sn) * hu + fabsf(cs) * hv + 1;
        S.shape(x - bx, y - by, x + bx, y + by, [&](float px, float py) {
          float u, v; loc(px, py, u, v);
          return SD::roundBox(u, v * sq, s * 0.55f, s * 0.32f, 0.6f);
        }, c, a);
        break;
      }
      case PT_CIRCLE:
        S.shape(x - R, y - R, x + R, y + R, [&](float px, float py) { return SD::circle(px - x, py - y, s); }, c, a);
        break;
      case PT_RING: {
        const float wd = p.width ? p.width : 1.4f;
        S.shape(x - R, y - R, x + R, y + R, [&](float px, float py) { return fabsf(SD::circle(px - x, py - y, s)) - wd; }, c, a);
        break;
      }
      case PT_PUFF:
        S.shape(x - R, y - R, x + R, y + R, [&](float px, float py) {
          return SD::circle(px - x, py - y, s) + vnoise(px * 0.3f, py * 0.3f) * 2;
        }, c, a * 0.9f);
        break;
      case PT_DROP:
        S.shape(x - R, y - R, x + R, y + R, [&](float px, float py) {
          float u, v; loc(px, py, u, v);
          return SD::drop(u, v, s * 0.5f, s * 0.06f, s * 1.1f);
        }, c, a);
        break;
      case PT_SHARD:  // eggshell piece: a jagged triangle
        S.shape(x - R, y - R, x + R, y + R, [&](float px, float py) {
          float u, v; loc(px, py, u, v);
          const float d1 = SD::segment(u, v, -s, s * 0.5f, s * 0.8f, s * 0.7f);
          const float tri = maxf(-v - s * 0.6f + fabsf(u) * 0.9f, v - s * 0.7f);
          return minf(tri, d1 - 1);
        }, c, a);
        break;
      case PT_LINE: {
        const float wd = p.width ? p.width : 1.2f;
        S.shape(x - R * 2, y - R * 2, x + R * 2, y + R * 2, [&](float px, float py) {
          float u, v; loc(px, py, u, v);
          return SD::segment(u, v, -s, 0, s, 0) - wd;
        }, c, a);
        break;
      }
      case PT_NOTE:
        S.shape(x - R, y - R * 1.5f, x + R, y + R, [&](float px, float py) {
          float u, v; loc(px, py, u, v);
          return min3(SD::ellipse(u + s * 0.25f, v - s * 0.55f, s * 0.42f, s * 0.32f),
                      SD::roundBox(u - s * 0.12f, v + s * 0.1f, s * 0.09f, s * 0.7f, 0.3f),
                      SD::roundBox(u + s * 0.08f, v + s * 0.7f, s * 0.28f, s * 0.12f, 0.3f));
        }, c, a);
        break;
      case PT_TEXT: {
        if (!p.text) break;
        TextOpt o;
        o.font = p.font ? p.font : &FONT_SHOUT;
        o.size = p.fontSize ? p.fontSize : 14;
        o.color = p.textColor;
        o.outline = p.outline;
        o.oc = p.oc;
        o.rot = p.rot;
        o.alpha = a;
        o.scale = s / (p.size0 ? p.size0 : s);
        S.text(p.text, x, y, o);
        break;
      }
    }
  }
  if (flash > 0) S.lighten(minf(1, flash));
}

}  // namespace face
