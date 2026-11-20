#include "physics.h"

#include <math.h>

namespace game {

Tuning kTuning;

namespace {

constexpr float kDeg = 0.017453293f;

// Pushes the ball out of a circle (radius rr at cx,cy, moving at mvx,mvy) and bounces it. Returns impact speed.
float collideCircle(Ball& b, float cx, float cy, float rr, float mvx, float mvy, float bounce) {
  const float dx = b.x - cx, dy = b.y - cy;
  const float minD = rr + kTuning.ballR;
  const float d2 = dx * dx + dy * dy;
  if (d2 >= minD * minD) return 0;
  float d = sqrtf(d2);
  float nx = 1, ny = 0;
  if (d > 1e-4f) {
    nx = dx / d;
    ny = dy / d;
  } else {
    d = 0;
  }
  b.x = cx + nx * minD;
  b.y = cy + ny * minD;
  const float rvx = b.vx - mvx, rvy = b.vy - mvy;
  const float vn = rvx * nx + rvy * ny;
  if (vn >= 0) {
    // Separating already; a moving animal still nudges a resting ball along.
    if (mvx != 0 || mvy != 0) {
      const float push = mvx * nx + mvy * ny;
      if (push > 0 && vn < push) {
        b.vx += nx * (push - vn);
        b.vy += ny * (push - vn);
        b.moving = true;
      }
    }
    return 0;
  }
  b.vx -= (1 + bounce) * vn * nx;
  b.vy -= (1 + bounce) * vn * ny;
  b.moving = true;
  return -vn;
}

}  // namespace

float puttSpeed(float power) {
  if (power < 0) power = 0;
  if (power > 1) power = 1;
  // Slightly curved: the bottom half of the bar covers the short putts finely.
  return kTuning.minSpeed + (kTuning.maxSpeed - kTuning.minSpeed) * powf(power, 1.4f);
}

void Ball::putt(float angleDeg, float power) {
  const float s = puttSpeed(power);
  vx = cosf(angleDeg * kDeg) * s;
  vy = -sinf(angleDeg * kDeg) * s;  // screen y points down
  moving = true;
  stillTime_ = 0;
  trackX_ = x;
  trackY_ = y;
}

float Ball::speed() const { return sqrtf(vx * vx + vy * vy); }

BallEvents Ball::step(float dt, const Hole& hole, const uint8_t* surface, const Mover* movers, int moverCount) {
  BallEvents ev;
  // Even a resting ball can be nudged by an animal walking into it.
  if (!moving) {
    for (int i = 0; i < moverCount; ++i) {
      const Mover& m = movers[i];
      if (!m.solid) continue;
      const float hit = collideCircle(*this, m.x, m.y, m.r, m.vx, m.vy, kTuning.animalBounce);
      if (moving) {
        ev.animalHit = i;
        ev.animalHitSpeed = hit;
      }
    }
    if (!moving) return ev;
  }
  const float sp = speed();
  int n = static_cast<int>(ceilf(sp * dt / 0.75f));
  if (n < 1) n = 1;
  if (n > 64) n = 64;
  const float h = dt / n;
  for (int i = 0; i < n && moving && !ev.sunk && !ev.splash && !ev.lost; ++i)
    substep(h, hole, surface, movers, moverCount, ev);

  if (moving && !ev.sunk && !ev.splash && !ev.lost) {
    // Stopped, or stuck rocking against a rail on a slope.
    stillTime_ += dt;
    const float mx = x - trackX_, my = y - trackY_;
    if (speed() < kTuning.stopSpeed && surfaceAt(surface, x, y) < kSlope0) {
      moving = false;
    } else if (stillTime_ > 0.8f) {
      if (mx * mx + my * my < 1.5f * 1.5f) moving = false;
      stillTime_ = 0;
      trackX_ = x;
      trackY_ = y;
    }
    if (!moving) {
      vx = vy = 0;
      ev.stopped = true;
    }
  }
  return ev;
}

void Ball::substep(float h, const Hole& hole, const uint8_t* surface, const Mover* movers, int moverCount,
                   BallEvents& ev) {
  const uint8_t surf = surfaceAt(surface, x, y);
  if (surf == kWater) {
    ev.splash = true;
    moving = false;
    vx = vy = 0;
    return;
  }
  if (surf == kOut) {
    ev.lost = true;
    moving = false;
    vx = vy = 0;
    return;
  }
  if (surf == kSand && lastSurface_ != kSand) ev.enteredSand = true;
  lastSurface_ = surf;

  // Surface forces.
  float decel = kTuning.feltDecel, drag = kTuning.feltDrag;
  if (surf == kSand) {
    decel = kTuning.sandDecel;
    drag = kTuning.sandDrag;
  } else if (surf == kIce) {
    decel = kTuning.iceDecel;
    drag = kTuning.iceDrag;
  }
  if (surf >= kSlope0 && surf < kSlope0 + 8) {
    const float a = (surf - kSlope0) * 45 * kDeg;
    vx += cosf(a) * kTuning.slopeAccel * h;
    vy -= sinf(a) * kTuning.slopeAccel * h;
  }
  float sp = speed();
  if (sp > 0) {
    const float drop = (decel + drag * sp) * h;
    const float k = sp > drop ? (sp - drop) / sp : 0;
    vx *= k;
    vy *= k;
    sp *= k;
  }
  if (sp > kTuning.maxSpeed * 1.3f) {
    vx *= kTuning.maxSpeed * 1.3f / sp;
    vy *= kTuning.maxSpeed * 1.3f / sp;
  }

  // The cup: a slow ball over it drops; the lip curls a nearby slow ball in.
  {
    const float dx = hole.cupX - x, dy = hole.cupY - y;
    const float d = sqrtf(dx * dx + dy * dy);
    if (d < kTuning.cupR + kTuning.ballR && d > 0.01f) {
      const float pull = kTuning.cupPull * (1 - d / (kTuning.cupR + kTuning.ballR));
      vx += dx / d * pull * h;
      vy += dy / d * pull * h;
    }
    if (d < kTuning.cupR - 1.0f && speed() < kTuning.captureSpeed) {
      ev.sunk = true;
      moving = false;
      x = hole.cupX;
      y = hole.cupY;
      vx = vy = 0;
      return;
    }
  }

  x += vx * h;
  y += vy * h;

  // Rails.
  const float railR = kTuning.ballR + kTuning.railInset;
  for (uint16_t i = 0; i < hole.segCount; ++i) {
    const Seg& s = hole.segs[i];
    const float ax = s.x1, ay = s.y1, bx = s.x2 - s.x1, by = s.y2 - s.y1;
    const float len2 = bx * bx + by * by;
    float t = len2 > 0 ? ((x - ax) * bx + (y - ay) * by) / len2 : 0;
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    const float qx = ax + bx * t, qy = ay + by * t;
    float dx = x - qx, dy = y - qy;
    const float d2 = dx * dx + dy * dy;
    if (d2 >= railR * railR) continue;
    float d = sqrtf(d2);
    float nx, ny;
    if (d > 1e-4f) {
      nx = dx / d;
      ny = dy / d;
    } else {  // exactly on the line: push along the segment's normal
      const float l = sqrtf(len2);
      nx = -by / l;
      ny = bx / l;
      d = 0;
    }
    x = qx + nx * railR;
    y = qy + ny * railR;
    const float vn = vx * nx + vy * ny;
    if (vn < 0) {
      vx -= (1 + kTuning.railBounce) * vn * nx;
      vy -= (1 + kTuning.railBounce) * vn * ny;
      if (-vn > ev.wallHit) ev.wallHit = -vn;
    }
  }

  // Bumpers.
  for (uint8_t i = 0; i < hole.bumperCount; ++i) {
    const Bumper& bu = hole.bumpers[i];
    const float hit = collideCircle(*this, bu.x, bu.y, bu.r, 0, 0, bu.bounce / 100.0f);
    if (hit > ev.bumperHit) ev.bumperHit = hit;
  }

  // Animals.
  for (int i = 0; i < moverCount; ++i) {
    const Mover& m = movers[i];
    if (!m.solid) continue;
    const float hit = collideCircle(*this, m.x, m.y, m.r, m.vx, m.vy, kTuning.animalBounce);
    if (hit > 0 && hit >= ev.animalHitSpeed) {
      ev.animalHit = i;
      ev.animalHitSpeed = hit;
    }
  }
}

float castBall(const Hole& hole, float x, float y, float dx, float dy, float maxDist, float& nx, float& ny) {
  // March in half-pixel steps: a hole has a dozen rails, so this is cheap, and it handles corners without fuss.
  // A touch counts only while the ball is moving into the wall, so a ball resting against a rail can aim away.
  const float railR = kTuning.ballR + kTuning.railInset;
  for (float t = 0.5f; t <= maxDist; t += 0.5f) {
    const float px = x + dx * t, py = y + dy * t;
    for (uint16_t i = 0; i < hole.segCount; ++i) {
      const Seg& s = hole.segs[i];
      const float ax = s.x1, ay = s.y1, bx = s.x2 - s.x1, by = s.y2 - s.y1;
      const float len2 = bx * bx + by * by;
      float u = len2 > 0 ? ((px - ax) * bx + (py - ay) * by) / len2 : 0;
      u = u < 0 ? 0 : (u > 1 ? 1 : u);
      const float ex = px - (ax + bx * u), ey = py - (ay + by * u);
      const float d2 = ex * ex + ey * ey;
      if (d2 >= railR * railR || d2 < 1e-6f) continue;
      const float d = sqrtf(d2);
      if (ex * dx + ey * dy >= 0) continue;  // moving away from this wall
      nx = ex / d;
      ny = ey / d;
      return t;
    }
    for (uint8_t i = 0; i < hole.bumperCount; ++i) {
      const Bumper& b = hole.bumpers[i];
      const float ex = px - b.x, ey = py - b.y, r = b.r + kTuning.ballR;
      const float d2 = ex * ex + ey * ey;
      if (d2 >= r * r || d2 < 1e-6f) continue;
      if (ex * dx + ey * dy >= 0) continue;
      const float d = sqrtf(d2);
      nx = ex / d;
      ny = ey / d;
      return t;
    }
  }
  return maxDist;
}

}  // namespace game
