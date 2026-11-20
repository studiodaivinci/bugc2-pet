#include "moves.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <math.h>
#include <string.h>

#include "base.h"
#include "dances.h"
#include "motion.h"
#include "sound.h"

#ifndef PET_MOTORS
#define PET_MOTORS 0
#endif

namespace moves {
namespace {

// --- Limits --------------------------------------------------------------------------------------------------------
constexpr float kFloorSettled = 1.0f;  // the floor check: stiller than a still hand this long (motion.cpp)...
constexpr float kFloorZ = 0.9f;        // ...lying this flat, screen up, and not held
constexpr float kBaseLowV = 3.45f;     // the base's motor cell below this: no dance
constexpr uint32_t kFeedMs = 500;      // the loop silent this long: stop
constexpr float kTippedZ = 0.6f;       // smoothed az below this while moving: tipped, or lifted at a slant
constexpr uint32_t kTurnGraceMs = 400;  // a spin or waggle should be turning by now...
constexpr float kSpinDps = 150;         // ...faster than this (a waggle: kWaggleDps), and if it stops for...
constexpr float kWaggleDps = 80;
constexpr uint32_t kTurnLostMs = 250;   // ...this long, it's lifted or on carpet: stop
// Motor time per minute: rapid reversing hammers the small gearboxes. It was 10 s, a cautious guess; Trick Lab's
// longer moves met it often (a six-move trick and its replay), so it became 20 s on 2026-11-11.
constexpr float kBudgetMs = 20000;
constexpr uint32_t kResendMs = 200;     // re-send the running speeds this often (a write the base missed)
constexpr uint32_t kTickMs = 5;
// Travelling (DRIVE, STRAFE): the bench's heading hold (firmware/bench, steerMove; HARDWARE.md, "steertest"). Forward
// and back both spin the robot, so a turn is added to every motor and resized 50 times a second from the heading
// error, the spin rate and a slowly learned trim. A positive turn pushes clockwise; heading is counter-clockwise.
constexpr float kSteerKp = 0.2f;    // turn per degree off the heading
constexpr float kSteerKd = 0.04f;   // turn per deg/s of spin it didn't ask for
constexpr float kSteerKi = 0.2f;    // turn per degree-second: the trim
constexpr float kSteerFf = 0.03f;   // turn per deg/s of turning it did ask for (arcs); a first guess, untuned
constexpr float kSteerCap = 40;     // the most turn
constexpr uint32_t kSteerMs = 20;
constexpr uint32_t kRampMs = 150;   // eases in from a standstill and out to one: abrupt stops twisted it 15-20°
constexpr uint32_t kLightMs = 20;
constexpr float kGlow = 0.8f;           // the lights' top brightness
constexpr float kCoastDps = 25;         // Trick Lab: the next move waits until it turns slower than this...
constexpr uint32_t kLabGapMs = 200;     // ...and this long after the move before
constexpr uint32_t kSelfMs = 900;       // its own spin isn't a pick-up, until this long after the motors stop
constexpr gpio_num_t kBigPin = GPIO_NUM_37, kSidePin = GPIO_NUM_39;  // pressed = low

// --- The dances are in dances.cpp ---------------------------------------------------------------------------------
// Lights fixed to the room, not the robot: they change with the heading every tick, so while it spins each light
// draws the same colours in the same places on every turn.
bool roomLocked(Light l) { return l == L_HALO || l == L_COMETS || l == L_GALAXY || l == L_SLOWHALO; }

// Just before the first move, after a dance's lights-only start: still lying flat, the right way up, and not picked
// up. Not the whole floor check (start() did that): the dance's own sounds buzz the robot, and on 2026-11-11 the
// hummingbird's hum kept it from counting as settled, so it stopped "not on the floor" with nobody near it. Lifted
// or tipped once it moves, the guard stops it as before.
bool stillDown() { return !motion::held() && !motion::flipped() && motion::latest().az > kFloorZ; }

// --- Shared with the loop (under gMux) -----------------------------------------------------------------------------
portMUX_TYPE gMux = portMUX_INITIALIZER_UNLOCKED;
int gStartReq = -1;
bool gStartDry = false, gStartQuick = false;
volatile uint32_t gEndMs = 0;  // when the last dance ended
float gStartV = 0;
bool gStopReq = false;
char gStopWhy[24] = "";
char gWhy[40] = "";
Report gReport{};
bool gReportReady = false;
volatile bool gBusy = false;
volatile uint32_t gFeedMs = 0;
float gBudget = kBudgetMs;  // the guard's; start() only reads it

void setWhy(const char* w) {
  portENTER_CRITICAL(&gMux);
  strlcpy(gWhy, w, sizeof(gWhy));
  portEXIT_CRITICAL(&gMux);
}

bool stopAsked(char* why, size_t n) {
  portENTER_CRITICAL(&gMux);
  const bool asked = gStopReq;
  if (asked) {
    strlcpy(why, gStopWhy, n);
    gStopReq = false;
  }
  portEXIT_CRITICAL(&gMux);
  return asked;
}

void refill(uint32_t dtMs, bool motorOn) {
  gBudget += kBudgetMs * dtMs / 60000.0f - (motorOn ? dtMs : 0);
  if (gBudget > kBudgetMs) gBudget = kBudgetMs;
}

// --- The lights ----------------------------------------------------------------------------------------------------
uint8_t sc(float v) { return v <= 0 ? 0 : v >= 255 ? 255 : static_cast<uint8_t>(v * kGlow); }
void led(int i, float r, float g, float b) { base::setLed(i, sc(r), sc(g), sc(b)); }
void both(float r, float g, float b) {
  led(0, r, g, b);
  led(1, r, g, b);
}

// Colour a, blended toward b by k.
void mix(int i, const float a[3], const float b[3], float k, float v) {
  led(i, (a[0] + (b[0] - a[0]) * k) * v, (a[1] + (b[1] - a[1]) * k) * v, (a[2] + (b[2] - a[2]) * k) * v);
}
// 0..1, the same for the same n: flicker and twinkles that don't need a random generator.
float noise(uint32_t n) {
  n = (n ^ 61) ^ (n >> 16);
  n *= 9;
  n ^= n >> 4;
  n *= 0x27d4eb2d;
  n ^= n >> 15;
  return (n & 0xffff) / 65535.0f;
}

// The scenes' colours (face_scenes.cpp; each animal has its own, art/face/DESIGN.md "Seven colours"), so the base
// glows like the screen. Tuned for the LEDs, whose green is strong: a yellow needs less green than on the screen.
const float kGreen[3] = {54, 209, 124}, kPink[3] = {255, 122, 184}, kWhite[3] = {255, 250, 225},
            kRed[3] = {255, 30, 12}, kGold[3] = {255, 190, 0}, kYellow[3] = {255, 215, 0},
            kPurple[3] = {182, 90, 255}, kViolet[3] = {80, 20, 170}, kNight[3] = {40, 60, 255},
            kMoon[3] = {255, 235, 170}, kSky[3] = {127, 214, 255}, kDeep[3] = {10, 63, 120},
            kFoam[3] = {217, 246, 255};

// Every dance's colour round a circle, h in turns, in the whirligig's screen rings' order: cheetah, honeybee,
// whirligig, hummingbird, dolphin, owl, spider.
constexpr int kWheelN = 7;
const float kWheel[kWheelN][3] = {{255, 30, 12}, {255, 215, 0}, {150, 230, 30}, {40, 210, 100},
                                  {110, 200, 255}, {60, 80, 255}, {182, 90, 255}};
void wheel(int i, float h, float v) {
  const float p = (h - floorf(h)) * kWheelN, f = p - floorf(p);
  const int a = static_cast<int>(p) % kWheelN, b = (a + 1) % kWheelN;
  mix(i, kWheel[a], kWheel[b], f, v);
}
void glow(const float c[3], float v) { both(c[0] * v, c[1] * v, c[2] * v); }
// How close (0..1) the angle a (degrees) is to the nearest of n evenly spaced directions, within `width` degrees.
float near(float a, int n, float width) {
  const float step = 360.0f / n, d = fabsf(fmodf(fmodf(a, step) + step, step) - step / 2);
  return fmaxf(0.0f, 1 - (step / 2 - d) / width);
}

// el: ms into the step; dur: how long the step is meant to run; yaw: the heading in degrees (counter-clockwise).
// Led 0 is on the left, so it faces yaw + 90° from the middle, and led 1 yaw - 90°.
void lights(const Step& st, uint32_t el, uint32_t dur, float yaw) {
  const float u = dur ? fminf(1.0f, el / static_cast<float>(dur)) : 1.0f, t = el / 1000.0f;
  const float az[2] = {yaw + 90, yaw - 90};
  switch (st.light) {
    case L_REVUP: {  // an engine revving: red blinks, faster and brighter, then a white flash as it lets go
      if (el + 70 >= st.ms) {
        both(255, 250, 225);
        break;
      }
      const float cycles = 3 * u + 9 * u * u;
      const float v = fmodf(cycles, 1) < 0.5f ? 0.3f + 0.7f * u : 0.03f;
      glow(kRed, v);
      break;
    }
    case L_STREAK: {  // light trails: gold and racing red swap sides 15 times a second, so the spin draws a dashed ring,
                      // with a white spark on every fifth swap
      const int k = el / 33;
      const bool goldLeft = k % 2;
      const bool spark = k % 5 == 0;
      for (int i = 0; i < 2; ++i) {
        const bool gold = (i == 0) == goldLeft;
        if (spark && gold) led(i, 255, 250, 225);
        else if (gold) led(i, kGold[0], kGold[1], kGold[2]);
        else led(i, kRed[0], kRed[1], kRed[2]);
      }
      break;
    }
    case L_COAST: {  // cooling from gold to red
      for (int i = 0; i < 2; ++i) mix(i, kGold, kRed, u, 1 - 0.5f * u);
      break;
    }
    case L_PANT: {  // red breaths in time with the face's panting (sin(t * 22) in Cheetah::pframe), tiring out
      const float v = (0.12f + 0.88f * (0.5f + 0.5f * sinf(t * 22))) * (1 - 0.8f * u);
      glow(kRed, v);
      break;
    }
    case L_BZZ: {  // bzz-bzz: two quick yellow blinks
      const bool on = el < 90 || (el >= 180 && el < 270);
      if (on) glow(kYellow, 1);
      else glow(kYellow, 0.07f);
      break;
    }
    case L_WAGGLE: {  // a bee's stripes: the yellow light hops sides with every waggle
      const int k = st.flipMs ? el / st.flipMs : 0;
      led(k % 2, kYellow[0], kYellow[1], kYellow[2]);
      led(1 - k % 2, 14, 12, 0);
      break;
    }
    case L_SWING: glow(kYellow, 0.55f); break;  // a softer yellow while it loops back
    case L_BLOOM: {  // a flower opens: pink, then fades
      const float v = fminf(1.0f, el / 200.0f) * (el > 500 ? fmaxf(0.0f, 1 - (el - 500) / 700.0f) : 1.0f);
      led(0, 255 * v, 40 * v, 110 * v);
      led(1, 255 * v, 90 * v, 170 * v);
      break;
    }
    case L_HUM: {  // wings: green and pink drifting past each other, fluttering 25 times a second
      const float flutter = (el / kLightMs) % 2 ? 1.0f : 0.5f;
      for (int i = 0; i < 2; ++i) mix(i, kGreen, kPink, 0.5f + 0.5f * sinf(t * 4 + i * 3.1416f), flutter);
      break;
    }
    case L_DART: both(255 - 201 * u, 250 - 41 * u, 225 - 101 * u); break;  // a white flash, back to green
    case L_SIP: {  // nectar: pink rises, sips (slow breaths), and fades out at the end
      const float in = fminf(1.0f, el / 300.0f), out = dur > 600 && el > dur - 600 ? (dur - el) / 600.0f : 1.0f;
      const float v = in * fmaxf(0.0f, out) * (0.55f + 0.45f * sinf(t * 5));
      both(kPink[0] * v, kPink[1] * v, kPink[2] * v);
      break;
    }
    case L_NECTAR: {  // sipping at a flower: pink, fluttering with the wings, swelling with each sip
      const float flutter = (el / kLightMs) % 2 ? 1.0f : 0.6f;
      for (int i = 0; i < 2; ++i) mix(i, kGreen, kPink, 0.85f, flutter * (0.5f + 0.5f * fabsf(sinf(t * 7))));
      break;
    }
    case L_DIVE: {  // the dive: green and pink swapping sides 15 times a second, a dashed ring as it spins
      const int k = el / 33;
      for (int i = 0; i < 2; ++i) mix(i, kGreen, kPink, (k + i) % 2 ? 1.0f : 0.0f, 1);
      break;
    }
    case L_CREEP: {  // something's coming: a dim purple, growing
      const float v = 0.08f + 0.3f * u;
      both(kPurple[0] * v, kPurple[1] * v, kPurple[2] * v);
      break;
    }
    case L_TREMBLE: {  // a nervous flicker, each side on its own
      for (int i = 0; i < 2; ++i) mix(i, kViolet, kPurple, noise(el / 40 * 2 + i), 0.35f + 0.6f * noise(el / 40 * 2 + i + 101));
      break;
    }
    case L_EEK: {  // eek!: white flashes in time with the jumps
      const int k = st.flipMs ? el / st.flipMs : 0;
      if (k % 2) both(kWhite[0], kWhite[1], kWhite[2]);
      else both(kPurple[0], kPurple[1], kPurple[2]);
      break;
    }
    case L_PHEW: {  // phew: a calm violet that fades away
      const float v = 0.7f * (1 - u);
      both(kPurple[0] * v, kPurple[1] * v, kPurple[2] * v);
      break;
    }
    case L_STARE: {  // the night sky, barely breathing
      glow(kNight, 0.6f + 0.15f * sinf(t * 3));
      break;
    }
    case L_BLINK: both(0, 0, 0); break;
    case L_LOOK: {  // looking one way: moonlight on that side, the night on the other (left is led 0; +1 turns left)
      const int side = st.dir >= 0 ? 0 : 1;
      led(side, kMoon[0], kMoon[1], kMoon[2]);
      led(1 - side, kNight[0] * 0.25f, kNight[1] * 0.25f, kNight[2] * 0.25f);
      break;
    }
    case L_HOO: {  // hoo-hoo: two swells of moonlight over the night, then the night slowly fades
      if (el < 800) {
        for (int i = 0; i < 2; ++i) mix(i, kNight, kMoon, fabsf(sinf(el / 400.0f * 3.1416f)), 1);
      } else {
        glow(kNight, 1 - fminf(1.0f, (el - 800) / static_cast<float>(dur > 800 ? dur - 800 : 1)));
      }
      break;
    }
    case L_SWIM: {  // waves: deep and sky blue rolling from one side to the other
      for (int i = 0; i < 2; ++i) mix(i, kDeep, kSky, 0.5f + 0.5f * sinf(t * 6 - i * 1.6f), 1);
      break;
    }
    case L_SPRAY: {  // the leap: foam and sky swapping sides 15 times a second
      const int k = el / 33;
      for (int i = 0; i < 2; ++i) {
        if ((k + i) % 2) led(i, kFoam[0], kFoam[1], kFoam[2]);
        else led(i, kSky[0] * 0.7f, kSky[1] * 0.7f, kSky[2] * 0.7f);
      }
      break;
    }
    case L_SPLASH: {  // splash: white, settling back into the waves
      if (el < 300) {
        for (int i = 0; i < 2; ++i) mix(i, kWhite, kSky, el / 300.0f, 1);
      } else {
        for (int i = 0; i < 2; ++i) mix(i, kDeep, kSky, 0.5f + 0.5f * sinf(t * 6 - i * 1.6f), 1);
      }
      break;
    }
    case L_BUBBLES: {  // bubbles rising: foam twinkles over deep blue, fading out
      const float fade = 1 - u;
      for (int i = 0; i < 2; ++i) {
        const bool pop = noise(el / 60 * 2 + i) > 0.8f;
        if (pop) mix(i, kSky, kFoam, 1, fade);
        else mix(i, kDeep, kSky, 0.3f, 0.6f * fade);
      }
      break;
    }
    case L_WINDUP: {  // winding up: white, brighter and flickering faster until it lets go
      const float rate = 4 + 26 * u * u, v = (0.15f + 0.85f * u * u) * (0.6f + 0.4f * (fmodf(t * rate, 1) < 0.5f));
      both(kWhite[0] * v, kWhite[1] * v, kWhite[2] * v);
      break;
    }
    case L_HALO:  // a rainbow ring that stands still in the room: each direction keeps its colour
    case L_SLOWHALO:
      for (int i = 0; i < 2; ++i) wheel(i, az[i] / 360, st.light == L_SLOWHALO ? 1 - 0.7f * u : 1);
      break;
    case L_COMETS: {  // three white-gold comets frozen in the air, over a dim violet ring
      for (int i = 0; i < 2; ++i) mix(i, kViolet, kWhite, near(az[i], 3, 28), 0.25f + 0.75f * near(az[i], 3, 28));
      break;
    }
    case L_GALAXY: {  // three arms in every colour, turning slowly backwards round the room
      for (int i = 0; i < 2; ++i) {
        const float a = az[i] + t * 90;
        const float arm = near(a, 3, 50);
        wheel(i, a / 360 + t * 0.2f, 0.12f + 0.88f * arm * arm);
      }
      break;
    }
    case L_DIZZY: {  // dizzy: colours wandering and wobbling, fading out
      for (int i = 0; i < 2; ++i) wheel(i, t * 0.6f + i * 0.5f, (0.35f + 0.65f * (0.5f + 0.5f * sinf(t * 9 + i * 3))) * (1 - u));
      break;
    }
  }
}

// --- The guard -----------------------------------------------------------------------------------------------------
bool stopDelivered() {
  for (int i = 0; i < 20; ++i) {
    if (base::stopMotors()) return true;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  return false;
}

void play(Dance d, bool dry, float baseV, bool quick) {
  const Program& p = program(d);
  Report rep{};
  rep.dance = d;
  rep.dry = dry;
  rep.baseV = baseV;
  char stopWhy[24] = "";
  float lpz = motion::latest().az;
  uint32_t last = millis();
  const uint32_t danceT0 = last;
  bool moved = false;
  float ref = 0, trim = 0;  // travelling: the heading that `head` counts from, and the steering's learned trim
  bool refOk = false;

  for (int i = 0; i < p.n && !stopWhy[0]; ++i) {
    const Step& st = p.steps[i];
    const bool motor = st.kind != LIGHTS && !dry;
    const bool travel = st.kind == DRIVE || st.kind == STRAFE;
    const bool rampIn = travel && (i == 0 || p.steps[i - 1].kind == LIGHTS);
    const bool rampOut = travel && (i + 1 >= p.n || p.steps[i + 1].kind == LIGHTS);
    // The trim it learned carries on only through steps that travel the same way (the bench learned it per move).
    if (travel && !(i > 0 && p.steps[i - 1].kind == st.kind && p.steps[i - 1].dir == st.dir)) trim = 0;
    if (motor && !moved && !stillDown()) {  // once more, just before the first move
      strlcpy(stopWhy, "not on the floor", sizeof(stopWhy));
      break;
    }
    const motion::Sample s0 = motion::latest();
    const uint32_t t0 = millis();
    // A step with `until` ends at that point on the dance's clock (at once, if a turn ran past it).
    const uint32_t clock = t0 - danceT0;
    const uint32_t dur = st.kind == LIGHTS && st.until ? (st.until > clock ? st.until - clock : 0) : st.ms;
    uint32_t lastTurn = t0, lastSend = 0, lastLight = 0, lastSteer = t0;
    int sign = 0, wiggle = 0;
    float peak = 0, turn = 0, offMax = 0, spin = 0;
    if (st.kind == SPIN || st.kind == TURN) refOk = false;  // the gyro clips in a fast spin: the heading is lost
    if (travel && !refOk) {
      ref = s0.yaw;
      refOk = true;
    }
    StepLog& lg = rep.steps[rep.n < 20 ? rep.n++ : 19];
    strlcpy(lg.name, st.name, sizeof(lg.name));
    if (st.sfx != sound::NONE) sound::play(st.sfx);  // muted: silent, in time all the same

    for (;;) {
      const uint32_t now = millis(), el = now - t0;
      const motion::Sample s = motion::latest();
      peak = fmaxf(peak, fabsf(s.gz));
      lpz += (s.az - lpz) * fminf(1.0f, (now - last) / 100.0f);
      refill(now - last, motor && sign != 0);
      last = now;

      if (stopAsked(stopWhy, sizeof(stopWhy))) break;
      if (motor && quick) motion::selfMoving(kSelfMs);
      if (motor) {
        if (now - gFeedMs > kFeedMs) strlcpy(stopWhy, "loop stalled", sizeof(stopWhy));
        else if (!gpio_get_level(kBigPin) || !gpio_get_level(kSidePin)) strlcpy(stopWhy, "button", sizeof(stopWhy));
        else if (lpz < kTippedZ) strlcpy(stopWhy, "tipped", sizeof(stopWhy));
        // A travelling step isn't meant to turn, so the turn check can't tell it's been lifted: the tip check and
        // the buttons still can, and it runs a second at most.
        if (travel || fabsf(s.gz) > (st.kind == WAGGLE ? kWaggleDps : kSpinDps)) lastTurn = now;
        if (!stopWhy[0] && el > kTurnGraceMs && now - lastTurn > kTurnLostMs)
          strlcpy(stopWhy, "no turn (lifted, or carpet)", sizeof(stopWhy));
        if (stopWhy[0]) break;
      }
      if (el >= dur) break;
      // Only turning its own way counts: 2026-11-10's honeybee turns "finished" in 110-190 ms, carried 150° the
      // wrong way by the waggle before them.
      if (st.kind == TURN && st.angle && st.dir * (s.yaw - s0.yaw) >= st.angle) break;

      if (motor && travel) {
        const int wig = st.flipMs && (el / st.flipMs) % 2 ? -1 : 1;
        const bool tick = now - lastSteer >= kSteerMs || sign == 0;
        if (tick) {
          const float dt = (now - lastSteer) / 1000.0f;
          lastSteer = now;
          const float err = remainderf(s.yaw - (ref + st.head + st.rate * (el / 1000.0f)), 360.0f);
          offMax = fmaxf(offMax, fabsf(err));
          trim = fmaxf(-kSteerCap, fminf(kSteerCap, trim + kSteerKi * err * dt));
          // A wiggle is spin it did ask for: damp the spin averaged over a whole swing each way, not each swing.
          spin = st.flipMs ? spin + (s.gz - spin) * fminf(1.0f, dt / (st.flipMs * 0.002f)) : s.gz;
          turn = fmaxf(-kSteerCap, fminf(kSteerCap, kSteerKp * err + kSteerKd * (spin - st.rate) + trim - kSteerFf * st.rate));
        }
        if (tick || wig != wiggle) {
          wiggle = wig;
          const float in = rampIn ? el / static_cast<float>(kRampMs) : 1, out = rampOut ? (dur - el) / static_cast<float>(kRampMs) : 1;
          const float x = fmaxf(0.0f, fminf(1.0f, fminf(in, out))), gain = x * x * (3 - 2 * x);
          const float d = st.dir * st.speed, t = turn + wig * (st.flipMs ? st.angle : 0);
          // forward (d, -d, d, -d); left (-d, -d, d, d): HARDWARE.md, the library's patterns; the turn on every motor
          const float m[4] = {st.kind == DRIVE ? d : -d, -d, d, st.kind == DRIVE ? -d : d};
          int v[4];
          for (int k = 0; k < 4; ++k) v[k] = static_cast<int>(lroundf(fmaxf(-60.0f, fminf(60.0f, (m[k] + t) * gain))));
          if (!base::setMotors(v[0], v[1], v[2], v[3])) {
            strlcpy(stopWhy, "base lost", sizeof(stopWhy));
            break;
          }
          sign = 1;
          moved = true;
        }
      } else if (motor) {
        int want = st.dir;
        if (st.kind == WAGGLE && st.flipMs) want = (el / st.flipMs) % 2 ? -st.dir : st.dir;
        if (want != sign || now - lastSend >= kResendMs) {
          const int v = want * st.speed;
          if (!base::setMotors(v, v, v, v)) {
            strlcpy(stopWhy, "base lost", sizeof(stopWhy));
            break;
          }
          sign = want;
          lastSend = now;
          moved = true;
        }
      }
      if (now - lastLight >= (roomLocked(st.light) ? kTickMs : kLightMs)) {
        lights(st, el, dur, s.yaw);
        lastLight = now;
      }
      vTaskDelay(pdMS_TO_TICKS(kTickMs));
    }
    // Stop, unless the next step moves too and takes over at once (the whirligig's speeding up runs straight on).
    const bool handOver = !stopWhy[0] && i + 1 < p.n && p.steps[i + 1].kind != LIGHTS;
    if (motor && sign != 0 && !handOver) base::stopMotors();
    lg.ms = static_cast<uint16_t>(millis() - t0);
    lg.peakDps = static_cast<int16_t>(peak);
    lg.yaw = static_cast<int16_t>(constrain(motion::latest().yaw - s0.yaw, -32000.0f, 32000.0f));
    lg.off = travel && motor ? static_cast<int16_t>(offMax) : -1;
  }

  if (!dry && !stopDelivered()) strlcpy(stopWhy, "stop not delivered", sizeof(stopWhy));  // the loop keeps trying
  if (stopWhy[0]) sound::stop();  // stopped early: the dance's sound stops too
  base::ledsOff();
  strlcpy(rep.stop, stopWhy, sizeof(rep.stop));
  portENTER_CRITICAL(&gMux);
  gReport = rep;
  gReportReady = true;
  strlcpy(gWhy, stopWhy, sizeof(gWhy));
  gEndMs = millis();
  gBusy = false;
  portEXIT_CRITICAL(&gMux);
}

void task(void*) {
  uint32_t last = millis();
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(kLightMs));
    const uint32_t now = millis();
    refill(now - last, false);
    last = now;
    int req = -1;
    bool dry = false, quick = false;
    float v = 0;
    portENTER_CRITICAL(&gMux);
    if (gStartReq >= 0) {
      req = gStartReq;
      dry = gStartDry;
      quick = gStartQuick;
      v = gStartV;
      gStartReq = -1;
    }
    portEXIT_CRITICAL(&gMux);
    if (req >= 0) {
      play(static_cast<Dance>(req), dry, v, quick);
      last = millis();
    }
  }
}

}  // namespace

