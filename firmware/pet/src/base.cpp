// The base's bus, as in firmware/bench (where it was verified): open-drain bit-banging at about 100 kHz.
#include "base.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <rom/ets_sys.h>

namespace base {
namespace {

constexpr gpio_num_t kSda = GPIO_NUM_0;
constexpr gpio_num_t kScl = GPIO_NUM_26;
constexpr uint8_t kAddr = 0x38;
constexpr uint8_t kRegMotors = 0x00, kRegLed = 0x10, kRegBattery = 0x30;
constexpr int kMaxSpeed = 60;  // the bench capped at 80; speed 70 logged 5 g jolts for little extra spin (docs/LESSONS.md)

int16_t gLed[2][3] = {{-1, -1, -1}, {-1, -1, -1}};  // last colour written; -1 forces the next write

// The loop (lights, the battery) and the motor guard (moves.cpp) both use the bus: one transaction at a time.
SemaphoreHandle_t gLock = nullptr;
struct Hold {
  Hold() {
    if (gLock) xSemaphoreTakeRecursive(gLock, portMAX_DELAY);
  }
  ~Hold() {
    if (gLock) xSemaphoreGiveRecursive(gLock);
  }
};

inline void tick() { ets_delay_us(5); }
inline void sdaSet(bool high) { gpio_set_level(kSda, high); }
inline bool sdaGet() { return gpio_get_level(kSda); }
inline void sclLow() { gpio_set_level(kScl, 0); }
inline void sclHigh() {
  gpio_set_level(kScl, 1);
  const uint32_t started = micros();
  while (!gpio_get_level(kScl) && micros() - started < 5000) {
  }
}
void busStart() {
  sdaSet(true);
  sclHigh();
  tick();
  sdaSet(false);
  tick();
  sclLow();
  tick();
}
void busStop() {
  sdaSet(false);
  tick();
  sclHigh();
  tick();
  sdaSet(true);
  tick();
}
bool busWriteByte(uint8_t value) {
  for (int bit = 7; bit >= 0; --bit) {
    sdaSet((value >> bit) & 1);
    tick();
    sclHigh();
    tick();
    sclLow();
  }
  sdaSet(true);
  tick();
  sclHigh();
  const bool ack = !sdaGet();
  tick();
  sclLow();
  tick();
  return ack;
}
uint8_t busReadByte(bool ack) {
  uint8_t value = 0;
  sdaSet(true);
  for (int bit = 7; bit >= 0; --bit) {
    tick();
    sclHigh();
    value = (value << 1) | (sdaGet() ? 1 : 0);
    tick();
    sclLow();
  }
  sdaSet(!ack);
  tick();
  sclHigh();
  tick();
  sclLow();
  sdaSet(true);
  tick();
  return value;
}
bool write(uint8_t reg, const uint8_t* data, size_t length) {
  Hold hold;
  busStart();
  bool ok = busWriteByte(kAddr << 1) && busWriteByte(reg);
  for (size_t i = 0; ok && i < length; ++i) ok = busWriteByte(data[i]);
  busStop();
  return ok;
}
// The base prepares its reply when it sees the STOP after the register byte, then reads.
bool read(uint8_t reg, uint8_t* data, size_t length, uint32_t settleMs) {
  Hold hold;
  busStart();
  const bool ok = busWriteByte(kAddr << 1) && busWriteByte(reg);
  busStop();
  if (!ok) return false;
  delay(settleMs);
  busStart();
  if (!busWriteByte((kAddr << 1) | 1)) {
    busStop();
    return false;
  }
  for (size_t i = 0; i < length; ++i) data[i] = busReadByte(i + 1 < length);
  busStop();
  return true;
}

}  // namespace

void begin() {
  if (!gLock) gLock = xSemaphoreCreateRecursiveMutex();
  for (gpio_num_t pin : {kSda, kScl}) {
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_level(pin, 1);
  }
  for (int i = 0; i < 9; ++i) {  // clock out a slave stuck mid-byte
    tick();
    sclLow();
    tick();
    sclHigh();
  }
  busStop();
  stopMotors();
}

bool setMotors(int a, int b, int c, int d) {
  const int v[4] = {a, b, c, d};
  uint8_t speeds[4];
  for (int i = 0; i < 4; ++i) speeds[i] = static_cast<uint8_t>(static_cast<int8_t>(constrain(v[i], -kMaxSpeed, kMaxSpeed)));
  for (int attempt = 0; attempt < 3; ++attempt) {
    if (write(kRegMotors, speeds, 4)) return true;
    delay(2);
  }
  return false;
}

bool stopMotors() {
  const uint8_t zeros[4] = {0, 0, 0, 0};
  for (int attempt = 0; attempt < 3; ++attempt) {
    if (write(kRegMotors, zeros, 4)) return true;
    delay(2);
  }
  return false;
}

bool setLed(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
  if (index > 1) return false;
  Hold hold;  // the cache too
  int16_t* last = gLed[index];
  if (last[0] == r && last[1] == g && last[2] == b) return true;
  const uint8_t data[4] = {index, r, g, b};
  if (!write(kRegLed, data, 4)) {
    last[0] = -1;  // try again next time
    return false;
  }
  last[0] = r;
  last[1] = g;
  last[2] = b;
  return true;
}

void setLeds(uint8_t r, uint8_t g, uint8_t b) {
  setLed(0, r, g, b);
  setLed(1, r, g, b);
}

void ledsOff() { setLeds(0, 0, 0); }

float volts() {
  uint8_t raw[2] = {0, 0};
  if (!read(kRegBattery, raw, 2, 10)) return -1;
  return ((raw[0] | (raw[1] << 8)) / 4095.0f * 3.3f) * 2.960784f;
}

}  // namespace base
