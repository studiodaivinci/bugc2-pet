// The animation catalogue: each scene is a short script for the face engine. Port of art/face/scenes.js, one class per
// scene, in the same order; the JS scene's S.data fields are the class's members. 53 scenes (2026-11-09); hold-button,
// floor-ask, charging and muted were added for the gift build on 2026-11-10, and the animal powers show their fact
// first, big, while the robot is held.
//
// Where a JS expression called S.r() more than once, the calls are made one per statement here, in the JS order: C++
// leaves the order of function arguments open, and the sequence of random numbers has to match.
#include <stdio.h>
#include <string.h>

#include <initializer_list>

#include "face_kit.h"
#include "pet_config.h"

namespace face {
using namespace kit;

namespace {

// ===== FACE =====
struct Idle : Scene {
  void setup(Sc& S) override {
    S.at(4.2, [](Sc& S) { S.face.set(CURIOUS, KEEPLOOK); });
    S.at(5.8, [](Sc& S) { S.face.set(NEUTRAL, KEEPLOOK); });
    S.at(8.0, [](Sc& S) { S.face.set(HAPPY, KEEPLOOK); S.face.boing(0.15f); });
    S.at(9.2, [](Sc& S) { S.face.set(NEUTRAL, KEEPLOOK); });
    S.at(11.0, [](Sc& S) { S.face.autoBlink = false; S.face.blink(0.7f); });
    S.at(11.8, [](Sc& S) { S.face.autoBlink = true; });
  }
};

struct Googly : Scene {
  void setup(Sc& S) override {
    Face& f = S.face;
    f.googly = true;
    f.autoLook = false;
    f.set(NEUTRAL);
    f.gp[0][0] = 0;
  }
  void frame(Sc& S, float t) override {
    if (live) return;  // the host sets face.grav from the accelerometer
    Face& f = S.face;
    float gx, gy;
    if (t < 2.4f) { gx = sinf(t * 2.2f) * 0.7f; gy = 0.8f; }
    else if (t < 4) { gx = t < 3.2f ? -1.1f : 1.1f; gy = 0.3f; }
    else if (t < 5.6f) {
      gx = (S.r() - 0.5f) * 6;
      gy = (S.r() - 0.5f) * 6;
      f.f[F_X].snap(sinf(t * 50) * 3);
      f.faceSet(F_MOPEN, 0.8f);
      f.faceSet(F_MCURVE, 0);
    } else if (t < 7.6f) {
      gx = cosf(t * 11) * 1.4f;
      gy = sinf(t * 11) * 1.4f;
      f.faceSet(F_ROT, sinf(t * 2) * 0.05f);
    } else {
      gx = 0;
      gy = 1;
      f.faceSet(F_MOPEN, 0);
      f.faceSet(F_MCURVE, 0.4f);
      f.faceSet(F_ROT, 0);
    }
    f.grav[0] = gx;
    f.grav[1] = gy;
  }
};

// ===== MOODS =====
struct Mood : Scene {
  Ex ex;
  explicit Mood(Ex e) : ex(e) {}
  virtual void go(Sc&) {}
  virtual void end(Sc&) {}
  void setup(Sc& S) override {
    S.at(0.25, [](Sc& S) { S.face.blink(); });
    S.at(0.7, [this](Sc& S) { S.face.set(ex); go(S); });
    S.at(3.6, [this](Sc& S) { S.face.set(NEUTRAL); S.face.blink(); end(S); });
  }
};

struct MoodHappy : Mood {
  MoodHappy() : Mood(HAPPY) {}
  void go(Sc& S) override {
    S.face.boing(0.3f);
    S.after(0.45, [](Sc& S) { S.face.boing(0.3f); });
    S.after(0.15, [](Sc& S) { S.face.kick(F_Y, -90); });
    S.after(0.6, [](Sc& S) { S.face.kick(F_Y, -90); });
  }
};

struct MoodLove : Mood {
  MoodLove() : Mood(LOVE) {}
  void frame(Sc& S, float t) override {
    if (t > 0.9f && t < 3.5f) {
      const float ph = fmodf(t - 0.9f, 0.9f);
      const bool beat = ph < 0.12f || (ph > 0.24f && ph < 0.34f);
      for (auto& e : S.face.eyes) {
        e[E_SW].to(beat ? 1.22f : 1.04f, 900, 0.4f);
        e[E_SH].to(beat ? 1.22f : 1.04f, 900, 0.4f);
      }
      if (every(S, 3)) {
        const float x = S.L.cx + (S.r() - 0.5f) * 60;
        hearts(S, 1, x, S.L.cy - 20, heartOpts(S).spd(30).ang(-PIF / 2).spr(0.6f));
      }
    }
  }
};

struct MoodSleepy : Mood {
  MoodSleepy() : Mood(SLEEPY) {}
  void frame(Sc& S, float t) override {
    Face& f = S.face;
    if (t > 1.0f && t < 1.9f) {
      f.faceSet(F_MOPEN, 1.1f);
      f.faceSet(F_MCURVE, 0);
      f.faceSet(F_MWIDE, 0.7f);
      f.eyeSet(E_LIDT, 0.85f);
      f.faceSet(F_SY, 1.08f);
    } else if (t > 1.9f && t < 3.5f) {
      f.faceSet(F_MOPEN, 0);
      f.faceSet(F_SY, 1);
      f.eyeSet(E_LIDT, 0.55f + 0.35f * prog(t, 1.9f, 1.2f));
      f.faceSet(F_Y, 10 * prog(t, 2.0f, 1.3f));
    }
    if (t > 3.2f && t < 3.25f) {
      f.kick(F_Y, -200);
      f.set(SURPRISED);
    }
    if (t > 2.1f && every(S, 1.6f) && t < 3.2f)
      textPop(S, "z", S.L.cx + 40, S.L.cy - 30, TP().sz(12 + (t - 2) * 6).vel(12, -25).lif(1.2f));
  }
};

struct MoodScared : Mood {
  MoodScared() : Mood(SCARED) {}
  void frame(Sc& S, float t) override {
    Face& f = S.face;
    if (t > 0.7f && t < 3.5f) {
      shiver(S, 1.3f, 75);
      f.mouthShape = M_WAVE;
      if (every(S, 4)) f.look(S.r() < 0.5f ? -0.9f : 0.9f, -0.1f, 900);
    } else {
      f.mouthShape = M_SMILE;
      f.f[F_X].to(0);
    }
  }
};

struct MoodDizzy : Mood {
  MoodDizzy() : Mood(DIZZY) {}
  void frame(Sc& S, float t) override {
    Face& f = S.face;
    if (t > 0.7f && t < 3.5f) {
      f.faceSet(F_X, cosf(t * 4.5f) * 6);
      f.faceSet(F_Y, sinf(t * 4.5f) * 3);
      f.faceSet(F_ROT, sinf(t * 3) * 0.13f);
      f.mouthShape = M_WAVE;
    } else {
      f.mouthShape = M_SMILE;
    }
  }
  void over(Sc& S, float t) override {
    if (t > 0.9f && t < 3.5f) stars(S, t, S.L.cx + S.face.f[F_X].x, S.L.cy - 50, 38, 7, 3, GOLD);
  }
};

struct MoodSuspicious : Mood {
  MoodSuspicious() : Mood(SUSPICIOUS) {}
  void frame(Sc& S, float t) override {
    if (t > 0.9f && t < 3.4f) S.face.look(sinf((t - 0.9f) * 1.6f) > 0 ? 0.8f : -0.8f, 0, 120);
  }
  void go(Sc& S) override { S.after(0.4, [](Sc& S) { S.say("Hmm...", Line()); }); }
  void end(Sc& S) override { S.unsay(); }
};

struct MoodSad : Mood {
  MoodSad() : Mood(SAD) {}
  void go(Sc& S) override {
    S.after(0.6, [](Sc& S) { S.face.faceSet(F_TEARS, 1); });
    S.after(1.2, [](Sc& S) { S.face.kick(F_Y, -60); });
    S.after(1.9, [](Sc& S) { S.face.kick(F_Y, -60); });
  }
  void end(Sc& S) override { S.face.faceSet(F_TEARS, 0); }
};

struct MoodGrumpy : Mood {
  MoodGrumpy() : Mood(GRUMPY) {}
  void frame(Sc& S, float t) override {
    if (t > 1.0f && t < 3.4f && every(S, 5))
      for (int side = -1; side <= 1; side += 2)
        S.fx.emit(Pt(PT_PUFF).at(S.L.cx + side * 52, S.L.cy - 44).vel(side * 18.f, -50).sz(3).grw(10).lif(0.7f).fd(0.4f).col(WHITE).al(0.85f));
    if (t > 1.2f && t < 1.7f) shiver(S, 1.5f, 60);
  }
  void go(Sc& S) override { S.after(0.5, [](Sc& S) { S.say("Hmph!", Shout()); }); }
  void end(Sc& S) override { S.unsay(); }
};

struct MoodSurprised : Mood {
  MoodSurprised() : Mood(SURPRISED) {}
  void go(Sc& S) override {
    S.face.boing(0.7f);
    S.face.kick(F_Y, -120);
  }
  void over(Sc& S, float t) override {
    if (t > 0.75f && t < 3.5f) {
      TextOpt o;
      o.size = 34;
      o.color = S.skin->text;
      o.outline = 2.5f;
      o.oc = S.skin->textOutline;
      o.scale = Ease::outBack(prog(t, 0.75f, 0.3f));
      o.rot = sinf(t * 12) * 0.12f * (1 - prog(t, 0.9f, 0.6f));
      S.scr->text("!", S.L.cx, S.L.cy - 64 + S.face.f[F_Y].x, o);
    }
  }
};

struct MoodProud : Mood {
  MoodProud() : Mood(PROUD) {}
  void go(Sc& S) override {
    S.after(0.5, [](Sc& S) {
      const P2 a = eyeA(S);
      S.fx.emit(Pt(PT_SPARK).at(a.x - 12, a.y - 18).sz(7).lif(0.7f).fd(0.4f).col(WHITE).spin(3));
    });
  }
};

// ===== HANDLING =====
struct Wheee : Scene {
  void setup(Sc& S) override {
    S.at(0.2, [](Sc& S) { S.face.set(SURPRISED); S.face.boing(0.8f); S.face.kick(F_Y, -160); });
    S.at(0.55, [](Sc& S) { S.face.set(JOY); S.face.faceSet(F_MOPEN, 1); S.say("Wheee!", Shout().bnc()); });
    S.at(3.3, [](Sc& S) { S.unsay(); S.face.set(HAPPY); });
  }
  void frame(Sc& S, float t) override {
    Face& f = S.face;
    if (t > 0.55f && t < 3.3f) {
      f.faceSet(F_ROT, sinf(t * 5) * 0.2f);
      f.faceSet(F_Y, -8 + sinf(t * 10) * 2);
      if (every(S, 6)) {
        const PType ty = S.r() < 0.5f ? PT_SPARK : PT_HEART;
        const float x = S.L.cx + (S.r() - 0.5f) * 120;
        const float vy = -120 - S.r() * 60;
        const float vx = (S.r() - 0.5f) * 40;
        const float sz = 4 + S.r() * 2;
        const C3 c = S.r() < 0.5f ? S.skin->heart : WHITE;
        S.fx.emit(Pt(ty).at(x, 175).vel(vx, vy).sz(sz).lif(0.9f).fd(0.3f).col(c).spin(4));
      }
    } else {
      f.faceSet(F_ROT, 0);
    }
  }
};

struct Phew : Scene {
  void setup(Sc& S) override {
    Face& f = S.face;
    f.f[F_Y].snap(-26);
    f.set(SCARED, SNAP);
    f.f[F_SWEAT].snap(1);
    S.at(0.35, [](Sc& S) { S.face.faceSet(F_Y, 0, 500, 0.5f); });
    S.at(0.55, [](Sc& S) {
      S.face.kick(F_SY, -9);
      S.face.kick(F_SX, 7);
      for (int side = -1; side <= 1; side += 2)
        S.fx.burst(PT_PUFF, S.L.cx + side * 40, S.L.cy + 62, 3,
                   Pt().ang(side < 0 ? PIF : 0).spr(0.6f).spd(60).drg(4).sz(4).grw(8).lif(0.6f).col(WHITE));
    });
    S.at(0.9, [](Sc& S) {
      Face& f = S.face;
      f.set(HAPPY);
      f.eyeSet(E_LIDT, 0.75f);
      f.faceSet(F_MOPEN, 0.5f);
      f.faceSet(F_MCURVE, 0);
      f.faceSet(F_MWIDE, 0.5f);
      S.say("phew...", Line());
    });
    S.at(1.3, [](Sc& S) {
      S.face.faceSet(F_SWEAT, 0);
      const float x = S.L.cx + (S.L.sep / 2 + S.L.ew * 0.85f), y = S.L.cy - S.L.eh * 0.9f;
      S.fx.emit(Pt(PT_DROP).at(x, y).vel(0, 20).grav(500).sz(7).lif(0.9f).col(hex("#bfeaff")));
    });
    S.at(2.0, [](Sc& S) { S.face.faceSet(F_MOPEN, 0); S.face.faceSet(F_MCURVE, 0.8f); S.face.faceSet(F_MWIDE, 1); });
    S.at(3.4, [](Sc& S) { S.unsay(); S.face.set(NEUTRAL); });
  }
};

struct Shaken : Scene {
  void setup(Sc& S) override {
    S.face.googly = true;
    S.face.autoLook = false;
    S.at(1.7, [](Sc& S) { S.face.googly = false; S.face.set(DIZZY); });
    S.at(3.4, [](Sc& S) {
      S.face.kick(F_Y, -150);
      S.face.boing(0.3f);
      textPop(S, "hic!", S.L.cx + 34, S.L.cy - 52, TP().sz(14).up(-20));
    });
    S.at(4.3, [](Sc& S) {
      S.face.kick(F_Y, -150);
      S.face.boing(0.3f);
      textPop(S, "hic!", S.L.cx - 34, S.L.cy - 52, TP().sz(14).up(-20));
    });
    S.at(5.1, [](Sc& S) { S.face.set(NEUTRAL); });
  }
  void frame(Sc& S, float t) override {
    Face& f = S.face;
    if (t < 1.7f) {
      const float gx = (S.r() - 0.5f) * 7;
      const float gy = (S.r() - 0.5f) * 7;
      f.grav[0] = gx;
      f.grav[1] = gy;
      f.f[F_X].snap(sinf(t * 55) * 4);
      f.f[F_Y].snap(cosf(t * 41) * 3);
      f.faceSet(F_MOPEN, 0.9f);
      f.faceSet(F_MCURVE, 0);
    } else if (t < 5.1f) {
      f.faceSet(F_X, cosf(t * 4.5f) * 5);
      f.faceSet(F_ROT, sinf(t * 3) * 0.12f);
      f.mouthShape = M_WAVE;
    } else {
      f.mouthShape = M_SMILE;
      f.faceSet(F_X, 0);
      f.faceSet(F_ROT, 0);
    }
  }
  void over(Sc& S, float t) override {
    if (t > 1.8f && t < 5.1f) stars(S, t, S.L.cx + S.face.f[F_X].x, S.L.cy - 50, 38, 7, 3, GOLD);
  }
};

struct Turtle : Scene {
  bool freed = false;  // turned back over
  static void unflipped(Sc& S) {
    Face& f = S.face;
    S.unsay();
    f.set(JOY);
    f.mouthShape = M_SMILE;
    f.boing(0.6f);
    f.faceSet(F_ROT, 0);
    f.faceSet(F_X, 0);
  }
  static void thanks(Sc& S) { S.say("Thanks!", Shout()); }
  static void settle(Sc& S) { S.unsay(); S.face.set(HAPPY); }
  void setup(Sc& S) override {
    S.face.autoLook = false;
    S.at(0.2, [](Sc& S) { S.face.set(SCARED); S.face.mouthShape = M_WAVE; S.say("Help!\nI'm stuck!", Line().wob()); });
    if (live) return;  // stuck until the host sends "unflip"
    S.at(3.5, [this](Sc& S) { freed = true; unflipped(S); });
    S.at(3.8, thanks);
    S.at(4.8, settle);
  }
  void event(Sc& S, const char* name) override {
    if (freed || strcmp(name, "unflip") != 0) return;
    freed = true;
    unflipped(S);
    S.after(0.3, thanks);
    S.after(1.3, settle);
  }
  void frame(Sc& S, float t) override {
    if (t > 0.2f && !freed) {
      S.face.faceSet(F_ROT, sinf(t * 16) * 0.12f, 900, 0.3f);
      if (every(S, 5)) {
        const float x = (S.r() - 0.5f) * 2;
        const float y = (S.r() - 0.5f) * 1.2f;
        S.face.look(x, y, 900);
      }
    }
  }
};

// A pat: a giggle and hearts; the third in a row melts it. gig is the scene's "last giggle" time.
void patOnce(Sc& S, int n, double* gig) {
  Face& f = S.face;
  f.kick(F_SY, -10);
  f.kick(F_SX, 6);
  f.set(n >= 3 ? LOVE : JOY);
  f.f[F_BLUSH].to(1);
  *gig = S.t;
  textPop(S, n >= 3 ? "hee hee!" : "hee!", S.L.cx + (n % 2 ? 30 : -30), S.L.cy - 52, TP().sz(13).up(-35).rt(n % 2 ? 0.2f : -0.2f));
  hearts(S, n >= 3 ? 8 : 2, S.L.cx, S.L.cy - 40);
  const double wait = n >= 3 ? 1.35 : 0.55;
  S.after(n >= 3 ? 1.4 : 0.6, [gig, wait](Sc& S) {
    if (S.t - *gig >= wait) S.face.set(HAPPY);
  });
}

struct Pats : Scene {
  int n = 0;
  double gig = -1e9;
  void setup(Sc& S) override {
    if (live) return;  // pats come from tap()
    for (double t : {0.6, 1.5, 2.4}) S.at(t, [this](Sc& S) { n++; patOnce(S, n, &gig); });
    S.at(4.4, [](Sc& S) { S.face.set(NEUTRAL); });
  }
  void frame(Sc& S, float t) override {
    if (t - gig < 0.45) S.face.f[F_Y].snap(sinf(t * 60) * 1.6f);
  }
  void tap(Sc& S) override {
    n++;
    patOnce(S, n, &gig);
  }
};

// ===== HATCHING =====
const C3 SHELL3[] = {hex("#fbf3e2"), hex("#e9dcc2"), hex("#3f8f8a")};
const C3 SHELL4[] = {hex("#fbf3e2"), hex("#e9dcc2"), hex("#3f8f8a"), hex("#d9772b")};
constexpr C3 CRACK = hex("#3a2410");

// The egg's "hint" (the robot sends it after about 8 s with no taps, and plays the peep): a wobble and a "peep!".
float hintWobble(Sc& S, double hint) {
  const float k = (float)(S.t - hint);
  return k >= 0 && k < 0.8f ? sinf(k * 30) * 0.08f * (1 - k / 0.8f) : 0;
}
void hintPeep(Sc& S, double* hint) {
  *hint = S.t;
  textPop(S, "peep!", S.L.cx + 30, 56, TP().sz(14).vel(10, -22).lif(1.1f));
}

struct EggSleep : Scene {
  double hint = -9;
  void setup(Sc& S) override {
    S.face.hidden = true;
    S.at(0.6, [](Sc& S) { S.say("Tap me to help\nme hatch!", Line()); });
  }
  void event(Sc& S, const char* name) override {
    if (strcmp(name, "hint") == 0) hintPeep(S, &hint);
  }
  void under(Sc& S, float t) override {
    const float wob = sinf(t * 1.4f) * 0.05f + (t > 3 && t < 3.8f ? sinf((t - 3) * 30) * 0.08f * (1 - prog(t, 3, 0.8f)) : 0) +
                      hintWobble(S, hint);
    EggOpt o;
    o.sy = 1 + sinf(t * 1.8f) * 0.015f;
    o.rot = wob;
    drawEgg(S, S.L.cx, 106 + sinf(t * 1.8f) * 1, o);
    if (every(S, 1 / 1.3f)) textPop(S, "z", S.L.cx + 30, 52, TP().sz(13).vel(14, -18).lif(1.4f));
  }
};

struct EggCrack : Scene {
  int lv = 0;
  double kick = -9, hint = -9;
  void setup(Sc& S) override {
    S.face.hidden = true;
    if (live) return;  // cracks come from tap()
    for (double t : {0.8, 2.0, 3.2, 4.4}) S.at(t, [](Sc& S) { S.player->tap(); });
  }
  void event(Sc& S, const char* name) override {
    if (strcmp(name, "hint") == 0) hintPeep(S, &hint);
  }
  void tap(Sc& S) override {
    lv++;
    kick = S.t;
    S.fx.burst(PT_SHARD, S.L.cx, 80, 3 + lv, Pt().spd(90).grav(300).sz(3).lif(0.8f).cols(SHELL3, 3).spin(12).jit(20));
    static const char* const words[] = {"Again!", "Harder!", "Almost!", "Almost...!", "Almost...!!"};
    S.say(words[lv - 1 < 4 ? lv - 1 : 4], Shout());
  }
  void under(Sc& S, float t) override {
    const float k = (float)(t - kick);
    const float rot = (k < 0.6f ? sinf(k * 40) * 0.12f * (1 - k / 0.6f) : sinf(t * 1.4f) * 0.03f) + hintWobble(S, hint);
    const float cx = S.L.cx, cy = 106;
    EggOpt o;
    o.rot = rot;
    o.sy = 1 - (k < 0.15f ? 0.05f : 0);
    drawEgg(S, cx, cy, o);
    drawCracks(S, cx, cy, lv, 1, rot, CRACK);
    if (lv >= 3) {
      // a dark hole in the shell, and two eyes blinking inside it
      const float hx = cx + cosf(rot) * -3 - sinf(rot) * -14, hy = cy - 14 + sinf(rot) * -3;
      const float open = ((int)floorf(t * 1.7f) % 4 == 0) ? 0.2f : 1;
      const float ry = 8 + (lv - 3) * 2.f;
      S.scr->shape(hx - 18, hy - 12, hx + 18, hy + 12, [&](float x, float y) { return SD::ellipse(x - hx, y - hy, 15, ry); }, hex("#120a04"), 1);
      const float drift = sinf(t) * 2;
      for (int s = -1; s <= 1; s += 2)
        S.scr->shape(hx + s * 6 - 4, hy - 5, hx + s * 6 + 4, hy + 5, [&](float x, float y) {
          return SD::ellipse(x - hx - s * 6 - drift, y - hy, 2.4f, 3.4f * open);
        }, hex("#7ff0dc"), 1);
    }
  }
};

struct EggHatch : Scene {
  void setup(Sc& S) override {
    Face& f = S.face;
    f.hidden = true;
    f.autoBlink = false;
    f.autoLook = false;
    for (auto& e : f.eyes) e[E_OPEN].snap(0);
    S.at(0.9, [](Sc& S) {
      Face& f = S.face;
      S.fx.flash = 1.2f;
      f.hidden = false;
      f.f[F_SX].snap(0.6f);
      f.f[F_SY].snap(0.6f);
      f.faceSet(F_SX, 1, 260, 0.35f);
      f.faceSet(F_SY, 1, 260, 0.35f);
      S.fx.burst(PT_SHARD, S.L.cx, 70, 12, Pt().spd(170).grav(420).sz(5).lif(1.4f).cols(SHELL4, 4).spin(14).jit(30).ang(-PIF / 2).spr(2.6f));
      S.fx.burst(PT_SPARK, S.L.cx, 90, 8, Pt().spd(120).drg(2).sz(5).lif(0.8f).col(WHITE));
    });
    S.at(4.0, [](Sc& S) { S.face.blink(0.2f); });
    S.at(4.35, [](Sc& S) { S.face.blink(0.2f); });
    S.at(4.9, [](Sc& S) { S.face.look(-0.9f, 0.1f, 300); });
    S.at(5.5, [](Sc& S) { S.face.look(0.9f, 0.1f, 300); });
    S.at(6.1, [](Sc& S) { S.face.look(0, 0, 300); S.face.set(CURIOUS, KEEPLOOK); });
    S.at(6.6, [](Sc& S) { textPop(S, "?", S.L.cx + 46, S.L.cy - 48, TP().sz(22).up(-12).lif(0.9f)); });
  }
  void frame(Sc& S, float t) override {
    Face& f = S.face;
    if (t < 0.9f) f.f[F_X].snap(0);
    if (t > 1.6f && t < 3.6f) {
      const float o = Ease::inOut(prog(t, 1.9f, 1.5f));
      for (auto& e : f.eyes) e[E_OPEN].snap(o < 0.98f ? o * (1 - 0.15f * sinf(t * 9) * (1 - o)) : 1);
    }
    if (t > 3.6f && f.blinkT_ < 0)
      for (auto& e : f.eyes) e[E_OPEN].snap(1);
  }
  void under(Sc& S, float t) override {
    if (t < 0.9f) {
      const float sh = sinf(t * 60) * 0.06f * prog(t, 0, 0.8f);
      EggOpt o;
      o.rot = sh;
      drawEgg(S, S.L.cx, 106, o);
      drawCracks(S, S.L.cx, 106, 4, 1, sh, CRACK);
    }
  }
  void mid(Sc& S, float t) override {
    // the bottom of the shell stays on like a cup, then falls away
    if (t >= 0.9f && t < 6.0f) {
      const float drop = t > 5.2f ? Ease::inCubic(prog(t, 5.2f, 0.7f)) * 160 : 0;
      const float cy = 214 + drop;
      EggOpt o;
      o.scale = 1.65f;
      drawEgg(S, S.L.cx, cy, o, [&](float x, float y) { return y > 172 + drop + fabsf(ffmod(x * 0.16f, 2) - 1) * 9; });
    }
  }
};

struct HelloKid : Scene {
  bool done = false;
  void setup(Sc& S) override {
    S.at(0.3, [](Sc& S) { S.face.set(CURIOUS); S.say("Are you...", Line()); });
    S.at(1.5, [](Sc& S) {
      Face& f = S.face;
      f.set(NEUTRAL);
      for (auto& e : f.eyes) {
        e[E_PUPIL].to(1.35f);
        e[E_SH].to(1.1f);
      }
      f.faceSet(F_MOPEN, 0.3f);
      f.faceSet(F_MCURVE, 0);
      f.faceSet(F_MWIDE, 0.5f);
      f.kick(F_Y, -60);
      S.say("..." PET_KID_NAME "?", Shout());
    });
    if (!live) S.at(3.3, [](Sc& S) { S.player->button(); });  // live: when the kid presses the big button
  }
  void button(Sc& S) override {
    if (done) return;
    done = true;
    S.face.set(LOVE);
    S.face.boing(0.7f);
    hearts(S, 14, S.L.cx, S.L.cy, heartOpts(S).spd(140).spr(TAU));
    S.say("You're my\nperson!", Line());
    S.after(2.6, [](Sc& S) { S.face.set(HAPPY); });
  }
  void tap(Sc& S) override { button(S); }
};

// ----- helpers for hold-button, floor-ask and muted
// Where the mouth sits on screen (it follows the face's springs).
P2 mouthPos(Sc& S, float* sx = nullptr) {
  const Face& f = S.face;
  const XF X = f.xf();
  const Layout& L = S.L;
  if (sx) *sx = X.sx;
  return P2{X.cx + sinf(f.f[F_ROT].x) * -(L.my - L.cy), X.cy + (L.my - L.cy + f.f[F_MY].x) * X.sy};
}
inline float tri(float u, float c, float w) { return maxf(0, 1 - fabsf(u - c) / w); }
inline float smoothBump(float p, float c, float w) {
  const float k = clampf(1 - fabsf(p - c) / w, 0, 1);
  return k * k * (3 - 2 * k);
}
// The heartbeat: a "lub" and a softer "dub" every BEAT seconds from `from` (the robot thumps its buzzer on each lub).
constexpr double BEAT = 0.7;
float beatPulse(double t, double from) {
  if (from < 0 || t < from) return 0;
  const float p = (float)fmod(t - from, BEAT);
  return smoothBump(p, 0.06f, 0.07f) + 0.6f * smoothBump(p, 0.24f, 0.07f);
}
// A heart that fills like a glass: an empty slot in darker orange, red liquid with a wobbling surface.
void fillHeart(Sc& S, float x, float y, float s, float fill, float t) {
  Screen& scr = *S.scr;
  const float R = s * 1.2f + 4, top = y - s, bot = y + s;
  scr.shape(x - R, y - R, x + R, y + R, [&](float px, float py) { return SD::heart(px - x, py - y, s) - 2.2f; }, BROWN, 1);
  const float lvl = bot - (bot - top) * fill, wav = fill > 0.02f && fill < 0.98f ? 1.3f : 0;
  const C3 empty = hex("#d0700c"), hi = hex("#ff6f86"), lo = hex("#d81e45");
  scr.shade(x - R, y - R, x + R, y + R, [&](float px, float py) { return SD::heart(px - x, py - y, s); },
            [&](float px, float py, float) {
              const float k = clampf(py - (lvl + (wav ? sinf(px * 0.45f + t * 9) * wav : 0)) + 0.5f, 0, 1);
              return mixc(empty, mixc(hi, lo, clampf((py - top) / (bot - top), 0, 1)), k);
            });
  if (fill > 0.6f)
    scr.shape(x - s, y - s, x, y, [&](float px, float py) {
      return SD::ellipse(px - (x - s * 0.45f), py - (y - s * 0.42f), s * 0.22f, s * 0.13f);
    }, WHITE, 0.6f * (fill - 0.6f) / 0.4f);
}
// A heart monitor's line through the heart: each lub (from `from`, none after `until`) sends a blip out both sides.
// ys: Screen::W floats of scratch.
void ecgLine(Sc& S, float* ys, float cx, float cy, double from, double until, float alpha) {
  if (alpha <= 0.01f) return;
  Screen& scr = *S.scr;
  const int w = scr.w;
  const float V = 95;
  const int n = (int)floor((S.t - from) / BEAT);
  for (int x = 0; x < w; x++) {
    const float d = fabsf(x + 0.5f - cx) - 18;
    float y = cy;
    if (d > 0)
      for (int k = n - 1 > 0 ? n - 1 : 0; k <= n; k++) {
        const double tb = from + k * BEAT;
        if (tb > until) break;
        const float run = (float)((S.t - tb) * V), u = d - run, fade = clampf(1 - run / 75, 0, 1);
        y += (-12 * tri(u, 0, 3) + 4 * tri(u, 4.5f, 2.5f) + 3 * tri(u, -4.5f, 2.5f)) * fade;
      }
    ys[x] = y;
  }
  auto f = [&](float px, float py) {
    const int i = (int)px < w - 1 ? (int)px : w - 1;
    const float sl = (ys[i + 1 < w ? i + 1 : w - 1] - ys[i > 0 ? i - 1 : 0]) / 2;
    return fabsf(py - ys[i]) / sqrtf(1 + sl * sl);
  };
  scr.shape(0, cy - 17, w, cy + 7, [&](float px, float py) { return f(px, py) - 2; }, BROWN, alpha);
  scr.shape(0, cy - 17, w, cy + 7, [&](float px, float py) { return f(px, py) - 0.8f; }, contrast(S), alpha);
}
// Stickers in the captions' style (cream, brown outline): a chunky down arrow (tip at y + 16k) and a speaker with an X.
template <class F>
void sticker(Sc& S, float x0, float y0, float x1, float y1, F sd) {
  S.scr->shape(x0, y0, x1, y1, [&](float px, float py) { return sd(px, py) - 2.4f; }, BROWN, 1);
  S.scr->shape(x0, y0, x1, y1, sd, contrast(S), 1);
}
void downArrow(Sc& S, float x, float y, float k) {
  if (k <= 0.01f) return;
  sticker(S, x - 13 * k - 4, y - 17 * k - 4, x + 13 * k + 4, y + 16 * k + 4, [&](float px, float py) {
    const float u = (px - x) / k, v = (py - y) / k;
    return minf(SD::roundBox(u, v + 7, 5, 10, 2), maxf((15 * fabsf(u) + 13 * (v - 16)) / 19.849f, 1 - v)) * k;
  });
}
void speakerOff(Sc& S, float x, float y, float k) {
  if (k <= 0.01f) return;
  sticker(S, x - 13 * k - 4, y - 11 * k - 4, x + 16 * k + 4, y + 11 * k + 4, [&](float px, float py) {
    const float u = (px - x) / k, v = (py - y) / k;
    const float box = SD::roundBox(u + 9, v, 3.5f, 4.5f, 1),
                cone = maxf(maxf(-(u + 6), u - 2), (fabsf(v) - 4.5f - (u + 6) * 0.75f) / 1.25f);
    const float x1 = SD::segment(u, v, 6, -4.5f, 14, 4.5f) - 1.7f, x2 = SD::segment(u, v, 6, 4.5f, 14, -4.5f) - 1.7f;
    return minf(minf(box, cone), minf(x1, x2)) * k;
  });
}
// A brass zipper across the mouth, closed from the left up to the slider (k = 0 open, 1 shut).
void zipper(Sc& S, float k) {
  Screen& scr = *S.scr;
  float msx = 1;
  const P2 m = mouthPos(S, &msx);
  const float W = 18 * msx, x0 = m.x - W, y = m.y, xe = x0 + 2 * W * k;
  if (xe > x0 + 0.5f) {
    scr.shape(x0 - 4, y - 5, xe + 1, y + 5, [&](float px, float py) { return maxf(fabsf(py - y) - 3.4f, maxf(x0 - px, px - xe)); }, BROWN, 1);
    scr.shape(x0 - 1, y - 5, xe + 1, y + 5, [&](float px, float py) {
      const float q = (px - x0) / 4;
      const int i = (int)ffloor(q);
      const float u = (q - i - 0.5f) * 4, v = py - y - (i % 2 ? 1.1f : -1.1f);
      return maxf(SD::roundBox(u, v, 1.4f, 1.7f, 0.5f), maxf(x0 - px, px - xe));
    }, GOLD, 1);
  }
  auto body = [&](float px, float py) {
    return minf(SD::roundBox(px - xe, py - y, 3.4f, 4, 1.4f), SD::roundBox(px - xe, py - y - 8, 2.4f, 4.6f, 2));
  };
  scr.shape(xe - 6, y - 7, xe + 6, y + 16, [&](float px, float py) { return body(px, py) - 1; }, BROWN, 1);
  scr.shape(xe - 6, y - 7, xe + 6, y + 16, body, GOLD, 1);
  scr.shape(xe - 3, y + 7, xe + 3, y + 13, [&](float px, float py) { return SD::circle(px - xe, py - y - 10, 1.1f); }, BROWN, 1);
}

// Hatching step 3: while the kid holds the big button the heart fills and beats with the buzzer, then "You're my person!".
// The robot sends "hold" on the press and "done" 1.4 s later ("release" if they let go first).
constexpr float HEART_Y = 34;
struct HoldButton : Scene {
  float fill = 0;
  bool holding = false, wobble = false;
  double holdAt = -1, doneAt = -1, until = 1e9;
  int beatN = -1;
  float ecg[Screen::W];
  void setup(Sc& S) override {
    S.face.autoLook = false;
    S.at(0.2, [this](Sc& S) {
      S.say("Hold my\nbutton!", Line());
      if (holdAt < 0 && doneAt < 0) {  // looking down at its button
        for (auto& e : S.face.eyes) e[E_PUPIL].to(1.25f);
        S.face.look(0, 0.85f, 300);
      }
    });
    if (live) return;
    S.at(1.3, [](Sc& S) { S.player->event("hold"); });
    S.at(2.7, [](Sc& S) { S.player->event("done"); });
  }
  void frame(Sc& S, float) override {
    Face& f = S.face;
    if (holding) {
      fill = minf(1, fill + (float)S.dt / 1.3f);
      const int n = (int)floor((S.t - holdAt) / BEAT);
      if (n != beatN) {
        beatN = n;
        S.fx.emit(Pt(PT_RING).at(S.L.cx, HEART_Y).sz(15).grw(70).lif(0.5f).fd(0.45f).col(S.skin->heart).wid(1.8f));
        for (auto& e : f.eyes) e[E_PUPIL].kick(4);
        f.kick(F_SX, 0.8f);
        f.kick(F_SY, 0.9f);
      }
    } else if (doneAt < 0) {
      fill = maxf(0, fill - (float)S.dt * 2);
    }
  }
  void over(Sc& S, float t) override {
    float sc = 1 + 0.2f * (holding ? beatPulse(S.t, holdAt) : 0);
    if (doneAt >= 0) {
      const float k = prog(t, (float)doneAt, 0.4f);
      sc = 1 + sinf(k * PIF) * 0.4f + sinf(t * 3) * 0.04f * k;
    }
    if (holdAt >= 0)
      ecgLine(S, ecg, S.L.cx, HEART_Y, holdAt, until,
              prog(t, (float)holdAt, 0.25f) * (doneAt >= 0 ? 1 - prog(t, (float)doneAt + 0.5f, 0.5f) : 1));
    fillHeart(S, S.L.cx, HEART_Y, 16 * sc * Ease::outBack(prog(t, 0.05f, 0.4f)), fill, t);
  }
  void event(Sc& S, const char* name) override {
    Face& f = S.face;
    if (!strcmp(name, "hold") && !holding && doneAt < 0) {
      holding = true;
      holdAt = S.t;
      until = 1e9;
      beatN = -1;
      f.set(HAPPY);
      f.look(0, 0, 300);
      for (auto& e : f.eyes) e[E_PUPIL].to(1.3f);
      if (wobble) {
        wobble = false;
        S.say("Hold my\nbutton!", Line());
      }
    } else if (!strcmp(name, "release") && holding) {
      holding = false;
      until = S.t;
      wobble = true;
      f.set(NEUTRAL);
      for (auto& e : f.eyes) e[E_PUPIL].to(1.25f);
      f.look(0, 0.85f, 300);
      S.say("Hold my\nbutton!", Line().wob());
    } else if (!strcmp(name, "done") && doneAt < 0) {
      doneAt = S.t;
      holding = false;
      if (S.t < until) until = S.t;
      fill = 1;
      f.set(LOVE);
      f.boing(0.7f);
      hearts(S, 14, S.L.cx, S.L.cy, heartOpts(S).spd(140).spr(TAU));
      hearts(S, 6, S.L.cx, HEART_Y, heartOpts(S).spd(90).spr(TAU));
      S.say("You're my\nperson!", Line());
      S.after(2.6, [](Sc& S) { S.face.set(HAPPY); });
    }
  }
  void button(Sc& S) override {
    if (holding || doneAt >= 0) return;
    event(S, "hold");
    S.after(1.4, [this](Sc& S) { event(S, "done"); });
  }
  void tap(Sc& S) override { button(S); }
};

// Listening: sound rings out of both sides like ears, and a level meter (say-hi and every voice command).
void listenOver(Sc& S, float t, double listen) {
  const float lt = (float)(t - listen);
  Screen& scr = *S.scr;
  const Layout& L = S.L;
  const C3 col = contrast(S);
  for (int i = 0; i < 3; i++) {
    const float r = fmodf(lt * 30 + i * 12, 36) + 6, a = 1 - r / 42;
    for (int s = -1; s <= 1; s += 2) {
      const float x = L.cx + s * 58, y = L.cy;
      scr.shape(x - r - 2, y - r - 2, x + r + 2, y + r + 2, [&](float px, float py) {
        return maxf(fabsf(SD::circle(px - x, py - y, r)) - 1.2f, s * (x - px) + 2);
      }, col, a * 0.8f);
    }
  }
  const float env = (sinf(lt * 9) * 0.5f + 0.5f) * (sinf(lt * 2.3f) * 0.4f + 0.6f);
  for (int i = 0; i < 9; i++) {
    const float x = L.cx + (i - 4) * 10, hgt = 4 + 26 * env * (0.4f + 0.6f * vnoise(i * 3.1f, lt * 7));
    scr.shape(x - 4, 212 - hgt / 2 - 1, x + 4, 212 + hgt / 2 + 1,
              [&](float px, float py) { return SD::roundBox(px - x, py - 212, 3, hgt / 2, 3); },
              i % 2 ? S.skin->accent : col, 1);
  }
}

struct SayHi : Scene {
  double listen = 0;
  void setup(Sc& S) override {
    S.at(0.3, [](Sc& S) { S.say("Hold my button\nand say hi!"); });
    S.at(1.9, [this](Sc& S) {
      S.unsay();
      S.face.set(SURPRISED);
      S.face.eyeSet(E_LOOKY, -0.25f);
      S.face.faceSet(F_MOPEN, 0);
      listen = S.t;
    });
    S.at(4.3, [this](Sc& S) {
      listen = 0;
      S.face.set(LOVE);
      S.face.boing(0.5f);
      hearts(S, 10, S.L.cx, S.L.cy, heartOpts(S).spd(120).spr(TAU));
      S.say("Hi " PET_KID_NAME "!", Shout());
    });
    S.at(5.8, [](Sc& S) { S.unsay(); S.face.set(HAPPY); });
  }
  void over(Sc& S, float t) override {
    if (listen) listenOver(S, t, listen);
  }
};

struct Birthday : Scene {
  static constexpr float FLAMES[6][2] = {{-27.2f, -50.5f}, {-18.2f, -53.4f}, {-13.5f, -44.4f},
                                         {-4.5f, -54.3f},  {12.4f, -52.9f},  {23.4f, -47.2f}};
  void setup(Sc& S) override {
    S.at(0.3, [](Sc& S) { S.face.set(SURPRISED); S.face.boing(0.5f); S.say("It's your\nbirthday?", Line()); });
    S.at(1.9, [](Sc& S) {
      S.face.set(STARRY);
      S.face.boing(0.8f);
      S.say("It's MY birthday\ntoo!", Line());
      confetti(S, 0, 240, 26, -1.2f);
      confetti(S, S.L.w, 240, 26, -PIF + 1.2f);
    });
    S.at(3.7, [](Sc& S) {
      S.unsay();
      S.face.faceSet(F_Y, 6);
      S.face.faceSet(F_SX, 0.78f);
      S.face.faceSet(F_SY, 0.78f);
      S.face.set(JOY, KEEP);
    });
    S.at(4.1, [](Sc& S) { S.say("Happy Birthday\n" PET_KID_NAME "!", Shout().at(32).sz(24).bnc()); });
  }
  void frame(Sc& S, float t) override {
    if (t > 4.0f) {
      if (every(S, 9)) {
        const float x = S.r() * S.L.w;
        const float vx = (S.r() - 0.5f) * 30;
        const float vy = 40 + S.r() * 30;
        const float rot = S.r() * TAU;
        const C3 c = S.skin->confetti[(int)(S.r() * 5)];
        S.fx.emit(Pt(PT_CONFETTI).at(x, -6).vel(vx, vy).grav(20).sz(6).lif(6).rt(rot).spin(8).col(c));
      }
      if (every(S, 2)) {
        S.face.kick(F_Y, -70);
        const float x = S.L.cx + (S.r() - 0.5f) * 100;
        const float vx = (S.r() - 0.5f) * 20;
        const float rot = (S.r() - 0.5f) * 0.4f;
        S.fx.emit(Pt(PT_NOTE).at(x, 140).vel(vx, -40).sz(8).lif(1.4f).col(contrast(S)).rt(rot));
      }
    }
  }
  void over(Sc& S, float t) override {
    if (t < 3.7f) return;
    const float k = Ease::outBack(prog(t, 3.7f, 0.5f)), y = 240 - 52 * k;
    S.scr->sprite(A(IMG_CAKE), S.L.cx, y);
    // candle flames, flickering (drawn on top of the toy's unlit candles)
    for (const auto& fl : FLAMES) {
      const float x = S.L.cx + fl[0], yy = y + fl[1], h = 4.5f + sinf(t * 17 + fl[0]) * 1.2f;
      auto flame = [&](float px, float py) { return SD::drop(px - x, py - yy, 2, 0.2f, h); };
      S.scr->glow(x - 10, yy - 14, x + 10, yy + 8, flame, hex("#ffb340"), 3, 0.5f);
      S.scr->shape(x - 4, yy - 10, x + 4, yy + 4, flame, hex("#ffd76a"), 1);
      S.scr->shape(x - 2, yy - 4, x + 2, yy + 2, [&](float px, float py) { return SD::drop(px - x, py - yy + 0.5f, 0.9f, 0.1f, h * 0.45f); }, WHITE, 1);
    }
  }
};
constexpr float Birthday::FLAMES[6][2];

// Hatching step 5: it asks to go down for its first tricks, hopping and pointing at the floor. The robot sends "putdown"
// once it is level and still on the floor.
struct FloorAsk : Scene {
  double down = -1;
  void setup(Sc& S) override {
    S.face.autoLook = false;
    S.at(0.2, [this](Sc& S) {
      if (down >= 0) return;
      Face& f = S.face;
      f.set(NEUTRAL);
      f.faceSet(F_MCURVE, 1);
      f.faceSet(F_MOPEN, 0.45f);
      f.faceSet(F_BLUSH, 0.5f);
      for (auto& e : f.eyes) e[E_PUPIL].to(1.2f);
      f.look(0, 0.9f, 300);
      S.say("Put me on\nthe floor!", Line().at(40));
    });
    if (!live) S.at(3.4, [](Sc& S) { S.player->event("putdown"); });
  }
  void frame(Sc& S, float t) override {
    if (down < 0 && t > 0.2f && every(S, 1.1)) {
      S.face.kick(F_Y, -70);
      S.face.boing(0.15f);
    }
  }
  void over(Sc& S, float t) override {
    const float k = down < 0 ? Ease::outBack(prog(t, 0.3f, 0.35f)) : 1 - Ease::inCubic(prog(t, (float)down, 0.25f));
    downArrow(S, S.L.cx, 194 + fabsf(sinf(t * 5.5f)) * 5, k);
  }
  void event(Sc& S, const char* name) override {
    if ((strcmp(name, "putdown") && strcmp(name, "floor")) || down >= 0) return;
    Face& f = S.face;
    down = S.t;
    S.unsay();
    f.set(STARRY);
    f.boing(0.8f);
    f.look(0, 0, 300);
    S.fx.burst(PT_SPARK, S.L.cx, S.L.cy, 10, Pt().spd(130).spr(TAU).sz(5).lif(0.8f).col(GOLD));
    S.after(0.3, [](Sc& S) { S.say("Watch\nthis!", Shout()); });
    S.after(1.8, [](Sc& S) { S.face.set(HAPPY); });
  }
};

// ===== ANIMAL POWERS =====
// In the hand: the card (toy + name), then the eyes change and the true fact shows, big (22 px, a few words a screen,
// three short lines at most; the big button skips ahead). Then "Put me down!" until the robot is on the floor
// ("putdown" or "floor"; it may come early), and the trick plays. Hand powers don't ask.
// The trick (pframe, punder, pover, trick()'s cues) runs on its own clock, trickT(), which reads 1.45 as the trick
// starts, so its timings are the ones it had when it followed the card directly. The animal's look (pbackground, pmid,
// plook) runs on the scene clock from the zap on. The host gets onCue("trick") as the trick starts, and length() ends
// the scene `end` seconds of trick clock later.
constexpr double FACT_READ = 2.6, ASK_DEMO = 1.6, GO_DELAY = 0.4;
constexpr float FACT_Y = 201, FACT_SIZE = 22;
enum PowerPhase { PH_CARD, PH_FACT, PH_ASK, PH_GO, PH_TRICK };

struct Power : Scene {
  Img img;
  const char* name;
  C3 burst;
  Animal eyes;
  const char* facts[3] = {nullptr, nullptr, nullptr};
  int nFacts = 0;
  double end;        // the trick's end, on its clock
  bool hand = false;  // played in the hand: no "Put me down!"
  int ph = PH_CARD, fi = -1;
  double next = 0, askAt = -1, downAt = -1, goAt = 0, t0 = 0;
  bool ready = false;
  Power(Img i, const char* n, C3 b, const Animal& e, std::initializer_list<const char*> f, double endT)
      : img(i), name(n), burst(b), eyes(e), end(endT) {
    for (const char* x : f)
      if (nFacts < 3) facts[nFacts++] = x;
  }
  virtual void trick(Sc&) {}
  virtual void plook(Sc&, float) {}
  virtual void pframe(Sc&, float) {}
  virtual bool pbackground(Sc&, float) { return false; }
  virtual void punder(Sc&, float) {}
  virtual void pmid(Sc&, float) {}
  virtual void pover(Sc&, float) {}
  double trickT(const Sc& S) const { return S.t - t0 + 1.45; }
  // every() on the trick's clock
  bool everyT(const Sc& S, double rate) const {
    const double tt = trickT(S);
    return floor(tt * rate) != floor((tt - S.dt) * rate);
  }
  void go(Sc& S) {
    ph = PH_GO;
    goAt = S.t + GO_DELAY;
    S.unsay();
    S.face.look(0, 0, 300);
  }
  void setup(Sc& S) override {
    S.face.autoLook = false;
    ph = PH_CARD;
    fi = -1;
    next = 0;
    ready = false;
    askAt = -1;
    downAt = -1;
    goAt = 0;
    t0 = 0;
    S.at(1.45, [this](Sc& S) {
      animalEyes(S, eyes);
      zap(S, burst);
      ph = PH_FACT;
      next = S.t + 0.45;
    });
  }
  void frame(Sc& S, float t) override {
    Face& f = S.face;
    f.hidden = t < 1.45f;
    if (ph == PH_FACT && S.t >= next) {
      fi++;
      if (fi < nFacts) {
        S.say(facts[fi], SayOpt().at(FACT_Y).sz(FACT_SIZE));
        f.boing(0.2f);
        next = S.t + FACT_READ;
      } else if (hand || ready) {
        go(S);
      } else {
        ph = PH_ASK;
        askAt = S.t;
        S.say("Put me\ndown!", Line().at(40));
        f.look(0, 0.9f, 300);
        if (!live) S.after(ASK_DEMO, [](Sc& S) { S.player->event("putdown"); });
      }
    }
    if (ph == PH_GO && S.t >= goAt) {
      ph = PH_TRICK;
      t0 = S.t;
      trick(S);
      if (onCue) onCue("trick");
    }
    if (t > 1.45f) plook(S, t);
    if (ph == PH_TRICK) pframe(S, (float)trickT(S));
  }
  void background(Sc& S, float t) override {
    if (!pbackground(S, t)) S.face.drawBackground();
  }
  void under(Sc& S, float) override { if (ph == PH_TRICK) punder(S, (float)trickT(S)); }
  void mid(Sc& S, float t) override { if (t > 1.45f) pmid(S, t); }
  void over(Sc& S, float t) override {
    powerCard(S, t, img, name, burst);
    if (askAt >= 0) {
      const float k = downAt < 0 ? Ease::outBack(prog(t, (float)(askAt + 0.1), 0.35f)) : 1 - Ease::inCubic(prog(t, (float)downAt, 0.25f));
      downArrow(S, S.L.cx, 194 + fabsf(sinf(t * 5.5f)) * 5, k);
    }
    if (ph == PH_TRICK) pover(S, (float)trickT(S));
  }
  void button(Sc& S) override {
    if (ph == PH_FACT && fi >= 0) next = S.t;
  }
  void event(Sc& S, const char* nm) override {
    if (strcmp(nm, "putdown") && strcmp(nm, "floor")) return;
    ready = true;
    if (ph == PH_ASK) {
      downAt = S.t;
      S.face.boing(0.4f);
      go(S);
    }
  }
  double length(Sc&) override { return ph == PH_TRICK ? t0 + end - 1.45 : 1e9; }
};

Animal animal(const char* eye, const char* dark, const char* light) {
  Animal a;
  a.eye = eye;
  a.dark = dark;
  a.light = light;
  return a;
}
const Animal CHEETAH_EYES = animal("#f0a81c", "#7a3d00", "#ffe27a");

// Each dancing animal has its own colour once its card has shown (art/face/DESIGN.md, "Seven colours"): the card
// plays on the robot's orange, then the animal's world fades in. The picker's cards and the base lights use the same.
float worldIn(float t) { return prog(t, 1.45f, 0.4f); }

struct Cheetah : Power {
  // Two sprints with a breath between ("nearly got it... again!"), then panting. On the trick clock: sprint 2.05-3.65,
  // breath 3.65-3.95, sprint 3.95-4.85, coasting, panting from 5.25 (dances.cpp, kCheetah).
  Cheetah() : Power(IMG_CHEETAH, "CHEETAH", hex("#ff4b3a"), CHEETAH_EYES, {"The fastest\nanimal\non land!"}, 7.3) {}
  bool pbackground(Sc& S, float t) override {  // racing red
    const float k = worldIn(t);
    if (k <= 0) return false;
    S.scr->fillGradient(mixc(S.skin->bg, hex("#ff5446"), k), mixc(S.skin->bg2, hex("#b30c2c"), k), 6);
    return true;
  }
  void pframe(Sc& S, float t) override {
    Face& f = S.face;
    if (t < 5.25f) {
      f.set(FOCUSED, KEEP);
      f.faceSet(F_ROT, sinf(t * 30) * 0.03f);
      f.f[F_X].snap(sinf(t * 47) * 1.2f);
    } else {
      f.mouthShape = M_TONGUE;
      f.faceSet(F_MOPEN, 0.9f + sinf(t * 22) * 0.25f);
      f.faceSet(F_MCURVE, 0.3f);
      f.f[F_Y].snap(sinf(t * 22) * 2.4f);
      f.eyeSet(E_LIDT, 0.3f);
      f.eyeSet(E_LIDB, 0.15f);
      f.f[F_SWEAT].to(1);
    }
  }
  void punder(Sc& S, float t) override {
    if (t > 5.25f) return;
    // whoosh: arcs racing round the screen centre (the robot is spinning), fading for the breath between sprints
    const float breath = prog(t, 3.65f, 0.1f) * (1 - prog(t, 3.9f, 0.15f));
    const float cx = S.L.cx, cy = S.L.cy + 10,
                k = prog(t, 1.5f, 0.4f) * (1 - prog(t, 4.85f, 0.4f)) * (1 - 0.8f * breath);
    for (int i = 0; i < 7; i++) {
      const float r = 30 + i * 14.f, a0 = -t * (9 + i) + i * 2.1f, span = 0.9f;
      S.scr->shape(cx - r - 3, cy - r - 3, cx + r + 3, cy + r + 3, [&](float x, float y) {
        const float ring = fabsf(len(x - cx, y - cy) - r);
        if (ring >= 1.9f) return 9.f;  // can't be drawn whatever the angle (saves the atan2)
        const float a = wrapf(fastAtan2(y - cy, x - cx) - a0, TAU);
        return a < span ? ring - 1.4f * (a / span) : 9.f;
      }, contrast(S), 0.5f * k);
    }
  }
  void pmid(Sc& S, float t) override {
    // tear stripes from the inner corners of the eyes down to the mouth
    const P2 ps[2] = {eyeA(S), eyeB(S)};
    const float sides[2] = {1, -1};
    const float my = S.L.my + S.face.f[F_Y].x;
    const C3 col = hex("#2a1204");
    for (int j = 0; j < 2; j++) {
      const P2 p = ps[j];
      const float s = sides[j];
      const float x0 = p.x + s * S.L.ew * 0.55f, y0 = p.y + S.L.eh * 0.72f, x1 = S.L.cx - s * 13, y1 = my + 1;
      // tapered and bowed outwards a little, like the real marks
      const float mx = (x0 + x1) / 2 + s * -4, mY = (y0 + y1) / 2;
      S.scr->shape(min3(x0, x1, mx) - 5, y0 - 5, max3(x0, x1, mx) + 5, y1 + 5, [&](float x, float y) {
        const float seg = minf(SD::segment(x, y, x0, y0, mx, mY), SD::segment(x, y, mx, mY, x1, y1));
        return seg - (3.2f - 1.8f * clampf((y - y0) / (y1 - y0 + 1), 0, 1));
      }, col, 1);
    }
  }
  void pover(Sc& S, float t) override {
    if (t >= 3.95f && t - S.dt < 3.95f) textPop(S, "again!", S.L.cx, 40, TP().sz(18));
  }
};

struct Honeybee : Power {
  // The face dances a figure of eight along its dotted path to 6.15 on the trick clock; on the floor the robot
  // waggles, takes a small step forward, and waggles again (dances.cpp, kHoneybee).
  Honeybee() : Power(IMG_HONEYBEE, "HONEYBEE", GOLD, eyesOf(), {"Bees dance\nto show", "where\nflowers are!"}, 7.5) {}
  static Animal eyesOf() {
    Animal a = animal("#3b2a12", "#0e0904", "#b08a2a");
    a.pupil = P_NONE;
    a.compound = 1;
    a.sw = 1.05f;
    a.sh = 1.08f;
    return a;
  }
  void pframe(Sc& S, float t) override {
    Face& f = S.face;
    if (t < 6.15f) {
      const float p = (t - 1.6f) * 1.3f, x = sinf(p) * 22, y = sinf(2 * p) * 10;
      const float waggle = cosf(p) > 0.3f ? sinf(t * 40) * 3 : 0;
      f.faceSet(F_X, x + waggle, 600, 0.5f);
      f.faceSet(F_Y, y - 8, 600, 0.5f);
      f.faceSet(F_ROT, waggle * 0.03f);
      f.set(HAPPY, KEEP);
      if (everyT(S, 2.5f)) {
        const float bx = S.L.cx + (S.r() - 0.5f) * 90;
        const float rot = (S.r() - 0.5f) * 0.4f;
        textPop(S, "bzz", bx, 186, TP().sz(12).up(-20).rt(rot));
      }
    } else {
      f.faceSet(F_X, 0);
      f.faceSet(F_Y, 0);
      f.faceSet(F_ROT, 0);
    }
  }
  // 0 on a honeycomb wall, up to 0.5 in a cell's middle; x and y in cells (pointy-top hexagons, one unit across).
  static float honeyEdge(float x, float y) {
    const float sy = 1.7320508f;
    const float ax = floorf(x) + 0.5f, ay = floorf(y / sy) + 0.5f;
    const float bx = floorf(x - 0.5f) + 1.0f, by = floorf(y / sy - 0.5f) + 1.0f;
    float hx = x - ax, hy = y - ay * sy;
    const float gx = x - bx, gy = y - by * sy;
    if (gx * gx + gy * gy < hx * hx + hy * hy) {
      hx = gx;
      hy = gy;
    }
    hx = fabsf(hx);
    hy = fabsf(hy);
    return 0.5f - maxf(hx * 0.5f + hy * 0.8660254f, hx);
  }
  bool pbackground(Sc& S, float t) override {  // a honeycomb: pale wax walls over golden honey
    const float k = worldIn(t);
    if (k <= 0) return false;
    const C3 top = hex("#ffe24f"), bot = hex("#ffb81c"), wax = hex("#fff7cf"), bg = S.skin->bg, bg2 = S.skin->bg2;
    const float H = (float)S.scr->h;
    S.scr->paint([&](int x, int y) -> C3 {
      const float fy = (y + 0.5f) / H;
      const float e = honeyEdge((x + 0.5f) / 19.0f, (y + 0.5f) / 19.0f);
      const C3 c = mixc(mixc(top, bot, fy), wax, 0.6f * clampf(1.4f - e / 0.05f, 0, 1));
      return k < 1 ? mixc(mixc(bg, bg2, fy), c, k) : c;
    });
    return true;
  }
  void punder(Sc& S, float t) override {
    // the dance path, dotted, appearing as it is danced (brown: cream doesn't show on honey)
    const C3 col = hex("#6b4204");
    const double upto = (t - 1.6) * 1.3, end = upto < TAU * 1.5 ? upto : TAU * 1.5;
    for (double p = 0; p < end; p += 0.16) {
      const float x = S.L.cx + sinf((float)p) * 22 * 1.6f, y = S.L.cy + 4 + sinf(2 * (float)p) * 10 * 2.2f;
      S.scr->shape(x - 3, y - 3, x + 3, y + 3, [&](float px, float py) { return SD::circle(px - x, py - y, 1.3f); }, col, 0.55f);
    }
    if (t > 3) S.scr->sprite(A(IMG_FLOWER), S.L.cx + 50, 180, SpOpt().scale(0.42f * Ease::outBack(prog(t, 3, 0.4f))).r(sinf(t * 2) * 0.1f));
  }
  void pmid(Sc& S, float t) override {
    // antennae with ball tips, bobbing
    const P2 ps[2] = {eyeA(S), eyeB(S)};
    const float sides[2] = {-1, 1};
    const C3 col = hex("#2a1204");
    for (int j = 0; j < 2; j++) {
      const float s = sides[j];
      const float x0 = ps[j].x + s * 4, y0 = ps[j].y - S.L.eh * 0.9f;
      const float x1 = x0 + s * 14 + sinf(t * 7 + s) * 3, y1 = y0 - 22 + cosf(t * 7) * 2;
      S.scr->shape(minf(x0, x1) - 6, y1 - 6, maxf(x0, x1) + 6, y0 + 3, [&](float x, float y) {
        return minf(SD::segment(x, y, x0, y0, x1, y1) - 1.4f, SD::circle(x - x1, y - y1, 3.6f));
      }, col, 1);
    }
  }
};

// The hummingbird visits two flowers and shows off (reworked 2026-11-11). It hovers,
// darts left to a flower and sips, backs out of it, darts right to another and sips, backs out, then does a
// courtship dive (a swoop down and up, the robot spinning), hovers, and glows pink, full of nectar. On the trick
// clock: hover 1.45-2.55, dart left 2.55, sip 2.85-3.75, back out 3.75, dart right 4.05, sip 4.35-4.95, back out
// 4.95, dive 5.25-5.95, hover 5.95-6.55, happy to 8.05. The robot's moves are dances.cpp's kHummingbird, timed the
// same: it really travels to each flower and flies backwards out of it.
struct Hummingbird : Power {
  Hummingbird() : Power(IMG_HUMMINGBIRD, "HUMMINGBIRD", hex("#36d17c"), eyesOf(), {"Its wings\nbeat about", "50 times\na second!"}, 8.05) {}
  static Animal eyesOf() {
    Animal a = animal("#121418", "#000000", "#3a4440");
    a.pupil = P_NONE;
    a.sw = 0.48f;
    a.sh = 0.48f;
    a.round = 1;
    return a;
  }
  static constexpr float kSide = 30;  // how far it darts to each flower
  // Where the face goes: left to the first flower and back, right to the second and back.
  static float sideX(float t) {
    if (t < 2.55f) return 0;
    if (t < 3.75f) return -kSide * Ease::outCubic(prog(t, 2.55f, 0.22f));
    if (t < 4.05f) return -kSide * (1 - Ease::outCubic(prog(t, 3.75f, 0.25f)));
    if (t < 4.95f) return kSide * Ease::outCubic(prog(t, 4.05f, 0.22f));
    return kSide * (1 - Ease::outCubic(prog(t, 4.95f, 0.25f)));
  }
  static bool sipping(float t) { return (t > 2.85f && t < 3.75f) || (t > 4.35f && t < 4.95f); }
  static bool diving(float t) { return t > 5.25f && t < 5.95f; }
  bool pbackground(Sc& S, float t) override {
    const float k = prog(t, 1.45f, 0.4f);
    if (k <= 0) return false;
    const float sh = sinf(t * 1.5f) * 0.5f + 0.5f;
    const C3 top = mixc(hex("#b8f5d8"), hex("#ffd1ec"), sh), bot = mixc(hex("#5fe0a0"), hex("#ff7ab8"), 1 - sh);
    S.scr->fillGradient(mixc(S.skin->bg, top, k), mixc(S.skin->bg2, bot, k), 6);
    return true;
  }
  void plook(Sc& S, float t) override {
    S.face.mouthOn = false;  // a beak instead (set here, before the face draws, so no frame shows the mouth)
    S.face.f[F_Y].snap(sinf(t * 3) * 3);
    S.face.f[F_X].snap(sinf(t * 90) * 0.6f);
  }
  void pframe(Sc& S, float t) override {
    Face& f = S.face;
    const float dip = sipping(t) ? fabsf(sinf(t * 7)) * 5 : 0;                        // beak in, beak out
    const float dive = diving(t) ? sinf((t - 5.25f) / 0.7f * PIF) * 58 : 0;            // down and back up
    f.f[F_X].snap(sideX(t) + sinf(t * 90) * 0.6f);
    f.f[F_Y].snap(sinf(t * 3) * 3 + dip + dive);
    if (diving(t)) f.set(JOY, KEEP);
    else if (t > 5.95f) f.set(HAPPY, KEEP);
    f.f[F_BLUSH].to(t > 6.55f ? 0.8f : sipping(t) ? 0.35f : 0);
  }
  void punder(Sc& S, float t) override {
    // the two flowers, each growing up as it darts over, and ducking away before the dive
    for (int i = 0; i < 2; i++) {
      const float at = i ? 4.0f : 2.5f, k = Ease::outBack(prog(t, at, 0.35f)) * (1 - Ease::inCubic(prog(t, 5.2f, 0.3f)));
      if (k <= 0.01f) continue;
      const float x = S.L.cx + (i ? kSide : -kSide), sway = sinf(t * 2 + i * 2) * 0.08f;
      S.scr->sprite(A(IMG_FLOWER), x, 182, SpOpt().scale(0.42f * k).r(sway));
    }
  }
  void pmid(Sc& S, float t) override {
    const Layout& L = S.L;
    const float x = L.cx + S.face.f[F_X].x, y = L.cy + S.face.f[F_Y].x;
    // wing blur: three ghost positions of a wing fanning up and down beside the face
    for (int s = -1; s <= 1; s += 2) {
      const float x0 = x + s * 50, y0 = y + 4;
      for (int k = 0; k < 3; k++) {
        const float ang = -0.9f + k * 0.55f + sinf(t * 60 + k) * 0.15f, cs = cosf(ang), sn = sinf(ang);
        S.scr->shape(x0 - 50, y0 - 50, x0 + 50, y0 + 50, [&](float px, float yy) {
          const float dx = (px - x0) * s, dy = yy - y0, u = cs * dx + sn * dy, v = -sn * dx + cs * dy;
          return SD::ellipse(u - 20, v, 22, 7);
        }, hex("#2e8a5e"), 0.3f);
      }
    }
    // beak
    const float bx = x, by = y + 14;
    S.scr->shape(bx - 6, by - 4, bx + 6, by + 46, [&](float px, float yy) {
      return max3(fabsf(px - bx) - 3.4f * (1 - (yy - by) / 42), by - yy, yy - by - 40);
    }, hex("#121418"), 1);
  }
  void pover(Sc& S, float t) override {
    if (t < 5.2f && !sipping(t) && everyT(S, 3))
      textPop(S, "hmmm", S.L.cx + (S.r() < 0.5f ? -38 : 38), 46, TP().sz(11).up(-14).lif(0.8f));
    const float slurps[2] = {3.05f, 4.55f};
    for (int i = 0; i < 2; i++)
      if (t >= slurps[i] && t - S.dt < slurps[i]) textPop(S, "slurp!", S.L.cx + (i ? -36 : 36), 40, TP().sz(13));
    if (t >= 5.3f && t - S.dt < 5.3f) textPop(S, "zoom!", S.L.cx, 34, TP().sz(18));
    if (t >= 6.65f && t - S.dt < 6.65f) textPop(S, "mmm!", S.L.cx, 196, TP().sz(18));
  }
};

struct Spider : Power {
  // Two frights, "eek!" at 2.2 and 3.98 on the trick clock, scared until 5.76, then calm (dances.cpp, kSpider).
  Spider() : Power(IMG_SPIDER, "SPIDER", hex("#b678ff"), eyesOf(), {"Most spiders\nhave 8 eyes!"}, 7.05) {}
  static Animal eyesOf() {
    Animal a = animal("#17171c", "#000000", "#5a5a70");
    a.pupil = P_NONE;
    a.extra = 1;
    a.sw = 1.08f;
    a.sh = 1.08f;
    return a;
  }
  bool pbackground(Sc& S, float t) override {  // dusk purple, with a web in the corner
    const float k = worldIn(t);
    if (k <= 0) return false;
    const C3 top = hex("#c39bff"), bot = hex("#6a2bc2"), silk = hex("#fbf4ff"), bg = S.skin->bg, bg2 = S.skin->bg2;
    const float H = (float)S.scr->h, step = PIF / 14, gap = 15, reach = 150;
    S.scr->paint([&](int x, int y) -> C3 {
      const float fx = x + 0.5f, fy = y + 0.5f, dx = fx + 6, dy = fy + 6, r = len(dx, dy);
      C3 c = mixc(top, bot, fy / H);
      if (r < reach) {
        const float rel = fastAtan2(dy, dx) / step, fr = rel - floorf(rel) - 0.5f;  // -0.5..0.5 across a sector
        const float thread = r * sinf((0.5f - fabsf(fr)) * step);                  // to the nearer thread
        const float pr = r * cosf(fr * step), ring = fabsf(pr - gap * floorf(pr / gap + 0.5f));  // straight between threads
        const float line = minf(thread, pr > 8 ? ring : 9.f);
        const float a = clampf(1.2f - line, 0, 1) * 0.7f * clampf((reach - r) / 40, 0, 1);
        if (a > 0) c = mixc(c, silk, a);
      }
      return k < 1 ? mixc(mixc(bg, bg2, fy / H), c, k) : c;
    });
    return true;
  }
  void pframe(Sc& S, float t) override {
    Face& f = S.face;
    if (t < 5.76f) {
      f.set(SCARED, KEEP);
      shiver(S, 1.4f, 80);
      f.mouthShape = M_WAVE;
    } else {
      f.mouthShape = M_SMILE;
      f.f[F_X].to(0);
      f.set(HAPPY, KEEP);
    }
  }
  void pover(Sc& S, float t) override {
    if (t >= 2.2f && t - S.dt < 2.2f) textPop(S, "eek!", S.L.cx - 40, S.L.cy - 56, TP().sz(14));
    if (t >= 3.98f && t - S.dt < 3.98f) textPop(S, "eek!", S.L.cx + 40, S.L.cy - 56, TP().sz(16));
  }
};

struct Owl : Power {
  // Looks round one way, all the way to the back, and comes in from the other side; hoots; then the other way round;
  // hoo-hoo. On the trick clock: away left 2.3, in from the right 3.2, hoo 3.6, away right 4.1, in from the left 5.0,
  // hoo-hoo 5.45 (dances.cpp, kOwl).
  Owl() : Power(IMG_OWL, "OWL", hex("#6f82ff"), eyesOf(), {"Owls can\nturn their\nheads 270\xC2\xB0!", "That's most\nof the way\nround!"}, 6.8) {}
  static Animal eyesOf() {
    Animal a = animal("#ff9b1f", "#9a3f00", "#ffe08a");
    a.pupilSize = 1.45f;
    a.sw = 1.14f;
    a.sh = 0.98f;
    a.round = 1;
    return a;
  }
  void pframe(Sc& S, float t) override {
    Face& f = S.face;
    if (t > 2.3f && t < 3.2f) f.faceSet(F_X, -150 * Ease::inCubic(prog(t, 2.3f, 0.5f)), 900, 0.9f);
    else if (t >= 3.2f && t < 3.25f) f.f[F_X].snap(150);
    else if (t >= 3.25f && t < 4.1f) f.faceSet(F_X, 0, 160, 0.55f);
    else if (t >= 4.1f && t < 5.0f) f.faceSet(F_X, 150 * Ease::inCubic(prog(t, 4.1f, 0.5f)), 900, 0.9f);
    else if (t >= 5.0f && t < 5.05f) f.f[F_X].snap(-150);
    else if (t >= 5.05f) f.faceSet(F_X, 0, 160, 0.55f);
    if (t > 1.9f && t < 2.1f) f.eyes[0][E_OPEN].snap(0.1f);
    else if (t > 2.1f && f.blinkT_ < 0) f.eyes[0][E_OPEN].snap(1);
  }
  bool pbackground(Sc& S, float t) override {  // night: an indigo sky with a moon and twinkling stars
    const float k = worldIn(t);
    if (k <= 0) return false;
    S.scr->fillGradient(mixc(S.skin->bg, hex("#3f55d6"), k), mixc(S.skin->bg2, hex("#10184d"), k), 5);
    const float mx = 110, my = 26, mr = 11;  // a crescent moon, top right
    S.scr->shape(mx - mr - 2, my - mr - 2, mx + mr + 2, my + mr + 2, [&](float x, float y) {
      return maxf(SD::circle(x - mx, y - my, mr), -SD::circle(x - mx + 6, y - my - 4, mr - 1));
    }, hex("#fff1c2"), k);
    static const float kStars[][2] = {{16, 20}, {46, 10}, {80, 16}, {12, 62}, {125, 58}, {9, 150}, {127, 140}, {18, 232}, {118, 226}};
    for (int i = 0; i < 9; i++) {
      const float x = kStars[i][0], y = kStars[i][1], tw = 0.5f + 0.5f * sinf(t * 2.6f + i * 2.1f), s = 2.0f + 1.6f * tw;
      S.scr->shape(x - 6, y - 6, x + 6, y + 6, [&](float px, float py) { return SD::star(px - x, py - y, s, 0.42f); },
                   hex("#fff6d8"), k * (0.45f + 0.55f * tw));
    }
    // pale facial discs behind the eyes
    const P2 ps[2] = {eyeA(S), eyeB(S)};
    for (const P2& p : ps)
      S.scr->shape(p.x - 46, p.y - 46, p.x + 46, p.y + 46, [&](float x, float y) { return SD::circle(x - p.x, y - p.y, 41); }, hex("#ffe6c4"), k);
    return true;
  }
  void pmid(Sc& S, float) override {
    const Layout& L = S.L;
    const float x0 = L.cx + S.face.f[F_X].x, y0 = L.cy + S.face.f[F_Y].x - L.eh - 12;
    for (int s = -1; s <= 1; s += 2) {  // ear tufts, tan so they show on the night sky
      const float x = x0 + s * 46;
      S.scr->shape(x - 14, y0 - 22, x + 14, y0 + 8, [&](float px, float py) {
        return maxf(SD::segment(px, py, x - s * 8, y0 + 4, x + s * 5, y0 - 16) - 4.5f, py - y0 - 4);
      }, hex("#c98a4e"), 1);
    }
    // a beak where the mouth was (a dark mouth would vanish on the night sky)
    const float bx = x0, by = (eyeA(S).y + S.L.my) / 2 - 6, bl = 19, bw = 8;
    for (int o = 1; o >= 0; o--)
      S.scr->shape(bx - bw - 3, by - 3, bx + bw + 3, by + bl + 3, [&](float x, float y) {
        return max3(fabsf(x - bx) - bw * (1 - (y - by) / bl), by - y, y - by - bl) - o * 1.6f;
      }, o ? hex("#5a2a06") : hex("#ffb13b"), 1);
  }
  void plook(Sc& S, float) override { S.face.mouthOn = false; }  // the beak instead, before the face draws
  void pover(Sc& S, float t) override {
    const float hoots[3] = {3.6f, 5.45f, 5.8f};
    for (int i = 0; i < 3; i++)
      if (t >= hoots[i] && t - S.dt < hoots[i])
        textPop(S, "hoo!", S.L.cx + (i == 1 ? -40 : 40), S.L.cy - 60, TP().sz(16));
  }
};

struct Dolphin : Power {
  double spin = 0;
  const char* n = nullptr;
  // Three leaps counted "1!" "2!" "3!", then the big one, counting on to "7!" as it spins, and a big splash. On the
  // trick clock: leaps 2.2, 3.1, 4.0; the big leap 4.8-6.0; splash 6.0; bubbles to 7.9 (dances.cpp, kDolphin).
  Dolphin() : Power(IMG_DOLPHIN, "DOLPHIN", SKY, eyesOf(), {"Spinner\ndolphins spin", "up to 7 times\nin one jump!"}, 7.9) {}
  static Animal eyesOf() {
    Animal a = animal("#24486e", "#0c1a2c", "#6fa3d6");
    a.pupilSize = 1.1f;
    return a;
  }
  bool pbackground(Sc& S, float t) override {
    const float k = prog(t, 1.45f, 0.5f);
    S.scr->fillGradient(mixc(S.skin->bg, hex("#3fb6e0"), k), mixc(S.skin->bg2, hex("#0a3f78"), k), 5);
    return true;
  }
  void trick(Sc& S) override {
    static const char* const names[7] = {"1!", "2!", "3!", "4!", "5!", "6!", "7!"};
    const double times[7] = {2.2, 3.1, 4.0, 4.85, 5.15, 5.45, 5.75};
    for (int i = 0; i < 7; i++) {
      const char* nm = names[i];
      const bool leap = i < 3;
      S.after(times[i] - 1.45, [this, nm, leap](Sc& S) {
        spin = trickT(S);
        n = nm;
        if (leap) S.fx.burst(PT_DROP, S.L.cx, 200, 9, Pt().ang(-PIF / 2).spr(1.6f).spd(160).grav(400).sz(5).lif(1).col(hex("#d9f6ff")));
        S.face.set(JOY, KEEP);
      });
    }
    S.after(6.0 - 1.45, [](Sc& S) {  // the big splash
      S.fx.burst(PT_DROP, S.L.cx, 205, 18, Pt().ang(-PIF / 2).spr(2.4f).spd(220).grav(420).sz(6).lif(1.2f).col(hex("#d9f6ff")));
      S.fx.burst(PT_DROP, S.L.cx, 205, 8, Pt().ang(-PIF / 2).spr(1.2f).spd(150).grav(420).sz(4).lif(1).col(WHITE));
    });
  }
  void pframe(Sc& S, float t) override {
    Face& f = S.face;
    if (t > 4.8f && t < 6.0f) f.f[F_ROT].snap(Ease::inOut(prog(t, 4.8f, 1.2f)) * TAU * 3);  // the big leap
    else if (spin && t < 4.8f && t - spin < 0.6) f.f[F_ROT].snap(Ease::inOut((float)(t - spin) / 0.6f) * TAU);
    else f.f[F_ROT].snap(0);
    if (t > 6.2f) f.set(HAPPY, KEEP);
    if (everyT(S, 3)) {
      const float x = S.r() * S.L.w;
      const float sz = 2 + S.r() * 2;
      S.fx.emit(Pt(PT_RING).at(x, 240).vel(0, -60).sz(sz).lif(3).col(hex("#d9f6ff")).wid(0.9f));
    }
  }
  void pover(Sc& S, float t) override {
    if (n && t - spin < 0.85) {
      TextOpt o;
      o.size = n[0] == '7' ? 54 : 40;
      o.color = WHITE;
      o.outline = 2.5f;
      o.oc = hex("#0a3f78");
      o.scale = Ease::outBack(prog((float)(t - spin), 0, 0.3f));
      S.scr->text(n, S.L.cx, 196, o);
    }
  }
};

// The whirligig beetle whirls round on ponds, and each eye is split in two: the top half looks out of the water, the
// bottom half under it. Its eyes sit on a pond's waterline, so the fact shows, with a gold line across each eye. The
// trick is the long spin (the robot's moves.cpp, kWhirligig: about 4 s flat out). A spinning screen smears anything
// that isn't round, so the whirl is rings in every animal's colour flowing out from the middle, which stay sharp
// however fast it turns; spirals show while it speeds up and slows down, when they can be seen to turn. Then dizzy.
// On the trick clock: winding up 1.45-2.25, speeding up 2.25-3.25, the whirl 3.25-8.65, slowing 8.65-9.15, dizzy
// to 11.4.
// Firmware only so far: the review page doesn't have it (DESIGN.md, "The whirligig").
struct Whirligig : Power {
  Whirligig()
      : Power(IMG_WHIRLIGIG, "WHIRLIGIG", hex("#a8e63a"), eyesOf(),
              {"Whirligig\nbeetles spin\non ponds!", "Their eyes\nsee above and\nbelow water!"}, 11.4) {}
  static Animal eyesOf() {
    Animal a = animal("#1d3b5c", "#08131f", "#5fd6e0");
    a.pupil = P_NONE;
    a.round = 1;
    return a;
  }
  float tt(const Sc& S) const { return ph == PH_TRICK ? (float)trickT(S) : 0; }
  bool crossed(const Sc& S, double at) const {
    if (ph != PH_TRICK) return false;
    const double t = trickT(S);
    return t >= at && t - S.dt < at;
  }
  // How much of the screen the spin's patterns cover (0: the pond), and how much of that is rings, not the spiral.
  static float spinK(float t) { return prog(t, 2.25f, 0.5f) * (1 - prog(t, 8.9f, 0.45f)); }
  static float ringK(float t) { return prog(t, 3.0f, 0.4f) * (1 - prog(t, 8.6f, 0.4f)); }
  // The spiral's turn: speeding up from 2.25, then slowing from 8.6 to a stop at 9.4.
  static float spiralRot(float t) {
    if (t < 5) return 3 * (t - 2.25f) * (t - 2.25f);
    const float u = clampf(t - 8.6f, 0, 0.8f);
    return 12 * u - 7.5f * u * u;
  }
  bool pbackground(Sc& S, float t) override {
    const float k = prog(t, 1.45f, 0.4f);
    if (k <= 0) return false;
    // Every dance's colour, round the rainbow: cheetah, honeybee, whirligig, hummingbird, dolphin, owl, spider.
    static const C3 kRing[7] = {hex("#ff4b3a"), hex("#ffd23a"), hex("#a8e63a"), hex("#36d17c"),
                                hex("#7fd6ff"), hex("#6f82ff"), hex("#b678ff")};
    const C3 sky0 = hex("#dff6ff"), sky1 = hex("#9edcf4"), w0 = hex("#3fb8cf"), w1 = hex("#0c5470"),
             foam = hex("#f4fdff"), bg = S.skin->bg, bg2 = S.skin->bg2, lime = burst;
    const Layout& L = S.L;
    const float tr = tt(S), sk = spinK(tr), rk = ringK(tr), flow = tr * 1.2f, rot = spiralRot(tr);
    const float H = (float)S.scr->h, ey = eyeA(S).y;
    float wl[Screen::W];  // the waterline, through the middle of the eyes, rippling
    for (int x = 0; x < Screen::W; x++) wl[x] = ey + sinf((x + 0.5f) * 0.09f + t * 2.6f) * 2.2f;
    S.scr->paint([&](int x, int y) -> C3 {
      const float fx = x + 0.5f, fy = y + 0.5f, dx = fx - L.cx, dy = fy - L.cy;
      C3 pond{0, 0, 0}, spin{0, 0, 0};
      if (sk < 1) {
        const float dw = fy - wl[x];
        pond = dw < 0 ? mixc(sky0, sky1, clampf(fy / wl[x], 0, 1)) : mixc(w0, w1, clampf(dw / (H - wl[x]), 0, 1));
        if (fabsf(dw) < 1.6f) pond = mixc(pond, foam, 1 - fabsf(dw) / 1.6f);
        if (k < 1) pond = mixc(mixc(bg, bg2, fy / H), pond, k);
      }
      if (sk > 0) {
        const float d = len(dx, dy);
        C3 ring{0, 0, 0}, spiral{0, 0, 0};
        if (rk > 0) {  // a tunnel of wide colour bands flowing outwards, a dark seam between each, white-hot in the middle
          // (34 px bands: at 22 they blurred into each other while it spun, 2026-11-11)
          const float v = d / 34 - flow, f = v - floorf(v), seam = clampf(minf(f, 1 - f) * 34 / 1.6f, 0, 1);
          const float tube = (0.82f + 0.18f * (1 - fabsf(f * 2 - 1))) * (0.25f + 0.75f * seam);
          const C3 c = kRing[(static_cast<int>(floorf(v)) % 7 + 7) % 7];
          ring = C3{c.r * tube, c.g * tube, c.b * tube};
          if (d < 20) ring = mixc(WHITE, ring, d / 20);
        }
        if (rk < 1) {  // two wide soft-edged arms, cream and the beetle's lime
          const float v = fastAtan2(dy, dx) / TAU * 2 + d / 46 - rot, f = v - floorf(v);
          spiral = mixc(CREAM, lime, clampf((fabsf(f * 2 - 1) - 0.5f) * 6 + 0.5f, 0, 1));
        }
        spin = mixc(spiral, ring, rk);
      }
      return sk <= 0 ? pond : sk >= 1 ? spin : mixc(pond, spin, sk);
    });
    return true;
  }
  void pframe(Sc& S, float t) override {
    Face& f = S.face;
    if (t < 2.25f) {  // winding up
      f.set(FOCUSED, KEEP);
      shiver(S, 0.6f + (t - 1.45f) * 1.5f, 60);
    } else if (t < 3.0f) {  // off!
      f.set(JOY, KEEP);
      f.faceSet(F_ROT, sinf(t * 40) * 0.05f);
    } else if (t < 9.1f) {
      f.hidden = true;  // the screen is all spin
    } else {  // dizzy, wobbling less and less
      f.set(DIZZY, KEEP);
      const float w = 1 - prog(t, 9.15f, 2.0f) * 0.7f;
      f.faceSet(F_ROT, sinf(t * 5) * 0.14f * w);
      f.f[F_X].snap(sinf(t * 2.5f) * 6 * w);
    }
    if (crossed(S, 3.0)) zap(S, burst);
  }
  void punder(Sc& S, float t) override {  // winding up: rings closing in on the eyes
    if (t > 2.4f) return;
    const float k = prog(t, 1.45f, 0.2f) * (1 - prog(t, 2.2f, 0.2f)), cx = S.L.cx, cy = eyeA(S).y;
    for (int i = 0; i < 3; i++) {
      const float r = wrapf(90 - (t - 1.45f) * 110 + i * 30, 90) + 18;
      S.scr->shape(cx - r - 3, cy - r - 3, cx + r + 3, cy + r + 3,
                   [&](float x, float y) { return fabsf(len(x - cx, y - cy) - r) - 1.5f; }, CREAM, 0.6f * k);
    }
  }
  void pmid(Sc& S, float) override {  // each eye split in two by a gold line, until it takes off
    if (S.face.hidden || (ph == PH_TRICK && trickT(S) > 2.25)) return;  // not across the happy arcs at take-off
    for (int i = 0; i < 2; i++) {
      const P2 p = i ? eyeB(S) : eyeA(S);
      const float w = S.L.ew * S.face.eyes[i][E_SW].x * 0.97f;
      const float a = clampf((S.face.eyes[i][E_OPEN].x - 0.3f) / 0.5f, 0, 1);  // not across a blink
      S.scr->shape(p.x - w - 3, p.y - 3, p.x + w + 3, p.y + 3,
                   [&](float x, float y) { return SD::segment(x, y, p.x - w, p.y, p.x + w, p.y) - 1.3f; }, GOLD, a);
    }
  }
  void pover(Sc& S, float t) override {
    if (crossed(S, 2.3)) textPop(S, "wheee!", S.L.cx, 50, TP().sz(18));
    if (t > 9.15f) stars(S, t, S.L.cx + S.face.f[F_X].x, S.L.cy - 52, 36, 7, 4, GOLD);
    if (crossed(S, 9.5)) textPop(S, "whoa...", S.L.cx, 196, TP().sz(16));
  }
};

struct Bat : Power {
  float dist = 0;
  Bat() : Power(IMG_BAT, "BAT", hex("#b28cff"), eyesOf(), {"Many bats\nfind bugs", "by listening\nfor echoes!"}, 6.8) { hand = true; }
  static Animal eyesOf() {
    Animal a = animal("#2a1a14", "#000000", "#6a4a3a");
    a.sw = 0.62f;
    a.sh = 0.62f;
    a.round = 1;
    a.pupilSize = 0.8f;
    return a;
  }
  bool pbackground(Sc& S, float t) override {
    const float k = prog(t, 1.45f, 0.5f);
    S.scr->fillGradient(mixc(S.skin->bg, hex("#241a4a"), k), mixc(S.skin->bg2, hex("#0c0a1e"), k), 5);
    return true;
  }
  void pframe(Sc& S, float t) override {
    const float d = maxf(4, 60 - (t - 1.6f) * 14), rate = 1 / (0.08f + d / 90);
    dist = d;
    if (t < 5.6f && everyT(S, rate))
      S.fx.emit(Pt(PT_RING).at(S.L.cx, 30).sz(4).grw(160).lif(0.7f).fd(0.6f).col(hex("#c9b8ff")).wid(1.6f));
    if (t > 4.6f && t < 5.6f) {
      S.face.set(SURPRISED, KEEP);
      for (auto& e : S.face.eyes) {
        e[E_SW].to(0.9f);
        e[E_SH].to(0.9f);
      }
    }
    if (t > 5.6f) S.face.set(HAPPY, KEEP);
  }
  void pmid(Sc& S, float t) override {
    const Layout& L = S.L;
    const float y0 = L.cy + S.face.f[F_Y].x - L.eh * 0.55f;
    for (int s = -1; s <= 1; s += 2) {
      const float x = L.cx + s * 40, wig = sinf(t * 8 + s) * 0.08f, cw = cosf(wig), sw = sinf(wig);
      S.scr->shape(x - 30, y0 - 56, x + 30, y0 + 10, [&](float px, float py) {
        const float u = (px - x) * cw - (py - y0) * sw, v = (px - x) * sw + (py - y0) * cw;
        return max3(fabsf(u + s * v * 0.25f) - 14 * (1 + v / 48), -v - 48, v);
      }, hex("#8a5a3a"), 1);
    }
  }
  void pover(Sc& S, float t) override {
    if (t < 5.6f && dist) {
      char buf[16];
      snprintf(buf, sizeof(buf), "%d cm", (int)floorf(dist + 0.5f));
      TextOpt o;
      o.font = &FONT_SAY;
      o.size = 18;
      o.color = WHITE;
      o.outline = 2;
      o.oc = hex("#0c0a1e");
      S.scr->text(buf, S.L.cx, 212, o);
    }
  }
};

struct Chameleon : Power {
  Chameleon() : Power(IMG_CHAMELEON, "CHAMELEON", hex("#3aa66b"), eyesOf(), {"Chameleons\ncan look", "two ways\nat once!"}, 6.1) { hand = true; }
  static Animal eyesOf() {
    Animal a = animal("#3aa66b", "#0d4a2a", "#c7ff8a");
    a.pupilSize = 0.7f;
    return a;
  }
  bool pbackground(Sc& S, float t) override {
    const float k = prog(t, 1.45f, 0.5f);
    if (k <= 0) return false;
    const float h = fmodf(t * 0.25f, 1);
    const C3 c = hsl(h, 0.65f, 0.55f), c2 = hsl(fmodf(h + 0.08f, 1), 0.7f, 0.42f);
    S.scr->fillGradient(mixc(S.skin->bg, c, k), mixc(S.skin->bg2, c2, k), 5);
    return true;
  }
  void plook(Sc& S, float t) override {
    const float h = fmodf(t * 0.25f + 0.5f, 1);
    const C3 eye = hsl(h, 0.7f, 0.45f), light = hsl(h, 0.8f, 0.75f), dark = hsl(h, 0.6f, 0.18f);
    S.face.setEyeColors(&eye, &dark, &light);
  }
  void pframe(Sc& S, float t) override {
    Face& f = S.face;
    if (t < 5) {
      f.eyes[0][E_LOOKX].to(sinf(t * 2.1f) * 0.9f, 300, 0.7f);
      f.eyes[0][E_LOOKY].to(cosf(t * 1.3f) * 0.6f, 300, 0.7f);
      f.eyes[1][E_LOOKX].to(sinf(t * 1.4f + 2) * 0.9f, 300, 0.7f);
      f.eyes[1][E_LOOKY].to(sinf(t * 2.6f) * 0.6f, 300, 0.7f);
    } else {
      f.look(0, 0);
    }
  }
};

// ===== GAMES =====
const C3 SMOKE[] = {hex("#5a5254"), hex("#2e2a2c"), hex("#777072")};
const C3 GOLDWHITE[] = {hex("#ffd23a"), hex("#ffffff")};

struct HotPotato : Scene {
  double tick = 0;
  bool boom = false;
  void background(Sc& S, float t) override {
    const float k = clampf((t - 0.3f) / 5.5f, 0, 1);
    if (t > 6.0f) {
      S.scr->fillGradient(hex("#3a3236"), hex("#1c181a"), 8);
      return;
    }
    const C3 red = mixc(hex("#ff6a3a"), hex("#d4142a"), k);
    S.scr->fillGradient(mixc(S.skin->bg, red, k * 0.9f), mixc(S.skin->bg2, hex("#8a0a1a"), k * 0.9f), 6);
  }
  void setup(Sc& S) override {
    S.face.autoLook = false;
    S.at(0.4, [this](Sc& S) { S.face.set(SCARED); tick = 0; });
    S.at(6.0, [this](Sc& S) {
      Face& f = S.face;
      S.fx.flash = 1.4f;
      S.fx.burst(PT_PUFF, S.L.cx, S.L.cy, 18, Pt().spd(130).drg(3).sz(8).grw(18).lif(1.4f).cols(SMOKE, 3).spr(TAU));
      S.fx.burst(PT_SPARK, S.L.cx, S.L.cy, 12, Pt().spd(200).drg(2).sz(6).lif(0.6f).cols(GOLDWHITE, 2).spr(TAU));
      f.set(DIZZY);
      f.boing(1);
      f.f[F_SWEAT].snap(0);
      f.mouthShape = M_WAVE;
      boom = true;
      S.say("BOOM!", Shout().col(hex("#ffd23a")).ocol(hex("#2e2a2c")));
    });
    S.at(7.4, [](Sc& S) {
      S.unsay();
      S.face.mouthShape = M_SMILE;
      S.face.set(XX);
      S.fx.emit(Pt(PT_PUFF).at(S.L.cx, S.L.my).vel(0, -40).sz(4).grw(14).lif(0.8f).col(hex("#8a8486")));
      textPop(S, "cough", S.L.cx + 30, S.L.my - 10, TP().sz(12).col(WHITE).ocol(hex("#2e2a2c")));
    });
  }
  void frame(Sc& S, float t) override {
    Face& f = S.face;
    if (t > 0.4f && t < 6.0f) {
      const float p = (t - 0.4f) / 5.6f, rate = 1.4f + p * p * 9;
      tick += S.dt * rate;
      if (tick >= 1) {
        tick -= 1;
        const float x = S.L.cx + (S.r() - 0.5f) * 80;
        textPop(S, "tick", x, 200, TP().sz(12 + p * 6).up(-30).lif(0.5f).col(WHITE).ocol(hex("#8a0a1a")));
        f.kick(F_SY, 2 + p * 6);
      }
      shiver(S, p * 3, 60);
      if (every(S, 4)) f.look((S.r() - 0.5f) * 1.8f, 0, 900);
    }
  }
  void mid(Sc& S, float) override {
    if (!boom) return;
    // soot smudges on the face
    static const float spots[4][3] = {{-30, 30, 9}, {28, -36, 7}, {42, 24, 6}, {-48, -20, 5}};
    for (const auto& sp : spots) {
      const float cx = S.L.cx + sp[0], cy = S.L.cy + sp[1], r = sp[2];
      S.scr->shape(cx - r - 4, cy - r - 4, cx + r + 4, cy + r + 4, [&](float x, float y) {
        return SD::circle(x - cx, y - cy, r) + vnoise(x * 0.5f, y * 0.5f) * 4;
      }, hex("#151214"), 0.55f);
    }
  }
};

struct QuickDraw : Scene {
  void setup(Sc& S) override {
    S.at(0.2, [](Sc& S) { S.face.set(FOCUSED); S.say("Wait...", Line()); });
    S.at(1.4, [](Sc& S) { S.say("Wait...", Line().sz(22)); });
    S.at(2.6, [](Sc& S) {
      S.fx.flash = 1;
      S.face.set(SURPRISED);
      S.face.boing(0.9f);
      S.say("GO!", Shout().sz(54).at(196));
      S.fx.burst(PT_SPARK, S.L.cx, 196, 14, Pt().spd(180).drg(3).sz(6).lif(0.6f).cols(GOLDWHITE, 2).spr(TAU));
    });
    S.at(3.5, [](Sc& S) { S.face.set(STARRY); S.face.boing(0.4f); S.say("0.31 s", Shout().sz(36)); });
    S.at(4.8, [](Sc& S) {
      S.say("NEW RECORD!", Shout().bnc());
      confetti(S, 0, 240, 22, -1.2f);
      confetti(S, S.L.w, 240, 22, -PIF + 1.2f);
    });
    S.at(6.4, [](Sc& S) { S.unsay(); S.face.set(HAPPY); });
  }
  void frame(Sc& S, float t) override {
    if (t > 0.2f && t < 2.6f) {
      const float z = 1 + prog(t, 0.2f, 2.4f) * 0.12f;
      S.face.faceSet(F_SX, z);
      S.face.faceSet(F_SY, z);
    } else if (t > 2.6f && t < 2.7f) {
      S.face.faceSet(F_SX, 1);
      S.face.faceSet(F_SY, 1);
    }
  }
};

struct QuickEarly : Scene {
  bool laugh = false;
  void setup(Sc& S) override {
    S.at(0.2, [](Sc& S) { S.face.set(FOCUSED); S.say("Wait...", Line()); });
    S.at(1.3, [this](Sc& S) {
      S.face.set(JOY);
      S.face.faceSet(F_MOPEN, 1);
      S.say("Too soon!\nHee hee!", Line().wob());
      laugh = true;
    });
    S.at(4.2, [this](Sc& S) { S.unsay(); laugh = false; S.face.set(HAPPY); });
  }
  void frame(Sc& S, float t) override {
    if (laugh) {
      S.face.f[F_Y].snap(fabsf(sinf(t * 18)) * -4);
      S.face.faceSet(F_ROT, sinf(t * 9) * 0.06f);
    }
  }
};

struct HoleInOne : Scene {
  void setup(Sc& S) override {
    S.face.autoLook = false;
    S.at(0, [](Sc& S) { S.face.look(0, 0.9f, 200); });
    S.at(1.0, [](Sc& S) {
      S.face.kick(F_SY, -12);
      S.face.kick(F_SX, 9);
      S.face.set(SURPRISED);
      S.fx.burst(PT_SPARK, S.L.cx, 196, 8, Pt().spd(100).drg(3).sz(5).lif(0.5f).col(WHITE).spr(PIF).ang(-PIF / 2));
    });
    S.at(1.4, [](Sc& S) { S.face.set(STARRY); S.face.boing(0.7f); S.say("HOLE\nIN ONE!", Shout().bnc().at(200)); });
    S.at(4.4, [](Sc& S) { S.unsay(); S.face.set(HAPPY); S.say("Hits today: 3", Line()); });
    S.at(6.0, [](Sc& S) { S.unsay(); });
  }
  void frame(Sc& S, float t) override {
    if (t > 1.5f && t < 4.4f && every(S, 2.2f)) {
      const float x = 20 + S.r() * 95;
      const float y = 22 + S.r() * 40;
      S.fx.burst(PT_SPARK, x, y, 10, Pt().spd(70).drg(2.5f).grav(40).sz(4).lif(0.9f).cols(S.skin->confetti, 5).spr(TAU));
    }
  }
  void under(Sc& S, float t) override {
    // The robot is the hole, so its pin springs up between the eyes when the ball drops in, sways, and sinks.
    if (t < 1.05f) return;
    const float up = Ease::outBack(prog(t, 1.05f, 0.45f)) * (1 - Ease::inCubic(prog(t, 6.0f, 0.35f))), w = t - 1.05f;
    SpOpt o;
    o.sx = 0.6f;
    o.sy = 0.6f * up;
    o.ay = 0.97f;
    o.rot = sinf(w * 7) * 0.14f * expf(-w * 1.1f);
    S.scr->sprite(A(IMG_GOLFFLAG), S.L.cx + 4, 66, o);
  }
  void over(Sc& S, float t) override {
    if (t < 1.05f) {
      const float k = Ease::outCubic(t / 1.0f), y = 250 - k * 64, r = 9 - k * 2.5f;
      golfBall(S, S.L.cx + sinf(k * 3) * 6, y, r);
    }
  }
};

struct WhoNext : Scene {
  void setup(Sc& S) override {
    S.face.autoLook = false;
    S.at(0.2, [](Sc& S) { S.face.set(DIZZY); });
    S.at(3.0, [](Sc& S) { S.face.set(SURPRISED); S.face.boing(0.5f); S.face.look(0, -0.8f, 500); });
    S.at(3.4, [](Sc& S) { S.say("YOU!", Shout().sz(40)); });
    S.at(4.6, [](Sc& S) { S.unsay(); });
  }
  void frame(Sc& S, float t) override {
    if (t < 3) S.face.f[F_ROT].snap(Ease::outCubic(t / 3) * TAU * 3);
    else S.face.f[F_ROT].snap(0);
  }
};

// ===== TRAINING =====
struct TrainGuess : Scene {
  double slot = 0;
  bool slotEnd = false, locked = false;
  double pos = 0;
  void setup(Sc& S) override {
    S.face.autoLook = false;
    S.at(0.2, [](Sc& S) {
      S.face.set(CURIOUS);
      S.face.look(-0.7f, -0.8f, 300);
      textPop(S, "?", S.L.cx - 40, S.L.cy - 54, TP().sz(22).up(-10).lif(1.4f));
    });
    S.at(0.6, [this](Sc& S) { slot = S.t; });
    S.at(3.2, [this](Sc& S) {
      slotEnd = true;
      S.face.look(0, -0.9f, 300);
      S.face.set(SURPRISED, KEEPLOOK);
      S.face.boing(0.4f);
    });
    S.at(4.0, [](Sc& S) {
      animalEyes(S, CHEETAH_EYES);
      zap(S, GOLD);
      S.face.look(0, 0);
      S.say("Cheetah?", Shout());
    });
  }
  void over(Sc& S, float t) override {
    if (!slot || t > 4.0f) return;
    static const Img keys[7] = {IMG_OWL, IMG_HONEYBEE, IMG_DOLPHIN, IMG_CHEETAH, IMG_SPIDER, IMG_BAT, IMG_HUMMINGBIRD};
    const float st = (float)(t - slot), speed = slotEnd ? 0 : maxf(1.2f, 12 - st * 4.2f);
    pos += S.dt * speed;
    if (slotEnd && !locked) {
      locked = true;
      pos = 3;
    }
    const int i = (int)floor(pos) % 7;
    const float fr = slotEnd ? 0 : (float)fmod(pos, 1);
    const float bx = S.L.cx, by = 200;
    S.scr->shape(bx - 34, by - 30, bx + 34, by + 30, [&](float x, float y) {
      return fabsf(SD::roundBox(x - bx, y - by, 30, 26, 9)) - 1.6f;
    }, contrast(S), 1);
    for (int k = 0; k < 2; k++) {
      const float y = by + (k - fr) * 44;
      S.scr->sprite(A(keys[(i + k) % 7]), bx, y, SpOpt().scale(0.42f),
                    [&](float x, float yy) { return fabsf(yy - by) < 24 && fabsf(x - bx) < 28; });
    }
  }
};

struct TrainYes : Scene {
  double chomp = 0;
  void setup(Sc& S) override {
    S.face.autoLook = false;
    S.at(0.1, [](Sc& S) { S.face.look(0, -0.9f, 300); S.face.set(SURPRISED, KEEPLOOK); });
    S.at(0.9, [this](Sc& S) { S.face.look(0, 0.3f, 400); S.face.set(JOY); S.face.mouthShape = M_CHOMP; chomp = S.t; });
    S.at(2.6, [this](Sc& S) {
      chomp = 0;
      S.face.mouthShape = M_SMILE;
      S.face.set(LOVE);
      hearts(S, 8, S.L.cx, S.L.cy);
      S.say("Yum!", Shout());
    });
    S.at(3.8, [](Sc& S) { S.face.set(PROUD); S.say("Cheetah:\n3 of 5", Line()); });
    S.at(5.2, [](Sc& S) { S.unsay(); });
  }
  void frame(Sc& S, float t) override {
    Face& f = S.face;
    if (chomp) {
      f.chomp = fabsf(sinf((float)(t - chomp) * 11));
      f.f[F_Y].snap(f.chomp * 2);
      if (every(S, 6))
        S.fx.burst(PT_CIRCLE, S.L.cx, S.L.my + 4, 2, Pt().spd(70).grav(300).sz(1.6f).lif(0.6f).col(hex("#c98a3a")).ang(PIF / 2).spr(2.4f));
    }
  }
  void over(Sc& S, float t) override {
    if (t < 0.95f) {
      const float k = Ease::inCubic(prog(t, 0.2f, 0.75f)), y = -20 + k * (S.L.my + 30), sc = 0.42f * (1 - k * 0.6f);
      S.scr->sprite(A(IMG_TREAT), S.L.cx, y, SpOpt().scale(sc).r(t * 4));
    }
    if (t > 3.8f && t < 5.4f) {
      for (int i = 0; i < 5; i++) {
        const float x = S.L.cx - 36 + i * 18.f, y = 166;
        const bool on = i < 3;
        S.scr->shape(x - 9, y - 9, x + 9, y + 9, [&](float px, float py) { return SD::star(px - x, py - y, 7, 0.5f); },
                     on ? GOLD : mixc(contrast(S), S.skin->bg, 0.6f), on ? 1 : 0.8f);
      }
    }
  }
};

struct TrainNo : Scene {
  void setup(Sc& S) override {
    S.at(0.2, [](Sc& S) { S.face.set(SAD); S.face.kick(F_Y, 60); S.say("Oops!", Shout()); });
    S.at(2.2, [](Sc& S) { S.face.set(FOCUSED); S.face.kick(F_Y, -60); S.say("I'll try\nagain!", Line()); });
    S.at(4.4, [](Sc& S) { S.unsay(); S.face.set(NEUTRAL); });
  }
};

// ===== EVERYDAY =====
struct Nap : Scene {
  double sleep = 0, pop = 0;
  void setup(Sc& S) override {
    S.face.autoLook = false;
    S.at(0.2, [](Sc& S) { S.face.set(SLEEPY); });
    S.at(1.8, [this](Sc& S) {
      Face& f = S.face;
      f.autoBlink = false;
      for (auto& e : f.eyes) e[E_LIDT].to(1, 80, 0.9f);
      f.faceSet(F_MOPEN, 0);
      f.faceSet(F_Y, 8, 40, 0.9f);
      sleep = S.t;
    });
    S.at(7.0, [this](Sc& S) {
      Face& f = S.face;
      sleep = 0;
      pop = S.t;
      f.autoBlink = true;
      f.set(SURPRISED);
      f.boing(0.9f);
      f.kick(F_Y, -200);
    });
    S.at(8.0, [](Sc& S) { S.face.set(GRUMPY); });
  }
  void frame(Sc& S, float t) override {
    S.scr->dim = sleep ? 1 - 0.6f * prog(t, (float)sleep + 0.5f, 2) : 1;
    if (sleep && every(S, 1 / 1.6f)) textPop(S, "z", S.L.cx + 44, S.L.cy - 40, TP().sz(13).vel(10, -18).lif(1.6f));
  }
  void over(Sc& S, float t) override {
    if (sleep) {
      const float br = sinf((float)(t - sleep) * 1.9f) * 0.5f + 0.5f, r = 3 + br * 9;
      const float x = S.L.cx + 6 + r * 0.4f, y = S.L.my + 6 + S.face.f[F_Y].x + r * 0.6f;
      S.scr->shape(x - r - 2, y - r - 2, x + r + 2, y + r + 2, [&](float px, float py) { return fabsf(SD::circle(px - x, py - y, r)) - 0.9f; }, hex("#d6f2ff"), 0.9f);
      S.scr->shape(x - r, y - r, x + r, y + r, [&](float px, float py) { return SD::circle(px - x + r * 0.4f, py - y + r * 0.4f, r * 0.22f); }, WHITE, 0.9f);
    }
    if (pop && t - pop < 0.3)
      S.fx.burst(PT_CIRCLE, S.L.cx + 10, S.L.my + 10, 6, Pt().spd(80).sz(1.5f).lif(0.3f).col(hex("#d6f2ff")).spr(TAU));
  }
};

struct Hungry : Scene {
  double rumble = 0, charge = 0;
  void setup(Sc& S) override {
    S.face.autoLook = false;
    S.at(0.2, [](Sc& S) { S.face.set(HUNGRY); S.say("I'm hungry...", Line()); });
    S.at(1.8, [this](Sc& S) { rumble = S.t; textPop(S, "grrrumble", S.L.cx, 178, TP().sz(12).up(-10).lif(1)); });
    S.at(3.6, [this](Sc& S) { S.unsay(); charge = S.t; S.face.set(JOY); S.face.mouthShape = M_CHOMP; });
    S.at(6.6, [](Sc& S) { S.face.mouthShape = M_SMILE; S.face.set(HAPPY); S.say("Full!", Shout()); });
  }
  void frame(Sc& S, float t) override {
    Face& f = S.face;
    if (rumble && t - rumble < 0.8) shiver(S, 2, 50);
    else if (!charge) f.f[F_X].to(0);
    if (charge && t < 6.6f) {
      f.chomp = fabsf(sinf(t * 10));
      if (every(S, 2)) textPop(S, "nom", S.L.cx + (S.r() < 0.5f ? -38 : 38), S.L.my, TP().sz(13).up(-30));
    }
  }
  void over(Sc& S, float t) override {
    const float lv = charge ? 0.1f + 0.9f * prog(t, (float)charge, 3) : 0.1f;
    const bool blink = !charge && ((int)floorf(t * 3) % 2);
    battery(S, S.L.cx, 22, lv, lv < 0.25f ? hex("#ff3b30") : hex("#34d17a"));
    if (blink) S.scr->shape(S.L.cx - 22, 10, S.L.cx + 24, 34, [](float, float) { return -1.f; }, S.skin->bg, 0.6f);
    if (charge && t < 6.6f) bolt(S, S.L.cx + 34, 22, 0.8f + sinf(t * 12) * 0.1f, GOLD);
  }
};

// On the charger: munching while the battery fills, then content. Live, the icon shows face::batteryLevel and the robot
// sends "full" when it is charged.
struct Charging : Scene {
  double full = -1;
  void setup(Sc& S) override {
    S.face.autoLook = false;
    S.at(0.2, [](Sc& S) {
      S.face.set(JOY);
      S.face.mouthShape = M_CHOMP;
      S.face.boing(0.4f);
      S.say("Yum!", Shout());
    });
    S.at(1.6, [](Sc& S) { S.unsay(); });
    S.at(3.6, [this](Sc& S) {
      if (full >= 0) return;
      S.face.mouthShape = M_SMILE;
      S.face.set(HAPPY);
    });
    if (!live) S.at(4.4, [](Sc& S) { S.player->event("full"); });
  }
  void frame(Sc& S, float t) override {
    if (t > 0.2f && t < 3.6f && full < 0) {
      S.face.chomp = fabsf(sinf(t * 10));
      if (every(S, 2)) textPop(S, "nom", S.L.cx + (S.r() < 0.5f ? -38 : 38), S.L.my, TP().sz(13).up(-30));
    }
  }
  void over(Sc& S, float t) override {
    const bool isFull = full >= 0;
    const float lv = isFull ? 1 : live && batteryLevel >= 0 ? clampf(batteryLevel, 0, 1) : 0.15f + 0.85f * prog(t, 0.2f, 3.8f);
    battery(S, S.L.cx, 22, lv, lv < 0.25f ? hex("#ff3b30") : hex("#34d17a"));
    if (!isFull) bolt(S, S.L.cx + 34, 22, 0.8f + sinf(t * 12) * 0.1f, GOLD);
  }
  void event(Sc& S, const char* name) override {
    if (strcmp(name, "full") || full >= 0) return;
    Face& f = S.face;
    full = S.t;
    f.mouthShape = M_SMILE;
    f.set(HAPPY);
    f.boing(0.5f);
    S.say("Full!", Shout());
    S.fx.burst(PT_SPARK, S.L.cx, 22, 8, Pt().spd(90).spr(TAU).sz(4).lif(0.7f).col(GOLD));
  }
};

// Sound switched off: "shh!", a wink, and it zips its mouth shut. The robot sends "unmute" to unzip it.
struct Muted : Scene {
  double zip = -1, unzip = -1;
  void setup(Sc& S) override {
    S.at(0.2, [](Sc& S) {
      Face& f = S.face;
      f.faceSet(F_MOPEN, 0.5f);
      f.faceSet(F_MWIDE, 0.4f);
      f.faceSet(F_MCURVE, 0);
      f.look(0, 0, 300);
      textPop(S, "shh!", S.L.cx + 36, S.L.my - 8, TP().sz(15).up(-24));
    });
    S.at(0.7, [](Sc& S) {  // a wink
      Face& f = S.face;
      f.shapes[0] = SH_ARC;
      f.eyeSet(E_MORPH, 1, 0, 600, 0.8f);
      f.faceSet(F_MOPEN, 0);
      f.faceSet(F_MCURVE, 0.6f);
    });
    S.at(1.1, [this](Sc& S) {
      S.face.eyeSet(E_MORPH, 0, 0);
      zip = S.t;
      S.face.faceSet(F_MSHOW, 0, 200, 0.9f);
      textPop(S, "zip!", S.L.cx - 32, S.L.my + 4, TP().sz(13).up(-26));
    });
    S.at(1.9, [](Sc& S) {
      S.face.set(HAPPY);
      S.face.boing(0.3f);
      S.say("Quiet mode", Line());
    });
    S.at(3.9, [](Sc& S) { S.unsay(); });
  }
  void mid(Sc& S, float t) override {
    if (zip < 0 || (unzip >= 0 && t - unzip > 0.45)) return;
    zipper(S, unzip >= 0 ? 1 - Ease::inOut(prog(t, (float)unzip, 0.45f)) : Ease::inOut(prog(t, (float)zip, 0.55f)));
  }
  void over(Sc& S, float t) override {
    speakerOff(S, S.L.cx, 26,
               unzip >= 0 ? 1 - Ease::inCubic(prog(t, (float)unzip, 0.25f)) : Ease::outBack(prog(t, 1.9f, 0.35f)));
  }
  void event(Sc& S, const char* name) override {
    if (strcmp(name, "unmute") || zip < 0 || unzip >= 0) return;
    unzip = S.t;
    S.unsay();
    S.after(0.4, [](Sc& S) {
      Face& f = S.face;
      f.faceSet(F_MSHOW, 1, 300, 0.6f);
      f.set(JOY);
      f.boing(0.6f);
      S.say("La la la!", Shout());
      for (int i = 0; i < 3; i++)
        S.fx.emit(Pt(PT_NOTE).at(S.L.cx - 30 + i * 30, S.L.my).vel((i - 1) * 15.f, -50).sz(8).lif(1.2f).col(contrast(S)).rt((i - 1) * 0.2f));
    });
  }
};

struct MissedYou : Scene {
  void setup(Sc& S) override {
    S.at(0.3, [](Sc& S) { S.face.set(SURPRISED); S.face.boing(0.6f); S.say("You were gone\n3 days!", Line()); });
    S.at(2.2, [](Sc& S) {
      S.face.set(LOVE);
      S.face.faceSet(F_TEARS, 1);
      S.say("I missed you!", Shout().bnc());
      hearts(S, 10, S.L.cx, S.L.cy, heartOpts(S).spd(120).spr(TAU));
    });
    S.at(5.4, [](Sc& S) { S.unsay(); S.face.faceSet(F_TEARS, 0); S.face.set(HAPPY); });
  }
};

struct Peekaboo : Scene {
  double dark = 0;
  void setup(Sc& S) override {
    S.face.autoLook = false;
    S.at(0.2, [this](Sc& S) { dark = S.t; S.face.set(SCARED); S.say("Where'd everyone\ngo?", Line()); });
    S.at(3.0, [this](Sc& S) {
      dark = 0;
      S.fx.flash = 0.9f;
      S.face.set(JOY);
      S.face.faceSet(F_MOPEN, 1);
      S.face.boing(0.9f);
      S.say("Peekaboo!", Shout().bnc());
    });
    S.at(5.2, [](Sc& S) { S.unsay(); S.face.set(HAPPY); });
  }
  void frame(Sc& S, float t) override {
    if (dark && every(S, 1.5f)) {
      const float x = (S.r() - 0.5f) * 1.8f;
      const float y = S.r() - 0.5f;
      S.face.look(x, y, 300);
    }
  }
  void post(Sc& S, float t) override {
    if (dark) S.scr->darken(1 - 0.72f * prog(t, (float)dark, 0.3f));
  }
};

struct CrystalBall : Scene {
  double shake = 0, ball = 0;
  void setup(Sc& S) override {
    S.at(0.2, [](Sc& S) { S.face.set(CURIOUS); S.say("Ask me a\nquestion!", Line()); });
    S.at(1.7, [this](Sc& S) { S.unsay(); S.face.googly = true; S.face.autoLook = false; shake = S.t; });
    S.at(2.8, [this](Sc& S) { shake = 0; S.face.googly = false; ball = S.t; });
    S.at(6.6, [this](Sc& S) { ball = 0; S.face.set(HAPPY); });
  }
  void frame(Sc& S, float) override {
    if (shake) {
      const float gx = (S.r() - 0.5f) * 7;
      const float gy = (S.r() - 0.5f) * 7;
      S.face.grav[0] = gx;
      S.face.grav[1] = gy;
      shiver(S, 4, 40);
    } else {
      S.face.f[F_X].to(0);
    }
  }
  void over(Sc& S, float t) override {
    if (!ball) return;
    Screen& scr = *S.scr;
    const float bt = (float)(t - ball), cx = S.L.cx, cy = 116, R = 58 * Ease::outBack(prog(bt, 0, 0.35f));
    if (R < 0.5f) return;
    const C3 lit = hex("#2a3bb8"), deep = hex("#070b2a");
    scr.shade(cx - R - 3, cy - R - 3, cx + R + 3, cy + R + 3, [&](float x, float y) { return SD::circle(x - cx, y - cy, R); },
              [&](float x, float y, float) { return mixc(lit, deep, clampf(len(x - cx + 10, y - cy + 14) / R, 0, 1)); });
    scr.shape(cx - R - 4, cy - R - 4, cx + R + 4, cy + R + 4, [&](float x, float y) { return fabsf(SD::circle(x - cx, y - cy, R)) - 2.2f; }, hex("#0b0f14"), 1);
    const float k = Ease::outCubic(prog(bt, 0.6f, 1.6f)), sz = 16 + 26 * k, rot = (1 - k) * 0.9f + sinf(bt * 2) * 0.05f;
    const float ty = cy + 6 + (1 - k) * 20, cs = cosf(rot), sn = sinf(rot);
    scr.shape(cx - 50, cy - 50, cx + 50, cy + 50, [&](float x, float y) {
      const float u = cs * (x - cx) + sn * (y - ty), v = -sn * (x - cx) + cs * (y - ty);
      return maxf(-v - sz * 0.75f, v * 0.5f + fabsf(u) * 0.866f - sz * 0.4f);
    }, hex("#3b5cff"), 0.25f + 0.75f * k);
    if (k > 0.5f) {
      TextOpt o;
      o.size = 18;
      o.color = WHITE;
      o.alpha = (k - 0.5f) * 2;
      o.rot = rot * 0.5f;
      scr.text("YES!", cx, ty - 6, o);
    }
    scr.shape(cx - R, cy - R, cx + R, cy + R, [&](float x, float y) { return SD::ellipse(x - cx + R * 0.38f, y - cy + R * 0.48f, R * 0.24f, R * 0.13f); }, WHITE, 0.6f);
  }
};

struct Dice : Scene {
  double roll = 0, landed = 0;
  void setup(Sc& S) override {
    Face& f = S.face;
    f.autoLook = false;
    f.hidden = true;
    S.at(0.2, [this](Sc& S) { roll = S.t; });
    S.at(2.6, [this](Sc& S) { landed = S.t; });
    S.at(3.2, [](Sc& S) {
      Face& f = S.face;
      f.hidden = false;
      f.f[F_Y].snap(-30);
      f.f[F_SX].snap(0.5f);
      f.f[F_SY].snap(0.5f);
      f.faceSet(F_SX, 0.62f);
      f.faceSet(F_SY, 0.62f);
      f.faceSet(F_Y, -52);
      f.look(0, 0.9f, 300);
      f.set(SURPRISED, KEEPLOOK);
    });
    S.at(3.8, [](Sc& S) { S.face.set(JOY); S.say("Five!", Shout()); });
  }
  void over(Sc& S, float t) override {
    if (!roll) return;
    const float rt = (float)(t - roll), land = landed ? Ease::outBack(prog(t, (float)landed, 0.4f)) : 0;
    const float rot = landed ? (1 - land) * 0.4f : rt * 9;
    const float y = landed ? 150 : 150 - fabsf(sinf(rt * 6)) * 40 * (1 - rt / 2.6f);
    const int n = landed ? 5 : 1 + ((int)floorf(rt * 10) % 6);
    const float cx = S.L.cx, s = 34, cs = cosf(rot), sn = sinf(rot);
    auto locU = [&](float x, float yy) { return cs * (x - cx) + sn * (yy - y); };
    auto locV = [&](float x, float yy) { return -sn * (x - cx) + cs * (yy - y); };
    S.scr->shape(cx - 56, y - 56, cx + 56, y + 56, [&](float x, float yy) { return SD::roundBox(locU(x, yy), locV(x, yy), s, s, 10) - 2; }, hex("#2a1204"), 1);
    const C3 top = WHITE, bot = hex("#e6ded0");
    S.scr->shade(cx - 56, y - 56, cx + 56, y + 56, [&](float x, float yy) { return SD::roundBox(locU(x, yy), locV(x, yy), s, s, 10); },
                 [&](float x, float yy, float) { return mixc(top, bot, clampf((locV(x, yy) + s) / (2 * s), 0, 1)); });
    static const int8_t PIPS[7][6][2] = {
        {},
        {{0, 0}},
        {{-1, -1}, {1, 1}},
        {{-1, -1}, {0, 0}, {1, 1}},
        {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}},
        {{-1, -1}, {1, -1}, {0, 0}, {-1, 1}, {1, 1}},
        {{-1, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {1, 1}},
    };
    for (int j = 0; j < n; j++) {
      const float a = PIPS[n][j][0], b = PIPS[n][j][1];
      // the pip's centre on screen bounds the work
      const float px0 = cx + cs * a * 17 - sn * b * 17, py0 = y + sn * a * 17 + cs * b * 17;
      S.scr->shape(px0 - 8, py0 - 8, px0 + 8, py0 + 8, [&](float x, float yy) {
        return SD::circle(locU(x, yy) - a * 17, locV(x, yy) - b * 17, 6);
      }, hex("#1d1a22"), 1);
    }
  }
};

struct WakeUp : Scene {
  void setup(Sc& S) override {
    Face& f = S.face;
    f.autoBlink = false;
    f.autoLook = false;
    for (auto& e : f.eyes) e[E_OPEN].snap(0);
    S.scr->dim = 0;
    S.at(2.2, [](Sc& S) {
      Face& f = S.face;
      f.faceSet(F_MOPEN, 1.1f);
      f.faceSet(F_MCURVE, 0);
      f.faceSet(F_MWIDE, 0.7f);
      f.faceSet(F_SY, 1.12f, 60, 0.5f);
      f.faceSet(F_SX, 0.92f, 60, 0.5f);
    });
    S.at(3.4, [](Sc& S) { S.face.faceSet(F_MOPEN, 0); S.face.faceSet(F_SY, 1); S.face.faceSet(F_SX, 1); S.face.boing(0.3f); });
    S.at(3.8, [](Sc& S) { S.face.autoBlink = true; S.face.blink(); });
    S.at(4.4, [](Sc& S) { S.face.set(HAPPY); S.say("Good morning!", Line()); });
    S.at(6.0, [](Sc& S) { S.unsay(); });
  }
  void frame(Sc& S, float t) override {
    Face& f = S.face;
    S.scr->dim = Ease::outCubic(prog(t, 0.1f, 1.2f));
    if (t < 2.2f) {
      const float o = Ease::inOut(prog(t, 0.6f, 1.4f)) * 0.45f;
      for (auto& e : f.eyes) e[E_OPEN].snap(o);
    } else if (t < 3.4f) {
      for (auto& e : f.eyes) e[E_OPEN].snap(0.05f);
    } else if (t < 3.8f) {
      for (auto& e : f.eyes) e[E_OPEN].snap(Ease::outCubic(prog(t, 3.4f, 0.3f)));
    }
  }
};

struct Listening : Scene {
  double listen = 0;
  void setup(Sc& S) override {
    S.face.autoLook = false;
    S.at(0.2, [this](Sc& S) { S.face.set(SURPRISED); S.face.look(0, -0.3f); listen = S.t; });
    S.at(3.4, [this](Sc& S) {
      listen = 0;
      S.face.set(HAPPY);
      S.face.look(0, 0);
      textPop(S, "Got it!", S.L.cx, 200, TP().sz(18).say().up(-10));
    });
  }
  void over(Sc& S, float t) override {
    if (listen) listenOver(S, t, listen);
  }
};

// ===== EXPLORE =====
struct Boop : Scene {
  void setup(Sc& S) override {
    S.face.autoLook = false;
    S.at(0.8, [](Sc& S) {
      Face& f = S.face;
      f.eyes[0][E_LOOKX].to(0.85f, 600, 0.5f);
      f.eyes[1][E_LOOKX].to(-0.85f, 600, 0.5f);
      f.eyeSet(E_LOOKY, 0.7f, 2, 600, 0.5f);
      f.kick(F_SY, -6);
      f.faceSet(F_MOPEN, 0.4f);
      f.faceSet(F_MCURVE, 0);
      f.faceSet(F_MWIDE, 0.5f);
      textPop(S, "boop!", S.L.cx, S.L.cy + 6, TP().sz(16).up(-40));
    });
    S.at(2.4, [](Sc& S) { S.face.look(0, 0); S.face.set(JOY); });
    S.at(3.4, [](Sc& S) { S.face.set(NEUTRAL); });
  }
};

struct Sneeze : Scene {
  void setup(Sc& S) override {
    S.at(0.3, [](Sc& S) {
      Face& f = S.face;
      f.eyeSet(E_LIDT, 0.5f);
      f.eyeSet(E_LIDB, 0.3f);
      f.faceSet(F_MOPEN, 0.4f);
      f.faceSet(F_MCURVE, 0);
      f.faceSet(F_MWIDE, 0.6f);
      textPop(S, "ah...", S.L.cx + 30, S.L.cy - 50, TP().sz(13));
    });
    S.at(1.0, [](Sc& S) {
      S.face.faceSet(F_Y, -10, 30, 0.7f);
      S.face.faceSet(F_SY, 1.08f, 30, 0.7f);
      textPop(S, "ahh...", S.L.cx - 30, S.L.cy - 56, TP().sz(15));
    });
    S.at(2.0, [](Sc& S) {
      Face& f = S.face;
      f.set(JOY);
      f.faceSet(F_MOPEN, 1);
      f.f[F_Y].snap(12);
      f.kick(F_SY, -14);
      S.fx.flash = 0.5f;
      S.say("ACHOO!", Shout().sz(36));
      S.fx.burst(PT_CIRCLE, S.L.cx, S.L.my, 16, Pt().ang(PIF / 2).spr(1.8f).spd(160).grav(100).sz(1.6f).lif(0.7f).col(hex("#bfeaff")));
    });
    S.at(3.4, [](Sc& S) {
      S.unsay();
      S.face.set(HAPPY);
      S.face.faceSet(F_Y, 0);
      S.face.faceSet(F_SY, 1);
      textPop(S, "excuse me", S.L.cx, 200, TP().sz(13).say());
    });
  }
};

struct ShyPeek : Scene {
  void setup(Sc& S) override {
    Face& f = S.face;
    f.autoLook = false;
    f.f[F_Y].snap(150);
    f.set(SCARED, SNAP);
    f.f[F_SWEAT].snap(0);
    S.at(0.4, [](Sc& S) { S.face.faceSet(F_Y, 96, 60, 0.8f); });
    S.at(1.4, [](Sc& S) { S.face.look(-0.9f, -0.6f, 200); });
    S.at(2.2, [](Sc& S) { S.face.look(0.9f, -0.6f, 200); });
    S.at(3.0, [](Sc& S) { S.face.look(0, -0.7f, 200); });
    S.at(3.6, [](Sc& S) { S.face.faceSet(F_Y, 40, 50, 0.8f); S.face.set(CURIOUS, KEEPLOOK); });
    S.at(5.0, [](Sc& S) { S.face.faceSet(F_Y, 0, 60, 0.7f); S.face.set(HAPPY); S.face.look(0, 0); S.say("Hi...", Line()); });
    S.at(7.0, [](Sc& S) { S.unsay(); S.fx.flash = 0.4f; S.face.set(SCARED); S.face.faceSet(F_Y, 150, 500, 0.7f); });
  }
};

struct FreezeDance : Scene {
  bool frozen = false;
  void background(Sc& S, float t) override {
    const int beat = (int)floorf(t * 2.4f);
    const bool still = t > 4.2f && t < 6.4f;
    const C3 c = still ? S.skin->bg : mixc(S.skin->bg, hsl(fmodf(beat * 0.17f, 1), 0.7f, 0.55f), 0.55f);
    S.scr->fillGradient(c, mixc(c, BLACK, 0.2f), 6);
  }
  void setup(Sc& S) override {
    S.face.set(JOY);
    S.at(4.2, [this](Sc& S) { frozen = true; S.face.set(SURPRISED); S.say("FREEZE!", Shout().sz(34)); });
    S.at(6.4, [this](Sc& S) { frozen = false; S.unsay(); S.face.set(JOY); });
  }
  void frame(Sc& S, float t) override {
    Face& f = S.face;
    if (frozen) {
      f.autoBlink = false;
      return;
    }
    f.autoBlink = true;
    const float b = t * 2.4f * PIF;
    f.faceSet(F_ROT, sinf(b) * 0.18f, 500, 0.5f);
    f.faceSet(F_Y, -fabsf(sinf(b)) * 10, 500, 0.5f);
    f.faceSet(F_X, sinf(b / 2) * 10, 500, 0.5f);
    if (every(S, 2.4f)) {
      const float x = S.L.cx + (S.r() - 0.5f) * 110;
      const float rot = (S.r() - 0.5f) * 0.5f;
      S.fx.emit(Pt(PT_NOTE).at(x, 200).vel(0, -60).sz(8).lif(1.2f).col(contrast(S)).rt(rot));
    }
  }
};

struct SpinArt : Scene {
  void background(Sc& S, float t) override {
    Screen& scr = *S.scr;
    const float cx = scr.w / 2.f, cy = scr.h / 2.f, rot = t * 18;
    scr.paint([&](int x, int y) {
      const float dx = x - cx, dy = y - cy, r = len(dx, dy), a = fastAtan2(dy, dx) + rot;
      const float k = wrapf((a / TAU) * 6 + r * 0.02f, 1);
      return hsl(ffmod(k + r * 0.004f, 1), 0.85f, 0.55f);
    });
  }
  void setup(Sc& S) override { S.face.hidden = true; }
};

// ===== PLAY: the living face you can poke =====
struct Play : Scene {
  enum Mode { IDLE, LIFTED, SHAKE, FLIPPED } mode = IDLE;
  double at = 0, gig = -1e9;
  bool dizzy = false;
  int n = 0;
  void frame(Sc& S, float t) override {
    Face& f = S.face;
    if (mode == SHAKE && t - at < 1.4) {
      f.googly = true;
      const float gx = (S.r() - 0.5f) * 7;
      const float gy = (S.r() - 0.5f) * 7;
      f.grav[0] = gx;
      f.grav[1] = gy;
      f.f[F_X].snap(sinf(t * 55) * 4);
    } else if (mode == SHAKE && !dizzy) {
      dizzy = true;
      f.googly = false;
      f.set(DIZZY);
      S.after(2.4, [this](Sc& S) { S.face.set(NEUTRAL); mode = IDLE; dizzy = false; });
    }
    if (mode == SHAKE && dizzy) {
      f.faceSet(F_X, cosf(t * 4.5f) * 5);
      f.faceSet(F_ROT, sinf(t * 3) * 0.12f);
    }
    if (mode == LIFTED) f.faceSet(F_ROT, sinf(t * 5) * 0.16f);
    if (mode == FLIPPED) f.faceSet(F_ROT, sinf(t * 16) * 0.12f, 900, 0.3f);
    if (t - gig < 0.45) f.f[F_Y].snap(sinf(t * 60) * 1.6f);
  }
  void over(Sc& S, float t) override {
    if (mode == SHAKE && dizzy) stars(S, t, S.L.cx + S.face.f[F_X].x, S.L.cy - 50, 38, 7, 3, GOLD);
  }
  void event(Sc& S, const char* name) override {
    Face& f = S.face;
    S.unsay();
    f.faceSet(F_ROT, 0);
    f.faceSet(F_X, 0);
    f.mouthShape = M_SMILE;
    f.googly = false;
    const auto is = [name](const char* s) { return strcmp(name, s) == 0; };
    if (is("pickup")) {
      mode = LIFTED;
      f.set(SURPRISED);
      f.boing(0.8f);
      f.kick(F_Y, -160);
      S.after(0.35, [](Sc& S) { S.face.set(JOY); S.say("Wheee!", Shout().bnc()); });
      S.after(2.6, [this](Sc& S) { if (mode == LIFTED) S.unsay(); });
    } else if (is("putdown")) {
      mode = IDLE;
      f.kick(F_SY, -9);
      f.kick(F_SX, 7);
      f.set(HAPPY);
      f.eyeSet(E_LIDT, 0.75f);
      S.say("phew...", Line());
      S.after(1.6, [](Sc& S) { S.unsay(); S.face.set(NEUTRAL); });
    } else if (is("shake")) {
      mode = SHAKE;
      at = S.t;
      dizzy = false;
      f.autoLook = false;
      f.faceSet(F_MOPEN, 0.9f);
      f.faceSet(F_MCURVE, 0);
      S.after(4, [](Sc& S) { S.face.autoLook = true; });
    } else if (is("pat")) {
      n++;
      patOnce(S, n, &gig);
      if (n >= 3) n = 0;
    } else if (is("flip")) {
      mode = FLIPPED;
      f.set(SCARED);
      f.mouthShape = M_WAVE;
      S.say("Help!\nI'm stuck!", Line().wob());
    } else if (is("unflip")) {
      mode = IDLE;
      f.set(JOY);
      f.boing(0.6f);
      S.say("Thanks!", Shout());
      S.after(1.4, [](Sc& S) { S.unsay(); S.face.set(NEUTRAL); });
    } else if (is("button")) {
      f.set(LOVE);
      f.boing(0.5f);
      hearts(S, 8, S.L.cx, S.L.cy, heartOpts(S).spd(120).spr(TAU));
      S.after(1.6, [](Sc& S) { S.face.set(NEUTRAL); });
    }
  }
  void tap(Sc& S) override { event(S, "pat"); }
};

template <class T>
Scene* mk() { return new T(); }

}  // namespace

const SceneInfo SCENES[] = {
    {"idle", "Face", "Living eyes", 13, 4.6f, mk<Idle>},
    {"googly", "Face", "Googly eyes", 9, 5, mk<Googly>},
    {"mood-happy", "Moods", "Happy", 4.4f, 2.2f, mk<MoodHappy>},
    {"mood-love", "Moods", "Love", 4.4f, 2.2f, mk<MoodLove>},
    {"mood-sleepy", "Moods", "Sleepy", 4.4f, 2.2f, mk<MoodSleepy>},
    {"mood-scared", "Moods", "Scared", 4.4f, 2.2f, mk<MoodScared>},
    {"mood-dizzy", "Moods", "Dizzy", 4.4f, 2.2f, mk<MoodDizzy>},
    {"mood-suspicious", "Moods", "Suspicious", 4.4f, 2.2f, mk<MoodSuspicious>},
    {"mood-sad", "Moods", "Sad", 4.4f, 2.2f, mk<MoodSad>},
    {"mood-grumpy", "Moods", "Grumpy", 4.4f, 2.2f, mk<MoodGrumpy>},
    {"mood-surprised", "Moods", "Surprised", 4.4f, 2.2f, mk<MoodSurprised>},
    {"mood-proud", "Moods", "Proud", 4.4f, 2.2f, mk<MoodProud>},
    {"wheee", "Handling", "Wheee! (picked up)", 4.2f, 1.4f, mk<Wheee>},
    {"phew", "Handling", "Phew (put down)", 4.2f, 1.6f, mk<Phew>},
    {"shaken", "Handling", "Dizzy (shaken)", 6.2f, 2.6f, mk<Shaken>},
    {"turtle", "Handling", "Turtle (upside down)", 5.2f, 1.8f, mk<Turtle>},
    {"pats", "Handling", "Pats", 5, 2.9f, mk<Pats>},
    {"egg-sleep", "Hatching", "The egg", 6, 2, mk<EggSleep>},
    {"egg-crack", "Hatching", "Cracking", 6.6f, 4.4f, mk<EggCrack>},
    {"egg-hatch", "Hatching", "Hatch!", 7.4f, 4.2f, mk<EggHatch>},
    {"hello-kid", "Hatching", "Are you... " PET_KID_NAME "?", 7, 2.2f, mk<HelloKid>},
    {"hold-button", "Hatching", "Hold my button!", 6.4f, 2.3f, mk<HoldButton>},
    {"say-hi", "Hatching", "Say hi (listening)", 6.4f, 2.8f, mk<SayHi>},
    {"birthday", "Hatching", "Happy Birthday!", 10, 6.2f, mk<Birthday>},
    {"floor-ask", "Hatching", "Put me on the floor!", 6, 2, mk<FloorAsk>},
    {"power-cheetah", "Animal powers", "Cheetah", 12.6f, 8.3f, mk<Cheetah>},
    {"power-honeybee", "Animal powers", "Honeybee", 15.4f, 10.9f, mk<Honeybee>},
    {"power-hummingbird", "Animal powers", "Hummingbird", 15.95f, 11.3f, mk<Hummingbird>},
    {"power-spider", "Animal powers", "Spider", 12.35f, 8.3f, mk<Spider>},
    {"power-owl", "Animal powers", "Owl", 14.7f, 10.3f, mk<Owl>},
    {"power-dolphin", "Animal powers", "Dolphin", 15.8f, 12.0f, mk<Dolphin>},
    {"power-whirligig", "Animal powers", "Whirligig", 19.3f, 12.7f, mk<Whirligig>},
    {"power-bat", "Animal powers", "Bat", 13.1f, 9.3f, mk<Bat>},
    {"power-chameleon", "Animal powers", "Chameleon", 12.4f, 9.3f, mk<Chameleon>},
    {"hot-potato", "Games", "Hot potato", 8.4f, 4.6f, mk<HotPotato>},
    {"quick-draw", "Games", "Quick draw", 7, 3.1f, mk<QuickDraw>},
    {"quick-early", "Games", "Quick draw: too soon!", 5, 2.2f, mk<QuickEarly>},
    {"hole-in-one", "Games", "Hallway golf: hole in one", 6.6f, 2.4f, mk<HoleInOne>},
    {"who-next", "Games", "Who's next?", 5, 3.6f, mk<WhoNext>},
    {"train-guess", "Training", "Guessing a power", 6.4f, 2.4f, mk<TrainGuess>},
    {"train-yes", "Training", "Treat! (got it right)", 5.6f, 2.2f, mk<TrainYes>},
    {"train-no", "Training", "Oops (got it wrong)", 5, 1.5f, mk<TrainNo>},
    {"nap", "Everyday", "Nap", 9, 4.5f, mk<Nap>},
    {"hungry", "Everyday", "Hungry, then dinner", 8, 1.8f, mk<Hungry>},
    {"charging", "Everyday", "Charging (dinner)", 6.4f, 1.6f, mk<Charging>},
    {"muted", "Everyday", "Quiet mode (muted)", 4.6f, 2.6f, mk<Muted>},
    {"missed-you", "Everyday", "I missed you!", 6.4f, 3.4f, mk<MissedYou>},
    {"peekaboo", "Everyday", "Peekaboo", 6, 1.8f, mk<Peekaboo>},
    {"crystal-ball", "Everyday", "Fortune ball", 7.4f, 4.6f, mk<CrystalBall>},
    {"dice", "Everyday", "Dice", 5.4f, 3.6f, mk<Dice>},
    {"wake-up", "Everyday", "Good morning", 6.4f, 3.2f, mk<WakeUp>},
    {"listening", "Everyday", "Listening (button held)", 4, 1.6f, mk<Listening>},
    {"boop", "Explore", "Boop", 4, 1.3f, mk<Boop>},
    {"sneeze", "Explore", "Achoo!", 5, 2.25f, mk<Sneeze>},
    {"shy-peek", "Explore", "Learning to be brave", 9, 2.6f, mk<ShyPeek>},
    {"freeze-dance", "Explore", "Freeze dance", 8, 1.7f, mk<FreezeDance>},
    {"spin-art", "Explore", "Spin art", 6, 2, mk<SpinArt>},
    {"play", "Play", "Poke it", 0, 1, mk<Play>},
};
const int SCENE_COUNT = sizeof(SCENES) / sizeof(SCENES[0]);

}  // namespace face
