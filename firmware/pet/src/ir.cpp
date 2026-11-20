#include "ir.h"

#include <Arduino.h>

namespace ir {
namespace {

// The receiver pulls the pin low while it hears the remote, so every burst starts with a falling edge. Timed from one
// falling edge to the next, in microseconds: the leader is 13.5 ms (9 + 4.5), a repeat code 11.25 ms (9 + 2.25), a 0
// bit 1.125 ms and a 1 bit 2.25 ms. Only falling edges are timed, so a receiver that stretches its bursts (they all
// do, a little) doesn't matter.
constexpr uint32_t kLeaderLo = 12300, kLeaderHi = 14700, kRepeatLo = 10300, kRepeatHi = 12200;
constexpr uint32_t kZeroLo = 800, kZeroHi = 1500, kOneLo = 1850, kOneHi = 2700;
constexpr uint32_t kHeldUs = 250000;  // the same code again within this of the last frame or repeat: still held

portMUX_TYPE gMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t gLastEdge = 0, gData = 0, gCode = 0, gSeenUs = 0, gPresses = 0, gFrames = 0, gEdges = 0;
volatile uint8_t gBits = 0;
volatile bool gActive = false, gAny = false;

void IRAM_ATTR isr() {
  const uint32_t now = micros();
  const uint32_t d = now - gLastEdge;
  gLastEdge = now;
  gEdges = gEdges + 1;
  if (d > kLeaderLo && d < kLeaderHi) {
    gActive = true;
    gBits = 0;
    gData = 0;
    return;
  }
  if (d > kRepeatLo && d < kRepeatHi) {  // a held button: keep its press alive
    gActive = false;
    if (gAny && now - gSeenUs < kHeldUs) {
      gSeenUs = now;
      gFrames = gFrames + 1;
    }
    return;
  }
  if (!gActive) return;
  uint32_t bit;
  if (d > kZeroLo && d < kZeroHi) bit = 0;
  else if (d > kOneLo && d < kOneHi) bit = 1;
  else {
    gActive = false;  // a glitch, or not NEC: wait for the next leader
    return;
  }
  gData = gData | (bit << gBits);  // least significant bit first
  gBits = gBits + 1;
  if (gBits < 32) return;
  gActive = false;
  const uint32_t data = gData;
  if ((((data >> 16) ^ (data >> 24)) & 0xff) != 0xff) return;  // the command and its inverse must agree
  portENTER_CRITICAL_ISR(&gMux);
  const bool held = gAny && data == gCode && now - gSeenUs < kHeldUs;
  gCode = data;
  gSeenUs = now;
  gAny = true;
  gFrames = gFrames + 1;
  if (!held) gPresses = gPresses + 1;
  portEXIT_CRITICAL_ISR(&gMux);
}

}  // namespace

void begin() {
  pinMode(GPIO_NUM_25, INPUT);  // shares a header pin with G36: left floating, or the receiver can't pull the pin
  pinMode(GPIO_NUM_36, INPUT);
  gLastEdge = micros();
  attachInterrupt(digitalPinToInterrupt(GPIO_NUM_36), isr, FALLING);
}

bool next(uint32_t& code) {
  static uint32_t taken = 0;
  portENTER_CRITICAL(&gMux);
  const uint32_t presses = gPresses, c = gCode;
  portEXIT_CRITICAL(&gMux);
  if (presses == taken) return false;
  taken = presses;
  code = c;
  return true;
}

uint32_t frames() { return gFrames; }
uint32_t edges() { return gEdges; }

}  // namespace ir
