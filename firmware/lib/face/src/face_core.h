// The face engine's math: colours, easing, noise and the 2-D signed distance functions every shape is drawn from.
// A line-for-line port of art/face/engine.js (the review page's prototype); names match it.
#pragma once

#include <math.h>
#include <stdint.h>

namespace face {

constexpr float PIF = 3.14159265358979f;  // not "PI": Arduino.h defines that as a macro
constexpr float TAU = PIF * 2;

inline float clampf(float x, float a, float b) { return x < a ? a : x > b ? b : x; }
// floorf without the library call (90 cycles on the ESP32, this is about 10). For |x| < 2^31.
inline float ffloor(float x) {
  const int i = (int)x;
  return (float)(i - (x < (float)i));
}
// fmodf (C and JS %: the result has a's sign) without the library call.
inline float ffmod(float a, float b) { return a - b * (float)(int)(a / b); }
// e^x, to about 0.1%, without the library call (300 cycles on the ESP32). For |x| < 80.
inline float fexp(float x) {
  const float t = x * 1.44269504f, fi = ffloor(t), f = t - fi;
  union {
    float f;
    int32_t i;
  } u{1 + f * (0.69314718f + f * (0.24022651f + f * (0.05550411f + f * 0.00961813f)))};
  u.i += (int32_t)fi << 23;
  return u.f;
}
inline float mixf(float a, float b, float t) { return a + (b - a) * t; }
inline float len(float x, float y) { return sqrtf(x * x + y * y); }
inline float smooth(float a, float b, float x) {
  const float t = clampf((x - a) / (b - a), 0, 1);
  return t * t * (3 - 2 * t);
}
inline float sgn(float x) { return x > 0 ? 1.f : x < 0 ? -1.f : 0.f; }
inline float minf(float a, float b) { return a < b ? a : b; }
inline float maxf(float a, float b) { return a > b ? a : b; }
inline float min3(float a, float b, float c) { return minf(a, minf(b, c)); }
inline float max3(float a, float b, float c) { return maxf(a, maxf(b, c)); }
// JS's ((a % b) + b) % b
inline float wrapf(float a, float b) { return a - b * ffloor(a / b); }
// Progress of t through [a, a+d], 0..1.
inline float prog(float t, float a, float d) { return clampf((t - a) / d, 0, 1); }
// "Did t cross a multiple of 1/rate this frame?" (the scenes' Math.floor(t*k) !== Math.floor((t-dt)*k))


// atan2 to within 1e-5 rad (Abramowitz and Stegun 4.4.49), several times faster than libm on the ESP32.
inline float fastAtan2(float y, float x) {
  const float ax = fabsf(x), ay = fabsf(y);
  const float mx = ax > ay ? ax : ay, mn = ax > ay ? ay : ax;
  if (mx == 0) return 0;
  const float a = mn / mx, s = a * a;
  float r = a * (0.9998660f + s * (-0.3302995f + s * (0.1801410f + s * (-0.0851330f + s * 0.0208351f))));
  if (ay > ax) r = 1.57079633f - r;
  if (x < 0) r = 3.14159265f - r;
  return y < 0 ? -r : r;
}

inline float hash2(int x, int y) {
  uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  h ^= h >> 16;
  return (float)(h >> 8) * (1.0f / 16777215.0f);
}
inline float vnoise(float x, float y) {
  const float fx = ffloor(x), fy = ffloor(y);
  const int xi = (int)fx, yi = (int)fy;
  const float xf = x - fx, yf = y - fy;
  const float u = xf * xf * (3 - 2 * xf), v = yf * yf * (3 - 2 * yf);
  return mixf(mixf(hash2(xi, yi), hash2(xi + 1, yi), u), mixf(hash2(xi, yi + 1), hash2(xi + 1, yi + 1), u), v);
}

// Deterministic random (xorshift), so a scene replays the same.
struct Rng {
  uint32_t s;
  explicit Rng(uint32_t seed = 1) : s(seed ? seed : 1) {}
  float operator()() {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return (float)(s >> 8) * (1.0f / 16777216.0f);
  }
};

namespace Ease {
inline float outCubic(float t) { t = 1 - clampf(t, 0, 1); return 1 - t * t * t; }
inline float inCubic(float t) { t = clampf(t, 0, 1); return t * t * t; }
inline float inOut(float t) {
  t = clampf(t, 0, 1);
  if (t < 0.5f) return 4 * t * t * t;
  const float u = -2 * t + 2;
  return 1 - u * u * u / 2;
}
inline float outBack(float t) {
  t = clampf(t, 0, 1);
  const float c1 = 1.9f, c3 = c1 + 1, u = t - 1;
  return 1 + c3 * u * u * u + c1 * u * u;
}
inline float bump(float t) { return sinf(clampf(t, 0, 1) * PIF); }
}  // namespace Ease

// ---------------------------------------------------------------- colour (0..255 floats, like the JS arrays)
struct C3 {
  float r, g, b;
};
constexpr int hexd(char c) { return c >= 'a' ? c - 'a' + 10 : c >= 'A' ? c - 'A' + 10 : c - '0'; }
constexpr C3 hex(const char* s) {
  return C3{float(hexd(s[1]) * 16 + hexd(s[2])), float(hexd(s[3]) * 16 + hexd(s[4])),
            float(hexd(s[5]) * 16 + hexd(s[6]))};
}
inline C3 mixc(C3 a, C3 b, float t) { return C3{mixf(a.r, b.r, t), mixf(a.g, b.g, t), mixf(a.b, b.b, t)}; }
inline C3 scalec(C3 a, float k) { return C3{a.r * k, a.g * k, a.b * k}; }
inline C3 hsl(float h, float s, float l) {
  const float a = s * minf(l, 1 - l);
  auto f = [&](float n) {
    const float k = ffmod(n + h * 12, 12);
    return l - a * maxf(-1, min3(k - 3, 9 - k, 1));
  };
  return C3{f(0) * 255, f(8) * 255, f(4) * 255};
}
constexpr C3 WHITE{255, 255, 255};
constexpr C3 BLACK{0, 0, 0};

// ---------------------------------------------------------------- signed distance functions (2-D, y down)
namespace SD {
inline float circle(float x, float y, float r) { return len(x, y) - r; }
// Ellipse, approximate (good to a pixel at these sizes).
inline float ellipse(float x, float y, float a, float b) {
  const float k0 = len(x / a, y / b), k1 = len(x / (a * a), y / (b * b));
  return k0 < 1e-5f ? -minf(a, b) : (k0 * (k0 - 1)) / k1;
}
inline float roundBox(float x, float y, float hw, float hh, float r) {
  r = min3(r, hw, hh);
  const float qx = fabsf(x) - hw + r, qy = fabsf(y) - hh + r;
  return len(maxf(qx, 0), maxf(qy, 0)) + minf(maxf(qx, qy), 0) - r;
}
inline float segment(float x, float y, float ax, float ay, float bx, float by) {
  const float pax = x - ax, pay = y - ay, bax = bx - ax, bay = by - ay;
  float dd = bax * bax + bay * bay;
  if (dd == 0) dd = 1;
  const float h = clampf((pax * bax + pay * bay) / dd, 0, 1);
  return len(pax - bax * h, pay - bay * h);
}
// Heart centred on (0,0), s = about half its height.
inline float heart(float x, float y, float s) {
  s *= 1.8f;
  const float px = fabsf(x) / s, py = -y / s + 0.55f;
  float d;
  if (py + px > 1) {
    d = len(px - 0.25f, py - 0.75f) - 1.41421356f / 4;
  } else {
    const float m = 0.5f * maxf(px + py, 0);
    d = sqrtf(minf(px * px + (py - 1) * (py - 1), (px - m) * (px - m) + (py - m) * (py - m))) * sgn(px - py);
  }
  return d * s;
}
// Five-point star, point up. r outer radius, rf inner ratio.
inline float star(float x, float y, float r, float rf) {
  const float k1x = 0.809016994375f, k1y = -0.587785252292f, k2x = -k1x, k2y = k1y;
  float px = fabsf(x), py = -y;
  const float d1 = maxf(k1x * px + k1y * py, 0);
  px -= 2 * d1 * k1x;
  py -= 2 * d1 * k1y;
  const float d2 = maxf(k2x * px + k2y * py, 0);
  px -= 2 * d2 * k2x;
  py -= 2 * d2 * k2y;
  px = fabsf(px);
  py -= r;
  const float bax = rf * -k1y, bay = rf * k1x - 1;
  const float h = clampf((px * bax + py * bay) / (bax * bax + bay * bay), 0, r);
  return len(px - bax * h, py - bay * h) * sgn(py * bax - px * bay);
}
// Arc stroke: centre (0,0), radius ra, half-aperture ang around straight up, thickness rb. Draws a cap shape.
inline float arc(float x, float y, float ra, float ang, float rb) {
  const float sx = sinf(ang), cy = cosf(ang);
  const float px = fabsf(x), py = -y;
  return (cy * px > sx * py ? len(px - sx * ra, py - cy * ra) : fabsf(len(px, py) - ra)) - rb;
}
// Egg, wide end down.
inline float egg(float x, float y, float ra, float rb) {
  const float k = 1.7320508f;
  const float px = fabsf(x), py = -y;
  const float r = ra - rb;
  return (py < 0 ? len(px, py) - r : k * (px + r) < py ? len(px, py - k * r) : len(px + r, py) - 2 * r) - rb;
}
// Drop: round end at (0,0) radius r1, tip at (0,-h) radius r2.
inline float drop(float x, float y, float r1, float r2, float h) {
  const float px = fabsf(x), py = -y;
  const float b = (r1 - r2) / h, a = sqrtf(1 - b * b);
  const float k = -b * px + a * py;
  if (k < 0) return len(px, py) - r1;
  if (k > a * h) return len(px, py - h) - r2;
  return a * px + b * py - r1;
}
// Distance to the edge of a hex cell (pointy-top) in a lattice of size s. Negative inside.
inline float hexCell(float x, float y, float s) {
  const float rx = 1, ry = 1.7320508f;
  const float ux = x / s, uy = y / s;
  const float ax = wrapf(ux, rx) - rx / 2, ay = wrapf(uy, ry) - ry / 2;
  const float bx = wrapf(ux - rx / 2, rx) - rx / 2, by = wrapf(uy - ry / 2, ry) - ry / 2;
  const bool ga = ax * ax + ay * ay < bx * bx + by * by;
  const float gx = fabsf(ga ? ax : bx), gy = fabsf(ga ? ay : by);
  const float c = maxf(gx * 0.5f + gy * 0.8660254f, gx);
  return (c - 0.5f) * s;
}
}  // namespace SD

}  // namespace face
