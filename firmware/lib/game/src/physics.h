// The ball: rolling, rails, bumpers, moving animals, surfaces, the cup. Pure C++, no Arduino, so the PC build can
// test holes and tune the feel. Units are screen pixels and seconds.
#pragma once

#include <stdint.h>

#include "course.h"

namespace game {

struct Tuning {
  float ballR = 3.5f;
  float cupR = 5.5f;
  float maxSpeed = 380;        // a full-power putt
  float minSpeed = 28;         // the gentlest putt
  float feltDecel = 55;        // constant rolling resistance, px/s^2
  float feltDrag = 0.45f;      // plus this much per px/s of speed
  float sandDecel = 420, sandDrag = 3.0f;
  float iceDecel = 10, iceDrag = 0.08f;
  float slopeAccel = 150;
  float railBounce = 0.72f;
  float railInset = 3;         // the painted rail is 6 px wide, centred on its segment: stop at its inner face
  float animalBounce = 0.9f;
  float captureSpeed = 200;    // slower than this over the cup and it drops
  float cupPull = 260;         // the cup's lip draws a slow ball in
  float stopSpeed = 5;
};

extern Tuning kTuning;

// Something that moves and can be hit: an animal, for this frame.
struct Mover {
  float x, y, vx, vy, r;
  bool solid;  // false while hopping in the air or ducked out of sight
};

struct BallEvents {
  bool stopped = false;
  bool sunk = false;
  bool splash = false;
  bool lost = false;         // somehow outside the course: put it back, no penalty
  float wallHit = 0;         // biggest impact speed against a rail this step (for the sound)
  float bumperHit = 0;
  int animalHit = -1;        // index of the animal hit, if any
  float animalHitSpeed = 0;
  bool enteredSand = false;
};

class Ball {
 public:
  float x = 0, y = 0, vx = 0, vy = 0;
  bool moving = false;

  void place(float px, float py) {
    x = px;
    y = py;
    vx = vy = 0;
    moving = false;
    stillTime_ = 0;
  }
  // angle in degrees, 0 = right, 90 = up the screen; power 0..1.
  void putt(float angleDeg, float power);
  BallEvents step(float dt, const Hole& hole, const uint8_t* surface, const Mover* movers, int moverCount);
  float speed() const;

 private:
  void substep(float h, const Hole& hole, const uint8_t* surface, const Mover* movers, int moverCount,
               BallEvents& ev);
  float stillTime_ = 0;
  float trackX_ = 0, trackY_ = 0;
  uint8_t lastSurface_ = kFelt;
};

// Where a ball rolling from (x,y) along the unit direction (dx,dy) first touches a rail or bumper, within maxDist.
// Returns how far it gets (maxDist if it touches nothing) and, on a touch, the wall's normal at that point.
// Animals are left out: they move. Used to draw the aim line up to the wall and along the bounce.
float castBall(const Hole& hole, float x, float y, float dx, float dy, float maxDist, float& nx, float& ny);

// Speed of a putt for a power 0..1 (the curve gives short putts more control).
float puttSpeed(float power);

inline uint8_t surfaceAt(const uint8_t* surface, float x, float y) {
  const int ix = static_cast<int>(x), iy = static_cast<int>(y);
  if (ix < 0 || iy < 0 || ix >= gfx::W || iy >= gfx::H) return kOut;
  return surface[iy * gfx::W + ix];
}

}  // namespace game
