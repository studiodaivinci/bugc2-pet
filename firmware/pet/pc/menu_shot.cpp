// PC check of the pet's own screens, drawn with the face library: the menu (each selection, the Sound switch off and
// bouncing, the home ring) and the dance picker
// (each card, then one mid-slide and the picked card's bounce), each as a PPM sheet.
// and Trick Lab (the strip in each state, then the remote's lesson).
//   bash firmware/pet/pc/build.sh && firmware/pet/pc/out/menu_shot.exe <menu.ppm> [<picker.ppm> [<lab.ppm>]]
#include <stdio.h>

#include <functional>
#include <vector>

#include "../src/lab.h"
#include "../src/menu.h"

namespace {

constexpr int W = face::Screen::W, H = face::Screen::H;

bool writeSheet(const char* out, int n, const std::function<void(face::Screen&, int)>& draw) {
  std::vector<uint16_t> frame(W * H), sheet(W * H * n);
  face::Screen scr;
  for (int i = 0; i < n; ++i) {
    scr.setFrame(frame.data(), frame.data() + W * face::Screen::HALF);
    draw(scr, i);
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x) sheet[y * W * n + i * W + x] = frame[y * W + x];
  }
  FILE* f = fopen(out, "wb");
  if (!f) return false;
  fprintf(f, "P6\n%d %d\n255\n", W * n, H);
  for (uint16_t s : sheet) {
    float r, g, b;
    face::Screen::unpack(s, r, g, b);
    const unsigned char px[3] = {(unsigned char)r, (unsigned char)g, (unsigned char)b};
    fwrite(px, 1, 3, f);
  }
  fclose(f);
  printf("wrote %s (%d screens)\n", out, n);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  const char* out = argc > 1 ? argv[1] : "menu.ppm";
  // each selection with the sound on; Sound selected with it off, and mid-bounce; the home ring two-thirds full
  if (!writeSheet(out, menu::COUNT + 3, [](face::Screen& scr, int i) {
        if (i < menu::COUNT) menu::draw(scr, i, 0.3f, true);
        else if (i == menu::COUNT) menu::draw(scr, menu::SOUND, 0.3f, false);
        else if (i == menu::COUNT + 1) menu::draw(scr, menu::SOUND, 0.3f, true, 0.5f);
        else {
          menu::draw(scr, menu::DANCE, 0.3f, true);
          menu::drawHomeRing(scr, 0.66f, 0.3f);
        }
      }))
    return 1;
  if (argc > 2) {
    const int cards = menu::pickCount();
    const bool ok = writeSheet(argv[2], cards + 2, [cards](face::Screen& scr, int i) {
      menu::PickView v;
      if (i < cards) {  // each card at rest
        v.pos = static_cast<float>(i);
        v.sel = i;
      } else if (i == cards) {  // sliding from Surprise! to the cheetah
        v.pos = 0.4f;
        v.sel = 1;
      } else {  // the owl, picked, mid-bounce
        v.sel = 5;
        v.show = 4;
        v.pop = 0.5f;
      }
      menu::drawPicker(scr, v, 0.3f + i * 0.7f);
    });
    if (!ok) return 1;
  }
  if (argc > 3) {
    const bool ok = writeSheet(argv[3], 12, [](face::Screen& scr, int i) {
      lab::View v;
      const uint8_t moves[6] = {lab::DASH, lab::SPIN, lab::WIGGLE, lab::SCOOT, lab::SPIN, lab::DASH};
      auto fill = [&](int n) {
        v.trick.n = static_cast<uint8_t>(n);
        for (int k = 0; k < n; ++k) v.trick.steps[k] = moves[k];
      };
      const float t = 0.3f + i * 0.37f;
      switch (i) {
        case 0: break;  // empty
        case 1: v.anySaved = true; break;  // empty, with tricks remembered
        case 2:  // a press: the cheetah lands and dashes
          fill(1);
          v.status = lab::S_BUILD;
          v.active = 0;
          v.word = lab::DASH;
          v.pop = 0.6f;
          break;
        case 3: fill(3); v.status = lab::S_BUILD; v.word = lab::WIGGLE; break;
        case 4: fill(6); v.status = lab::S_FULL; break;
        case 5: fill(4); v.status = lab::S_PLAY; v.active = 1; v.word = lab::SPIN; break;
        case 6: fill(4); v.status = lab::S_PUTDOWN; break;
        case 7: fill(4); v.status = lab::S_REST; v.rest = 0.6f; break;
        case 8: fill(4); v.status = lab::S_DONE; break;
        case 9: fill(6); v.status = lab::S_NAMED; break;  // played: its name
        default: {
          lab::LearnView lv;
          lv.role = i == 10 ? lab::R_SPIN : lab::R_GO;
          lv.taken = i == 11 ? 0.3f : 0;
          lab::drawLearn(scr, lv, t);
          return;
        }
      }
      lab::draw(scr, v, t);
    });
    if (!ok) return 1;
    // and the Freeze card on its own sheet would be one more screen: it's the last role
  }
  return 0;
}
