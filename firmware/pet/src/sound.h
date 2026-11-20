// Sounds on the Stick's buzzer: short effects, the dances' sounds and Happy Birthday (sfx.cpp). Tones only. A task plays them, so a slow frame
// never stretches a note. A new sound replaces the one playing.
#pragma once

#include <stdint.h>

#include "sfx.h"  // the sounds themselves (Sfx, name, find)

namespace sound {

void begin(uint8_t volume);
void play(Sfx s);                     // safe from any task
void tone(uint16_t hz, uint16_t ms);  // one tone (the golf game's)
void stop();
bool busy();                          // something is playing
void setVolume(uint8_t v);            // 0..255
uint8_t volume();
void setMuted(bool m);                // muted: everything is silent, the dances' sounds and golf's too
bool muted();

}  // namespace sound
