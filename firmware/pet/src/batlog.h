// The battery run-down log: overnight with the face running, ending at the low-battery power-off. `batrun` restarts the robot into it: Wi-Fi off (so the face gets both frames, as in the gift
// build), awake at home (no naps), and every 2 minutes both batteries saved to flash, so the log survives the
// switch-off. The start after that has Wi-Fi again, and `batlog` prints the log.
//
// Same rows and Preferences namespace ("batlog") as the bench firmware, so `python tools/capture.py batlog` reads
// either. A restart ends the run; the log stays until the next `batrun`.
#pragma once

#include <stdint.h>

namespace batlog {

void start();             // clears the old log and saves the first row
void loop(uint32_t now);  // a row every 2 minutes while running
void finalRow();          // one last row, just before the low-battery switch-off
bool running();
void print();             // the saved log, to the console

}  // namespace batlog
