#include "batlog.h"

#include <M5Unified.h>
#include <Preferences.h>

#include "base.h"
#include "net.h"

namespace batlog {
namespace {

struct Row {  // the bench firmware's BatRow, byte for byte
  uint16_t minute, baseMv, stickMv;
  uint8_t level, baseOk;
};
static_assert(sizeof(Row) == 8, "the bench firmware's row is 8 bytes");

constexpr int kMax = 360;  // 12 hours
constexpr uint32_t kEveryMs = 120000;

Row gRows[kMax];
int gCount = 0;
bool gRunning = false;
uint32_t gStarted = 0, gNext = 0;

void save() {
  Preferences p;
  if (p.begin("batlog", false)) {
    p.putBytes("rows", gRows, gCount * sizeof(Row));
    p.end();
  }
}

void sample() {
  if (gCount >= kMax) return;
  Row& r = gRows[gCount++];
  const float base = base::volts();
  r.minute = (millis() - gStarted) / 60000;
  r.baseMv = base > 0 ? static_cast<uint16_t>(base * 1000) : 0;
  r.stickMv = M5.Power.getBatteryVoltage();
  r.level = M5.Power.getBatteryLevel();
  r.baseOk = base > 0 ? 1 : 0;
  save();
}

}  // namespace

void start() {
  gCount = 0;
  gRunning = true;
  gStarted = millis();
  gNext = gStarted + kEveryMs;
  sample();
  console.printf("battery run-down started: base %.2f V, stick %.2f V %u%%\n", gRows[0].baseMv / 1000.0f,
                 gRows[0].stickMv / 1000.0f, gRows[0].level);
}

void loop(uint32_t now) {
  if (!gRunning || static_cast<int32_t>(now - gNext) < 0) return;
  gNext += kEveryMs;
  sample();
  const Row& r = gRows[gCount - 1];
  console.printf("batlog %u min: base %.2f V, stick %.2f V %u%%\n", r.minute, r.baseMv / 1000.0f,
                 r.stickMv / 1000.0f, r.level);
}

void finalRow() {
  if (gRunning) sample();
}

bool running() { return gRunning; }

void print() {
  // While running, gRows is what's saved. Otherwise it's free to load the saved log into (one buffer: the face's
  // second frame wants the internal memory).
  const Row* rows = gRows;
  size_t n = gCount;
  if (!gRunning) {
    Preferences p;
    n = 0;
    if (p.begin("batlog", true)) {
      n = min<size_t>(p.getBytesLength("rows") / sizeof(Row), kMax);
      p.getBytes("rows", gRows, n * sizeof(Row));
      p.end();
    }
  }
  console.printf("batlog rows=%u every=%lus%s\n", static_cast<unsigned>(n),
                 static_cast<unsigned long>(kEveryMs / 1000), gRunning ? " (running)" : "");
  for (size_t i = 0; i < n; ++i) {
    console.printf("%4u min  base %.2f V%s  stick %.2f V %u%%\n", rows[i].minute, rows[i].baseMv / 1000.0f,
                   rows[i].baseOk ? "" : " (no answer)", rows[i].stickMv / 1000.0f, rows[i].level);
  }
}

}  // namespace batlog
