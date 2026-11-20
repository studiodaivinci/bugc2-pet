#include "motion.h"

#include <M5Unified.h>
#include <string.h>

namespace motion {
namespace {

// --- Thresholds. Tuned 2026-11-10 from a handling run (bench-output/handling/2026-11-10-run1.log) ----------------
// What it showed, as |gyro| in deg/s: on the desk 0.1 (never above 0.6); in a still hand 2.6-2.8 median, 100 ms peaks
// above 2.8 nine times in ten; on a lap 0.5-1.2. Real shakes 2.4-4.1 on the shake scale; a brisk pick-up 0.5 and
// pulling out the USB cable 0.7. Taps on the case jolted 2-5.5 g and rocked it 90-210 deg/s; knocks on the desk beside
// it jolted up to 0.6 g and turned it 6-8 deg/s.
constexpr float kStillDps = 4.0f;       // gyro below this, and...
constexpr float kStillG = 0.04f;        // ...acceleration within this of 1 g, counts as still (naps, gyro bias)
constexpr float kSettledDps = 2.0f;     // put down needs stiller than a still hand: gyro below this, and...
constexpr float kSettledG = 0.03f;      // ...acceleration within this of 1 g...
constexpr float kRestSeconds = 1.0f;    // ...this long, lying flat (a still hand counted as put down at 4 deg/s)
constexpr float kFlatZ = 0.85f;         // az above this = lying screen up
constexpr float kLiftDps = 30.0f;       // resting, then turning faster than this...
constexpr float kLiftSeconds = 0.08f;   // ...for this long = picked up
constexpr float kLiftTiltDeg = 20.0f;   // or tilted this far from how it was resting...
constexpr float kLiftTiltSeconds = 0.15f;  // ...for this long (a 7 g tap swung the smoothed reading past 20° for an
                                           // instant and counted as a pick-up, then a shake: run 2, 2026-11-10)
constexpr float kShakeLevel = 1.0f;     // held, smoothed (|a| - 1)^2 over this = a shake (0.5 caught pick-ups)
constexpr float kShakeTau = 0.15f;      // seconds of smoothing
constexpr uint32_t kShakeGapMs = 3000;  // one shake event per 3 s
constexpr float kFlipZ = 0.6f;          // az below -this = upside down, above +this = right way up
constexpr uint32_t kFlipMs = 400;
constexpr float kTapG = 0.3f;           // resting, a jolt this big away from the smoothed reading...
constexpr float kTapMinDps = 15.0f;     // ...that rocks it at least this fast within the next 250 ms = a tap on it
constexpr uint32_t kTapWindowMs = 250;  // (a knock on the table jolts it without rocking it)
constexpr uint32_t kTapGapMs = 300;

SemaphoreHandle_t gBus = nullptr;
QueueHandle_t gEvents = nullptr;
portMUX_TYPE gMux = portMUX_INITIALIZER_UNLOCKED;
Sample gLatest;
Trace gTrace{};
float gAMin = 9, gAMax = 0, gGPeak = 0, gShakePeak = 0, gJerkPeak = 0;  // since the last trace()
float gBias[3] = {0, 0, 0};
volatile bool gHeld = false, gFlipped = false;
volatile float gStill = 0, gSettled = 0;
volatile uint32_t gSelfUntil = 0;  // selfMoving(): no pick-ups or taps until then

void push(Event e) {
  if (gEvents) xQueueSend(gEvents, &e, 0);
}

void measureBias() {
  double sum[3] = {0, 0, 0};
  float lo = 1e9f, hi = -1e9f;
  int n = 0;
  for (const uint32_t t0 = millis(); millis() - t0 < 600;) {
    if (M5.Imu.update()) {
      float g[3];
      M5.Imu.getGyro(&g[0], &g[1], &g[2]);
      for (int i = 0; i < 3; ++i) sum[i] += g[i];
      const float m = fabsf(g[0]) + fabsf(g[1]) + fabsf(g[2]);
      lo = min(lo, m);
      hi = max(hi, m);
      ++n;
    }
    delay(2);
  }
  if (n >= 100 && hi - lo < 15) {
    for (int i = 0; i < 3; ++i) gBias[i] = sum[i] / n;
  }
  Serial.printf("gyro bias %.2f %.2f %.2f (%d samples, %s)\n", gBias[0], gBias[1], gBias[2], n,
                hi - lo < 15 ? "kept" : "moving, left at 0");
}

void task(void*) {
  uint32_t last = micros();
  float yaw = 0, still = 0, settled = 0, shake = 0, lift = 0, tiltFor = 0, tapPeakDps = 0;
  float lp[3] = {0, 0, 1};    // smoothed acceleration
  float rest[3] = {0, 0, 1};  // the acceleration while it last rested
  uint32_t shakeAt = 0, tapAt = 0, tapJoltAt = 0, flipSince = 0, unflipSince = 0;
  bool held = false, flipped = false, first = true;
  for (;;) {
    float a[3], g[3];
    bool fresh = false;
    if (xSemaphoreTake(gBus, pdMS_TO_TICKS(20)) == pdTRUE) {
      if (M5.Imu.update()) {
        M5.Imu.getAccel(&a[0], &a[1], &a[2]);
        M5.Imu.getGyro(&g[0], &g[1], &g[2]);
        fresh = true;
      }
      xSemaphoreGive(gBus);
    }
    if (!fresh) {
      vTaskDelay(pdMS_TO_TICKS(2));
      continue;
    }
    const uint32_t nowUs = micros();
    const float dt = min((nowUs - last) / 1e6f, 0.05f);
    last = nowUs;
    const uint32_t now = millis();
    for (int i = 0; i < 3; ++i) g[i] -= gBias[i];

    const float amag = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
    const float gmag = sqrtf(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]);
    const bool calm = gmag < kStillDps && fabsf(amag - 1) < kStillG;
    const float stillBefore = still;  // a tap's own jolt resets still, so the tap test looks at the moment before
    still = calm ? still + dt : 0;
    settled = gmag < kSettledDps && fabsf(amag - 1) < kSettledG ? settled + dt : 0;

    // The gyro's bias follows its reading while it is still, so yaw doesn't creep.
    if (still > 1.0f) {
      const float k = min(1.0f, dt / 5.0f);
      for (int i = 0; i < 3; ++i) gBias[i] += g[i] * k;
    }
    if (fabsf(g[2]) > 2.0f || still < 1.0f) yaw += g[2] * dt;

    // Smoothed acceleration, and how far this sample jumps from it (taps).
    float jerk = 0;
    if (first) {
      memcpy(lp, a, sizeof(lp));
      memcpy(rest, a, sizeof(rest));
      first = false;
    }
    for (int i = 0; i < 3; ++i) {
      const float d = a[i] - lp[i];
      jerk += d * d;
      lp[i] += d * min(1.0f, dt / 0.05f);
    }
    jerk = sqrtf(jerk);

    // Shake: the smoothed squared distance of |a| from 1 g.
    const float d1 = amag - 1;
    shake += (d1 * d1 - shake) * min(1.0f, dt / kShakeTau);
    if (held && shake > kShakeLevel && now - shakeAt > kShakeGapMs) {  // held: a hard tap on the desk isn't a shake
      shakeAt = now;
      shake = 0;
      push(SHAKE);
    }

    // Upside down and back.
    if (!flipped) {
      if (a[2] < -kFlipZ) {
        if (!flipSince) flipSince = now;
        if (now - flipSince > kFlipMs) {
          flipped = true;
          unflipSince = 0;
          push(FLIP);
        }
      } else {
        flipSince = 0;
      }
    } else {
      if (a[2] > kFlipZ) {
        if (!unflipSince) unflipSince = now;
        if (now - unflipSince > kFlipMs) {
          flipped = false;
          flipSince = 0;
          push(UNFLIP);
        }
      } else {
        unflipSince = 0;
      }
    }

    // Picked up and put down. Resting: remember how it lies; a turn or a tilt away from that is a pick-up.
    float tilt = 0;
    if (!held && static_cast<int32_t>(now - gSelfUntil) < 0) {  // moving itself: not handling
      lift = tiltFor = 0;
      tapJoltAt = 0;
    } else if (!held) {
      const float rm = sqrtf(rest[0] * rest[0] + rest[1] * rest[1] + rest[2] * rest[2]);
      const float lm = sqrtf(lp[0] * lp[0] + lp[1] * lp[1] + lp[2] * lp[2]);
      if (rm > 0.5f && lm > 0.5f) {
        const float c = (rest[0] * lp[0] + rest[1] * lp[1] + rest[2] * lp[2]) / (rm * lm);
        tilt = acosf(constrain(c, -1.0f, 1.0f)) * 57.2958f;
      }
      if (still > 0.3f) memcpy(rest, lp, sizeof(rest));
      lift = gmag > kLiftDps ? lift + dt : 0;
      tiltFor = tilt > kLiftTiltDeg ? tiltFor + dt : 0;
      if ((lift > kLiftSeconds || tiltFor > kLiftTiltSeconds) && !flipped) {
        held = true;
        lift = tiltFor = 0;
        tapJoltAt = 0;  // picked up, not tapped
        push(PICKUP);
      } else if (tapJoltAt) {
        // A jolt while resting: a tap if it rocks the robot, a knock on the table if it doesn't.
        tapPeakDps = max(tapPeakDps, gmag);
        if (now - tapJoltAt > kTapWindowMs) {
          if (tapPeakDps >= kTapMinDps) {
            tapAt = now;
            push(TAP);
          }
          tapJoltAt = 0;
        }
      } else if (stillBefore > 0.3f && jerk > kTapG && now - tapAt > kTapGapMs) {
        tapJoltAt = now;
        tapPeakDps = gmag;
      }
    } else if (settled > kRestSeconds && a[2] > kFlatZ) {
      held = false;
      memcpy(rest, lp, sizeof(rest));
      push(PUTDOWN);
    }

    portENTER_CRITICAL(&gMux);
    gLatest.ax = a[0];
    gLatest.ay = a[1];
    gLatest.az = a[2];
    gLatest.gx = g[0];
    gLatest.gy = g[1];
    gLatest.gz = g[2];
    gLatest.yaw = yaw;
    gAMin = min(gAMin, amag);
    gAMax = max(gAMax, amag);
    gGPeak = max(gGPeak, gmag);
    gShakePeak = max(gShakePeak, shake);
    gJerkPeak = max(gJerkPeak, jerk);
    gTrace = {amag, gmag, still, shake, jerk, tilt, a[2], gAMin, gAMax, gGPeak, gShakePeak, gJerkPeak, held, flipped};
    portEXIT_CRITICAL(&gMux);
    gHeld = held;
    gFlipped = flipped;
    gStill = still;
    gSettled = settled;
    vTaskDelay(pdMS_TO_TICKS(4));
  }
}

const char* const kNames[EVENT_COUNT] = {"none", "pickup", "putdown", "shake", "flip", "unflip", "tap"};

}  // namespace

