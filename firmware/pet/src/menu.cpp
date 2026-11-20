#include "menu.h"

#include <math.h>

#include "face_assets.h"
#include "face_engine.h"
#include "moves.h"

namespace menu {
namespace {

struct Entry {
  const char* label;
  int icon;  // a face::Img, or -1 for the face (two little eyes)
};
const Entry kItems[COUNT] = {
    {"Golf", face::IMG_GOLFFLAG}, {"Dance", face::IMG_HONEYBEE}, {"Tricks", face::IMG_REMOTE},
    {"Sound", face::IMG_SOUND},   {"Face", -1}};

// Five choices between the title and the hints: centres 52, 86, 120, 154, 188.
constexpr float kTop = 52, kGap = 34;

// The picker's cards, in moves::Dance order. Names and colours are the scenes' own (face_scenes.cpp, each Power's
// name and burst), so the card the kid picks is the card the dance opens with.
struct Card {
  face::Img icon;
  const char* name;
  face::C3 burst;
};
const Card kCards[moves::DANCE_COUNT] = {
    {face::IMG_CHEETAH, "CHEETAH", face::hex("#ff4b3a")},
    {face::IMG_HONEYBEE, "HONEYBEE", face::hex("#ffd23a")},
    {face::IMG_HUMMINGBIRD, "HUMMINGBIRD", face::hex("#36d17c")},
    {face::IMG_SPIDER, "SPIDER", face::hex("#b678ff")},
    {face::IMG_OWL, "OWL", face::hex("#6f82ff")},
    {face::IMG_DOLPHIN, "DOLPHIN", face::hex("#7fd6ff")},
    {face::IMG_WHIRLIGIG, "WHIRLIGIG", face::hex("#a8e63a")},
};

// The picker's layout: the marble's centre and radius, the name under it, how far apart the cards slide, the dots.
constexpr float kCx = 67.5f, kMarbleY = 104, kMarbleR = 55, kRim = 3, kNameY = 182, kPitch = 175, kDotX = 130.5f,
                kDotGap = 10;

}  // namespace

const char* label(int item) { return item >= 0 && item < COUNT ? kItems[item].label : "?"; }

// The face's two little eyes (the Face item's icon, and the middle of the home ring), s = 1 at the menu's size.
static void faceIcon(face::Screen& scr, float cx, float cy, float s) {
  const face::Skin& k = face::MARBLE;
  for (int e = -1; e <= 1; e += 2) {
    const float ex = cx + e * 8 * s, ey = cy;
    scr.shape(ex - 8 * s, ey - 8 * s, ex + 8 * s, ey + 8 * s,
              [=](float x, float y) { return face::SD::circle(x - ex, y - ey, 6.8f * s); }, k.eye, 1);
    scr.shape(ex - 5 * s, ey - 5 * s, ex + 5 * s, ey + 5 * s,
              [=](float x, float y) { return face::SD::circle(x - ex, y - ey - 1 * s, 3.0f * s); }, k.pupil, 1);
    scr.shape(ex - 3 * s, ey - 5 * s, ex + 2 * s, ey,
              [=](float x, float y) { return face::SD::circle(x - ex + 1.2f * s, y - ey + 2.8f * s, 1.1f * s); },
              k.catchC, 1);
  }
}

void draw(face::Screen& scr, int selected, float t, bool soundOn, float soundPop) {
  using face::hex;
  const face::Skin& k = face::MARBLE;
  scr.fill(k.bg);

  face::TextOpt title;
  title.font = &face::FONT_SHOUT;
  title.size = 24;
  title.color = k.text;
  title.outline = 3;
  title.oc = k.textOutline;
  scr.text("Play!", 67.5f, 19, title);

  for (int i = 0; i < COUNT; ++i) {
    const bool sel = i == selected;
    const float cx = 67.5f, cy = kTop + i * kGap + (sel ? sinf(t * 5) * 1.5f : 0);
    const float hw = sel ? 58 : 54, hh = sel ? 15.5f : 14;
    if (sel)
      scr.shape(cx - hw - 5, cy - hh - 5, cx + hw + 5, cy + hh + 5,
                [=](float x, float y) { return face::SD::roundBox(x - cx, y - cy, hw + 3, hh + 3, 14); }, k.accent, 1);
    scr.shape(cx - hw - 1, cy - hh - 1, cx + hw + 1, cy + hh + 1,
              [=](float x, float y) { return face::SD::roundBox(x - cx, y - cy, hw, hh, 12); },
              sel ? hex("#fff6e5") : k.bg2, 1);
    const float ix = cx - 34;
    if (i == SOUND) {
      // The speaker: bouncing once when pressed; off, it's dimmed and crossed out in red (a tablet's mute sign).
      const float pop = soundPop > 0 && soundPop < 1 ? 1 + 0.3f * sinf(soundPop * 3.1416f) : 1;
      face::SpOpt o;
      o.scale(0.26f * pop);
      o.alpha = soundOn ? 1 : 0.45f;
      scr.sprite(face::IMAGES[kItems[i].icon], ix, cy, o);
      if (!soundOn) {
        const float r = 10.5f * pop;
        for (int pass = 0; pass < 2; ++pass)
          scr.shape(ix - r - 4, cy - r - 4, ix + r + 4, cy + r + 4, [=](float x, float y) {
            return face::SD::segment(x, y, ix - r, cy - r, ix + r, cy + r) - (pass ? 2.2f : 3.6f);
          }, pass ? hex("#ff3b5c") : hex("#4a1f04"), 1);
      }
    } else if (kItems[i].icon >= 0) {
      face::SpOpt o;
      o.scale(i == LAB ? 0.3f : 0.26f);  // the remote is tall and thin: a little bigger, and tipped like a wand
      if (i == LAB) o.r(0.6f);
      scr.sprite(face::IMAGES[kItems[i].icon], ix, cy, o);
    } else {
      faceIcon(scr, ix, cy, 0.8f);
    }
    face::TextOpt o;
    o.font = &face::FONT_SHOUT;
    o.size = 19;
    o.color = sel ? k.ink : k.text;
    if (!sel) {
      o.outline = 2;
      o.oc = k.textOutline;
    }
    scr.text(i == SOUND && !soundOn ? "Quiet" : kItems[i].label, cx + 16, cy, o);
  }

  face::TextOpt hint;
  hint.font = &face::FONT_SAY;
  hint.size = 13;
  hint.color = k.text;
  hint.outline = 2;
  hint.oc = k.textOutline;
  scr.text("Tilt to choose", 67.5f, 216, hint);
  scr.text("Big button: go", 67.5f, 231, hint);
}

void drawHomeRing(face::Screen& scr, float k, float t) {
  using face::hex;
  const float cx = 67.5f, cy = 112, R = 34, in = fminf(1.0f, k * 5);
  const float end = fminf(1.0f, k) * 6.2831853f;
  // a dark disc, the track, the filled part (clockwise from the top), and the eyes in the middle
  scr.shape(cx - R - 12, cy - R - 12, cx + R + 12, cy + R + 12,
            [=](float x, float y) { return face::SD::circle(x - cx, y - cy, R + 9); }, hex("#2a1204"), 0.72f * in);
  scr.shape(cx - R - 6, cy - R - 6, cx + R + 6, cy + R + 6,
            [=](float x, float y) { return fabsf(face::len(x - cx, y - cy) - R) - 3.2f; }, hex("#fff6e5"), 0.25f * in);
  scr.shape(cx - R - 6, cy - R - 6, cx + R + 6, cy + R + 6, [=](float x, float y) {
    float a = atan2f(x - cx, cy - y);
    if (a < 0) a += 6.2831853f;
    return a <= end ? fabsf(face::len(x - cx, y - cy) - R) - 3.6f : 9.0f;
  }, face::MARBLE.accent, in);
  faceIcon(scr, cx, cy, 1.5f + 0.06f * sinf(t * 9) * k);
}

// --- The dance picker ----------------------------------------------------------------------------------------------
int pickCount() { return moves::DANCE_COUNT + 2; }
int pickBack() { return moves::DANCE_COUNT + 1; }
face::C3 danceColor(int d) { return d >= 0 && d < moves::DANCE_COUNT ? kCards[d].burst : face::C3{255, 255, 255}; }

namespace {

// Surprise!'s marble runs through every animal's colour.
face::C3 rainbowOf(float t) {
  const float p = t * 0.8f;
  const int i = static_cast<int>(p) % moves::DANCE_COUNT;
  return face::mixc(kCards[i].burst, kCards[(i + 1) % moves::DANCE_COUNT].burst, p - floorf(p));
}

// A glass marble, the eyes' look (DESIGN.md, Marble): the brown rim, the animal's colour inside with its sunburst
// turning (the scene's card has the same burst), and a shade on the side away from the light.
void marble(face::Screen& scr, float cx, float cy, float r, face::C3 col, bool rays, float rot) {
  using namespace face;
  const Skin& k = MARBLE;
  const float R = r + kRim;
  scr.shape(cx - R - 1, cy - R - 1, cx + R + 1, cy + R + 1,
            [=](float x, float y) { return SD::circle(x - cx, y - cy, R); }, k.rimC, 1);
  scr.shape(cx - r - 1, cy - r - 1, cx + r + 1, cy + r + 1,
            [=](float x, float y) { return SD::circle(x - cx, y - cy, r); }, mixc(col, k.text, 0.45f), 1);
  if (rays)
    scr.shape(cx - r - 1, cy - r - 1, cx + r + 1, cy + r + 1, [=](float x, float y) {
      const float dx = x - cx, dy = y - cy, d = len(dx, dy);
      const float w = wrapf((fastAtan2(dy, dx) + rot) / TAU * 14, 1);
      return maxf((fabsf(w - 0.5f) - 0.25f) * d * 0.6f, d - r);
    }, col, 0.75f);
  scr.shape(cx - r - 1, cy - r - 1, cx + r + 1, cy + r + 1, [=](float x, float y) {
    return maxf(SD::circle(x - cx, y - cy, r), -SD::circle(x - cx + r * 0.12f, y - cy + r * 0.18f, r * 0.98f));
  }, k.rimC, 0.22f);
}

// The glass's catchlight, fixed to the light at the upper left, over whatever is inside.
void catchlight(face::Screen& scr, float cx, float cy, float r) {
  const float x0 = cx - r * 0.5f, y0 = cy - r * 0.58f, c = cosf(-0.65f), s = sinf(-0.65f);
  scr.shape(x0 - 12, y0 - 12, x0 + 12, y0 + 12, [=](float x, float y) {
    const float u = (x - x0) * c + (y - y0) * s, v = -(x - x0) * s + (y - y0) * c;
    return face::SD::ellipse(u, v, 9, 5);
  }, face::WHITE, 0.8f);
}

// Text that fits the screen: the size steps down until it's at most `w` wide.
void fitText(face::Screen& scr, const char* s, float y, face::TextOpt o, float w) {
  while (o.size > 10 && face::Screen::textWidth(*o.font, o.size, s) > w) o.size -= 1;
  scr.text(s, kCx, y, o);
}

// One card: 0 Surprise!, then the dances, then Back. s scales it (the pick's bounce).
void card(face::Screen& scr, int c, float cy, float t, float s) {
  using namespace face;
  const Skin& k = MARBLE;
  const int d = c - 1;
  const float r = kMarbleR * s;
  const char* name;
  if (c == kPickSurprise) {
    marble(scr, kCx, cy, r, rainbowOf(t), true, t * 1.6f);
    TextOpt q;
    q.font = &FONT_SHOUT;
    q.size = 80 * s;
    q.color = hex("#ffd23a");
    q.outline = 3.5f;
    q.oc = k.textOutline;
    q.rot = sinf(t * 3) * 0.12f;
    scr.text("?", kCx, cy + 4, q);
    name = "SURPRISE!";
  } else if (d >= 0 && d < moves::DANCE_COUNT) {
    marble(scr, kCx, cy, r, kCards[d].burst, true, t * 0.6f);
    SpOpt o;
    o.scale(0.86f * s * (1 + sinf(t * 7) * 0.03f)).r(sinf(t * 3) * 0.06f);
    scr.sprite(IMAGES[kCards[d].icon], kCx, cy, o);
    name = kCards[d].name;
  } else {
    marble(scr, kCx, cy, r, k.bg2, false, 0);
    for (int e = -1; e <= 1; e += 2) {  // the robot's own eyes, looking back the way it came
      const float ex = kCx + e * 19 * s, ey = cy - 2, er = 14 * s, px = ex - 5 * s;
      scr.shape(ex - er - 1, ey - er - 1, ex + er + 1, ey + er + 1,
                [=](float x, float y) { return SD::circle(x - ex, y - ey, er); }, k.eye, 1);
      scr.shape(px - 8 * s, ey - 8 * s, px + 8 * s, ey + 8 * s,
                [=](float x, float y) { return SD::circle(x - px, y - ey, 6.5f * s); }, k.pupil, 1);
      scr.shape(px - 5, ey - 7, px + 3, ey + 1,
                [=](float x, float y) { return SD::circle(x - px + 2, y - ey + 3.5f, 2.2f * s); }, k.catchC, 1);
    }
    name = "BACK";
  }
  catchlight(scr, kCx, cy, r);
  TextOpt o;
  o.font = &FONT_SHOUT;
  o.size = 26;
  o.color = k.text;
  o.outline = 2.5f;
  o.oc = k.textOutline;
  o.scale = s;
  fitText(scr, name, cy + (kNameY - kMarbleY), o, 124);
}

}  // namespace

void drawMarble(face::Screen& scr, float cx, float cy, float r, face::C3 col, bool rays, float rot) {
  marble(scr, cx, cy, r, col, rays, rot);
}
void drawFace(face::Screen& scr, float cx, float cy, float s) { faceIcon(scr, cx, cy, s); }
void drawFit(face::Screen& scr, const char* s, float y, face::TextOpt o, float w) { fitText(scr, s, y, o, w); }

void drawPicker(face::Screen& scr, const PickView& v, float t) {
  using namespace face;
  const Skin& k = MARBLE;
  scr.fill(k.bg);
  const int n = pickCount();
  if (v.show >= 0) {
    card(scr, v.show + 1, kMarbleY, t, 1 + 0.16f * sinf(fminf(v.pop, 1.0f) * PIF));
  } else {
    const int first = static_cast<int>(floorf(v.pos));
    for (int c = first; c <= first + 1; ++c)
      if (c >= 0 && c < n && fabsf(c - v.pos) < 1) card(scr, c, kMarbleY + (c - v.pos) * kPitch, t, 1);
  }

  TextOpt title;
  title.font = &FONT_SHOUT;
  title.size = 24;
  title.color = k.text;
  title.outline = 3;
  title.oc = k.textOutline;
  scr.text("Dance!", kCx, 22, title);

  // Where you are in the deck: a dot per card down the right edge, yours in the eyes' teal.
  const float y0 = kMarbleY - (n - 1) * kDotGap / 2;
  for (int i = 0; i < n; ++i) {
    const bool on = i == v.sel;
    const float y = y0 + i * kDotGap, rr = on ? 3.2f : 2.0f;
    scr.shape(kDotX - 4, y - 4, kDotX + 4, y + 4, [=](float x, float yy) { return SD::circle(x - kDotX, yy - y, rr); },
              on ? k.accent : k.text, on ? 1 : 0.6f);
  }

  if (v.show >= 0) return;  // picked: no more choosing
  TextOpt hint;
  hint.font = &FONT_SAY;
  hint.size = 14;
  hint.color = k.text;
  hint.outline = 2;
  hint.oc = k.textOutline;
  scr.text("Tilt to choose", kCx, 210, hint);
  const char* go = v.sel == kPickSurprise ? "Big button: surprise!" : v.sel == pickBack() ? "Big button: back"
                                                                                          : "Big button: dance!";
  fitText(scr, go, 227, hint, 128);
}

}  // namespace menu
