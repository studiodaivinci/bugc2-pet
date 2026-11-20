// What the game remembers, in Preferences namespace "game" (a file on the PC build). Written after every hole,
// because the robot can lose power at any time. Under 2 KB.
//
// The layouts only ever grow at the end, and saves are never thrown away:
// - An older (shorter) blob fills the front of the struct; newer fields keep their defaults.
// - A newer blob (from a later build, then an older build installed) is read for what this build understands and is
//   never overwritten.
// - New fields take bytes from `reserved`, so the size stays the same. Never reorder, resize or remove a field.
// - Scores are indexed by hole id (the hole's place in kHoles). New holes are only ever appended.
#pragma once

#include <stdint.h>

#include "game.h"

namespace game {

constexpr int kMaxHoles = 16;
constexpr int kMaxSeats = 4;

// A round in progress, so a power cut resumes it at the start of the hole being played.
struct RoundSave {
  uint8_t active = 0;
  uint8_t habitat = 0;
  uint8_t seatCount = 0;
  uint8_t seats[kMaxSeats] = {0, 0, 0, 0};       // Player per seat
  uint8_t holeIndex = 0;                         // index into the round's hole list
  uint8_t scores[kMaxSeats][6] = {};             // strokes per seat per hole of the round (0 = not played)
};

constexpr uint8_t kSaveVersion = 3;  // 2: first gift build; 3: reserved bytes added

struct SaveData {
  uint8_t version = kSaveVersion;
  uint8_t best[kPlayerCount][kMaxHoles] = {};  // personal bests (0 = never finished)
  uint8_t lastSeats[kMaxSeats] = {Kid, 0, 0, 0};
  uint8_t lastSeatCount = 1;
  uint8_t aimMode = 0;                          // 0 turn the robot, 1 tilt it
  uint16_t holesPlayed = 0;
  uint16_t aces = 0;
  uint16_t splashes = 0;
  uint16_t beatMaker = 0;
  RoundSave round;
  uint8_t reserved[40] = {};  // room for new fields (take bytes from here)
};

// The Maker's records live under their own key so a reset to unhatched keeps them. 0 = none set yet, and the
// hole's built-in record applies (game.cpp, makerRecord()). Written only before hatching.
struct MakerRecords {
  uint8_t best[kMaxHoles] = {};
  uint8_t reserved[16] = {};
};

extern SaveData gSave;
extern MakerRecords gMaker;

void loadSave();
void writeSave();      // the progress blob (not if it came from a newer build)
void writeMaker();      // the Maker's records
void clearProgress();  // everything except the Maker's records and the aim setting
void clearMaker();

}  // namespace game
