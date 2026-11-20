// The menu: the face is home, and the menu is one big button away. Drawn with the face's
// own screen, fonts and colours. No Arduino here, so the PC check (firmware/pet/pc) draws it too.
#pragma once

#include "face_screen.h"

namespace menu {

enum Item : int { GOLF, DANCE, LAB, SOUND, FACE, COUNT };  // LAB: Trick Lab (lab.h)

const char* label(int item);
// soundOn: the Sound switch's state ("Sound", or "Quiet" with the speaker crossed out); soundPop: 0..1 through the
// speaker's bounce after a press, 0 for none.
void draw(face::Screen& scr, int selected, float seconds, bool soundOn, float soundPop = 0);

// Holding the big button goes back to the face from the menu, the picker and a dance's card (main.cpp): while it's
// held, a ring fills round the face's two eyes, drawn over whatever is on the screen. k: 0..1 through the hold.
void drawHomeRing(face::Screen& scr, float k, float seconds);

// The dance picker (Dance in the menu): a deck of the animals' power cards, one at a time, each animal inside a glass
// marble like the robot's own eyes (art/face/DESIGN.md, "The dance picker"). Tilt or the side button moves through
// it; the big button picks. Card 0 is Surprise! (a dance, never the last one), then each dance in moves::Dance order,
// then Back.
constexpr int kPickSurprise = 0;
int pickCount();
int pickBack();
struct PickView {
  float pos = 0;   // the card in view: it eases toward the choice, so the deck slides
  int sel = 0;     // the chosen card, for the dots
  int show = -1;   // a dance shown in place of the deck (Surprise!'s shuffle, then the pick); -1: the deck
  float pop = 0;   // 0..1 through the picked card's bounce
};
void drawPicker(face::Screen& scr, const PickView& v, float seconds);
face::C3 danceColor(int dance);  // its marble's colour (the scene's), for the base lights

// The picker's pieces, for Trick Lab's screens (lab.cpp), so its marbles are the same glass.
// A glass marble of colour `col`, radius r; rays: its sunburst, turned by rot.
void drawMarble(face::Screen& scr, float cx, float cy, float r, face::C3 col, bool rays, float rot);
void drawFace(face::Screen& scr, float cx, float cy, float s);  // the robot's two little eyes; s = 1 at the menu's size
// Text centred on the screen that steps its size down until it's at most `w` wide.
void drawFit(face::Screen& scr, const char* s, float y, face::TextOpt o, float w);

}  // namespace menu
