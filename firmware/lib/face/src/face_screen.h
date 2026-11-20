// The 135x240 frame the face draws into. Pixels are RGB565 stored byte-swapped (panel order, the way M5Canvas keeps
// 16-bit sprites), so the host pushes the buffer unchanged. The frame is two halves (rows 0-119 and 120-239) that
// needn't be next to each other in memory: on the robot two whole frames don't fit in one piece of fast RAM. Every fill evaluates a signed distance per pixel, so
// edges are anti-aliased and shapes can morph. Port of Screen in art/face/engine.js.
#pragma once

#include <stdint.h>
#include <string.h>

#include "face_assets.h"
#include "face_core.h"

namespace face {

struct SpOpt {
  float sx = 1, sy = 1, rot = 0, alpha = 1, ax = 0.5f, ay = 0.5f, flash = 0;
  bool tinted = false;
  C3 tint{255, 255, 255};
  SpOpt& scale(float s) { sx = sy = s; return *this; }
  SpOpt& sxy(float x, float y) { sx = x; sy = y; return *this; }
  SpOpt& r(float v) { rot = v; return *this; }
  SpOpt& a(float v) { alpha = v; return *this; }
  SpOpt& anchor(float x, float y) { ax = x; ay = y; return *this; }
};

struct TextOpt {
  const Font* font = &FONT_SHOUT;
  float size = 20;
  C3 color{255, 255, 255};
  float outline = 0;
  C3 oc{0, 0, 0};
  float scale = 1, rot = 0, alpha = 1;
};

// Two-core drawing. The host sets splitRows to a function that runs job(ctx, 1) on its other core while it runs
// job(ctx, 0) itself, and returns when both have finished. Big fills then share their rows between the cores (one does
// the even rows, the other the odd ones). Null (the default, and on the PC): everything runs on the calling core.
// The per-pixel functions passed to shape() and the rest must only read shared state; all of them do.
using RowJob = void (*)(void* ctx, int half);
extern void (*splitRows)(RowJob job, void* ctx);

class Screen {
 public:
  static constexpr int W = 135, H = 240, N = W * H, HALF = H / 2;
  int w = W, h = H;
  float dim = 1;  // backlight 0..1 (the host applies it to the panel)

  // Where to draw: the top half (W x HALF pixels) and the bottom half. One buffer of W x H: setFrame(p, p + W * HALF).
  void setFrame(uint16_t* top, uint16_t* bottom) {
    half_[0] = top;
    half_[1] = bottom;
  }
  inline uint16_t* row(int y) const { return y < HALF ? half_[0] + y * W : half_[1] + (y - HALF) * W; }

  static inline uint16_t pack(float r, float g, float b) {
    const int ri = (int)clampf(r, 0, 255), gi = (int)clampf(g, 0, 255), bi = (int)clampf(b, 0, 255);
    const uint16_t v = (uint16_t)(((ri & 0xF8) << 8) | ((gi & 0xFC) << 3) | (bi >> 3));
    return (uint16_t)((v >> 8) | (v << 8));
  }
  static inline void unpack(uint16_t s, float& r, float& g, float& b) {
    const uint16_t v = (uint16_t)((s >> 8) | (s << 8));
    const int r5 = v >> 11, g6 = (v >> 5) & 63, b5 = v & 31;
    r = (float)((r5 << 3) | (r5 >> 2));
    g = (float)((g6 << 2) | (g6 >> 4));
    b = (float)((b5 << 3) | (b5 >> 2));
  }
  static inline void blendp(uint16_t* p, const C3& c, float a) {
    if (a >= 1) { *p = pack(c.r, c.g, c.b); return; }
    float r, g, b;
    unpack(*p, r, g, b);
    *p = pack(r + (c.r - r) * a, g + (c.g - g) * a, b + (c.b - b) * a);
  }
  inline void blend(int x, int y, const C3& c, float a) {
    if (a <= 0 || x < 0 || y < 0 || x >= w || y >= h) return;
    blendp(row(y) + x, c, a);
  }
  inline void add(int x, int y, const C3& c, float a) {
    if (a <= 0 || x < 0 || y < 0 || x >= w || y >= h) return;
    float r, g, b;
    uint16_t* p = row(y) + x;
    unpack(*p, r, g, b);
    *p = pack(r + c.r * a, g + c.g * a, b + c.b * a);
  }

  // Runs row(y) for y0..y1; split between the cores when the area (rows x cols) is worth it.
  template <class Row>
  static void forRows(int y0, int y1, int cols, Row&& row) {
    if (splitRows && y1 > y0 && (y1 - y0 + 1) * cols >= 384) {
      struct Ctx {
        Row* r;
        int y0, y1;
      } c{&row, y0, y1};
      splitRows([](void* p, int half) {
        Ctx& c = *static_cast<Ctx*>(p);
        for (int y = c.y0 + half; y <= c.y1; y += 2) (*c.r)(y);
      }, &c);
      return;
    }
    for (int y = y0; y <= y1; y++) row(y);
  }
  // Every pixel from a function: pix(x, y) -> C3 (no blending).
  template <class Pix>
  void paint(Pix pix) {
    forRows(0, h - 1, w, [&](int y) {
      uint16_t* r = row(y);
      for (int x = 0; x < w; x++) {
        const C3 c = pix(x, y);
        r[x] = pack(c.r, c.g, c.b);
      }
    });
  }

