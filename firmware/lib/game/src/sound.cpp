#include "sound.h"

namespace game {

namespace {

// Notes (Hz).
constexpr uint16_t C5 = 523, D5 = 587, E5 = 659, F5 = 698, G5 = 784, A5 = 880, B5 = 988;
constexpr uint16_t C6 = 1047, D6 = 1175, E6 = 1319, G6 = 1568, C7 = 2093;
constexpr uint16_t G4 = 392, E4 = 330, C4 = 262;

const Note kPutt[] = {{1400, 12}};
const Note kRail[] = {{900, 10}};
const Note kAnimal[] = {{1800, 30}, {2400, 40}};                 // a squeak
const Note kSink[] = {{G5, 60}, {0, 20}, {C6, 90}};               // plunk-plink
const Note kAce[] = {{C6, 90}, {E6, 90}, {G6, 90}, {C7, 260}, {0, 60}, {G6, 80}, {C7, 360}};
const Note kSplash[] = {{1200, 40}, {800, 50}, {500, 60}, {300, 120}};
const Note kSand[] = {{180, 40}};
const Note kTurn[] = {{E5, 70}, {G5, 110}};
const Note kSelect[] = {{C6, 40}, {G6, 60}};
const Note kMove[] = {{A5, 18}};
const Note kRecord[] = {{G5, 80}, {C6, 80}, {E6, 80}, {G6, 200}};
const Note kBeatMaker[] = {{C5, 100}, {E5, 100}, {G5, 100}, {C6, 160}, {0, 80}, {A5, 100}, {C6, 100}, {E6, 360}};
const Note kFanfare[] = {{G4, 120}, {C5, 120}, {E5, 120}, {G5, 240}, {E5, 120}, {G5, 400}};
const Note kPopUp[] = {{D5, 40}, {F5, 40}, {B5, 60}};
const Note kLocked[] = {{E4, 90}, {C4, 160}};

#define TUNE(x) x, static_cast<uint8_t>(sizeof(x) / sizeof(x[0]))

}  // namespace

void Sound::play(Sfx s) {
  switch (s) {
    case Sfx::Putt: playNotes(TUNE(kPutt)); break;
    case Sfx::Rail: playNotes(TUNE(kRail)); break;
    case Sfx::Animal: playNotes(TUNE(kAnimal)); break;
    case Sfx::Sink: playNotes(TUNE(kSink)); break;
    case Sfx::Ace: playNotes(TUNE(kAce)); break;
    case Sfx::Splash: playNotes(TUNE(kSplash)); break;
    case Sfx::Sand: playNotes(TUNE(kSand)); break;
    case Sfx::Turn: playNotes(TUNE(kTurn)); break;
    case Sfx::Select: playNotes(TUNE(kSelect)); break;
    case Sfx::Move: playNotes(TUNE(kMove)); break;
    case Sfx::Record: playNotes(TUNE(kRecord)); break;
    case Sfx::BeatMaker: playNotes(TUNE(kBeatMaker)); break;
    case Sfx::Fanfare: playNotes(TUNE(kFanfare)); break;
    case Sfx::PopUp: playNotes(TUNE(kPopUp)); break;
    case Sfx::Locked: playNotes(TUNE(kLocked)); break;
  }
}

void Sound::playNotes(const Note* notes, uint8_t count) {
  notes_ = notes;
  count_ = count;
  index_ = 0;
  started_ = false;
}

void Sound::tick(uint16_t hz, uint16_t ms) {
  // Don't cut a tune short for a blip.
  if (notes_ && index_ < count_) return;
  if (tone_) tone_(hz, ms);
}

void Sound::update(uint32_t ms) {
  if (!notes_ || index_ >= count_) return;
  if (started_ && static_cast<int32_t>(ms - nextAt_) < 0) return;
  const Note& n = notes_[index_];
  if (n.hz && tone_) tone_(n.hz, n.ms > 10 ? n.ms - 6 : n.ms);  // a hair of silence between notes
  nextAt_ = ms + n.ms;
  started_ = true;
  ++index_;
}

}  // namespace game
