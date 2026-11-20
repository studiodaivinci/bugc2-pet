// The dances as data: each a list of timed steps, with the lights' mood and the sound that starts with each step.
// Plain C++ with no Arduino, so the PC reads them too (firmware/pet/pc/sfx_wav.cpp plays each dance's sounds in time).
// moves.cpp runs them, with the motor guard.
#pragma once

#include <stdint.h>

#include "moves.h"
#include "sfx.h"

namespace moves {

// SPIN, WAGGLE and TURN run all four motors one way: it turns on the spot. DRIVE (forward, back) and STRAFE (left,
// right) travel, with the gyro holding the heading (moves.cpp).
enum Kind : uint8_t { LIGHTS, SPIN, WAGGLE, TURN, DRIVE, STRAFE };
enum Light : uint8_t {
  L_REVUP, L_STREAK, L_COAST, L_PANT,                         // cheetah
  L_BZZ, L_WAGGLE, L_SWING, L_BLOOM,                          // honeybee
  L_HUM, L_DART, L_SIP, L_NECTAR, L_DIVE,                     // hummingbird
  L_CREEP, L_TREMBLE, L_EEK, L_PHEW,                          // spider
  L_STARE, L_BLINK, L_LOOK, L_HOO,                            // owl
  L_SWIM, L_SPRAY, L_SPLASH, L_BUBBLES,                       // dolphin
  L_WINDUP, L_HALO, L_COMETS, L_GALAXY, L_SLOWHALO, L_DIZZY,  // whirligig
};

struct Step {
  const char* name;
  Kind kind;
  int8_t speed;     // 0..60, all four motors
  int8_t dir;       // +1 the bench's "rotate", -1 the other way (which way it really turns depends on which feet
                    // grip). DRIVE: +1 forward (the screen end first), -1 back. STRAFE: +1 left, -1 right
  uint16_t ms;      // the longest it runs
  uint16_t flipMs;  // WAGGLE: reverses this often. DRIVE, STRAFE: a wiggle on top, swinging this often...
  uint16_t angle;   // TURN: done once the gyro has seen it turn this far its own way (it coasts a little further);
                    // 0: by time only. DRIVE, STRAFE: ...the wiggle's size, in motor speed
  Light light;
  uint16_t until;   // LIGHTS: ends this many ms into the dance instead (0: after ms), to stay in time with the screen
                    // after turns that end by the gyro
  sound::Sfx sfx;   // starts with the step; NONE: the sound before plays on
  int16_t head = 0;  // DRIVE, STRAFE: the heading it holds, degrees counter-clockwise from the heading it had at the
                     // dance's first travelling step (taken again after any SPIN or TURN)...
  int16_t rate = 0;  // ...turning this many degrees a second through the step: arcs and loops
};

struct Program {
  const Step* steps;
  int n;
};
const Program& program(Dance d);
constexpr int kLabSteps = 16;
void setLab(const Step* steps, int n);  // Trick Lab's program (the LAB dance), copied; never while it's running
float motorMs(Dance d);  // the motors' running time, for the minute's budget

}  // namespace moves