  void fill(C3 c);
  // Vertical gradient with grain (the grain also stops RGB565 banding). The last one drawn is cached.
  void fillGradient(C3 top, C3 bot, float grain);
  // White flash over everything (k 0..1), and darkening (multiply by k).
  void lighten(float k);
  void darken(float k);

  // Fill an SDF shape. f(x, y) -> distance at the pixel centre. The box limits the work.
  template <class F>
  void shape(float x0, float y0, float x1, float y1, F f, C3 c, float alpha = 1) {
    if (alpha <= 0) return;
    int ix0, iy0, ix1, iy1;
    if (!box(x0, y0, x1, y1, ix0, iy0, ix1, iy1)) return;
    forRows(iy0, iy1, ix1 - ix0 + 1, [&](int y) {
      const float fy = y + 0.5f;
      uint16_t* rp = row(y);
      for (int x = ix0; x <= ix1; x++) {
        const float d = f(x + 0.5f, fy);
        if (d < 0.5f) {
          const float a = clampf(0.5f - d, 0, 1) * alpha;
          if (a >= 1) { rp[x] = pack(c.r, c.g, c.b); continue; }
          float r, g, b;
          unpack(rp[x], r, g, b);
          rp[x] = pack(r + (c.r - r) * a, g + (c.g - g) * a, b + (c.b - b) * a);
        }
      }
    });
  }
  // Fill with a per-pixel colour: shader(x, y, d) -> C3.
  template <class F, class Sh>
  void shade(float x0, float y0, float x1, float y1, F f, Sh shader, float alpha = 1) {
    if (alpha <= 0) return;
    int ix0, iy0, ix1, iy1;
    if (!box(x0, y0, x1, y1, ix0, iy0, ix1, iy1)) return;
    forRows(iy0, iy1, ix1 - ix0 + 1, [&](int y) {
      const float fy = y + 0.5f;
      uint16_t* rp = row(y);
      for (int x = ix0; x <= ix1; x++) {
        const float d = f(x + 0.5f, fy);
        if (d < 0.5f) blendp(rp + x, shader(x + 0.5f, fy, d), clampf(0.5f - d, 0, 1) * alpha);
      }
    });
  }
  // A shaded shape with a rim around it, in one pass: the same pixels as shape(f - rim, rimC) then
  // shade(f, shader), with f evaluated once per pixel.
  template <class F, class Sh>
  void rimShade(float x0, float y0, float x1, float y1, F f, float rim, C3 rimC, Sh shader, float alpha = 1) {
    if (alpha <= 0) return;
    int ix0, iy0, ix1, iy1;
    if (!box(x0, y0, x1, y1, ix0, iy0, ix1, iy1)) return;
    forRows(iy0, iy1, ix1 - ix0 + 1, [&](int y) {
      const float fy = y + 0.5f;
      uint16_t* rp = row(y);
      for (int x = ix0; x <= ix1; x++) {
        const float d = f(x + 0.5f, fy);
        if (d - rim >= 0.5f) continue;
        uint16_t* p = rp + x;
        if (d < 0.5f) {
          const float a = clampf(0.5f - d, 0, 1) * alpha;
          if (a < 1) blendp(p, rimC, clampf(0.5f - (d - rim), 0, 1) * alpha);  // fully covered: the rim never shows
          blendp(p, shader(x + 0.5f, fy, d), a);
        } else {
          blendp(p, rimC, clampf(0.5f - (d - rim), 0, 1) * alpha);
        }
      }
    });
  }
  // Soft additive glow around a shape: falls off over r pixels outside it.
  template <class F>
  void glow(float x0, float y0, float x1, float y1, F f, C3 c, float r, float strength) {
    int ix0, iy0, ix1, iy1;
    if (!box(x0, y0, x1, y1, ix0, iy0, ix1, iy1)) return;
    forRows(iy0, iy1, ix1 - ix0 + 1, [&](int y) {
      for (int x = ix0; x <= ix1; x++) {
        const float d = f(x + 0.5f, y + 0.5f);
        if (d > 0 && d < r * 3) add(x, y, c, fexp(-d / r) * strength);
      }
    });
  }

