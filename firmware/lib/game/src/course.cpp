#include "course.h"

namespace game {

void decodeSurface(const Hole& h, uint8_t* out) {
  const uint32_t total = gfx::W * gfx::H;
  uint32_t o = 0;
  for (uint32_t i = 0; i + 1 < h.surfaceSize && o < total; i += 2) {
    uint32_t n = h.surface[i] + 1u;
    const uint8_t v = h.surface[i + 1];
    while (n-- && o < total) out[o++] = v;
  }
  while (o < total) out[o++] = kOut;
}

}  // namespace game
