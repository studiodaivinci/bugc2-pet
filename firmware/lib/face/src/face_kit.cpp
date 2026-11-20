#include "face_kit.h"

namespace face {
namespace kit {

void sunburst(Sc& S, float cx, float cy, C3 col, float alpha, float rot, int n) {
  Screen& scr = *S.scr;
  scr.shape(0, 0, (float)scr.w, (float)scr.h, [&](float x, float y) {
    const float a = fastAtan2(y - cy, x - cx) + rot, k = wrapf((a / TAU) * n, 1);
    return (fabsf(k - 0.5f) - 0.25f) * len(x - cx, y - cy) * 0.6f;
  }, col, alpha);
}

void animalEyes(Sc& S, const Animal& o) {
  Face& f = S.face;
  C3 eye{}, dark{}, light{};
  if (o.eye) eye = hex(o.eye);
  if (o.dark) dark = hex(o.dark);
  if (o.light) light = hex(o.light);
  f.setEyeColors(o.eye ? &eye : nullptr, o.dark ? &dark : nullptr, o.light ? &light : nullptr);
  f.pupilShape = o.pupil;
  f.compound = o.compound;
  f.extraEyes = o.extra;
  for (auto& e : f.eyes) {
    e[E_SW].to(o.sw, 320, 0.45f);
    e[E_SH].to(o.sh, 320, 0.45f);
    e[E_ROUND].to(o.round);
    e[E_PUPIL].to(o.pupilSize);
  }
}

void powerCard(Sc& S, float t, Img key, const char* name, C3 burst) {
  const float tz = 1.45f;
  if (t > tz + 0.25f) return;
  Screen& scr = *S.scr;
  const Layout& L = S.L;
  const float k = Ease::outBack(prog(t, 0.05f, 0.45f)), out = prog(t, tz, 0.25f);
  sunburst(S, L.cx, 100, burst, 0.22f * (1 - out), t * 0.6f, 14);
  const float sc = k * (1 - Ease::inCubic(out)) * (1 + sinf(t * 7) * 0.03f);
  scr.sprite(A(key), L.cx, 100 - out * 6, SpOpt().scale(sc).r(sinf(t * 3) * 0.06f));
  const float nk = Ease::outBack(prog(t, 0.3f, 0.4f)) * (1 - out);
  float ns = 30;
  while (ns > 12 && ceilf(Screen::textWidth(FONT_SHOUT, ns, name)) + 10 > L.w - 4) ns -= 2;
  if (nk > 0.01f) {
    TextOpt o;
    o.font = &FONT_SHOUT;
    o.size = ns;
    o.color = S.skin->text;
    o.outline = 2.5f;
    o.oc = S.skin->textOutline;
    o.scale = nk;
    o.rot = sinf(t * 5) * 0.04f;
    scr.text(name, L.cx, 190, o);
  }
}

void zap(Sc& S, C3 col) {
  const P2 ps[2] = {eyeA(S), eyeB(S)};
  for (const P2& p : ps)
    for (int i = 0; i < 2; i++)
      S.fx.emit(Pt(PT_RING).at(p.x, p.y).sz(8 + i * 6.f).grw(140).lif(0.4f).fd(0.3f).col(col).wid(2));
  S.face.boing(0.45f);
}

void stars(Sc& S, float t, float cx, float cy, float rx, float ry, int n, C3 col) {
  for (int i = 0; i < n; i++) {
    const float a = t * 4 + i * TAU / n, x = cx + cosf(a) * rx, y = cy + sinf(a) * ry, s = 4 + sinf(a) * 1.5f;
    S.scr->shape(x - 9, y - 9, x + 9, y + 9, [&](float px, float py) { return SD::star(px - x, py - y, s, 0.5f); }, col, 1);
  }
}

void textPop(Sc& S, const char* s, float x, float y, const TP& o) {
  Pt p(PT_TEXT);
  p.text = s;
  p.x = x;
  p.y = y;
  p.vx = o.vx;
  p.vy = o.vy;
  p.life = o.life;
  p.fade = 0.3f;
  p.size = o.size;
  p.size0 = o.size;
  p.fontSize = o.size;
  p.font = o.font;
  p.textColor = o.hasColor ? o.color : S.skin->text;
  p.outline = o.outline;
  p.oc = o.hasOc ? o.oc : S.skin->textOutline;
  p.rot = o.rot;
  p.vr = o.vr;
  p.grow = o.grow;
  S.fx.emit(p);
}

Pt heartOpts(Sc& S) {
  return Pt().spd(70).grav(-40).drg(1.5f).sz(5).lif(1.3f).col(S.skin->heart).spin(2).jit(10).spr(2.2f);
}
Pt confettiOpts(Sc& S, float angle) {
  return Pt().ang(angle).spr(0.8f).spd(190).grav(160).drg(1.2f).sz(6).lif(2.2f).cols(S.skin->confetti, 5).spin(14);
}

// Crack lines: deterministic jagged paths from the egg's upper middle.
static const float CRACKS[4][5][2] = {
    {{0, -20}, {-6, -12}, {2, -6}, {-4, 2}, {1e9f, 0}},
    {{0, -20}, {8, -14}, {4, -8}, {12, -2}, {9, 5}},
    {{-4, 2}, {-14, 6}, {-10, 12}, {-22, 16}, {1e9f, 0}},
    {{9, 5}, {18, 10}, {14, 16}, {26, 20}, {24, 26}},
};

void drawCracks(Sc& S, float cx, float cy, int level, float s, float rot, C3 col) {
  const float cs = cosf(rot), sn = sinf(rot);
  for (int i = 0; i < level && i < 4; i++) {
    float pts[5][2];
    int n = 0;
    for (int j = 0; j < 5 && CRACKS[i][j][0] < 1e8f; j++, n++) {
      const float u = CRACKS[i][j][0], v = CRACKS[i][j][1];
      pts[j][0] = cx + (cs * u - sn * v) * s * 1.4f;
      pts[j][1] = cy + (sn * u + cs * v) * s * 1.4f - 8 * s;
    }
    for (int j = 0; j < n - 1; j++) {
      const float ax = pts[j][0], ay = pts[j][1], bx = pts[j + 1][0], by = pts[j + 1][1];
      S.scr->shape(minf(ax, bx) - 3, minf(ay, by) - 3, maxf(ax, bx) + 3, maxf(ay, by) + 3,
                   [&](float x, float y) { return SD::segment(x, y, ax, ay, bx, by) - 1.1f; }, col, 1);
    }
  }
}

void golfBall(Sc& S, float x, float y, float r, float sh) {
  Screen& scr = *S.scr;
  scr.shape(x - r - 3, y + r * 0.6f - 4, x + r + 3, y + r * 1.4f,
            [&](float px, float py) { return SD::ellipse(px - x, py - y - r * 0.95f, r * 0.9f, r * 0.3f); }, BLACK, 0.25f * sh);
  scr.shade(x - r - 2, y - r - 2, x + r + 2, y + r + 2, [&](float px, float py) { return SD::circle(px - x, py - y, r); },
            [&](float px, float py, float) {
              const float u = px - x, v = py - y;
              const float cu = ffloor(u / 3 + 0.5f), odd = ffmod(cu, 2.f);  // -1, 0 or 1, like JS's %
              const float du = cu * 3 - u, dv = floorf(v / 3 + odd * 0.5f + 0.5f) * 3 - v - odd * 1.5f;
              const float d = du * du + dv * dv;
              const float shade = 1 - clampf((u + v) / (r * 3), -0.1f, 0.35f);
              const C3 c = d < 0.9f ? C3{210, 214, 220} : C3{250, 250, 248};
              return C3{c.r * shade, c.g * shade, c.b * shade};
            });
}

void battery(Sc& S, float x, float y, float level, C3 col) {
  Screen& scr = *S.scr;
  const float w = 34, h = 16;
  scr.shape(x - w / 2 - 2, y - h / 2 - 2, x + w / 2 + 6, y + h / 2 + 2, [&](float px, float py) {
    return minf(fabsf(SD::roundBox(px - x, py - y, w / 2, h / 2, 4)) - 1.4f, SD::roundBox(px - x - w / 2 - 2, py - y, 2, 4, 1));
  }, contrast(S), 1);
  const float fw = (w - 6) * clampf(level, 0, 1);
  if (fw > 0.5f)
    scr.shape(x - w / 2 + 3, y - h / 2 + 3, x - w / 2 + 3 + fw, y + h / 2 - 3, [&](float px, float py) {
      return SD::roundBox(px - (x - w / 2 + 3 + fw / 2), py - y, fw / 2, h / 2 - 3, 1.5f);
    }, col, 1);
}

void bolt(Sc& S, float x, float y, float s, C3 col, float a) {
  S.scr->shape(x - 12 * s, y - 14 * s, x + 12 * s, y + 14 * s, [&](float px, float py) {
    const float u = (px - x) / s, v = (py - y) / s;
    const float d1 = maxf(maxf(-(u + 0.5f) + v * 0.55f - 1, (u + 0.5f) - v * 0.55f - 6), maxf(v - 1, -v - 11));
    const float d2 = maxf(maxf(-(u - 0.5f) + v * 0.55f + 6 - 6.5f, (u - 0.5f) - v * 0.55f - 0.5f), maxf(-v - 1, v - 11));
    return minf(d1, d2) * s;
  }, col, a);
}

}  // namespace kit
}  // namespace face
