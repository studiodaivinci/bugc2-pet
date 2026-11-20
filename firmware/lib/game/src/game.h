// Pop-Up Golf: the screen game, as a library the robot firmware calls.
//
// The robot firmware (the host) owns the screen, IMU, buttons, buzzer and timing. The game never touches
// M5.Display, M5.Imu, M5.update(), I2C or GPIO. Each frame the host fills an Input, calls game::update(), and
// pushes the canvas to the screen. The full contract and its reasons are in docs/GAME.md.
#pragma once

#include <stdint.h>

#ifdef ARDUINO
#include <M5GFX.h>
#endif

namespace game {

struct Button {
  bool pressed = false;   // went down this frame
  bool held = false;      // is down now
  bool released = false;  // came up this frame
};

struct Input {
  uint32_t ms = 0;     // millis()
  float dt = 0;        // seconds since the last update; the game clamps large steps itself
  Button a;            // big front button (G37)
  Button b;            // side button (G39). The game ignores a press seen on one frame only (Wi-Fi glitches on G39)
  float ax = 0, ay = 0, az = 0;  // accelerometer in g, the IMU's own axes (screen up: az is about +1)
  float gz = 0;        // gyro z in deg/s, bias-corrected (not M5Unified's stored offset)
  float yaw = 0;       // integrated yaw in degrees, counter-clockwise positive seen from above (screen up); integrate
                       // the gyro at 100 Hz or more, not once a frame: aiming follows this
};

// Moments the real robot can react to (a face, a dance, a sound of its own).
enum class Event : uint8_t {
  HoleInOne,       // a player sank the ball in one stroke
  UnderPar,        // a hole finished under par (not a hole in one)
  NewRecord,       // a player set the best score on a hole (any player, any time)
  BeatMaker,   // a player beat the Maker's record on a hole
  BallInWater,     // splash
  RoundWon,        // a round finished; `player` is the winner
  TiedMaker,   // a player matched the Maker's record on a hole (new values are only ever appended)
};

// The six names on the scoreboard, in menu order.
enum Player : uint8_t { Kid, Player2, Maker, Player4, Player5, Guest, kPlayerCount };

struct Host {
  void (*tone)(uint16_t hz, uint16_t ms) = nullptr;  // must not block; a newer tone replaces the playing one
  int (*daysSinceHatch)() = nullptr;                 // < 0 before hatching: the Maker's scores become records
  void (*event)(Event e, uint8_t player) = nullptr;  // optional
};

#ifdef ARDUINO
// canvas: 135x240 portrait, 16-bit, in PSRAM, created by the host. The game draws a whole frame into it on
// every update(); the host pushes it to the screen afterwards. Call once at boot, or again before each new visit to
// the game (it re-reads the saves).
void begin(M5Canvas& canvas, const Host& host);
#endif

// The same, with a raw byte-swapped RGB565 buffer of 135x240 (what M5Canvas holds). Used by the PC test build.
void beginRaw(uint16_t* pixels, const Host& host);

// One frame, about 30 times a second. Never blocks; returns false when the player quits back to the robot.
bool update(const Input& in);

// Clears all progress (scores, trophies, the round in progress) but keeps the Maker's records. The robot's
// "reset to unhatched" calls this. Never touches any other Preferences namespace.
void resetProgress();

// Clears everything, the Maker's records too.
void resetAll();

constexpr int kWidth = 135;
constexpr int kHeight = 240;

}  // namespace game
