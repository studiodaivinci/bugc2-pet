#include "lab.h"

#include <math.h>

#include "face_assets.h"
#include "face_engine.h"
#include "menu.h"

namespace lab {
namespace {

namespace snd = sound;

// Each move's toy, colour and words. The colours are the animals' own (menu.cpp's cards; art/face/DESIGN.md, "Seven
// colours"), so a move on the strip looks like its animal's marble in the dance picker. GO is the eyes' teal and
// Freeze an icy blue: neither is an animal's colour, so a sticker on the remote can't be mistaken for one.
struct RoleInfo {
  int icon;  // a face::Img, or -1: GO's arrow, -2: Freeze's square
  const char* name;
  const char* does;
  const char* key;
  const char* shout;
  face::C3 color;
};
const RoleInfo kRoles[ROLE_COUNT] = {
    {face::IMG_CHEETAH, "CHEETAH", "dashes forward", "dash", "DASH!", face::hex("#ff4b3a")},
    {face::IMG_SPIDER, "SPIDER", "scoots back", "scoot", "EEK!", face::hex("#b678ff")},
    {face::IMG_WHIRLIGIG, "WHIRLIGIG", "spins", "spin", "SPIN!", face::hex("#a8e63a")},
    {face::IMG_HONEYBEE, "HONEYBEE", "wiggles", "wiggle", "WIGGLE!", face::hex("#ffd23a")},
    {-1, "GO!", "plays my trick", "go", "GO!", face::hex("#13b5ae")},
    {-2, "FREEZE!", "stops, or starts again", "freeze", "FREEZE!", face::hex("#8fdcff")},
};

// The moves themselves. Speeds and sizes are the dances' own, which ran on the floor on 2026-11-11: the cheetah's
// pounce, the spider's jump back with its shiver, the whirligig's spin, the bee's waggle. They began at about half a
// second each (dash 450, scoot 420, spin 550, wiggle 480 ms); after trying them they felt
// too short, so each is now half as long again or more. DRIVE eases in and out over 150 ms
// each (moves.cpp), so a 700 ms dash is about 550 ms at full speed, near 20 cm.
// The cost in motor time: six of these and a replay use up to twelve seconds. The motor guard allows twenty a
// minute (moves.cpp; it was ten until these got longer), so two long tricks back to back meet "Phew!" and its bar.
using moves::DRIVE;
using moves::LIGHTS;
using moves::Step;
const Step kMoves[MOVE_COUNT] = {
    {"dash", DRIVE, 36, 1, 700, 0, 0, moves::L_STREAK, 0, snd::T_DASH},
    {"scoot", DRIVE, 36, -1, 600, 60, 20, moves::L_EEK, 0, snd::T_SCOOT},
    {"spin", moves::SPIN, 42, 1, 1000, 0, 0, moves::L_HALO, 0, snd::T_SPIN},
    {"wiggle", moves::WAGGLE, 40, 1, 840, 60, 0, moves::L_WAGGLE, 0, snd::T_WIGGLE},
};
// Between two moves of a whole trick: long enough to read as two moves. After a spin, long enough to stop coasting
// (it turned another 594 degrees in the 300 ms after a sprint: docs/HARDWARE.md), or the next dash would set off
// while still turning.
constexpr uint16_t kBeatMs = 260, kAfterSpinMs = 800, kTadaMs = 800;
uint16_t beatAfter(int move) { return move == SPIN ? kAfterSpinMs : kBeatMs; }

const char* const kAdj[] = {"TURBO", "WOBBLY", "MEGA", "SNEAKY", "ZIPPY", "FUNKY", "DIZZY", "SUPER", "BOUNCY", "ROCKET",
                            "JELLY", "THUNDER", "FUZZY", "NINJA", "DISCO", "SILLY", "CRAZY", "MIGHTY", "SPARKLY", "TICKLY"};
const char* const kNoun[] = {"NOODLE", "PICKLE", "WAFFLE", "BANANA", "PRETZEL", "MEATBALL", "TACO", "SOCK", "NUGGET", "BURRITO",
                             "PANCAKE", "POTATO", "DONUT", "PENGUIN", "YETI", "TORNADO", "WOMBAT", "PIZZA", "MUFFIN", "GOBLIN"};
constexpr int kAdjN = sizeof(kAdj) / sizeof(kAdj[0]), kNounN = sizeof(kNoun) / sizeof(kNoun[0]);

// --- The screens' layout (135 x 240, the Marble skin, as menu.cpp) ---------------------------------------------------
constexpr float kCx = 67.5f;
constexpr float kSlotX[3] = {25, 67.5f, 110}, kSlotY[2] = {72, 118}, kSlotR = 17;
constexpr float kWordY = 164, kHintY = 206, kHintGap = 17, kNameY = 153, kNameGap = 25;

face::TextOpt shoutOpt(float size, face::C3 color) {
  face::TextOpt o;
  o.font = &face::FONT_SHOUT;
  o.size = size;
  o.color = color;
  o.outline = 3;
  o.oc = face::MARBLE.textOutline;
  return o;
}
face::TextOpt sayOpt() {
  face::TextOpt o;
  o.font = &face::FONT_SAY;
  o.size = 14;
  o.color = face::MARBLE.text;
  o.outline = 2;
  o.oc = face::MARBLE.textOutline;
  return o;
}

// A role in its marble: the animal's toy, GO's arrow or Freeze's square. `lit`: the sunburst behind it turns.
void roleMarble(face::Screen& scr, int role, float cx, float cy, float r, bool lit, float t) {
  using namespace face;
  if (role < 0 || role >= ROLE_COUNT) return;
  const RoleInfo& ri = kRoles[role];
  menu::drawMarble(scr, cx, cy, r, ri.color, lit, t * 1.6f);
  if (ri.icon >= 0) {
    SpOpt o;
    o.scale(0.86f * r / 55 * (lit ? 1 + sinf(t * 9) * 0.05f : 1)).r(lit ? sinf(t * 7) * 0.12f : 0);
    scr.sprite(IMAGES[ri.icon], cx, cy, o);
  } else if (ri.icon == -1) {  // a play arrow, cream, with the ink's edge
    for (int pass = 0; pass < 2; ++pass) {
      const float s = r * (pass ? 0.44f : 0.56f), ox = cx + r * 0.06f;
      scr.shape(cx - r, cy - r, cx + r, cy + r, [=](float x, float y) {
        // a triangle pointing right: inside all three edges
        const float px = x - ox, py = y - cy;
        return maxf(-px - s * 0.5f, maxf(px * 0.5f + py * 0.866f, px * 0.5f - py * 0.866f) - s * 0.5f);
      }, pass ? hex("#fff6e5") : MARBLE.textOutline, 1);
    }
  } else {  // a stop square
    for (int pass = 0; pass < 2; ++pass) {
      const float s = r * (pass ? 0.34f : 0.42f);
      scr.shape(cx - r, cy - r, cx + r, cy + r,
                [=](float x, float y) { return SD::roundBox(x - cx, y - cy, s, s, s * 0.3f); },
                pass ? hex("#fff6e5") : MARBLE.textOutline, 1);
    }
  }
  // the glass's catchlight, sized to the marble
  const float x0 = cx - r * 0.5f, y0 = cy - r * 0.58f, c = cosf(-0.65f), s = sinf(-0.65f), a = r * 0.17f, b = r * 0.09f;
  scr.shape(x0 - a - 2, y0 - a - 2, x0 + a + 2, y0 + a + 2, [=](float x, float y) {
    const float u = (x - x0) * c + (y - y0) * s, v = -(x - x0) * s + (y - y0) * c;
    return SD::ellipse(u, v, a, b);
  }, WHITE, 0.8f);
}

}  // namespace

bool Trick::operator==(const Trick& o) const {
  if (n != o.n) return false;
  for (int i = 0; i < n && i < kMaxSteps; ++i)
    if (steps[i] != o.steps[i]) return false;
  return true;
}

void nameOf(const Trick& t, const char*& adjective, const char*& noun) {
  adjective = noun = "";
  if (!t.n) return;
  uint32_t h = 2166136261u;  // FNV-1a over the moves, then stirred, so one move's difference changes both words
  for (int i = 0; i < t.n && i < kMaxSteps; ++i) h = (h ^ (t.steps[i] + 1)) * 16777619u;
  h ^= h >> 15;
  h *= 0x2c1b3c6du;
  h ^= h >> 12;
  adjective = kAdj[h % kAdjN];
  noun = kNoun[(h >> 9) % kNounN];
}
const char* roleName(int r) { return r >= 0 && r < ROLE_COUNT ? kRoles[r].name : "?"; }
const char* roleDoes(int r) { return r >= 0 && r < ROLE_COUNT ? kRoles[r].does : "?"; }
const char* roleKey(int r) { return r >= 0 && r < ROLE_COUNT ? kRoles[r].key : "?"; }
const char* moveShout(int m) { return m >= 0 && m < MOVE_COUNT ? kRoles[m].shout : ""; }
face::C3 roleColor(int r) { return r >= 0 && r < ROLE_COUNT ? kRoles[r].color : face::C3{255, 255, 255}; }

int build(const Trick& t, int only, Step* out) {
  int n = 0;
  if (only >= 0) {
    if (only < t.n && t.steps[only] < MOVE_COUNT) out[n++] = kMoves[t.steps[only]];
    return n;
  }
  for (int i = 0; i < t.n && i < kMaxSteps; ++i) {
    const int m = t.steps[i] < MOVE_COUNT ? t.steps[i] : 0;
    out[n++] = kMoves[m];
    if (i + 1 < t.n) out[n++] = Step{"beat", LIGHTS, 0, 0, beatAfter(m), 0, 0, moves::L_BLINK, 0, snd::NONE};
  }
  if (n) out[n++] = Step{"tada", LIGHTS, 0, 0, kTadaMs, 0, 0, moves::L_DIZZY, 0, snd::CHEER};
  return n;
}

int stepAt(const Trick& t, uint32_t ms) {
  uint32_t at = 0;
  for (int i = 0; i < t.n && i < kMaxSteps; ++i) {
    const int m = t.steps[i] < MOVE_COUNT ? t.steps[i] : 0;
    at += kMoves[m].ms;
    if (ms < at) return i;
    if (i + 1 < t.n) {
      at += beatAfter(m);
      if (ms < at) return -1;
    }
  }
  return t.n;
}

void draw(face::Screen& scr, const View& v, float t) {
  using namespace face;
  const Skin& k = MARBLE;
  scr.fill(k.bg);

  scr.text("Trick Lab", kCx, 24, shoutOpt(24, k.text));

  // The strip: six places, filled in reading order. The move being done swells, with its sunburst turning.
  for (int i = 0; i < kMaxSteps; ++i) {
    const float x = kSlotX[i % 3], y = kSlotY[i / 3];
    if (i >= v.trick.n) {  // an empty place: a soft socket, the next one breathing
      const bool next = i == v.trick.n && v.status != S_PLAY;
      const float r = next ? 11 + sinf(t * 5) * 1.5f : 10;
      scr.shape(x - 14, y - 14, x + 14, y + 14, [=](float px, float py) { return SD::circle(px - x, py - y, r); },
                k.bg2, next ? 1 : 0.7f);
      continue;
    }
    float s = 1;
    const bool on = i == v.active;
    if (on) s = 1.2f + sinf(t * 14) * 0.04f;
    else if (i == v.trick.n - 1 && v.pop < 1) s = 0.3f + 0.7f * Ease::outBack(clampf(v.pop, 0, 1));
    else if (v.active >= 0 || v.status == S_PLAY) s = 0.9f;  // the others step back while one is on
    roleMarble(scr, v.trick.steps[i] < MOVE_COUNT ? v.trick.steps[i] : 0, x, y, kSlotR * s, on, t);
  }

  // The word, and two lines of what to do next.
  const char* word = "";
  C3 wc = k.text;
  const char *h1 = "", *h2 = "";
  switch (v.status) {
    case S_EMPTY:
      h1 = "Press an animal";
      h2 = v.anySaved ? "or GO: my tricks" : "on the remote!";
      break;
    case S_BUILD:
      h1 = "GO: play it";
      h2 = "Freeze: start again";
      break;
    case S_FULL:
      word = "FULL!";
      h1 = "Press GO!";
      h2 = "Freeze: start again";
      break;
    case S_PLAY:
      h2 = "Freeze: stop";
      break;
    case S_PUTDOWN:
      word = "PUT ME DOWN!";
      h1 = "on the floor,";
      h2 = "then stand back";
      break;
    case S_REST:
      word = "PHEW!";
      h2 = "Catching my breath";
      break;
    case S_DONE:
      word = "TA-DA!";
      wc = hex("#ffd23a");
      break;
    case S_NAMED:
      h1 = "GO: again!";
      h2 = "Animals: a new trick";
      break;
  }
  if (v.word >= 0 && v.word < MOVE_COUNT && (v.status == S_BUILD || v.status == S_PLAY || v.status == S_FULL)) {
    word = moveShout(v.word);
    wc = roleColor(v.word);
  }
  if (v.status == S_NAMED) {  // the trick's name, two words, gold, wobbling against each other
    const char *adj, *noun;
    nameOf(v.trick, adj, noun);
    TextOpt o = shoutOpt(24, hex("#ffd23a"));
    const float wob = sinf(t * 5) * 0.04f;
    o.rot = wob;
    menu::drawFit(scr, adj, kNameY, o, 126);
    o.rot = -wob;
    menu::drawFit(scr, noun, kNameY + kNameGap, o, 126);
  } else if (*word) {
    TextOpt o = shoutOpt(26, wc);
    o.rot = sinf(t * 5) * 0.03f;
    menu::drawFit(scr, word, kWordY, o, 126);
  } else {
    menu::drawFace(scr, kCx, kWordY, 1.5f);  // nothing to shout: its own two eyes, watching
  }
  if (v.status == S_REST) {  // how rested: a bar filling in the eyes' teal
    const float w = 96, x0 = kCx - w / 2, y0 = kHintY - 9, f = clampf(v.rest, 0, 1);
    scr.shape(x0 - 4, y0 - 8, x0 + w + 4, y0 + 8,
              [=](float x, float y) { return SD::roundBox(x - kCx, y - y0, w / 2, 5, 5); }, k.bg2, 1);
    if (f > 0.02f)
      scr.shape(x0 - 4, y0 - 8, x0 + w + 4, y0 + 8, [=](float x, float y) {
        return SD::roundBox(x - (x0 + w * f / 2), y - y0, w * f / 2, 5, 5);
      }, k.accent, 1);
  } else if (*h1) {
    menu::drawFit(scr, h1, kHintY, sayOpt(), 130);
  }
  if (*h2) menu::drawFit(scr, h2, kHintY + kHintGap, sayOpt(), 130);
}

void drawLearn(face::Screen& scr, const LearnView& v, float t) {
  using namespace face;
  const Skin& k = MARBLE;
  scr.fill(k.bg);
  scr.text("My remote", kCx, 24, shoutOpt(24, k.text));

  const int role = v.role < 0 ? 0 : v.role >= ROLE_COUNT ? ROLE_COUNT - 1 : v.role;
  const float shake = v.taken > 0 ? sinf(t * 40) * 4 : 0;
  const float s = 0.5f + 0.5f * Ease::outBack(clampf(v.pop, 0, 1));
  roleMarble(scr, role, kCx + shake, 96, 46 * s, true, t);
  TextOpt o = shoutOpt(24, k.text);
  o.outline = 2.5f;
  menu::drawFit(scr, roleName(role), 160, o, 124);

  menu::drawFit(scr, v.taken > 0 ? "That button is taken!" : roleDoes(role), 188, sayOpt(), 130);
  menu::drawFit(scr, "Press its button", 205, sayOpt(), 130);

  // a dot for each button to teach, the current one in the eyes' teal
  for (int i = 0; i < ROLE_COUNT; ++i) {
    const bool on = i == role, done = i < role;
    const float x = kCx + (i - (ROLE_COUNT - 1) / 2.0f) * 12, y = 228, r = on ? 3.4f : 2.2f;
    scr.shape(x - 5, y - 5, x + 5, y + 5, [=](float px, float py) { return SD::circle(px - x, py - y, r); },
              on ? k.accent : done ? roleColor(i) : k.text, on || done ? 1 : 0.6f);
  }
}

}  // namespace lab
