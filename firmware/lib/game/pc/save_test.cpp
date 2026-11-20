// Checks that saves are never thrown away: an old (version 2) blob migrates, a newer one is left untouched, and a
// damaged one reads as empty. Run from an empty folder: bash firmware/lib/game/pc/build.sh builds it as save_test.exe.
#include <stdio.h>
#include <string.h>

#include "../src/save.h"

using namespace game;

namespace {

int failures = 0;
void check(bool ok, const char* what) {
  printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++failures;
}

uint8_t checksum(const uint8_t* p, size_t n) {
  uint8_t s = 0x5A;
  for (size_t i = 0; i < n; ++i) s = static_cast<uint8_t>((s << 1 | s >> 7) ^ p[i]);
  return s;
}

void writeFile(const char* key, const uint8_t* payload, size_t n, bool damage = false) {
  char path[64];
  snprintf(path, sizeof(path), "save_%s.bin", key);
  FILE* f = fopen(path, "wb");
  fwrite(payload, 1, n, f);
  const uint8_t c = static_cast<uint8_t>(checksum(payload, n) ^ (damage ? 1 : 0));
  fwrite(&c, 1, 1, f);
  fclose(f);
}

long fileSize(const char* key) {
  char path[64];
  snprintf(path, sizeof(path), "save_%s.bin", key);
  FILE* f = fopen(path, "rb");
  if (!f) return -1;
  fseek(f, 0, SEEK_END);
  const long n = ftell(f);
  fclose(f);
  return n;
}

}  // namespace

int main() {
  // The version 2 layout: SaveData without `reserved`, exactly as the first gift build wrote it.
  const size_t v2Size = sizeof(SaveData) - sizeof(SaveData::reserved);
  check(v2Size == 144, "version 2 layout is 144 bytes");
  uint8_t v2[256] = {};
  {
    SaveData s;
    s.version = 2;
    s.best[Kid][0] = 3;
    s.best[Player2][2] = 4;
    s.holesPlayed = 7;
    s.aimMode = 1;
    s.round.active = 1;
    s.round.seatCount = 2;
    s.round.seats[1] = Player2;
    memcpy(v2, &s, v2Size);
  }
  writeFile("data", v2, v2Size);
  uint8_t maker[16] = {2, 3, 3};
  writeFile("maker", maker, sizeof(maker));  // the old 16-byte records
  loadSave();
  check(gSave.version == kSaveVersion, "old save migrates to the current version");
  check(gSave.best[Kid][0] == 3 && gSave.best[Player2][2] == 4, "old scores kept");
  check(gSave.holesPlayed == 7 && gSave.aimMode == 1, "old counters and settings kept");
  check(gSave.round.active == 1 && gSave.round.seats[1] == Player2, "old round kept");
  check(gMaker.best[0] == 2 && gMaker.best[1] == 3 && gMaker.best[2] == 3, "old Maker records kept");
  writeSave();
  check(fileSize("data") == static_cast<long>(sizeof(SaveData) + 1), "rewritten at the new size");
  loadSave();
  check(gSave.best[Kid][0] == 3 && gSave.version == kSaveVersion, "new save reads back");

  // A newer build's save, longer and with a higher version: read what we know, never write over it.
  uint8_t newer[sizeof(SaveData) + 20] = {};
  memcpy(newer, &gSave, sizeof(SaveData));
  newer[0] = kSaveVersion + 1;
  newer[sizeof(SaveData) + 5] = 0xAB;
  writeFile("data", newer, sizeof(newer));
  loadSave();
  check(gSave.best[Kid][0] == 3, "newer save: known fields read");
  gSave.best[Kid][0] = 1;
  writeSave();
  check(fileSize("data") == static_cast<long>(sizeof(newer) + 1), "newer save: left untouched");
  clearProgress();
  check(fileSize("data") == static_cast<long>(sizeof(SaveData) + 1), "an explicit reset may replace it");
  check(gSave.best[Kid][0] == 0, "reset clears progress");

  // A damaged blob reads as nothing saved.
  writeFile("data", v2, v2Size, true);
  loadSave();
  check(gSave.best[Kid][0] == 0 && gSave.version == kSaveVersion, "damaged save reads as defaults");

  // Out-of-range values are put right.
  {
    SaveData s;
    s.lastSeatCount = 9;
    s.lastSeats[0] = 77;
    s.round.active = 1;
    s.round.seatCount = 0;
    writeFile("data", reinterpret_cast<uint8_t*>(&s), sizeof(s));
  }
  loadSave();
  check(gSave.lastSeatCount == kMaxSeats && gSave.lastSeats[0] == Kid, "bad seats fixed");
  check(gSave.round.active == 0, "a round that makes no sense is dropped");

  remove("save_data.bin");
  remove("save_maker.bin");
  printf(failures ? "%d FAILED\n" : "all passed\n", failures);
  return failures ? 1 : 0;
}
