#include "face_screen.h"

#include <stdlib.h>

#if defined(ARDUINO) || defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#endif

namespace face {

uint32_t (*profClock)() = nullptr;
void (*splitRows)(RowJob job, void* ctx) = nullptr;
uint32_t profUs[PR_N] = {0};

void* bigAlloc(size_t bytes) {
#if defined(ARDUINO) || defined(ESP_PLATFORM)
  void* p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (p) return p;
#endif
  return malloc(bytes);
}

void Screen::fill(C3 c) {
  const uint16_t v = pack(c.r, c.g, c.b);
  forRows(0, h - 1, w, [&](int y) {
    uint16_t* r = row(y);
    for (int x = 0; x < w; x++) r[x] = v;
  });
}

static bool same(const C3& a, const C3& b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

void Screen::fillGradient(C3 top, C3 bot, float grain) {
  const bool isLast = same(top, lastTop_) && same(bot, lastBot_) && grain == lastGrain_;
  lastTop_ = top;
  lastBot_ = bot;
  lastGrain_ = grain;
  if (bgValid_ && same(top, bgTop_) && same(bot, bgBot_) && grain == bgGrain_) {
    forRows(0, h - 1, w, [&](int y) { memcpy(row(y), bgCache_ + y * w, sizeof(uint16_t) * w); });
    return;
  }
  forRows(0, h - 1, w, [&](int y) {
    const float t = (float)y / (h - 1);
    const float r0 = mixf(top.r, bot.r, t), g0 = mixf(top.g, bot.g, t), b0 = mixf(top.b, bot.b, t);
    uint16_t* r = row(y);
    for (int x = 0; x < w; x++) {
      const float n = grain ? (hash2(x, y) - 0.5f) * grain : 0;
      r[x] = pack(r0 + n, g0 + n, b0 + n);
    }
  });
  // Keep a copy once the same gradient comes twice in a row (a still background), never for animated ones.
  if (isLast) {
    if (!bgCache_) bgCache_ = static_cast<uint16_t*>(bigAlloc(sizeof(uint16_t) * w * h));
    if (bgCache_) {
      for (int y = 0; y < h; y++) memcpy(bgCache_ + y * w, row(y), sizeof(uint16_t) * w);
      bgTop_ = top;
      bgBot_ = bot;
      bgGrain_ = grain;
      bgValid_ = true;
    }
  }
}

void Screen::lighten(float k) {
  k = minf(1, k);
  if (k <= 0) return;
  forRows(0, h - 1, w, [&](int y) {
    uint16_t* p = row(y);
    for (int x = 0; x < w; x++) {
      float r, g, b;
      unpack(p[x], r, g, b);
      p[x] = pack(r + (255 - r) * k, g + (255 - g) * k, b + (255 - b) * k);
    }
  });
}

void Screen::darken(float k) {
  forRows(0, h - 1, w, [&](int y) {
    uint16_t* p = row(y);
    for (int x = 0; x < w; x++) {
      float r, g, b;
      unpack(p[x], r, g, b);
      p[x] = pack(r * k, g * k, b * k);
    }
  });
}

// ---------------------------------------------------------------- text
namespace {
uint32_t nextCp(const char*& s) {
  const uint8_t c = (uint8_t)*s++;
  if (c < 0x80) return c;
  if ((c & 0xE0) == 0xC0 && *s) return ((c & 0x1F) << 6) | ((uint8_t)*s++ & 0x3F);
  if ((c & 0xF0) == 0xE0 && s[0] && s[1]) {
    const uint32_t v = ((c & 0x0F) << 12) | (((uint8_t)s[0] & 0x3F) << 6) | ((uint8_t)s[1] & 0x3F);
    s += 2;
    return v;
  }
  return '?';
}
const Glyph* findGlyph(const Font& f, uint32_t cp) {
  if (cp >= 32 && cp < 127 && (int)(cp - 32) < f.count && f.glyphs[cp - 32].cp == cp) return &f.glyphs[cp - 32];
  for (int i = 0; i < f.count; i++)
    if (f.glyphs[i].cp == cp) return &f.glyphs[i];
  return cp == '?' ? nullptr : findGlyph(f, '?');
}
float kernOf(const Font& f, uint32_t a, uint32_t b) {
  int lo = 0, hi = f.kernCount - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    const Kern& k = f.kern[mid];
    if (k.a == a && k.b == b) return k.dx;
    if (k.a < a || (k.a == a && k.b < b)) lo = mid + 1;
    else hi = mid - 1;
  }
  return 0;
}
// Bilinear sample of a glyph's distance cell; outside the cell is far outside.
inline float sample(const Font& f, const Glyph& g, float u, float v) {
  const float fu0 = ffloor(u), fv0 = ffloor(v);
  const int iu = (int)fu0, iv = (int)fv0;
  const float fu = u - fu0, fv = v - fv0;
  const uint8_t* d = f.sdf + g.off;
  auto at = [&](int x, int y) -> float { return (x < 0 || y < 0 || x >= g.w || y >= g.h) ? 0.f : (float)d[y * g.w + x]; };
  const float a = at(iu, iv), b = at(iu + 1, iv), c = at(iu, iv + 1), e = at(iu + 1, iv + 1);
  return mixf(mixf(a, b, fu), mixf(c, e, fu), fv);
}
}  // namespace

float Screen::textWidth(const Font& f, float size, const char* s) {
  float wsum = 0;
  uint32_t prev = 0;
  while (*s) {
    const uint32_t cp = nextCp(s);
    const Glyph* g = findGlyph(f, cp);
    if (!g) continue;
    if (prev) wsum += kernOf(f, prev, g->cp);
    wsum += g->adv;
    prev = g->cp;
  }
  return wsum * size / f.em;
}

void Screen::text(const char* s, float x, float y, const TextOpt& o) {
  if (o.scale <= 0 || o.alpha <= 0 || !*s) return;
  const Font& f = *o.font;
  const float k = o.size / f.em, sc = o.scale, isc = 1 / sc;
  const float cs = cosf(o.rot), sn = sinf(o.rot);
  const float base = 0.36f * o.size;
  const float W = textWidth(f, o.size, s);
  const float toDist = f.spread / 127.f * k * sc;  // table units -> screen px
  for (int pass = (o.outline > 0 ? 0 : 1); pass < 2; pass++) {
    const C3 col = pass == 0 ? o.oc : o.color;
    const float grow = pass == 0 ? o.outline * sc : 0;
    float pen = -W / 2;
    uint32_t prev = 0;
    for (const char* p = s; *p;) {
      const uint32_t cp = nextCp(p);
      const Glyph* gp = findGlyph(f, cp);
      if (!gp) continue;
      const Glyph& g = *gp;
      if (prev) pen += kernOf(f, prev, g.cp) * k;
      prev = g.cp;
      if (g.w) {
        const float gx0 = pen + g.x0 * k, gy0 = base + g.y0 * k, gw = g.w * k, gh = g.h * k;
        float bx0 = 1e9f, by0 = 1e9f, bx1 = -1e9f, by1 = -1e9f;
        for (int c = 0; c < 4; c++) {
          const float u = (gx0 + ((c & 1) ? gw : 0)) * sc, v = (gy0 + ((c >> 1) ? gh : 0)) * sc;
          const float dx = cs * u - sn * v, dy = sn * u + cs * v;
          bx0 = minf(bx0, x + dx); bx1 = maxf(bx1, x + dx); by0 = minf(by0, y + dy); by1 = maxf(by1, y + dy);
        }
        int ix0, iy0, ix1, iy1;
        if (box(bx0 - 1, by0 - 1, bx1 + 1, by1 + 1, ix0, iy0, ix1, iy1)) {
          const float ik = 1 / k;
          forRows(iy0, iy1, ix1 - ix0 + 1, [&](int py) {
            for (int pxx = ix0; pxx <= ix1; pxx++) {
              const float dx = pxx + 0.5f - x, dy = py + 0.5f - y;
              const float lu = (cs * dx + sn * dy) * isc, lv = (-sn * dx + cs * dy) * isc;
              const float v = sample(f, g, (lu - gx0) * ik - 0.5f, (lv - gy0) * ik - 0.5f);
              if (v <= 0) continue;
              const float dist = (128 - v) * toDist - grow;
              if (dist < 0.5f) blendp(row(py) + pxx, col, clampf(0.5f - dist, 0, 1) * o.alpha);
            }
          });
        }
      }
      pen += g.adv * k;
    }
  }
}

}  // namespace face
