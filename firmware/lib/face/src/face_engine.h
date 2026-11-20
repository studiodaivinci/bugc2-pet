// The robot's face: springs, the Marble look, eyes with lids and pupils, the mouth, and particles.
// Port of Spring, SKINS.marble, Face and FX in art/face/engine.js (the Marble look, picked 2026-11-09).
#pragma once

#include "face_screen.h"

namespace face {

// ---------------------------------------------------------------- springs
struct Spring {
  float x = 0, t = 0, v = 0, k = 220, z = 0.62f;
  void init(float val, float kk, float zz) { x = t = val; v = 0; k = kk; z = zz; }
  // k = 0 and z < 0 keep the current stiffness and damping.
  void to(float target, float kk = 0, float zz = -1) {
    t = target;
    if (kk) k = kk;
    if (zz >= 0) z = zz;
  }
  void snap(float val) { x = t = val; v = 0; }
  void kick(float dv) { v += dv; }
  void step(float dt) {
    int n = (int)ceilf(dt / 0.004f);
    if (n < 1) n = 1;
    const float h = dt / n, c = 2 * z * sqrtf(k);
    for (int i = 0; i < n; i++) {
      v += (-k * (x - t) - c * v) * h;
      x += v * h;
    }
  }
};

// ---------------------------------------------------------------- the look
struct Skin {
  C3 bg, bg2;
  float grain;
  C3 eye, eyeDark, eyeLight, pupil, catchC;
  float rim;
  C3 rimC, ink, mouth, mouthIn, tongue, text, textOutline, accent, heart, blush;
  C3 confetti[5];
};
extern const Skin MARBLE;

struct Layout {
  float w, h, cx, cy, sep, ew, eh, my, capY;
};
constexpr Layout PORTRAIT{135, 240, 67.5f, 104, 63, 26, 33, 158, 212};

// ---------------------------------------------------------------- face
enum EyeKey { E_DX, E_DY, E_SW, E_SH, E_ROUND, E_OPEN, E_LIDT, E_LIDTA, E_LIDB, E_LIDBA, E_LIDBC, E_LOOKX, E_LOOKY,
              E_PUPIL, E_MORPH, E_TILT, E_SPIN, E_N };
enum FaceKey { F_X, F_Y, F_SX, F_SY, F_ROT, F_MSHOW, F_MOPEN, F_MCURVE, F_MWIDE, F_MY, F_BLUSH, F_SWEAT, F_TEARS,
               F_GLOW, F_N };
enum Shape : int8_t { SH_NONE = -1, SH_EYE, SH_HEART, SH_STAR, SH_ARC, SH_X, SH_DOT, SH_LINE, SH_RING, SH_CROSS,
                      SH_SPIRAL };
enum Ex { NEUTRAL, HAPPY, JOY, LOVE, STARRY, SLEEPY, SAD, GRUMPY, SCARED, SURPRISED, SUSPICIOUS, PROUD, CURIOUS,
          DIZZY, XX, FOCUSED, HUNGRY, EX_N };
enum Mouth : uint8_t { M_SMILE, M_CAT, M_WAVE, M_TONGUE, M_CHOMP };
enum Pupil : uint8_t { P_ROUND, P_SLIT, P_BAR, P_NONE };
enum SetFlag { SNAP = 1, KEEP = 2, KEEPLOOK = 4 };

struct Eye {
  int side = 1;
  Spring k[E_N];
  float lidT_ = 0, lidB_ = 0;  // the lid closures last drawn (for the lash line)
  Spring& operator[](int i) { return k[i]; }
  const Spring& operator[](int i) const { return k[i]; }
};
struct P2 {
  float x, y;
};
struct XF {
  float cx, cy, sx, sy, cs, sn;
};

class Face {
 public:
  Screen* scr = nullptr;
  Layout L = PORTRAIT;
  const Skin* skin = &MARBLE;
  bool mouthOn = true;
  float t = 0;
  Eye eyes[2];
  Spring f[F_N];
  Shape shapes[2] = {SH_EYE, SH_EYE};
  Pupil pupilShape = P_ROUND;
  bool hasEyeColor = false, hasEyeDark = false, hasEyeLight = false;
  C3 eyeColor{}, eyeDark{}, eyeLight{};
  float extraEyes = 0, compound = 0;
  bool autoBlink = true, autoLook = true;
  float visible = 1;
  bool hidden = false;
  Mouth mouthShape = M_SMILE;
  float chomp = 0;
  bool googly = false;
  float grav[2] = {0, 1};
  float gp[2][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}};
  double blinkT_ = -1, blinkD_ = 0.16;
  Ex expr = NEUTRAL;

