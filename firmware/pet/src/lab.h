// Trick Lab: the kid builds a trick with the remote, the robot plays it back, names it and remembers it
// (2026-11-11).
//
// Four of the remote's buttons are moves, each a small piece of an animal's dance, so each already has a toy, a
// colour, lights and a sound: the cheetah dashes forward, the spider scoots back, the whirligig spins, the honeybee
// wiggles. A press does the move at once and adds it to the strip on the screen, six at most. GO plays the whole
// strip back, and at the end the robot shouts the trick's silly two-word name. Freeze stops it, or wipes the strip.
// Nothing is saved by hand (2026-11-11: saving by hand was confusing):
// the latest trick is the GO trick, switched off or on, until a new animal
// press starts another. A trick's name comes from its moves, so the same moves always have the same name.
//
// This file is the lab's data and its screens: no Arduino here, so the PC check (firmware/pet/pc) draws them too.
// main.cpp runs it (the Lab state); ir.cpp reads the remote; moves.cpp runs the moves behind the motor guard.
#pragma once

#include <stdint.h>

#include "dances.h"
#include "face_screen.h"

namespace lab {

enum Move : uint8_t { DASH, SCOOT, SPIN, WIGGLE, MOVE_COUNT };
// What a remote button means. The first four are the moves, in Move order.
enum Role : int8_t { NO_ROLE = -1, R_DASH, R_SCOOT, R_SPIN, R_WIGGLE, R_GO, R_FREEZE, ROLE_COUNT };

constexpr int kMaxSteps = 6;

struct Trick {
  uint8_t n = 0;
  uint8_t steps[kMaxSteps] = {0, 0, 0, 0, 0, 0};  // Move values
  bool operator==(const Trick& o) const;
};

// The trick's name, from its moves: "TURBO" and "NOODLE". Both "" for an empty trick.
void nameOf(const Trick& t, const char*& adjective, const char*& noun);
const char* roleName(int role);   // "CHEETAH" ... "GO!", "FREEZE!"
const char* roleDoes(int role);   // "dashes forward" ... for the remote's lesson
const char* roleKey(int role);    // "dash" ... "go", "freeze": the shell's words
const char* moveShout(int move);  // "DASH!"
face::C3 roleColor(int role);

// The moves as steps for moves.cpp. only >= 0: just that step of the trick (a press, played at once). only < 0: the
// whole trick, with a beat between the moves and a ta-da at the end. Returns the number of steps (at most
// moves::kLabSteps).
int build(const Trick& t, int only, moves::Step* out);
// Which of the trick's moves is playing `ms` into the whole trick: -1 in a beat between two, t.n in the ta-da.
int stepAt(const Trick& t, uint32_t ms);

enum Status : uint8_t {
  S_EMPTY,    // nothing on the strip
  S_BUILD,    // some moves on it
  S_FULL,     // six
  S_PLAY,     // GO: playing it back
  S_PUTDOWN,  // GO, but it isn't on the floor
  S_REST,     // GO, but the motors need a rest first
  S_DONE,     // the end of the replay: ta-da
  S_NAMED,    // played, or remembered from before: its name, and GO plays it again
};
struct View {
  Trick trick;
  int active = -1;    // the move being done now; -1 none
  float pop = 1;      // 0..1 through the newest move's bounce onto the strip
  Status status = S_EMPTY;
  int word = -1;      // the move whose shout shows ("DASH!"); -1 none
  float rest = 0;     // S_REST: 0..1, how rested
  bool anySaved = false;  // it remembers a trick: GO on an empty strip brings it back
};
void draw(face::Screen& scr, const View& v, float seconds);

// The remote's lesson: "press the button for the cheetah", one role at a time.
struct LearnView {
  int role = 0;
  float pop = 1;      // 0..1 through this role's card arriving
  float taken = 0;    // > 0: that button already has a job (counts down, seconds)
};
void drawLearn(face::Screen& scr, const LearnView& v, float seconds);

}  // namespace lab