void begin() {
  gBus = xSemaphoreCreateMutex();
  gEvents = xQueueCreate(16, sizeof(Event));
  M5.Imu.clearOffsetData();  // M5Unified's stored offset can be badly wrong (docs/HARDWARE.md)
  measureBias();
  // Above the face's row worker (5) on core 0, so a big fill doesn't starve it. Each read takes a fraction of a ms.
  xTaskCreatePinnedToCore(task, "motion", 4096, nullptr, 6, nullptr, 0);
}

Sample latest() {
  portENTER_CRITICAL(&gMux);
  const Sample s = gLatest;
  portEXIT_CRITICAL(&gMux);
  return s;
}

Trace trace() {
  portENTER_CRITICAL(&gMux);
  const Trace t = gTrace;
  gAMin = 9;
  gAMax = gGPeak = gShakePeak = gJerkPeak = 0;
  portEXIT_CRITICAL(&gMux);
  return t;
}

bool next(Event& e) { return gEvents && xQueueReceive(gEvents, &e, 0) == pdTRUE; }
void inject(Event e) { push(e); }
bool held() { return gHeld; }
bool flipped() { return gFlipped; }
float stillSeconds() { return gStill; }
float settledSeconds() { return gSettled; }
void selfMoving(uint32_t ms) { gSelfUntil = millis() + ms; }
const char* name(Event e) { return e < EVENT_COUNT ? kNames[e] : "?"; }
Event find(const char* n) {
  for (int i = 1; i < EVENT_COUNT; ++i)
    if (strcmp(n, kNames[i]) == 0) return (Event)i;
  return NONE;
}
void lockBus() {
  if (gBus) xSemaphoreTake(gBus, portMAX_DELAY);
}
void unlockBus() {
  if (gBus) xSemaphoreGive(gBus);
}

}  // namespace motion
