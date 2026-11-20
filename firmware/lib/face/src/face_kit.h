// Shared helpers for the scene scripts (port of the helpers at the top of art/face/scenes.js).
#pragma once

#include "face_player.h"

namespace face {
namespace kit {

// palette shared by the scenes
constexpr C3 TEAL = hex("#13b5ae"), GOLD = hex("#ffd23a"), PINK = hex("#ff3b5c"), CREAM = hex("#fff6e5"),
             BROWN = hex("#4a1f04"), NIGHT = hex("#141a33"), SKY = hex("#7fd6ff");

inline const Bitmap& A(Img i) { return IMAGES[i]; }
inline C3 contrast(Sc&) { return CREAM; }
inline P2 eyeA(Sc& S) { return S.face.eyePos(0); }
inline P2 eyeB(Sc& S) { return S.face.eyePos(1); }

void sunburst(Sc& S, float cx, float cy, C3 col, float alpha, float rot, int n = 12);

// An animal's eyes (anything left out keeps the look's own).
struct Animal {
  const char* eye = nullptr;
  const char* dark = nullptr;
  const char* light = nullptr;
  Pupil pupil = P_ROUND;
  float compound = 0, extra = 0, sw = 1, sh = 1, round = 0.82f, pupilSize = 1;
};
void animalEyes(Sc& S, const Animal& o);
inline void plainEyes(Sc& S) { animalEyes(S, Animal()); }

// The power card: the toy pops in on a sunburst, the name below; then it zaps into the eyes.
void powerCard(Sc& S, float t, Img key, const char* name, C3 burst);
void zap(Sc& S, C3 col);
void stars(Sc& S, float t, float cx, float cy, float rx, float ry, int n, C3 col);

struct TP {
  float size = 14, vx = 0, vy = -30, life = 0.9f, rot = 0, vr = 0, grow = 0, outline = 2;
  const Font* font = &FONT_SHOUT;
  bool hasColor = false, hasOc = false;
  C3 color{}, oc{};
  TP& sz(float v) { size = v; return *this; }
  TP& vel(float a, float b) { vx = a; vy = b; return *this; }
  TP& up(float v) { vy = v; return *this; }
  TP& lif(float v) { life = v; return *this; }
  TP& rt(float v) { rot = v; return *this; }
  TP& say() { font = &FONT_SAY; return *this; }
  TP& col(C3 c) { hasColor = true; color = c; return *this; }
  TP& ocol(C3 c) { hasOc = true; oc = c; return *this; }
};
void textPop(Sc& S, const char* s, float x, float y, const TP& o = TP());

Pt heartOpts(Sc& S);  // speed 70, g -40, drag 1.5, size 5, life 1.3, the heart colour, vr 2, jitter 10, spread 2.2
inline void hearts(Sc& S, int n, float x, float y, const Pt& o) { S.fx.burst(PT_HEART, x, y, n, o); }
inline void hearts(Sc& S, int n, float x, float y) { hearts(S, n, x, y, heartOpts(S)); }
Pt confettiOpts(Sc& S, float angle);  // spread 0.8, speed 190, g 160, drag 1.2, size 6, life 2.2, vr 14
inline void confetti(Sc& S, float x, float y, int n, float angle) { S.fx.burst(PT_CONFETTI, x, y, n, confettiOpts(S, angle)); }
inline void shiver(Sc& S, float amt, float freq = 70) { S.face.f[F_X].snap(sinf((float)S.t * freq) * amt); }

// The egg from the toy art, optionally clipped (clip(x, y) -> keep).
struct EggOpt {
  float scale = 1, sx = 1, sy = 1, rot = 0;
};
template <class Clip>
void drawEgg(Sc& S, float cx, float cy, const EggOpt& o, Clip clip) {
  SpOpt so;
  so.sx = o.scale * o.sx;
  so.sy = o.scale * o.sy;
  so.rot = o.rot;
  S.scr->sprite(A(IMG_EGG), cx, cy, so, clip);
}
inline void drawEgg(Sc& S, float cx, float cy, const EggOpt& o) {
  drawEgg(S, cx, cy, o, [](float, float) { return true; });
}
void drawCracks(Sc& S, float cx, float cy, int level, float s, float rot, C3 col);
void golfBall(Sc& S, float x, float y, float r, float sh = 1);
void battery(Sc& S, float x, float y, float level, C3 col);
void bolt(Sc& S, float x, float y, float s, C3 col, float a = 1);

}  // namespace kit
}  // namespace face