  void begin(Screen* s, uint32_t seed = 7);
  void reseed(uint32_t seed) { r_ = Rng(seed); }
  Face& set(Ex name, int flags = 0);
  Face& eyeSet(int key, float v, int which = 2, float k = 0, float z = -1);
  Face& eyeSnap(int key, float v, int which = 2);
  Face& faceSet(int key, float v, float k = 0, float z = -1) { f[key].to(v, k, z); return *this; }
  Face& look(float x, float y, float k = 420);
  Face& blink(float dur = 0.16f) { blinkT_ = 0; blinkD_ = dur; return *this; }
  Face& kick(int key, float v) { f[key].kick(v); return *this; }
  // A quick squash-and-stretch of the whole face: amt > 0 stretches up first.
  Face& boing(float amt = 0.25f) { f[F_SY].kick(amt * 14); f[F_SX].kick(-amt * 10); return *this; }
  // Animal eye colours (none = the look's own).
  void setEyeColors(const C3* eye, const C3* dark, const C3* light);
  void update(double dt);  // double: the blink and look countdowns must match the JS frame for frame
  void drawBackground();
  void draw(bool noBg = false);
  P2 eyePos(int i) const;
  XF xf() const;

 private:
  Rng r_;
  double nextBlink_ = 1.2, nextLook_ = 0.8;
  void googlyStep(float dt);
  void drawEye(int i);
  template <class F, class TL>
  void eyeBody(int i, F& f, float x0, float y0, float x1, float y1, C3 col, P2 P, float w, float h, float lookX,
               float lookY, TL& toLocal, float alpha, float open, float morph, Shape shape, float cs, float sn);
  template <class TL>
  void maybeLash(int i, P2 P, float w, float h, float open, TL& toLocal);
  void drawExtraEyes();
  // The iris (fibres and the light-to-dark rings) depends only on where a pixel is from the pupil, in units of the
  // eye's size, so it is computed once into a table per eye: two mix amounts per cell (0..255).
  static constexpr int IRIS_N = 160;
  static constexpr float IRIS_R = 2;  // the table covers -2..2 eye sizes around the pupil
  static uint8_t* iris_[2];
  static void buildIris();
  void drawBlush();
  void drawMouth();
  void drawSweat();
  void drawTears();
};

// ---------------------------------------------------------------- particles
enum PType : uint8_t { PT_CIRCLE, PT_HEART, PT_STAR, PT_SPARK, PT_CONFETTI, PT_RING, PT_PUFF, PT_DROP, PT_SHARD, PT_LINE,
                       PT_NOTE, PT_TEXT };

// A particle, and also the options of a burst (angle, spread, speed, jitter, vrRange, colors).
struct Pt {
  PType type = PT_CIRCLE;
  float x = 0, y = 0, vx = 0, vy = 0, g = 0, drag = 0, life = 1, age = 0, size = 6, rot = 0, vr = 0;
  C3 color{255, 255, 255};
  float fade = 0.3f, grow = 0, alpha = 1, width = 0;
  // text particles
  const char* text = nullptr;
  const Font* font = nullptr;
  float fontSize = 14, outline = 0, size0 = 0;
  C3 textColor{255, 255, 255}, oc{0, 0, 0};
  // burst options
  float angle = -PIF / 2, spread = TAU, speed = 80, jitter = 0, vrRange = 10;
  const C3* colors = nullptr;
  int ncolors = 0;

  Pt() {}
  explicit Pt(PType t) : type(t) {}
  Pt& at(float px, float py) { x = px; y = py; return *this; }
  Pt& vel(float a, float b) { vx = a; vy = b; return *this; }
  Pt& grav(float v) { g = v; return *this; }
  Pt& drg(float v) { drag = v; return *this; }
  Pt& lif(float v) { life = v; return *this; }
  Pt& sz(float v) { size = v; return *this; }
  Pt& rt(float v) { rot = v; return *this; }
  Pt& spin(float v) { vr = v; vrRange = v; return *this; }
  Pt& col(C3 c) { color = c; return *this; }
  Pt& cols(const C3* c, int n) { colors = c; ncolors = n; return *this; }
  Pt& fd(float v) { fade = v; return *this; }
  Pt& grw(float v) { grow = v; return *this; }
  Pt& al(float v) { alpha = v; return *this; }
  Pt& wid(float v) { width = v; return *this; }
  Pt& ang(float v) { angle = v; return *this; }
  Pt& spr(float v) { spread = v; return *this; }
  Pt& spd(float v) { speed = v; return *this; }
  Pt& jit(float v) { jitter = v; return *this; }
};

class FX {
 public:
  static constexpr int MAX = 220;
  Screen* scr = nullptr;
  float flash = 0;
  void begin(Screen* s);
  void reseed(uint32_t seed) { r_ = Rng(seed); }
  Pt* emit(const Pt& p);
  void burst(PType type, float x, float y, int n, const Pt& o);
  void update(float dt);
  void draw();
  void clear() { n_ = 0; }
  int count() const { return n_; }

 private:
  Pt* ps_ = nullptr;  // MAX of them, in PSRAM on the robot
  int n_ = 0;
  Rng r_{99};
};

}  // namespace face
