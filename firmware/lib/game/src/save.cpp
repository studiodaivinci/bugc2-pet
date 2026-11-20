#include "save.h"

#include <string.h>

#ifdef ARDUINO
#include <Preferences.h>
#else
#include <stdio.h>
#endif

namespace game {

SaveData gSave;
MakerRecords gMaker;

namespace {

bool gSaveFrozen = false;  // the stored progress came from a newer build: never overwrite it
constexpr size_t kMaxBlob = 1024;

constexpr const char* kNamespace = "game";  // never touch any other namespace: the robot's Wi-Fi lives there

// A blob is stored with a one-byte checksum so a write cut off by a power loss reads back as "no data", not junk.
uint8_t checksum(const uint8_t* p, size_t n) {
  uint8_t s = 0x5A;
  for (size_t i = 0; i < n; ++i) s = static_cast<uint8_t>((s << 1 | s >> 7) ^ p[i]);
  return s;
}

// Both builds: readBlob() copies a stored blob over `dst`, which already holds the defaults. A shorter blob (older)
// fills the front; a longer one (newer) fills all of dst and the rest is ignored. False if nothing valid is stored.
bool acceptBlob(const uint8_t* buf, size_t got, void* dst, size_t n) {
  if (got < 2 || checksum(buf, got - 1) != buf[got - 1]) return false;
  memcpy(dst, buf, got - 1 < n ? got - 1 : n);
  return true;
}

#ifdef ARDUINO
bool readBlob(const char* key, void* dst, size_t n) {
  Preferences p;
  if (!p.begin(kNamespace, true)) return false;  // nothing saved yet
  uint8_t buf[kMaxBlob];
  const size_t len = p.getBytesLength(key);
  const size_t got = len >= 2 && len <= sizeof(buf) ? p.getBytes(key, buf, len) : 0;
  p.end();
  return got == len && acceptBlob(buf, got, dst, n);
}
void writeBlob(const char* key, const void* src, size_t n) {
  uint8_t buf[kMaxBlob];
  memcpy(buf, src, n);
  buf[n] = checksum(buf, n);
  Preferences p;
  if (!p.begin(kNamespace, false)) return;
  p.putBytes(key, buf, n + 1);
  p.end();
}
void removeKey(const char* key) {
  Preferences p;
  if (!p.begin(kNamespace, false)) return;
  p.remove(key);
  p.end();
}
#else
// PC build: one file per key next to the program.
bool readBlob(const char* key, void* dst, size_t n) {
  char path[64];
  snprintf(path, sizeof(path), "save_%s.bin", key);
  FILE* f = fopen(path, "rb");
  if (!f) return false;
  uint8_t buf[kMaxBlob];
  const size_t got = fread(buf, 1, sizeof(buf), f);
  fclose(f);
  return acceptBlob(buf, got, dst, n);
}
void writeBlob(const char* key, const void* src, size_t n) {
  uint8_t buf[kMaxBlob];
  memcpy(buf, src, n);
  buf[n] = checksum(buf, n);
  char path[64];
  snprintf(path, sizeof(path), "save_%s.bin", key);
  FILE* f = fopen(path, "wb");
  if (!f) return;
  fwrite(buf, 1, n + 1, f);
  fclose(f);
}
void removeKey(const char* key) {
  char path[64];
  snprintf(path, sizeof(path), "save_%s.bin", key);
  remove(path);
}
#endif

static_assert(sizeof(SaveData) < kMaxBlob && sizeof(MakerRecords) < kMaxBlob, "keep the saves small");

// Values that would index past an array are put right; a round that doesn't make sense is dropped (never the scores).
void sanitize(SaveData& s) {
  if (s.lastSeatCount > kMaxSeats) s.lastSeatCount = kMaxSeats;
  for (uint8_t& p : s.lastSeats)
    if (p >= kPlayerCount) p = Kid;
  if (s.aimMode > 1) s.aimMode = 0;
  RoundSave& r = s.round;
  bool ok = r.seatCount >= 1 && r.seatCount <= kMaxSeats && r.holeIndex < 6;
  for (int i = 0; ok && i < r.seatCount; ++i) ok = r.seats[i] < kPlayerCount;
  if (!ok) r = RoundSave();
}

}  // namespace

void loadSave() {
  gSave = SaveData();
  gSaveFrozen = false;
  SaveData s;
  if (readBlob("data", &s, sizeof(s)) && s.version >= 2) {  // 2 is the first layout that ever ran on the robot
    if (s.version > kSaveVersion) gSaveFrozen = true;
    else s.version = kSaveVersion;
    sanitize(s);
    gSave = s;
  }
  gMaker = MakerRecords();
  readBlob("maker", &gMaker, sizeof(gMaker));
}

void writeSave() {
  if (!gSaveFrozen) writeBlob("data", &gSave, sizeof(gSave));
}
void writeMaker() { writeBlob("maker", &gMaker, sizeof(gMaker)); }

void clearProgress() {
  const uint8_t aim = gSave.aimMode;
  gSave = SaveData();
  gSave.aimMode = aim;  // a setting, not progress
  gSaveFrozen = false;  // an explicit reset may replace even a newer build's save
  writeSave();
}

void clearMaker() {
  gMaker = MakerRecords();
  removeKey("maker");
}

}  // namespace game
