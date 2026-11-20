// Runs one scene: scheduled cues, captions and the draw order
// background -> under() -> face -> mid() -> particles -> over() -> caption -> post(). Port of art/face/player.js.
#pragma once

#include <functional>
#include <vector>

#include "face_engine.h"

namespace face {

struct Sc;
class Player;

// false (the default): scenes play their scripted demo, matching the review page frame for frame.
// true (the robot firmware): scenes wait for the real input instead of faking it. The egg cracks only on tap(),
// hello-kid loves you only on button(), pats only on tap(), googly takes face.grav from the host's accelerometer,
// and turtle stays stuck until the "unflip" event. The gift build's scenes wait too: hold-button for "hold" and "done" (and
// "release"), floor-ask for "putdown", charging for "full", muted for "unmute". The animal powers show their fact
// in the hand, then ask "Put me down!" until "putdown" (or "floor", which may come early); the big button skips a
// fact screen.
extern bool live;
// The Stick's battery, 0..1, for the charging face's battery icon (live only). Below 0 the scene animates its own.
extern float batteryLevel;
// Set by the host to hear moments it may act on: "trick" as an animal power's trick starts (start the motors and the
// sound with it). Null: nobody listens.
extern void (*onCue)(const char* what);

// One scene of the catalogue. Per-play state lives in the subclass's members (a fresh object for every play).
struct Scene {
  virtual ~Scene() {}
  virtual void setup(Sc&) {}
  virtual void frame(Sc&, float) {}
  virtual void background(Sc& S, float t);  // default: the look's own background
  virtual void under(Sc&, float) {}
  virtual void mid(Sc&, float) {}
  virtual void over(Sc&, float) {}
  virtual void post(Sc&, float) {}
  virtual void tap(Sc&) {}
  virtual void button(Sc&) {}
  virtual void event(Sc&, const char*) {}
  // The scene's own end, when it depends on the robot (the animal powers wait to be put down): below 0, the
  // catalogue's dur.
  virtual double length(Sc&) { return -1; }
};

struct SceneInfo {
  const char* id;
  const char* group;
  const char* title;
  float dur;     // seconds; 0 = runs until something else is played
  float poster;  // a representative moment
  Scene* (*make)();
};
extern const SceneInfo SCENES[];
extern const int SCENE_COUNT;
int findScene(const char* id);  // -1 if unknown

struct SayOpt {
  bool shout = false, glance = true, keepLook = false, bounce = false, wobble = false, noPop = false;
  float y = -1e9f, size = 0, width = 0, outline = -1;
  bool hasColor = false, hasOc = false;
  C3 color{}, oc{};
  SayOpt& sh() { shout = true; return *this; }
  SayOpt& ng() { glance = false; return *this; }
  SayOpt& at(float v) { y = v; return *this; }
  SayOpt& sz(float v) { size = v; return *this; }
  SayOpt& bnc() { bounce = true; return *this; }
  SayOpt& wob() { wobble = true; return *this; }
  SayOpt& col(C3 c) { hasColor = true; color = c; return *this; }
  SayOpt& ocol(C3 c) { hasOc = true; oc = c; return *this; }
};
inline SayOpt Shout() { return SayOpt().sh().ng(); }  // the common case: a shout that keeps eye contact
inline SayOpt Line() { return SayOpt().ng(); }         // a sentence that keeps eye contact

struct Caption {
  bool on = false;
  char str[72] = {0};
  double t0 = 0, out = -1;
  SayOpt o;
};

// The scene context ("S" in the scripts).
struct Sc {
  Face face;
  FX fx;
  Screen* scr = nullptr;
  Layout L = PORTRAIT;
  const Skin* skin = &MARBLE;
  double t = 0;
  double dt = 0;
  Caption cap;
  Rng r{1234};
  Player* player = nullptr;

  void at(double when, std::function<void(Sc&)> fn);
  void after(double d, std::function<void(Sc&)> fn) { at(t + d, std::move(fn)); }
  void say(const char* s, const SayOpt& o = SayOpt());
  void unsay() {
    if (cap.on && cap.out < 0) cap.out = t;
  }

  struct Cue {
    double t;
    std::function<void(Sc&)> fn;
  };
  std::vector<Cue> timers;
};

// True on the frame where t*rate passes a whole number. It runs on the double clock: at a steady 30 fps the crossings
// land exactly on whole numbers, where float rounding drops or repeats one (and that shifts every random number after).
inline bool every(const Sc& S, double rate) { return floor(S.t * rate) != floor((S.t - S.dt) * rate); }

class Player {
 public:
  Screen scr;
  Sc S;
  int index = -1;

  // frame: a 135x240 swapped-565 buffer that stays valid while the player runs (or call scr.setFrame with halves).
  void begin(uint16_t* frame);
  void play(int sceneIndex);
  // A scene the host owns (it is never deleted here). reset() calls its setup() again, so setup must reset its state.
  void play(Scene* scene);
  // Seeds every random source (the scene's S.r, the face's blinks and glances, the particles). Call after play();
  // the robot passes esp_random() so no two plays are the same.
  void reseed(uint32_t seed);
  void reset();  // restart the current scene
  void step(double dt);
  void draw();
  void renderAt(double t);  // simulate to t at 30 fps, then draw once
  void tap();
  void button();
  void event(const char* name);
  const SceneInfo* info() const { return index >= 0 ? &SCENES[index] : nullptr; }
  // When the scene ends, on S.t: the scene's own length if it has one (an animal power: far off until it's put
  // down), else the catalogue's dur; 0 = runs until something else is played. Hosts use this, not info()->dur.
  double length();

 private:
  Scene* scene_ = nullptr;
  Scene* hostScene_ = nullptr;  // play(Scene*): not ours to delete
  void drawCaption();
};

}  // namespace face