void begin() {
  gFeedMs = millis();
  // Core 1 with the loop, above it: it sleeps between 5 ms ticks, and a motor write takes about half a millisecond.
  xTaskCreatePinnedToCore(task, "moves", 4096, nullptr, 3, nullptr, 1);
}

bool floorReady() {
  return !motion::held() && !motion::flipped() && motion::settledSeconds() >= kFloorSettled &&
         motion::latest().az > kFloorZ;
}

bool labReady() {
  const motion::Sample s = motion::latest();
  return !motion::held() && !motion::flipped() && s.az > kFloorZ && fabsf(s.gx) < kCoastDps &&
         fabsf(s.gy) < kCoastDps && fabsf(s.gz) < kCoastDps && millis() - gEndMs >= kLabGapMs;
}

float restMs(Dance d) {
  portENTER_CRITICAL(&gMux);
  const float budget = gBudget;
  portEXIT_CRITICAL(&gMux);
  const float need = motorMs(d);
  return budget >= need ? 0 : (need - budget) * 60000.0f / kBudgetMs;
}

bool start(Dance d, bool dry, bool quick) {
  if (d > LAB) return false;
  if (gBusy) {
    setWhy("already dancing");
    return false;
  }
#if !PET_MOTORS
  dry = true;
#endif
  float v = 0;
  if (!dry) {
    if (!(quick ? labReady() : floorReady())) {
      setWhy("not on the floor");
      return false;
    }
    v = base::volts();
    if (v < 0) {
      setWhy("base not answering");
      return false;
    }
    if (v < kBaseLowV) {
      setWhy("base battery low");
      return false;
    }
    portENTER_CRITICAL(&gMux);
    const float budget = gBudget;
    portEXIT_CRITICAL(&gMux);
    if (budget < motorMs(d)) {
      setWhy("resting its motors");
      return false;
    }
  }
  portENTER_CRITICAL(&gMux);
  gStartReq = d;
  gStartDry = dry;
  gStartQuick = quick;
  gStartV = v;
  gStopReq = false;
  gWhy[0] = 0;
  gBusy = true;
  portEXIT_CRITICAL(&gMux);
  return true;
}

void stop(const char* w) {
  if (!gBusy) return;
  portENTER_CRITICAL(&gMux);
  gStopReq = true;
  strlcpy(gStopWhy, w, sizeof(gStopWhy));
  portEXIT_CRITICAL(&gMux);
#if PET_MOTORS
  base::stopMotors();  // now, from here too; the guard stops for good within a tick
#endif
}

void feed() { gFeedMs = millis(); }
bool busy() { return gBusy; }
const char* why() { return gWhy; }

bool takeReport(Report& r) {
  portENTER_CRITICAL(&gMux);
  const bool ready = gReportReady;
  if (ready) {
    r = gReport;
    gReportReady = false;
  }
  portEXIT_CRITICAL(&gMux);
  return ready;
}

}  // namespace moves
