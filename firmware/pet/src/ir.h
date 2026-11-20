// The remote: the base's IR receiver (SL0038GD) on G36, decoded here (docs/HARDWARE.md, "IR receiver").
//
// It reads NEC frames, the code a cheap six-button heater remote sends (and most cheap remotes): a 9 ms burst, a
// 4.5 ms gap, then 32 bits. A frame counts only if its command byte and its inverse agree, which throws away the
// false edges G36 sees from the ADC and from Wi-Fi power saving. A held button sends short repeat codes (or the whole
// frame again); those count as the same press.
#pragma once

#include <stdint.h>

namespace ir {

void begin();
// A new button press since the last call: its 32 bits as sent (address, inverse or high address, command, inverse).
bool next(uint32_t& code);
uint32_t frames();  // good frames since the start, repeats of a held button included; for "info"
uint32_t edges();   // every falling edge seen, noise included

}  // namespace ir
