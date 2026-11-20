// The robot's sounds, as notes: tones only, for the Stick's buzzer. Plain C++ with no Arduino, so the firmware plays
// them (sound.cpp) and the PC renders the same notes to WAV files to listen to (firmware/pet/pc/sfx_wav.cpp).
#pragma once

#include <stdint.h>

namespace sound {

enum Sfx : int8_t {
  NONE = -1,
  CRACK, PEEP, HATCH, THUMP, LOVE, WHEEE, PHEW, DIZZY, HELP, THANKS, GIGGLE,
  CLICK, PICK, BACK, YAWN, WAKE, HUNGRY, CHEER, BIRTHDAY, SAD,
  SOUND_ON,  // the menu's Sound switch, turned on
  // The dances, a sound for each step (dances.cpp; moves.cpp starts each with its step, so it stays in time)
  REVUP, ZOOM, SKID, PANT,                       // cheetah
  BZZ, WIGGLE, SWOOP, BLOOM,                     // honeybee
  HUM, DART, SIP, DIVE, CHIRP, ZIPBACK,          // hummingbird
  TIPTOE, TREMBLE, EEK,                          // spider (and PHEW)
  BLINK, CREAK, HOO, HOOT,                       // owl
  CHATTER, LEAP, SPLASH, BUBBLES, BIGLEAP,       // dolphin
  WINDUP, REV, WHIRL, COMETS, GALAXY, SLOWDOWN,  // whirligig (and DIZZY)
  T_DASH, T_SCOOT, T_SPIN, T_WIGGLE,             // Trick Lab's moves (lab.cpp), each as long as its move
  SFX_COUNT
};

struct Note {
  uint16_t hz;  // 0 = rest
  uint16_t ms;  // 0 ends the sequence
};

void build();                // makes the swept and noisy sounds; once, before sequence()
const Note* sequence(Sfx s); // nullptr for NONE
uint32_t lengthMs(Sfx s);
const char* name(Sfx s);
int find(const char* name);  // -1 if unknown

}  // namespace sound
