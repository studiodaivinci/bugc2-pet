#include "sound.h"

#include <M5Unified.h>

namespace sound {
namespace {

struct Request {
  int8_t sfx;  // -1 = one tone, -2 = stop
  uint16_t hz, ms;
};
QueueHandle_t gQueue = nullptr;
volatile bool gBusy = false;
volatile bool gMuted = false;
uint8_t gVolume = 128;

void task(void*) {
  const Note* seq = nullptr;
  Note one[2];
  int pos = 0;
  uint32_t next = 0;
  for (;;) {
    TickType_t wait = portMAX_DELAY;
    if (seq) {
      const int32_t left = (int32_t)(next - millis());
      wait = left > 0 ? pdMS_TO_TICKS(left) : 0;
    }
    Request r;
    if (xQueueReceive(gQueue, &r, wait) == pdTRUE) {
      if (r.sfx == -2) {
        M5.Speaker.stop();
        seq = nullptr;
        gBusy = false;
        continue;
      }
      if (r.sfx == -1) {
        one[0] = {r.hz, r.ms};
        one[1] = {0, 0};
        seq = one;
      } else {
        seq = sequence(static_cast<Sfx>(r.sfx));
      }
      pos = 0;
      next = millis();
    }
    if (!seq || (int32_t)(millis() - next) < 0) continue;
    const Note n = seq[pos];
    if (n.ms == 0) {
      seq = nullptr;
      gBusy = false;
      continue;
    }
    gBusy = true;
    if (n.hz && !gMuted) {
      M5.Speaker.tone(n.hz, n.ms > 60 ? n.ms - 12 : n.ms);  // a small gap keeps repeated notes apart
    } else {
      M5.Speaker.stop();
    }
    next = millis() + n.ms;
    ++pos;
  }
}

void send(int8_t sfx, uint16_t hz = 0, uint16_t ms = 0) {
  if (!gQueue) return;
  const Request r{sfx, hz, ms};
  xQueueSend(gQueue, &r, 0);
}

}  // namespace

void begin(uint8_t volume) {
  build();
  M5.Speaker.begin();
  setVolume(volume);
  gQueue = xQueueCreate(8, sizeof(Request));
  xTaskCreatePinnedToCore(task, "sound", 3072, nullptr, 4, nullptr, 1);
}

void play(Sfx s) {
  if (s >= 0 && s < SFX_COUNT) send(s);
}
void tone(uint16_t hz, uint16_t ms) { send(-1, hz, ms); }
void stop() { send(-2); }
bool busy() { return gBusy; }
void setVolume(uint8_t v) {
  gVolume = v;
  M5.Speaker.setVolume(v);
}
uint8_t volume() { return gVolume; }
void setMuted(bool m) {
  gMuted = m;
  if (m) stop();
}
bool muted() { return gMuted; }
}  // namespace sound
