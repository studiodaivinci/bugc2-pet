// Drawing into the 135x240 frame. Pixels are RGB565 stored byte-swapped, the way M5Canvas keeps 16-bit sprites,
// so the host can push the buffer to the panel unchanged. Everything here works on the PC build too.
#pragma once

#include <stdint.h>

namespace game {
namespace gfx {

constexpr int W = 135;
constexpr int H = 240;

// 565 in panel (byte-swapped) order.
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)) >> 8) |
                               ((((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)) << 8));
}

// A sprite: swapped-565 colours plus an optional 8-bit alpha plane (nullptr = opaque).
struct Image {
  uint16_t w, h;
  const uint16_t* px;
  const uint8_t* alpha;
};

// A full-screen or large picture, 8-bit indexed, LZ-packed (see art/game/tools/pack.py).
struct Packed {
  uint16_t w, h;
  const uint16_t* palette;  // 256 swapped-565 entries
  const uint8_t* data;
  uint32_t size;
};

// 4-bit antialiased bitmap font, ASCII 32..126 plus 127 drawn as a middle dot.
struct Glyph {
  uint32_t offset;  // into Font::bits, in nibbles
  uint8_t w, h;
  int8_t dx, dy;    // from the pen position (on the baseline) to the glyph's top-left
  uint8_t advance;
};
struct Font {
  uint8_t lineHeight;
  uint8_t ascent;   // baseline to top of capitals
  const Glyph* glyphs;
  const uint8_t* bits;
};

enum class Align : uint8_t { Left, Center, Right };

class Frame {
 public:
  explicit Frame(uint16_t* px = nullptr) : px_(px) {}
  void attach(uint16_t* px) { px_ = px; }
  uint16_t* pixels() const { return px_; }

  void fill(uint16_t c);
  void copyFrom(const uint16_t* src);  // a whole 135x240 frame
  void fillRect(int x, int y, int w, int h, uint16_t c);
  void blendRect(int x, int y, int w, int h, uint16_t c, uint8_t a);
  // Darkens an ellipse with soft edges (shadows). amount 0..255.
  void shade(float cx, float cy, float rx, float ry, uint8_t amount);
  // Antialiased filled circle.
  void disc(float cx, float cy, float r, uint16_t c, uint8_t a = 255);
  // A thick antialiased line with round caps.
  void line(float x0, float y0, float x1, float y1, float width, uint16_t c, uint8_t a = 255);

  // Sprites. x,y is the top-left. opacity scales the alpha plane.
  void blit(const Image& img, int x, int y, uint8_t opacity = 255, bool flipX = false);
  // Scaled about an anchor point given as a fraction of the image (0.5,1 = bottom centre). Nearest-neighbour with
  // the alpha plane kept, used for squash, stretch and pop-in.
  void blitScaled(const Image& img, float ax, float ay, float sx, float sy, float anchorX = 0.5f,
                  float anchorY = 0.5f, uint8_t opacity = 255, bool flipX = false);
  // Rotated (degrees, counter-clockwise on screen) and scaled about the image centre, bilinear alpha.
  void blitRotated(const Image& img, float cx, float cy, float degrees, float scale = 1, uint8_t opacity = 255);

  // Text. y is the baseline. outline > 0 draws a halo of that many pixels in `outlineColor` behind the letters.
  int text(const Font& f, int x, int y, const char* s, uint16_t c, Align align = Align::Left, int outline = 0,
           uint16_t outlineColor = 0);
  static int textWidth(const Font& f, const char* s);

  void plot(int x, int y, uint16_t c, uint8_t a);

 private:
  uint16_t* px_;
};

// Unpacks a Packed picture into a full frame buffer (its size must be 135x240).
void unpack(const Packed& p, uint16_t* dst);

uint16_t blend(uint16_t dst, uint16_t src, uint8_t a);  // both swapped-565
uint16_t darken(uint16_t c, uint8_t amount);

}  // namespace gfx
}  // namespace game