  // An RGBA bitmap placed by its anchor (a fraction of the bitmap) at (x, y), scaled and rotated, bilinear.
  // clip(x, y) -> false skips a pixel.
  template <class Clip>
  void sprite(const Bitmap& bmp, float x, float y, const SpOpt& o, Clip clip) {
    if (!bmp.w || o.alpha <= 0 || o.sx == 0 || o.sy == 0) return;
    const float cs = cosf(o.rot), sn = sinf(o.rot);
    // screen box from the four corners
    float bx0 = 1e9f, by0 = 1e9f, bx1 = -1e9f, by1 = -1e9f;
    for (int k = 0; k < 4; k++) {
      const float u = ((k & 1) ? (1 - o.ax) : -o.ax) * bmp.w * o.sx, v = ((k >> 1) ? (1 - o.ay) : -o.ay) * bmp.h * o.sy;
      const float dx = cs * u - sn * v, dy = sn * u + cs * v;
      bx0 = minf(bx0, x + dx); bx1 = maxf(bx1, x + dx); by0 = minf(by0, y + dy); by1 = maxf(by1, y + dy);
    }
    int ix0, iy0, ix1, iy1;
    if (!box(bx0 - 2, by0 - 2, bx1 + 2, by1 + 2, ix0, iy0, ix1, iy1)) return;
    const uint8_t* d = bmp.rgba;
    const int bw = bmp.w, bh = bmp.h;
    const float isx = 1 / o.sx, isy = 1 / o.sy, ox = o.ax * bw - 0.5f, oy = o.ay * bh - 0.5f;
    forRows(iy0, iy1, ix1 - ix0 + 1, [&](int py) {
      for (int pxx = ix0; pxx <= ix1; pxx++) {
        if (!clip(pxx + 0.5f, py + 0.5f)) continue;
        const float dx = pxx + 0.5f - x, dy = py + 0.5f - y;
        const float u = (cs * dx + sn * dy) * isx + ox, v = (-sn * dx + cs * dy) * isy + oy;
        if (u < -1 || v < -1 || u > bw || v > bh) continue;
        const float fu0 = ffloor(u), fv0 = ffloor(v);
        const int iu = (int)fu0, iv = (int)fv0;
        const float fu = u - fu0, fv = v - fv0;
        float r = 0, g = 0, b = 0, a = 0;
        for (int k = 0; k < 4; k++) {
          const int qx = iu + (k & 1), qy = iv + (k >> 1);
          if (qx < 0 || qy < 0 || qx >= bw || qy >= bh) continue;
          const float wgt = ((k & 1) ? fu : 1 - fu) * ((k >> 1) ? fv : 1 - fv);
          const uint8_t* q = d + (qy * bw + qx) * 4;
          const float aa = q[3] * (1.f / 255) * wgt;
          r += q[0] * aa; g += q[1] * aa; b += q[2] * aa; a += aa;
        }
        if (a <= 0.002f) continue;
        const float ia = 1 / a;
        C3 c{r * ia, g * ia, b * ia};
        if (o.tinted) c = C3{c.r * o.tint.r / 255, c.g * o.tint.g / 255, c.b * o.tint.b / 255};
        if (o.flash > 0) c = mixc(c, WHITE, o.flash);
        blendp(row(py) + pxx, c, a * o.alpha);
      }
    });
  }
  void sprite(const Bitmap& bmp, float x, float y, const SpOpt& o = SpOpt()) {
    sprite(bmp, x, y, o, [](float, float) { return true; });
  }

  // Text, centred on x; y is the visual middle of the line (baseline - 0.36 size), like the review page's text().
  void text(const char* s, float x, float y, const TextOpt& o);
  static float textWidth(const Font& f, float size, const char* s);

 private:
  bool box(float x0, float y0, float x1, float y1, int& ix0, int& iy0, int& ix1, int& iy1) const {
    ix0 = (int)maxf(0, floorf(x0));
    iy0 = (int)maxf(0, floorf(y0));
    ix1 = (int)minf((float)(w - 1), ceilf(x1));
    iy1 = (int)minf((float)(h - 1), ceilf(y1));
    return ix0 <= ix1 && iy0 <= iy1;
  }
  uint16_t* half_[2] = {nullptr, nullptr};
  uint16_t* bgCache_ = nullptr;
  bool bgValid_ = false;
  C3 bgTop_{-1, -1, -1}, bgBot_{-1, -1, -1}, lastTop_{-1, -1, -1}, lastBot_{-1, -1, -1};
  float bgGrain_ = -1, lastGrain_ = -1;
};

// Memory for big buffers (PSRAM on the robot when there is some).
void* bigAlloc(size_t bytes);

// Optional timing of the draw stages, for finding what is slow on the robot: the host sets profClock (microseconds);
// draws add into profUs, which the host reads and clears. Costs nothing while profClock is null.
enum ProfStage { PR_BG, PR_UNDER, PR_FACE, PR_MID, PR_FX, PR_OVER, PR_CAPTION, PR_POST, PR_EYES, PR_MOUTH, PR_N };
extern uint32_t (*profClock)();
extern uint32_t profUs[PR_N];
struct ProfMark {
  uint32_t t = profClock ? profClock() : 0;
  void to(ProfStage k) {
    if (!profClock) return;
    const uint32_t n = profClock();
    profUs[k] += n - t;
    t = n;
  }
};

}  // namespace face
