// The motor guard and the dances: the only code that moves the robot (docs/LESSONS.md, "The motor guard").
//
// One task owns the motors. A dance is a list of timed steps (spin, waggle, turn, lights), and the task stops the
// motors at the end of each one, whatever the loop is doing. It stops early, at once, if the robot tips or is lifted
// (the gyro stops seeing it turn: lifted, or on carpet), if a button is pressed, if the loop goes quiet, or if the
// loop asks. A dance starts only after the floor check: lying flat, screen up, not held, and stiller than a still hand
// for a second. The base's lights belong to the dance while it runs.
#pragma once

#include <stdint.h>

namespace moves {

// Each dance is an animal power, and plays with that animal's face scene ("power-" + its name).
enum Dance : uint8_t { CHEETAH, HONEYBEE, HUMMINGBIRD, SPIDER, OWL, DOLPHIN, WHIRLIGIG, DANCE_COUNT,
                       TRIAL = DANCE_COUNT,  // the shell's test of driving each way; not in the picker
                       LAB };                // Trick Lab's moves: whatever setLab() last gave it (dances.h)
const char* name(Dance d);
bool find(const char* name, Dance& d);

void begin();        // after base::begin() and motion::begin()
bool floorReady();   // the floor check
// Trick Lab's check between its moves: not picked up since it last passed the floor check (its own moves don't count
// as handling: motion::selfMoving), lying flat, and no longer coasting from the move before.
bool labReady();
// Starts a dance. dry: the lights and the timing only, the motors stay off (always so in a build without motors).
// quick: labReady() in place of the floor check. Returns false, and why() says why, if it's refused: not on the
// floor, the base not answering or its cell low, or the minute's motor time used up.
bool start(Dance d, bool dry, bool quick = false);
float restMs(Dance d);  // how long until the minute's motor time covers this dance; 0: it does now
void stop(const char* why);  // "Freeze!": the motors stop at once and the dance ends
void feed();                 // the loop's heartbeat, every frame; the guard stops the motors if it goes quiet
bool busy();
const char* why();           // the last refusal or early stop; "" if none

// What a dance did, once, after it ends: for the shell and for tuning.
struct StepLog {
  char name[8];
  uint16_t ms;      // how long it ran
  int16_t peakDps;  // the fastest turn (the gyro reads up to 2000)
  int16_t yaw;      // degrees turned, counter-clockwise from above positive
  int16_t off;      // a steered move: the furthest it was from the heading it was holding, degrees; else -1
};
struct Report {
  Dance dance;
  bool dry;
  float baseV;   // the motor cell at the start; 0 when dry
  char stop[24];  // why it stopped early; "" if it finished
  int n;
  StepLog steps[20];
};
bool takeReport(Report& r);

}  // namespace moves
