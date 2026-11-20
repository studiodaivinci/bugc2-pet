// The motion sensor: read about every 4 ms in its own task, which integrates yaw for golf and spots handling.
//
// The detectors are first guesses, to be tuned from the gesture logs (bench-output/gestures/). Every
// threshold is a named constant in motion.cpp. "trace" in the shell prints what they see.
#pragma once

#include <stdint.h>

namespace motion {

struct Sample {
  float ax = 0, ay = 0, az = 1;  // g, the IMU's own axes. Lying screen up: az is about +1
  float gx = 0, gy = 0, gz = 0;  // deg/s, bias-corrected
  float yaw = 0;                 // integrated gz, degrees, counter-clockwise from above positive
};

enum Event : uint8_t { NONE, PICKUP, PUTDOWN, SHAKE, FLIP, UNFLIP, TAP, EVENT_COUNT };

void begin();         // measures the gyro bias (hold still about 0.6 s) and starts the task
Sample latest();
bool next(Event& e);  // the oldest unread event
void inject(Event e);  // from the shell, as if detected
bool held();           // picked up and not yet put down
bool flipped();        // upside down
float stillSeconds();  // how long it has been still
float settledSeconds();  // how long it has been stiller than a still hand (the put-down test; the floor check)
// The robot is moving itself for the next `ms` (Trick Lab's moves, moves.cpp): its own spinning isn't a pick-up or a
// tap, so nothing counts as one until then. Without this every spin "picked it up", and the next move had to wait a
// second for the put-down.
void selfMoving(uint32_t ms);
const char* name(Event e);
Event find(const char* name);  // NONE if unknown

// What the detectors see, for "trace": the latest values, and the peaks since the last call (taps last a few ms, so
// a trace that only sampled them would miss them). Calling it resets the peaks.
struct Trace {
  float amag, gmag, still, shake, jerk, tilt, az;
  float aMin, aMax, gPeak, shakePeak, jerkPeak;
  bool held, flipped;
};
Trace trace();

// The internal I2C bus (IMU 0x68, clock 0x51) is shared: hold this around M5.Rtc calls.
void lockBus();
void unlockBus();

}  // namespace motion
