// Little tunes on the buzzer through Host::tone, which must not block. The game plays at most one tune at a time;
// a new tune replaces the old one.
#pragma once

#include <stdint.h>

namespace game {

struct Note {
  uint16_t hz;  // 0 = rest
  uint16_t ms;
};

enum class Sfx : uint8_t {
  Putt, Rail, Animal, Sink, Ace, Splash, Sand, Turn, Select, Move, Record, BeatMaker, Fanfare, PopUp, Locked
};

class Sound {
 public:
  void attach(void (*tone)(uint16_t, uint16_t)) { tone_ = tone; }
  void play(Sfx s);
  void playNotes(const Note* notes, uint8_t count);
  void update(uint32_t ms);
  void tick(uint16_t hz, uint16_t ms);  // a single short blip, interrupting nothing longer than itself

 private:
  void (*tone_)(uint16_t, uint16_t) = nullptr;
  const Note* notes_ = nullptr;
  uint8_t count_ = 0, index_ = 0;
  uint32_t nextAt_ = 0;
  bool started_ = false;
};

}  // namespace game
