// The base (BugC2): its STM32 at 0x38 on a bit-banged I2C bus, G0 = SDA, G26 = SCL (docs/HARDWARE.md).
// The motors are moves.cpp's alone (the guard); the loop sets the two lights and reads the motor cell. Each transaction
// holds a lock, so the guard's task and the loop can share the bus.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace base {

void begin();       // takes the pins, unsticks the bus and sends the stop
bool stopMotors();  // all four motors to 0; true if the base acknowledged
bool setMotors(int a, int b, int c, int d);  // speeds -60..60 per motor (bench order); only moves.cpp calls this
bool setLed(uint8_t index, uint8_t r, uint8_t g, uint8_t b);  // index 0 or 1; skips the write if nothing changed
void setLeds(uint8_t r, uint8_t g, uint8_t b);                 // both
void ledsOff();
float volts();  // the base's 700 mAh motor cell, or -1 if the base doesn't answer. Blocks about 12 ms

}  // namespace base
