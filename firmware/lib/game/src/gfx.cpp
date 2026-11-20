#include "gfx.h"

#include <math.h>
#include <string.h>

namespace game {
namespace gfx {

namespace {

inline uint16_t unswap(uint16_t c) { return static_cast<uint16_t>((c >> 8) | (c << 8)); }

inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

}  // namespace

uint16_t blend(uint16_t dst, uint16_t src, uint8_t a) {
  if (a == 255) return src;
  if (a == 0) return dst;
  const uint16_t d = unswap(dst), s = unswap(src);
  // Blend R+B and G in parallel (the classic 565 trick), 5-bit alpha.
  const uint32_t a5 = (a + 4) >> 3;
  uint32_t dx = (d | (static_cast<uint32_t>(d) << 16)) & 0x07E0F81F;
  const uint32_t sx = (s | (static_cast<uint32_t>(s) << 16)) & 0x07E0F81F;
  dx = (dx + (((sx - dx) * a5) >> 5)) & 0x07E0F81F;
  return unswap(static_cast<uint16_t>(dx | (dx >> 16)));
}

uint16_t darken(uint16_t c, uint8_t amount) { return blend(c, 0, amount); }

void Frame::plot(int x, int y, uint16_t c, uint8_t a) {
  if (static_cast<unsigned>(x) >= W || static_cast<unsigned>(y) >= H || a == 0) return;
  uint16_t& p = px_[y * W + x];
  p = blend(p, c, a);
}

void Frame::fill(uint16_t c) {
  for (int i = 0; i < W * H; ++i) px_[i] = c;
}

void Frame::copyFrom(const uint16_t* src) { memcpy(px_, src, W * H * 2); }

void Frame::fillRect(int x, int y, int w, int h, uint16_t c) {
  const int x0 = clampi(x, 0, W), x1 = clampi(x + w, 0, W);
  const int y0 = clampi(y, 0, H), y1 = clampi(y + h, 0, H);
  for (int yy = y0; yy < y1; ++yy)
    for (int xx = x0; xx < x1; ++xx) px_[yy * W + xx] = c;
}

void Frame::blendRect(int x, int y, int w, int h, uint16_t c, uint8_t a) {
  const int x0 = clampi(x, 0, W), x1 = clampi(x + w, 0, W);
  const int y0 = clampi(y, 0, H), y1 = clampi(y + h, 0, H);
  for (int yy = y0; yy < y1; ++yy)
    for (int xx = x0; xx < x1; ++xx) px_[yy * W + xx] = blend(px_[yy * W + xx], c, a);
}

void Frame::shade(float cx, float cy, float rx, float ry, uint8_t amount) {
  if (rx <= 0 || ry <= 0) return;
  const int x0 = clampi(static_cast<int>(floorf(cx - rx)), 0, W), x1 = clampi(static_cast<int>(ceilf(cx + rx)) + 1, 0, W);
  const int y0 = clampi(static_cast<int>(floorf(cy - ry)), 0, H), y1 = clampi(static_cast<int>(ceilf(cy + ry)) + 1, 0, H);
  for (int y = y0; y < y1; ++y) {
    const float dy = (y + 0.5f - cy) / ry;
    for (int x = x0; x < x1; ++x) {
      const float dx = (x + 0.5f - cx) / rx;
      const float d = dx * dx + dy * dy;
      if (d >= 1) continue;
      const float k = 1 - d;  // soft falloff to the rim
      px_[y * W + x] = darken(px_[y * W + x], static_cast<uint8_t>(amount * (k < 0.5f ? k * 2 : 1)));
    }
  }
}

void Frame::disc(float cx, float cy, float r, uint16_t c, uint8_t a) {
  const int x0 = clampi(static_cast<int>(floorf(cx - r - 1)), 0, W), x1 = clampi(static_cast<int>(ceilf(cx + r + 1)), 0, W);
  const int y0 = clampi(static_cast<int>(floorf(cy - r - 1)), 0, H), y1 = clampi(static_cast<int>(ceilf(cy + r + 1)), 0, H);
  for (int y = y0; y < y1; ++y)
    for (int x = x0; x < x1; ++x) {
      const float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
      float cov = r + 0.5f - sqrtf(dx * dx + dy * dy);
      if (cov <= 0) continue;
      if (cov > 1) cov = 1;
      plot(x, y, c, static_cast<uint8_t>(a * cov));
    }
}

void Frame::line(float x0, float y0, float x1, float y1, float width, uint16_t c, uint8_t a) {
  const float r = width * 0.5f;
  const int bx0 = clampi(static_cast<int>(floorf(fminf(x0, x1) - r - 1)), 0, W);
  const int bx1 = clampi(static_cast<int>(ceilf(fmaxf(x0, x1) + r + 1)), 0, W);
  const int by0 = clampi(static_cast<int>(floorf(fminf(y0, y1) - r - 1)), 0, H);
  const int by1 = clampi(static_cast<int>(ceilf(fmaxf(y0, y1) + r + 1)), 0, H);
  const float vx = x1 - x0, vy = y1 - y0, len2 = vx * vx + vy * vy;
  for (int y = by0; y < by1; ++y)
    for (int x = bx0; x < bx1; ++x) {
      const float px = x + 0.5f - x0, py = y + 0.5f - y0;
      float t = len2 > 0 ? (px * vx + py * vy) / len2 : 0;
      t = t < 0 ? 0 : (t > 1 ? 1 : t);
      const float dx = px - vx * t, dy = py - vy * t;
      float cov = r + 0.5f - sqrtf(dx * dx + dy * dy);
      if (cov <= 0) continue;
      if (cov > 1) cov = 1;
      plot(x, y, c, static_cast<uint8_t>(a * cov));
    }
}

void Frame::blit(const Image& img, int x, int y, uint8_t opacity, bool flipX) {
  for (int j = 0; j < img.h; ++j) {
    const int yy = y + j;
    if (static_cast<unsigned>(yy) >= H) continue;
    for (int i = 0; i < img.w; ++i) {
      const int xx = x + i;
      if (static_cast<unsigned>(xx) >= W) continue;
      const int si = j * img.w + (flipX ? img.w - 1 - i : i);
      uint8_t a = img.alpha ? img.alpha[si] : 255;
      if (opacity != 255) a = static_cast<uint8_t>((a * opacity + 127) / 255);
      if (a == 0) continue;
      uint16_t& p = px_[yy * W + xx];
      p = blend(p, img.px[si], a);
    }
  }
}

void Frame::blitScaled(const Image& img, float ax, float ay, float sx, float sy, float anchorX, float anchorY,
                       uint8_t opacity, bool flipX) {
  if (sx <= 0.01f || sy <= 0.01f) return;
  const float w = img.w * sx, h = img.h * sy;
  const float left = ax - w * anchorX, top = ay - h * anchorY;
  const int x0 = clampi(static_cast<int>(floorf(left)), 0, W), x1 = clampi(static_cast<int>(ceilf(left + w)), 0, W);
  const int y0 = clampi(static_cast<int>(floorf(top)), 0, H), y1 = clampi(static_cast<int>(ceilf(top + h)), 0, H);
  for (int y = y0; y < y1; ++y) {
    const int sj = static_cast<int>((y + 0.5f - top) / sy);
    if (sj < 0 || sj >= img.h) continue;
    for (int x = x0; x < x1; ++x) {
      int si = static_cast<int>((x + 0.5f - left) / sx);
      if (si < 0 || si >= img.w) continue;
      if (flipX) si = img.w - 1 - si;
      const int k = sj * img.w + si;
      uint8_t a = img.alpha ? img.alpha[k] : 255;
      if (opacity != 255) a = static_cast<uint8_t>((a * opacity + 127) / 255);
      if (a == 0) continue;
      uint16_t& p = px_[y * W + x];
      p = blend(p, img.px[k], a);
    }
  }
}

void Frame::blitRotated(const Image& img, float cx, float cy, float degrees, float scale, uint8_t opacity) {
  const float rad = degrees * 0.017453293f;
  const float c = cosf(rad), s = sinf(rad);
  const float hw = img.w * 0.5f, hh = img.h * 0.5f;
  const float reach = sqrtf(hw * hw + hh * hh) * scale + 1;
  const int x0 = clampi(static_cast<int>(cx - reach), 0, W), x1 = clampi(static_cast<int>(cx + reach) + 1, 0, W);
  const int y0 = clampi(static_cast<int>(cy - reach), 0, H), y1 = clampi(static_cast<int>(cy + reach) + 1, 0, H);
  const float inv = 1.0f / scale;
  for (int y = y0; y < y1; ++y)
    for (int x = x0; x < x1; ++x) {
      // Screen y points down, so a counter-clockwise turn on screen uses -degrees in these maths.
      const float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
      const float u = (c * dx - s * dy) * inv + hw;
      const float v = (s * dx + c * dy) * inv + hh;
      const int iu = static_cast<int>(floorf(u)), iv = static_cast<int>(floorf(v));
      if (iu < 0 || iv < 0 || iu >= img.w || iv >= img.h) continue;
      const int k = iv * img.w + iu;
      uint8_t a = img.alpha ? img.alpha[k] : 255;
      if (opacity != 255) a = static_cast<uint8_t>((a * opacity + 127) / 255);
      if (a == 0) continue;
      uint16_t& p = px_[y * W + x];
      p = blend(p, img.px[k], a);
    }
}

namespace {

const Glyph* glyphFor(const Font& f, char ch) {
  const unsigned char u = static_cast<unsigned char>(ch);
  if (u < 32 || u > 127) return &f.glyphs['?' - 32];
  return &f.glyphs[u - 32];  // 127 is the middle dot
}

inline uint8_t nibble(const uint8_t* bits, uint32_t index) {
  const uint8_t b = bits[index >> 1];
  return (index & 1) ? (b & 0x0F) : (b >> 4);
}

}  // namespace

int Frame::textWidth(const Font& f, const char* s) {
  int w = 0;
  for (; *s; ++s) w += glyphFor(f, *s)->advance;
  return w;
}

int Frame::text(const Font& f, int x, int y, const char* s, uint16_t c, Align align, int outline,
                uint16_t outlineColor) {
  const int width = textWidth(f, s);
  if (align == Align::Center) x -= width / 2;
  if (align == Align::Right) x -= width;
  // Outline first for every glyph, so a halo never covers a neighbouring letter.
  for (int pass = outline > 0 ? 0 : 1; pass < 2; ++pass) {
    int pen = x;
    for (const char* p = s; *p; ++p) {
      const Glyph* g = glyphFor(f, *p);
      for (int j = 0; j < g->h; ++j)
        for (int i = 0; i < g->w; ++i) {
          const uint8_t a4 = nibble(f.bits, g->offset + j * g->w + i);
          if (a4 == 0) continue;
          const int gx = pen + g->dx + i, gy = y + g->dy + j;
          if (pass == 0) {
            for (int oy = -outline; oy <= outline; ++oy)
              for (int ox = -outline; ox <= outline; ++ox) {
                if (ox * ox + oy * oy > outline * outline + outline) continue;
                plot(gx + ox, gy + oy, outlineColor, static_cast<uint8_t>(a4 * 17));
              }
          } else {
            plot(gx, gy, c, static_cast<uint8_t>(a4 * 17));
          }
        }
      pen += g->advance;
    }
  }
  return width;
}

void unpack(const Packed& p, uint16_t* dst) {
  // Tokens: 0nnnnnnn = n+1 literal palette indices follow; 1nnnnnnn dd dd = copy n+3 pixels from `d` pixels back.
  const uint32_t total = static_cast<uint32_t>(p.w) * p.h;
  uint32_t out = 0, in = 0;
  while (in < p.size && out < total) {
    const uint8_t t = p.data[in++];
    if (t & 0x80) {
      uint32_t n = (t & 0x7F) + 3;
      const uint32_t d = p.data[in] | (p.data[in + 1] << 8);
      in += 2;
      if (d == 0 || d > out) break;  // corrupt
      while (n-- && out < total) {
        dst[out] = dst[out - d];
        ++out;
      }
    } else {
      uint32_t n = t + 1;
      while (n-- && out < total && in < p.size) dst[out++] = p.palette[p.data[in++]];
    }
  }
}

}  // namespace gfx
}  // namespace game
