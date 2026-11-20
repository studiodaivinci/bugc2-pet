// The face's frames, as in firmware/face-test: each frame is two halves of 135 x 120 in fast internal RAM (two whole
// frames don't fit in one piece). A task on core 0 sends one frame to the screen while the face draws the next into
// the other, and a worker on core 0 takes half the rows of every big fill. Without room for a second frame, the one
// frame is sent and waited for.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace frames {

bool begin();       // the first frame, and the tasks
bool addSecond(size_t keepFree);  // the second, last in setup once Wi-Fi and sound have taken theirs, if keepFree
                                  // bytes of internal RAM stay free after it (a 2026-11-10 crash: taken first, it left
                                  // Wi-Fi and sound so little that the first battery reading couldn't get a lock)
uint16_t* top();     // the frame to draw into next
uint16_t* bottom();
void ready();        // call before drawing: with one frame, waits until it has gone out
void present();      // sends the frame just drawn, and moves on to the other one
void wait();         // until the frame in flight has gone out (before using the display directly)
int count();         // 1 or 2
void releaseSecond();  // frees the second frame: an update needs the RAM

}  // namespace frames
