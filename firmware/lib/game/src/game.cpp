// Pop-Up Golf: screens, turns, animals, records. See docs/GAME.md for the design and the contract.
#include "game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets.h"
#include "course.h"
#include "gfx.h"
#include "pet_config.h"
#include "physics.h"
#include "save.h"
#include "sound.h"

#ifdef ARDUINO
#include <esp_heap_caps.h>
#endif

namespace game {

namespace {

using gfx::Align;
using gfx::rgb;

// --- Tokens (art/game/design/DESIGN.md) ----------------------------------------------------------------------------

constexpr uint16_t kInk = rgb(44, 36, 30);
constexpr uint16_t kInkSoft = rgb(110, 96, 82);
constexpr uint16_t kPaper = rgb(246, 238, 218);
constexpr uint16_t kPaperEdge = rgb(214, 200, 170);
constexpr uint16_t kRed = rgb(218, 48, 46);      // the flag; also "your turn / press the big button"
constexpr uint16_t kGold = rgb(240, 180, 40);
constexpr uint16_t kWhite = rgb(255, 255, 255);
constexpr uint16_t kBallEdge = rgb(40, 34, 32);

struct PlayerInfo {
  const char* name;
  uint16_t color;
};
// The kid gold, player 2 green, the Maker blue, player 4 coral, player 5 purple, Guest grey. Names: pet_config.h.
const PlayerInfo kPlayers[kPlayerCount] = {
    {PET_KID_NAME, rgb(240, 160, 30)}, {PET_PLAYER_2, rgb(70, 170, 60)},  {PET_MAKER_NAME, rgb(52, 110, 210)},
    {PET_PLAYER_4, rgb(236, 96, 128)}, {PET_PLAYER_5, rgb(132, 86, 206)}, {"Guest", rgb(130, 136, 142)},
};

constexpr float kDt = 0.05f;          // largest step taken at once; a paused robot resumes smoothly
constexpr float kChargePeriod = 2.2f;  // seconds for the power bar to go up and back down (once; it doesn't repeat)
constexpr float kMenuHold = 1.2f;      // holding the big button on the emptied bar this long opens the pause menu
constexpr int kMaxOverPar = 4;         // after par + 4 strokes the ball is picked up

// Tilt. Signs are set from the robot (docs/GAME.md, "Controls"); the game measures tilt from how the robot was being
// held when the screen opened, so any comfortable grip is neutral.
constexpr float kTiltXSign = -1.0f;
constexpr float kTiltYSign = 1.0f;

// Aim by turning (the default): hold the robot flat and turn it the way it turns on the floor, and the aim line swings
// the way its front moves. The gyro reads counter-clockwise from above as positive (docs/HARDWARE.md) and the aim
// angle is counter-clockwise on screen, so they add.
constexpr uint8_t kAimTurn = 0, kAimTilt = 1;  // SaveData::aimMode
constexpr float kTurnSign = 1.0f;
constexpr float kTurnGain = 1.5f;      // degrees of aim per degree the robot turns
constexpr float kTurnDeadband = 2.0f;  // deg/s: slower than this is gyro drift, not a hand

// --- State ---------------------------------------------------------------------------------------------------------

Host gHost;
gfx::Frame F;
uint16_t* gPage = nullptr;  // the current hole's page
uint16_t* gPrev = nullptr;  // the last frame, for page turns
uint8_t* gSurf = nullptr;   // the current hole's surface map
Sound gSound;
uint32_t gMs = 0;
float gTime = 0;            // seconds since boot of the game, for animation
Input gIn;
float gLastYaw = 0, gYawStep = 0;  // how far the robot turned since the last frame (degrees)
bool gQuit = false;

enum class Screen : uint8_t { Title, Resume, Players, Course, Play, HoleCard, RoundEnd, Pause };
Screen gScreen = Screen::Title;
float gScreenT = 0;
int gCursor = 0;
float gBaseAx = 0, gBaseAy = 0;
int8_t gNavLatchX = 0, gNavLatchY = 0;
float gNavTiltT = 0;
float gHoldB = 0;
float gMenuHold = 0;       // how long the big button has been held on an emptied power bar
bool gPauseAsked = false;  // the round asked for the pause menu (the big button held on an emptied bar)
char gToast[32] = "";
float gToastT = 0;

enum class Phase : uint8_t { PageTurn, PopUp, Turn, Aim, Charge, Roll, Splash, Sunk, PickedUp };

struct AnimalState {
  float x = 0, y = 0, px = 0, py = 0;  // now and last frame
  float vx = 0, vy = 0;
  int seg = 0;          // walk: segment index; hop/pop: point index
  float along = 0;      // walk: distance along the segment
  int dir = 1;
  float timer = 0;
  float hopT = -1;      // hop: 0..1 while in the air
  int hopFrom = 0, hopTo = 0;
  bool air = false;
  bool hidden = false;
  float popScale = 0;   // 0..1, the pop-up from the page
  float squash = 0;     // hit reaction
  bool flip = false;
};

struct PlayState {
  uint8_t holes[6];
  uint8_t holeCount = 0;
  uint8_t holeIdx = 0;
  const Hole* hole = nullptr;
  uint8_t holeId = 0;
  uint8_t seat = 0;
  uint8_t strokes = 0;
  Phase phase = Phase::PageTurn;
  float phaseT = 0;
  Ball ball;
  float lastX = 0, lastY = 0;
  float aim = 90, power = 0;
  int powerDir = 1;
  float baseAx = 0, yaw0 = 0, aim0 = 90;
  AnimalState animals[4];
  int animalCount = 0;
  char banner[28] = "";
  char sub[40] = "";
  uint16_t bannerColor = kRed;
  float shake = 0;
  float splashX = 0, splashY = 0;
  uint8_t flags[kMaxSeats] = {};  // per seat on this hole: 1 personal best, 2 hole record, 4 beat the Maker
};
PlayState P;

// --- Small helpers -------------------------------------------------------------------------------------------------

void* bigAlloc(size_t n) {
#ifdef ARDUINO
  void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = malloc(n);
  return p;
#else
  return malloc(n);
#endif
}

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
float easeOut(float t) {
  t = clampf(t, 0, 1);
  return 1 - (1 - t) * (1 - t) * (1 - t);
}
// Overshoot for pop-ups: 0 -> 1.15 -> 1.
float popEase(float t) {
  t = clampf(t, 0, 1);
  const float c = 2.2f;
  return 1 + (c + 1) * powf(t - 1, 3) + c * powf(t - 1, 2);
}

int daysSinceHatch() { return gHost.daysSinceHatch ? gHost.daysSinceHatch() : -1; }
// No date unlocks: every hole in this build is open, and new holes come with new builds. daysSinceHatch() only tells "before hatching" (< 0) from after.
bool holeOpen(int) { return true; }
bool beforeHatching() { return daysSinceHatch() < 0; }

void emit(Event e, uint8_t player) {
  if (gHost.event) gHost.event(e, player);
}

// The Maker's record on a hole: what they set before hatching, else a built-in one of par + 1, so every hole has a
// record to tie or beat from the first day.
uint8_t makerRecord(uint8_t hole) {
  return gMaker.best[hole] ? gMaker.best[hole] : static_cast<uint8_t>(kHoles[hole].par + 1);
}

void toast(const char* s) {
  snprintf(gToast, sizeof(gToast), "%s", s);
  gToastT = 1.6f;
}

void enter(Screen s) {
  gScreen = s;
  gScreenT = 0;
  if (s == Screen::Title || s == Screen::Pause) gCursor = 0;  // "Play" and "Keep playing" come first
  gBaseAx = gIn.ax;
  gBaseAy = gIn.ay;
  gNavLatchX = gNavLatchY = 0;
}

// Tilt as a menu stick: one step per deliberate tilt, so every menu beep is one the player meant.
// - The tilt has to last 0.15 s: pressing A jolts the robot for a moment.
// - No repeats: after a step, the robot goes back to level before the next one.
// - Nothing moves while a button is down.
// - Level follows the hands slowly while they're still, so a new way of holding it becomes the new level, and
//   quickly for the first 0.4 s of a screen, while the press that opened it may still be rocking the robot.
void readNav(int& dx, int& dy) {
  dx = dy = 0;
  if (gScreenT < 0.4f) {
    const float k = fminf(1.0f, gIn.dt / 0.1f);
    gBaseAx += (gIn.ax - gBaseAx) * k;
    gBaseAy += (gIn.ay - gBaseAy) * k;
    gNavLatchX = gNavLatchY = 0;
    gNavTiltT = 0;
    return;
  }
  const float tx = kTiltXSign * (gIn.ax - gBaseAx);
  const float ty = kTiltYSign * (gIn.ay - gBaseAy);
  if (gIn.a.held || gIn.b.held) {
    gNavTiltT = 0;
    return;
  }
  // A clear tip, about 27 degrees, held 0.15 s (17 degrees changed too easily and too often).
  if (fabsf(tx) < 0.2f && fabsf(ty) < 0.2f) {
    gNavLatchX = gNavLatchY = 0;
    gNavTiltT = 0;
    const float k = fminf(1.0f, gIn.dt / 1.5f);
    gBaseAx += (gIn.ax - gBaseAx) * k;
    gBaseAy += (gIn.ay - gBaseAy) * k;
    return;
  }
  if (gNavLatchX || gNavLatchY) return;  // already stepped for this tilt
  int sx = tx > 0.45f ? 1 : (tx < -0.45f ? -1 : 0);
  int sy = ty > 0.45f ? 1 : (ty < -0.45f ? -1 : 0);
  if (sx && sy) (fabsf(tx) > fabsf(ty) ? sy : sx) = 0;  // the stronger direction wins
  if (!sx && !sy) {
    gNavTiltT = 0;
    return;
  }
  gNavTiltT += gIn.dt;  // how long this tilt has lasted
  if (gNavTiltT < 0.15f) return;
  gNavLatchX = static_cast<int8_t>(sx);
  gNavLatchY = static_cast<int8_t>(sy);
  dx = sx;
  dy = sy;
}

// A B press counts once the button has been down on two frames in a row. B is on G39, which reads false presses for
// an instant while Wi-Fi power saving is on (Espressif errata 3.11), and one of those must not quit the game or go
// back a screen. Real presses of B last far longer than two frames.
struct ButtonFilter {
  uint8_t frames = 0;
  bool live = false;
};
ButtonFilter gFilterB;

void filterButton(Button& b, ButtonFilter& f) {
  const bool down = b.held || b.pressed;
  f.frames = down ? static_cast<uint8_t>(f.frames < 255 ? f.frames + 1 : 255) : 0;
  const bool was = f.live;
  f.live = f.frames >= 2;
  b.pressed = f.live && !was;
  b.released = was && !f.live;
  b.held = f.live;
}

// --- Drawing helpers -----------------------------------------------------------------------------------------------

void roundRect(int x, int y, int w, int h, int r, uint16_t c) {
  F.fillRect(x + r, y, w - 2 * r, h, c);
  F.fillRect(x, y + r, r, h - 2 * r, c);
  F.fillRect(x + w - r, y + r, r, h - 2 * r, c);
  F.disc(x + r - 0.5f, y + r - 0.5f, r, c);
  F.disc(x + w - r - 0.5f, y + r - 0.5f, r, c);
  F.disc(x + r - 0.5f, y + h - r - 0.5f, r, c);
  F.disc(x + w - r - 0.5f, y + h - r - 0.5f, r, c);
}

// A paper card with its drop shadow (the rhyme: every layer casts a soft shadow).
void card(int x, int y, int w, int h, uint16_t c = kPaper, int r = 5) {
  for (int k = 3; k >= 1; --k) {
    F.blendRect(x + k + 1, y + k + 1, w, h, 0, static_cast<uint8_t>(26));
  }
  roundRect(x, y, w, h, r, c);
}

void ball(float x, float y, float scale = 1) {
  const float r = kTuning.ballR * scale;
  F.shade(x + 1.6f, y + 2.0f, r + 1.2f, r * 0.8f + 0.8f, 120);
  F.disc(x, y, r + 0.9f, kBallEdge);
  F.disc(x, y, r, kWhite);
  F.disc(x - r * 0.35f, y - r * 0.4f, r * 0.35f, rgb(255, 255, 255));
  F.disc(x + r * 0.25f, y + r * 0.3f, r * 0.5f, rgb(222, 222, 228), 120);
}

// A punched paper dot (the rhyme from the sand traps).
void dot(float x, float y, float r, uint16_t c, uint8_t a = 255) {
  F.disc(x + 0.8f, y + 1.0f, r + 0.5f, 0, static_cast<uint8_t>(a / 3));
  F.disc(x, y, r + 0.8f, kBallEdge, a);
  F.disc(x, y, r, c, a);
}

// The red "press the big button" prompt. The buttons aren't lettered, so it shows a little drawing of the big
// button (a white rounded block, like the one under the screen) instead of a letter.
void pressA(int cx, int cy, const char* label) {
  const int w = gfx::Frame::textWidth(kFontSmall, label) + 28;
  const int bob = static_cast<int>(roundf(sinf(gTime * 5) * 1.0f));
  card(cx - w / 2, cy - 9 + bob, w, 18, kRed, 9);
  roundRect(cx - w / 2 + 5, cy - 4 + bob, 13, 9, 3, kWhite);
  F.fillRect(cx - w / 2 + 8, cy + 2 + bob, 7, 1, rgb(240, 196, 190));
  F.text(kFontSmall, cx - w / 2 + 21, cy + 4 + bob, label, kWhite);
}

// A one- or two-line hint at the bottom of a menu, in white with an ink halo.
void hint(const char* line1, const char* line2 = nullptr) {
  const uint16_t c = rgb(250, 246, 236);
  if (line2) {
    F.text(kFontSmall, 68, 222, line1, c, Align::Center, 1, kInk);
    F.text(kFontSmall, 68, 234, line2, c, Align::Center, 1, kInk);
  } else {
    F.text(kFontSmall, 68, 230, line1, c, Align::Center, 1, kInk);
  }
}

void playerTab(int x, int y, int w, uint8_t player, bool big = false) {
  const int h = big ? 22 : 16;
  card(x, y, w, h, kPaper, 4);
  F.fillRect(x, y + 2, 5, h - 4, kPlayers[player].color);
  F.disc(x + 3.5f, y + 3.5f, 3, kPlayers[player].color);
  F.disc(x + 3.5f, y + h - 4.5f, 3, kPlayers[player].color);
  F.text(big ? kFontBody : kFontSmall, x + 9, y + (big ? 16 : 12), kPlayers[player].name, kInk);
}

void dimFrame(uint8_t amount) { F.blendRect(0, 0, gfx::W, gfx::H, 0, amount); }

// The menus' backdrop: the first hole's page, dimmed. Drawing it takes about 20 ms on the robot (unpack, then a
// whole-screen blend), so it is drawn once and copied after that (about 8 ms). It lives in gPrev, which only page
// turns use: a page turn overwrites it and clears gMenuBgDim.
int gMenuBgDim = -1;  // reset by beginRaw()
void menuBackground(uint8_t dim) {
  if (gMenuBgDim == dim) {
    F.copyFrom(gPrev);
    return;
  }
  gfx::unpack(kHoleArt[0], F.pixels());
  dimFrame(dim);
  memcpy(gPrev, F.pixels(), gfx::W * gfx::H * 2);
  gMenuBgDim = dim;
}

// Text that shrinks to a smaller face when it would not fit.
void fitText(int x, int y, int maxW, const char* s, uint16_t c, Align a, const gfx::Font& big,
             const gfx::Font& small, int outline = 0, uint16_t oc = 0) {
  const gfx::Font& f = gfx::Frame::textWidth(big, s) <= maxW ? big : small;
  F.text(f, x, y, s, c, a, outline, oc);
}

// One choice in a menu (title, carry on, pause). The one the big button will pick wears the red big-button prompt,
// with the little drawing of the big button; the others are paper. Tilting moves the red from one to the next.
void choice(int y, int h, const char* label, bool cur, bool quiet = false) {
  const int x = 18, w = 99;
  if (!cur) {
    card(x, y, w, h, quiet ? kPaperEdge : kPaper, 8);
    fitText(x + w / 2, y + h / 2 + 5, w - 8, label, kInk, Align::Center, kFontBody, kFontSmall);
    return;
  }
  const int bob = static_cast<int>(roundf(sinf(gTime * 5) * 1.0f));
  card(x, y + bob, w, h, kRed, 8);
  roundRect(x + 6, y + h / 2 - 4 + bob, 13, 9, 3, kWhite);
  F.fillRect(x + 9, y + h / 2 + 2 + bob, 7, 1, rgb(240, 196, 190));
  fitText(x + w / 2 + 10, y + h / 2 + 5 + bob, w - 28, label, kWhite, Align::Center, kFontBody, kFontSmall);
}

// Shown while the big button is held on an emptied power bar: an ink capsule that fills red, then the pause menu.
// It sits just above the ball (below it near the top), never over it: tees are by the bottom rail.
void drawMenuHold(float f, float ballY) {
  const char* label = "keep holding: menu";
  const int w = gfx::Frame::textWidth(kFontSmall, label) + 16, h = 18;
  const int x = 68 - w / 2;
  const int y = ballY > 70 ? static_cast<int>(ballY) - 32 : static_cast<int>(ballY) + 14;
  card(x, y, w, h, kInk, 9);
  const int fw = static_cast<int>(clampf(f, 0, 1) * w);
  if (fw > 0) roundRect(x, y, fw > h ? fw : h, h, 9, kRed);
  F.text(kFontSmall, 68, y + 13, label, kWhite, Align::Center);
}

// Draws multi-line text (lines split by '\n'), returns the y after the last line.
int lines(const gfx::Font& f, int x, int y, const char* s, uint16_t c, Align a, int lineH) {
  char buf[64];
  while (*s) {
    const char* e = strchr(s, '\n');
    const size_t n = e ? static_cast<size_t>(e - s) : strlen(s);
    const size_t m = n < sizeof(buf) - 1 ? n : sizeof(buf) - 1;
    memcpy(buf, s, m);
    buf[m] = 0;
    F.text(f, x, y, buf, c, a);
    y += lineH;
    s += n;
    if (*s == '\n') ++s;
  }
  return y;
}

// --- The round -----------------------------------------------------------------------------------------------------

int seatCount() { return gSave.round.seatCount; }
uint8_t seatPlayer(int s) { return gSave.round.seats[s]; }

void layoutAnimals() {
  const Hole& h = *P.hole;
  P.animalCount = h.animalCount < 4 ? h.animalCount : 4;
  for (int i = 0; i < P.animalCount; ++i) {
    const AnimalDef& d = h.animals[i];
    AnimalState& a = P.animals[i];
    a = AnimalState();
    a.x = a.px = d.points[0];
    a.y = a.py = d.points[1];
    a.timer = 0.6f + 0.4f * i;
  }
}

void startHole(bool pageTurn) {
  P.holeId = P.holes[P.holeIdx];
  P.hole = &kHoles[P.holeId];
  if (pageTurn) {
    memcpy(gPrev, F.pixels(), gfx::W * gfx::H * 2);
    gMenuBgDim = -1;
  }
  gfx::unpack(*P.hole->art, gPage);
  decodeSurface(*P.hole, gSurf);
  layoutAnimals();
  P.seat = 0;
  for (int s = 0; s < kMaxSeats; ++s) P.flags[s] = 0;
  P.phase = pageTurn ? Phase::PageTurn : Phase::PopUp;
  P.phaseT = 0;
  // Resume mid-hole after a power cut: skip seats that already finished this hole.
  while (P.seat < seatCount() && gSave.round.scores[P.seat][P.holeIdx] != 0) ++P.seat;
}

void startTurn() {
  P.strokes = 0;
  P.ball.place(P.hole->teeX, P.hole->teeY);
  P.lastX = P.hole->teeX;
  P.lastY = P.hole->teeY;
  const float dx = P.hole->cupX - P.hole->teeX, dy = P.hole->teeY - P.hole->cupY;
  P.aim = atan2f(dy, dx) * 57.29578f;  // start aimed straight at the cup
  P.phase = Phase::Turn;
  P.phaseT = 0;
  gSound.play(Sfx::Turn);
}

void beginRound(uint8_t habitat) {
  RoundSave& r = gSave.round;
  r.active = 1;
  r.habitat = habitat;
  r.holeIndex = 0;
  memset(r.scores, 0, sizeof(r.scores));
  P.holeCount = 0;
  for (int i = 0; i < kHoleCount && P.holeCount < 6; ++i)
    if (kHoles[i].habitat == habitat && holeOpen(i)) P.holes[P.holeCount++] = static_cast<uint8_t>(i);
  P.holeIdx = 0;
  writeSave();
  enter(Screen::Play);
  startHole(true);
}

void resumeRound() {
  RoundSave& r = gSave.round;
  P.holeCount = 0;
  for (int i = 0; i < kHoleCount && P.holeCount < 6; ++i)
    if (kHoles[i].habitat == r.habitat && holeOpen(i)) P.holes[P.holeCount++] = static_cast<uint8_t>(i);
  if (P.holeCount == 0 || r.holeIndex >= P.holeCount) {
    r.active = 0;
    writeSave();
    enter(Screen::Title);
    return;
  }
  P.holeIdx = r.holeIndex;
  enter(Screen::Play);
  startHole(true);
}

void scoreWords(uint8_t strokes, uint8_t par, char* out, size_t n) {
  if (strokes == 1) snprintf(out, n, "Hole in one!");
  else if (strokes + 2 <= par) snprintf(out, n, "Eagle!");
  else if (strokes + 1 == par) snprintf(out, n, "Birdie!");
  else if (strokes == par) snprintf(out, n, "Par!");
  else snprintf(out, n, "%d strokes", strokes);
}

// A seat finished the hole with `strokes`. Records, events, save.
void finishSeat(uint8_t strokes, bool pickedUp) {
  const uint8_t pl = seatPlayer(P.seat);
  const uint8_t id = P.holeId;
  gSave.round.scores[P.seat][P.holeIdx] = strokes;
  uint8_t flags = 0;
  if (!pickedUp) {
    // Before hatching, whatever the Maker scores becomes their record (the latest, so they can retake one). After
    // hatching their record is fixed, and they play like anyone else.
    if (pl == Maker && beforeHatching()) {
      gMaker.best[id] = strokes;
      writeMaker();
    }
    // Best by anyone else before this putt (the Maker's record included).
    uint8_t others = pl == Maker ? 0 : makerRecord(id);
    for (uint8_t p = 0; p < kPlayerCount; ++p) {
      if (p == pl || p == Maker) continue;
      const uint8_t b = gSave.best[p][id];
      if (b && (!others || b < others)) others = b;
    }
    const uint8_t mine = gSave.best[pl][id];
    if (!mine || strokes < mine) {
      flags |= 1;
      gSave.best[pl][id] = strokes;
    }
    if ((!others || strokes < others) && (!mine || strokes < mine) && others) {
      flags |= 2;
      emit(Event::NewRecord, pl);
    }
    if (pl != Maker && strokes < makerRecord(id)) {
      flags |= 4;
      ++gSave.beatMaker;
      emit(Event::BeatMaker, pl);
    } else if (pl != Maker && strokes == makerRecord(id)) {
      flags |= 8;
      emit(Event::TiedMaker, pl);
    }
    if (strokes == 1) {
      ++gSave.aces;
      emit(Event::HoleInOne, pl);
    } else if (strokes < P.hole->par) {
      emit(Event::UnderPar, pl);
    }
  }
  P.flags[P.seat] = flags;
  ++gSave.holesPlayed;
  writeSave();
}

void nextAfterSeat() {
  ++P.seat;
  while (P.seat < seatCount() && gSave.round.scores[P.seat][P.holeIdx] != 0) ++P.seat;
  if (P.seat < seatCount()) {
    startTurn();
  } else {
    gSave.round.holeIndex = static_cast<uint8_t>(P.holeIdx + 1);
    writeSave();
    enter(Screen::HoleCard);
  }
}

// --- Animals -------------------------------------------------------------------------------------------------------

bool ballRestsNear(float x, float y, float r) {
  if (P.ball.moving || P.phase == Phase::Sunk) return false;
  const float dx = P.ball.x - x, dy = P.ball.y - y;
  return dx * dx + dy * dy < (r + kTuning.ballR + 1.5f) * (r + kTuning.ballR + 1.5f);
}

void updateAnimals(float dt) {
  const Hole& h = *P.hole;
  for (int i = 0; i < P.animalCount; ++i) {
    const AnimalDef& d = h.animals[i];
    AnimalState& a = P.animals[i];
    a.px = a.x;
    a.py = a.y;
    if (a.squash > 0) a.squash -= dt;
    const int n = d.pointCount;
    auto pt = [&](int k, int c) { return static_cast<float>(d.points[k * 2 + c]); };
    switch (d.motion) {
      case Motion::Walk: {
        if (n < 2) break;
        if (a.timer > 0) {  // a little pause at each end
          a.timer -= dt;
          break;
        }
        const int s0 = a.seg, s1 = a.seg + 1;
        const float x0 = pt(s0, 0), y0 = pt(s0, 1), x1 = pt(s1, 0), y1 = pt(s1, 1);
        const float len = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
        float along = a.along + a.dir * d.speed * dt;
        const float t = len > 0 ? clampf(along / len, 0, 1) : 1;
        const float nx = x0 + (x1 - x0) * t, ny = y0 + (y1 - y0) * t;
        if (ballRestsNear(nx, ny, d.radius) && a.squash <= 0) {
          a.dir = -a.dir;  // polite: turn round rather than shove a resting ball
          break;
        }
        a.x = nx;
        a.y = ny;
        a.along = along;
        if (along > len) {
          if (s1 < n - 1) {
            a.seg = s1;
            a.along = 0;
          } else {
            a.dir = -1;
            a.along = len;
            a.timer = 0.7f;
          }
        } else if (along < 0) {
          if (s0 > 0) {
            a.seg = s0 - 1;
            const float px0 = pt(a.seg, 0), py0 = pt(a.seg, 1);
            a.along = sqrtf((x0 - px0) * (x0 - px0) + (y0 - py0) * (y0 - py0));
          } else {
            a.dir = 1;
            a.along = 0;
            a.timer = 0.7f;
          }
        }
        break;
      }
      case Motion::Hop: {
        if (n < 2) break;
        if (a.hopT < 0) {
          a.timer -= dt;
          if (a.timer <= 0) {
            int to = a.seg + a.dir;
            if (to < 0 || to >= n) {
              a.dir = -a.dir;
              to = a.seg + a.dir;
            }
            if (ballRestsNear(pt(to, 0), pt(to, 1), d.radius)) {
              a.timer = 0.8f;  // wait for the ball to move on
              break;
            }
            a.hopFrom = a.seg;
            a.hopTo = to;
            a.hopT = 0;
          }
        } else {
          a.hopT += dt / 0.55f;
          const float t = clampf(a.hopT, 0, 1);
          a.x = pt(a.hopFrom, 0) + (pt(a.hopTo, 0) - pt(a.hopFrom, 0)) * t;
          a.y = pt(a.hopFrom, 1) + (pt(a.hopTo, 1) - pt(a.hopFrom, 1)) * t;
          a.air = t > 0.12f && t < 0.88f;
          if (a.hopT >= 1) {
            a.hopT = -1;
            a.air = false;
            a.seg = a.hopTo;
            a.squash = 0.18f;
            const float base = 10.0f / (d.speed > 0 ? d.speed : 5);
            a.timer = base * (0.7f + 0.6f * static_cast<float>(rand() % 100) / 100.0f);
          }
        }
        break;
      }
      case Motion::Pop: {
        a.timer -= dt;
        if (a.timer <= 0) {
          if (a.hidden) {
            a.seg = (a.seg + 1) % (n > 0 ? n : 1);
            a.x = pt(a.seg, 0);
            a.y = pt(a.seg, 1);
            a.hidden = false;
            a.timer = 2.6f;
          } else {
            a.hidden = true;
            a.timer = 1.1f;
          }
        }
        break;
      }
      case Motion::Sleep:
        break;
    }
    if (a.x != a.px) a.flip = a.x < a.px;
    a.vx = dt > 0 ? (a.x - a.px) / dt : 0;
    a.vy = dt > 0 ? (a.y - a.py) / dt : 0;
  }
}

void drawAnimal(int i, bool shadowOnly) {
  const AnimalDef& d = P.hole->animals[i];
  const AnimalState& a = P.animals[i];
  const gfx::Image& img = kAnimalArt[d.kind];
  float lift = 0;
  if (a.hopT >= 0) {
    const float t = clampf(a.hopT, 0, 1);
    lift = 4 * t * (1 - t) * 11;  // parabola, 11 px high
  }
  float sx = 1, sy = 1;
  if (a.squash > 0) {
    const float k = sinf(a.squash / 0.3f * 3.14159f) * 0.25f;
    sx += k;
    sy -= k;
  }
  if (d.motion == Motion::Sleep) sy *= 1 + 0.04f * sinf(gTime * 2.2f);
  if (d.motion == Motion::Walk && fabsf(a.vx) + fabsf(a.vy) > 1) lift += fabsf(sinf(gTime * 9 + i)) * 1.5f;
  float pop = a.popScale;
  if (d.motion == Motion::Pop) {
    // Ducking in and out of its hole.
    const float t = a.hidden ? 0 : clampf((2.6f - a.timer) / 0.25f, 0, 1) * clampf(a.timer / 0.25f, 0, 1);
    pop *= popEase(t);
  }
  if (shadowOnly) {
    if (pop > 0.05f) F.shade(a.x + 1.5f, a.y + img.h * 0.35f, img.w * 0.42f * pop, 3.0f * pop, 110);
    return;
  }
  if (pop <= 0.02f) return;
  F.blitScaled(img, a.x, a.y + img.h * 0.45f - lift, sx, sy * pop, 0.5f, 1.0f, 255, a.flip);
}

// --- Screens: Play -------------------------------------------------------------------------------------------------

void drawHud() {
  const uint8_t pl = seatPlayer(P.seat);
  playerTab(2, 2, 66, pl);
  // Strokes on the right: the number big, par small.
  card(93, 2, 40, 21, kPaper, 4);
  char s[8];
  snprintf(s, sizeof(s), "%d", P.strokes);
  F.text(kFontTitle, 105, 20, s, kInk, Align::Center);
  F.fillRect(117, 5, 1, 15, kPaperEdge);
  F.text(kFontSmall, 125, 11, "par", kInkSoft, Align::Center);
  snprintf(s, sizeof(s), "%d", P.hole->par);
  F.text(kFontSmall, 125, 20, s, kInk, Align::Center);
}

// The aim line: punched dots along the path the ball starts on, traced off the walls so they stay on the green.
// Where the path meets a wall, a bump mark in the player's colour sits on the wall's face (so aiming into a wall
// close by still shows), and the dots carry on along the bounce, smaller and fainter. Up to two bounces.
void drawAim(bool charging) {
  const float rad = P.aim * 0.017453293f;
  float dx = cosf(rad), dy = -sinf(rad);
  const uint16_t col = kPlayers[seatPlayer(P.seat)].color;
  const int count = charging ? 3 + static_cast<int>(P.power * 9) : 5;
  const float gap = charging ? 5.0f + P.power * 2.5f : 6.0f;

  // Trace the path first, then draw.
  struct Leg {
    float x, y, dx, dy, len;
  } legs[3];
  struct Bump {
    float x, y, tx, ty;  // the wall face point and the wall's direction
  } bumps[2];
  int legCount = 0, bumpCount = 0;
  float x = P.ball.x, y = P.ball.y, left = 9 + (count - 1) * gap;
  while (legCount < 3 && left > 0) {
    float nx = 0, ny = 0;
    const float t = castBall(*P.hole, x, y, dx, dy, left, nx, ny);
    legs[legCount++] = {x, y, dx, dy, t};
    x += dx * t;
    y += dy * t;
    left -= t;
    if (left <= 0 || bumpCount == 2) break;
    bumps[bumpCount++] = {x - nx * kTuning.ballR, y - ny * kTuning.ballR, -ny, nx};
    const float dn = dx * nx + dy * ny;
    dx -= 2 * dn * nx;
    dy -= 2 * dn * ny;
  }
  float dotX[12], dotY[12];
  int dotLeg[12];
  float lowest = 0;  // the lowest point the aim reaches on screen
  for (int k = 0; k < count; ++k) {
    float s = 9 + k * gap;
    int l = 0;
    while (l < legCount - 1 && s > legs[l].len) s -= legs[l++].len;
    dotX[k] = legs[l].x + legs[l].dx * s;
    dotY[k] = legs[l].y + legs[l].dy * s;
    dotLeg[k] = l;
    lowest = fmaxf(lowest, dotY[k]);
  }
  for (int b = 0; b < bumpCount; ++b) lowest = fmaxf(lowest, bumps[b].y);

  // The hint, unless the aim needs that strip (tees sit by the bottom rail).
  if (lowest < 224) {
    const char* tip = charging ? (P.powerDir < 0 && P.power <= 0 ? "let go: no putt" : "let go to putt!")
                      : gSave.aimMode == kAimTurn ? "turn \x7f hold big button" : "tilt \x7f hold big button";
    F.text(kFontSmall, 68, 236, tip, rgb(250, 246, 236), Align::Center, 1, kInk);
  }
  // Bumps: a short bar on the wall's face where the ball's edge meets it.
  for (int b = 0; b < bumpCount; ++b) {
    const Bump& m = bumps[b];
    F.line(m.x - m.tx * 4, m.y - m.ty * 4, m.x + m.tx * 4, m.y + m.ty * 4, 3.4f, kBallEdge, 230);
    F.line(m.x - m.tx * 4, m.y - m.ty * 4, m.x + m.tx * 4, m.y + m.ty * 4, 1.8f, col);
  }
  for (int k = 0; k < count; ++k) {
    const uint8_t fade = charging ? 255 : static_cast<uint8_t>(255 - k * 30);
    if (dotLeg[k] == 0) {
      dot(dotX[k], dotY[k], k == count - 1 && charging ? 2.2f : 1.5f, col, fade);
    } else {
      const float bx = dotX[k] - P.ball.x, by = dotY[k] - P.ball.y;
      if (bx * bx + by * by < 36) continue;  // a bounce straight back would cover the ball
      dot(dotX[k], dotY[k], 1.2f, col, static_cast<uint8_t>(fade * 0.75f));
    }
  }
  if (charging) {
    // The power strip on the right edge: a paper slot that fills.
    const int x = 124, y0 = 196, hgt = 140;
    card(x - 2, y0 - hgt - 2, 10, hgt + 4, kPaper, 3);
    const int fill = static_cast<int>(P.power * hgt);
    const uint16_t c = P.power < 0.5f ? rgb(70, 180, 70) : (P.power < 0.8f ? kGold : kRed);
    F.fillRect(x, y0 - fill, 6, fill, c);
  }
}

void drawPlayScene() {
  F.copyFrom(gPage);
  for (int i = 0; i < P.animalCount; ++i) drawAnimal(i, true);
  // Animals and ball, back to front.
  bool ballDrawn = false;
  const bool showBall = P.phase != Phase::PageTurn && P.phase != Phase::PopUp &&
                        !(P.phase == Phase::Sunk && P.phaseT > 0.35f) && P.phase != Phase::Splash;
  int order[4];
  for (int i = 0; i < P.animalCount; ++i) order[i] = i;
  for (int i = 1; i < P.animalCount; ++i)
    for (int j = i; j > 0 && P.animals[order[j]].y < P.animals[order[j - 1]].y; --j) {
      const int t = order[j];
      order[j] = order[j - 1];
      order[j - 1] = t;
    }
  for (int k = 0; k < P.animalCount; ++k) {
    const AnimalState& a = P.animals[order[k]];
    if (showBall && !ballDrawn && P.ball.y < a.y) {
      ballDrawn = true;
      ball(P.ball.x, P.ball.y);
    }
    drawAnimal(order[k], false);
  }
  if (showBall && !ballDrawn) {
    float s = 1;
    if (P.phase == Phase::Sunk) s = 1 - P.phaseT / 0.35f;
    ball(P.ball.x, P.ball.y, s);
  }
  if (P.phase == Phase::Splash) {
    const float t = P.phaseT;
    for (int k = 0; k < 3; ++k) {
      const float r = 3 + (t * 22 - k * 4);
      if (r > 0) F.line(P.splashX - r, P.splashY, P.splashX + r, P.splashY, 1.2f, kWhite,
                        static_cast<uint8_t>(clampf(1 - t, 0, 1) * 200));
      if (r > 0)
        for (int j = 0; j < 16; ++j) {
          const float ang = j * 0.3927f;
          F.disc(P.splashX + cosf(ang) * r, P.splashY + sinf(ang) * r * 0.6f, 0.9f, kWhite,
                 static_cast<uint8_t>(clampf(1 - t, 0, 1) * 220));
        }
    }
  }
}

void drawBanner() {
  if (!P.banner[0]) return;
  const float t = P.phaseT;
  const float s = popEase(clampf(t / 0.35f, 0, 1));
  const int w = 124, h = P.sub[0] ? 46 : 34;
  const int y = 92 - h / 2;
  if (s < 0.98f) {
    // Grow from the centre line.
    const int hh = static_cast<int>(h * s);
    card(6, 92 - hh / 2, w, hh > 4 ? hh : 4, kPaper, 6);
    return;
  }
  card(6, y, w, h, kPaper, 6);
  fitText(68, y + 25, w - 8, P.banner, P.bannerColor, Align::Center, kFontTitle, kFontBody);
  if (P.sub[0]) F.text(kFontSmall, 68, y + 39, P.sub, kInkSoft, Align::Center);
}

void setBanner(const char* main, const char* sub, uint16_t color) {
  snprintf(P.banner, sizeof(P.banner), "%s", main);
  snprintf(P.sub, sizeof(P.sub), "%s", sub ? sub : "");
  P.bannerColor = color;
}

void putt() {
  P.lastX = P.ball.x;
  P.lastY = P.ball.y;
  ++P.strokes;
  P.ball.putt(P.aim, P.power);
  gSound.play(Sfx::Putt);
  P.phase = Phase::Roll;
  P.phaseT = 0;
}

void updatePlay(float dt) {
  P.phaseT += dt;
  if (P.phase != Phase::PageTurn) updateAnimals(dt);
  const uint8_t pl = seatPlayer(P.seat);

  switch (P.phase) {
    case Phase::PageTurn:
      if (P.phaseT >= 0.55f) {
        P.phase = Phase::PopUp;
        P.phaseT = 0;
        gSound.play(Sfx::PopUp);
      }
      break;
    case Phase::PopUp:
      for (int i = 0; i < P.animalCount; ++i) P.animals[i].popScale = popEase((P.phaseT - i * 0.12f) / 0.5f);
      if (P.phaseT > 0.5f + P.animalCount * 0.12f) {
        for (int i = 0; i < P.animalCount; ++i) P.animals[i].popScale = 1;
        if (P.seat < seatCount()) startTurn();
        else enter(Screen::HoleCard);
      }
      break;
    case Phase::Turn: {
      char main[28], sub[40];
      snprintf(main, sizeof(main), "%s%s turn", kPlayers[pl].name,
               kPlayers[pl].name[strlen(kPlayers[pl].name) - 1] == 's' ? "'" : "'s");
      snprintf(sub, sizeof(sub), "Hole %d  \x7f  par %d", P.holeIdx + 1, P.hole->par);  // \x7f draws a middle dot
      setBanner(main, sub, kPlayers[pl].color);
      if (gIn.a.pressed && P.phaseT > 0.4f) {
        P.banner[0] = 0;
        P.phase = Phase::Aim;
        P.phaseT = 0;
        P.baseAx = gIn.ax;
        P.yaw0 = gIn.yaw;
        P.aim0 = P.aim;
      }
      break;
    }
    case Phase::Aim: {
      if (gSave.aimMode == kAimTurn) {
        if (fabsf(gYawStep) > kTurnDeadband * fmaxf(gIn.dt, 0.001f)) P.aim += kTurnSign * kTurnGain * gYawStep;
      } else {
        const float d = kTiltXSign * (gIn.ax - P.baseAx);
        const float mag = clampf((fabsf(d) - 0.05f) / 0.40f, 0, 1);
        P.aim -= (d > 0 ? 1 : -1) * powf(mag, 1.6f) * 160.0f * dt;
      }
      if (P.aim >= 360) P.aim -= 360;
      if (P.aim < 0) P.aim += 360;
      if (gIn.a.pressed) {
        P.phase = Phase::Charge;
        P.phaseT = 0;
        P.power = 0;
        P.powerDir = 1;
        gMenuHold = 0;
      }
      break;
    }
    case Phase::Charge: {
      // The bar goes up and back down once. Then it stays empty: letting go is no stroke, and holding on opens the
      // pause menu, so the big button alone can pause the round (the side button is hard to find).
      if (P.powerDir > 0) {
        P.power += dt * 2 / kChargePeriod;
        if (P.power >= 1) {
          P.power = 1;
          P.powerDir = -1;
        }
      } else if (P.power > 0) {
        P.power -= dt * 2 / kChargePeriod;
        if (P.power < 0) P.power = 0;
      } else {
        gMenuHold += dt;
      }
      if (gIn.a.released || !gIn.a.held) {
        gMenuHold = 0;
        if (P.power < 0.03f) {
          P.phase = Phase::Aim;  // a tap with no power, or a bar left to empty, is not a stroke
        } else {
          putt();
        }
      } else if (gMenuHold >= kMenuHold) {
        gMenuHold = 0;
        P.phase = Phase::Aim;  // where the round picks up after the menu
        gPauseAsked = true;
      }
      break;
    }
    case Phase::Roll: {
      Mover movers[4];
      for (int i = 0; i < P.animalCount; ++i) {
        const AnimalState& a = P.animals[i];
        movers[i] = {a.x, a.y, a.vx, a.vy, static_cast<float>(P.hole->animals[i].radius),
                     !a.air && !a.hidden && a.popScale > 0.9f};
      }
      const BallEvents ev = P.ball.step(dt, *P.hole, gSurf, movers, P.animalCount);
      if (ev.wallHit > 25) gSound.tick(static_cast<uint16_t>(700 + ev.wallHit * 2), 10);
      if (ev.bumperHit > 10) gSound.play(Sfx::Animal);
      if (ev.animalHit >= 0 && ev.animalHitSpeed > 8) {
        P.animals[ev.animalHit].squash = 0.3f;
        gSound.play(Sfx::Animal);
      }
      if (ev.enteredSand) gSound.play(Sfx::Sand);
      if (ev.sunk) {
        P.phase = Phase::Sunk;
        P.phaseT = 0;
        char words[24];
        scoreWords(P.strokes, P.hole->par, words, sizeof(words));
        finishSeat(P.strokes, false);
        const uint8_t f = P.flags[P.seat];
        const char* sub = (f & 4)   ? "You beat " PET_MAKER_NAME "!"
                          : (f & 8) ? "Tied " PET_MAKER_NAME "!"
                          : (f & 2) ? "New hole record!"
                          : (f & 1) ? "Your best yet!"
                                    : "";
        setBanner(words, sub, P.strokes == 1 ? kGold : kRed);
        gSound.play(P.strokes == 1 ? Sfx::Ace : (f & 4) ? Sfx::BeatMaker : (f & 10) ? Sfx::Record : Sfx::Sink);
      } else if (ev.splash) {
        P.phase = Phase::Splash;
        P.phaseT = 0;
        P.splashX = P.ball.x;
        P.splashY = P.ball.y;
        ++P.strokes;  // one-stroke penalty
        ++gSave.splashes;
        emit(Event::BallInWater, pl);
        gSound.play(Sfx::Splash);
        setBanner("Splash!", "+1 stroke", rgb(40, 110, 200));
      } else if (ev.lost) {
        P.ball.place(P.lastX, P.lastY);
        P.phase = Phase::Aim;
        P.phaseT = 0;
      } else if (ev.stopped) {
        if (P.strokes >= P.hole->par + kMaxOverPar) {
          P.phase = Phase::PickedUp;
          P.phaseT = 0;
          setBanner("Picked up", "next time!", kInkSoft);
          finishSeat(static_cast<uint8_t>(P.hole->par + kMaxOverPar), true);
        } else {
          P.phase = Phase::Aim;
          P.phaseT = 0;
          P.baseAx = gIn.ax;
          P.yaw0 = gIn.yaw;
          P.aim0 = P.aim;
        }
      }
      break;
    }
    case Phase::Splash:
      if (P.phaseT > 1.2f) {
        P.banner[0] = 0;
        P.ball.place(P.lastX, P.lastY);
        if (P.strokes >= P.hole->par + kMaxOverPar) {
          P.phase = Phase::PickedUp;
          P.phaseT = 0;
          setBanner("Picked up", "next time!", kInkSoft);
          finishSeat(static_cast<uint8_t>(P.hole->par + kMaxOverPar), true);
        } else {
          P.phase = Phase::Aim;
          P.phaseT = 0;
          P.baseAx = gIn.ax;
          P.yaw0 = gIn.yaw;
          P.aim0 = P.aim;
        }
      }
      break;
    case Phase::Sunk:
    case Phase::PickedUp:
      if (P.phaseT > 2.4f || (P.phaseT > 0.8f && gIn.a.pressed)) {
        P.banner[0] = 0;
        nextAfterSeat();
      }
      break;
  }
}

void drawPlay() {
  if (P.phase == Phase::PageTurn) {
    // The new page slides in over the old one, with a shadow at its edge.
    const float e = easeOut(P.phaseT / 0.55f);
    const int off = static_cast<int>((1 - e) * gfx::W);
    uint16_t* px = F.pixels();
    for (int y = 0; y < gfx::H; ++y) {
      uint16_t* row = px + y * gfx::W;
      const uint16_t* oldRow = gPrev + y * gfx::W;
      const uint16_t* newRow = gPage + y * gfx::W;
      for (int x = 0; x < gfx::W; ++x) row[x] = x < off ? oldRow[x] : newRow[x - off];
    }
    for (int k = 1; k <= 8; ++k) F.blendRect(off - k, 0, 1, gfx::H, 0, static_cast<uint8_t>(90 - k * 10));
    return;
  }
  drawPlayScene();
  if (P.phase == Phase::Aim) drawAim(false);
  if (P.phase == Phase::Charge) drawAim(true);
  drawHud();
  if (P.phase == Phase::Charge && P.powerDir < 0 && P.power <= 0) drawMenuHold(gMenuHold / kMenuHold, P.ball.y);
  if (P.phase == Phase::Turn) {
    drawBanner();
    if (P.phaseT > 0.4f) pressA(68, 134, "Ready!");
  }
  if (P.phase == Phase::Sunk || P.phase == Phase::PickedUp || P.phase == Phase::Splash) drawBanner();
}

// --- Screens: menus ------------------------------------------------------------------------------------------------

void drawTitle() {
  menuBackground(70);
  card(10, 54, 115, 92, kPaper, 8);
  F.text(kFontBody, 68, 76, PET_KID_NAME "'s", kInkSoft, Align::Center);
  F.text(kFontTitle, 68, 100, "Pop-Up", kInk, Align::Center);
  F.text(kFontTitle, 68, 122, "Golf", kRed, Align::Center);
  F.text(kFontSmall, 68, 138, gSave.aimMode == kAimTurn ? "aim: turn" : "aim: tilt", kInkSoft, Align::Center);
  choice(156, 28, "Play", gCursor == 0);
  choice(190, 24, "Back to robot", gCursor == 1, true);
  hint("tilt to move");
}

void updateTitle() {
  if (gIn.b.held) {
    gHoldB += gIn.dt;
    if (gHoldB > 2.0f) {  // hidden: hold B on the title to switch aiming between turn and tilt
      gSave.aimMode ^= 1;
      writeSave();
      toast(gSave.aimMode == kAimTurn ? "Aim: turn" : "Aim: tilt");
      gHoldB = -10;  // once per hold
    }
  } else {
    // A short press of B (let go within a second) goes back to the robot: a shortcut for "Back to robot".
    if (gIn.b.released && gHoldB > 0 && gHoldB < 1.0f && gScreenT > 0.3f) gQuit = true;
    gHoldB = 0;
  }
  int dx, dy;
  readNav(dx, dy);
  if (dy && (gCursor + dy) >= 0 && (gCursor + dy) <= 1) {
    gCursor += dy;
    gSound.play(Sfx::Move);
  }
  if (gIn.a.pressed && gScreenT > 0.3f) {
    gSound.play(Sfx::Select);
    if (gCursor == 1) {
      gQuit = true;
    } else {
      gCursor = 6;  // start on GO with last time's players
      enter(Screen::Players);
    }
  }
}

bool seatHas(uint8_t pl) {
  for (int s = 0; s < gSave.lastSeatCount; ++s)
    if (gSave.lastSeats[s] == pl) return true;
  return false;
}

void toggleSeat(uint8_t pl) {
  uint8_t* seats = gSave.lastSeats;
  uint8_t& n = gSave.lastSeatCount;
  for (int s = 0; s < n; ++s)
    if (seats[s] == pl) {
      for (int k = s; k < n - 1; ++k) seats[k] = seats[k + 1];
      --n;
      return;
    }
  if (n < kMaxSeats) seats[n++] = pl;
  else toast("4 players max");
}

void drawPlayers() {
  menuBackground(110);
  card(4, 4, 127, 26, kPaper, 6);
  fitText(68, 23, 121, "Who's playing?", kInk, Align::Center, kFontTitle, kFontBody);
  for (int i = 0; i < kPlayerCount; ++i) {
    const int col = i % 2, row = i / 2;
    const int x = 6 + col * 64, y = 40 + row * 44;
    const bool sel = seatHas(static_cast<uint8_t>(i));
    const bool cur = gCursor == i;
    if (cur) roundRect(x - 3, y - 3, 63, 41, 7, kRed);
    card(x, y, 57, 35, sel ? kPlayers[i].color : kPaper, 6);
    const uint16_t nameCol = sel ? kWhite : kInk;
    const char* sp = strchr(kPlayers[i].name, ' ');
    if (gfx::Frame::textWidth(kFontBody, kPlayers[i].name) > 52 && sp) {
      char first[16];
      snprintf(first, sizeof(first), "%.*s", static_cast<int>(sp - kPlayers[i].name), kPlayers[i].name);
      F.text(kFontSmall, x + 28, y + 15, first, nameCol, Align::Center);
      F.text(kFontBody, x + 28, y + 28, sp + 1, nameCol, Align::Center);
    } else {
      F.text(kFontBody, x + 28, y + 22, kPlayers[i].name, nameCol, Align::Center);
    }
    if (sel) {
      int seat = 0;
      for (int s = 0; s < gSave.lastSeatCount; ++s)
        if (gSave.lastSeats[s] == i) seat = s + 1;
      F.disc(x + 50.5f, y + 6.5f, 6, kWhite);
      char n[4];
      snprintf(n, sizeof(n), "%d", seat);
      F.text(kFontSmall, x + 51, y + 10, n, kPlayers[i].color, Align::Center);
    }
  }
  const bool cur = gCursor == 6;
  if (cur) roundRect(30, 180, 75, 34, 9, kInk);
  card(33, 183, 69, 28, gSave.lastSeatCount ? kRed : kPaperEdge, 8);
  F.text(kFontTitle, 68, 203, "Go!", kWhite, Align::Center);
  hint("tilt to move", "big button picks");
}

void updatePlayers() {
  int dx, dy;
  readNav(dx, dy);
  if (dx || dy) {
    if (gCursor == 6) {
      if (dy < 0) gCursor = 4 + (dx > 0 ? 1 : 0);
    } else {
      int col = gCursor % 2, row = gCursor / 2;
      col = col + dx < 0 ? 0 : (col + dx > 1 ? 1 : col + dx);
      row += dy;
      if (row < 0) row = 0;
      gCursor = row > 2 ? 6 : row * 2 + col;
    }
    gSound.play(Sfx::Move);
  }
  if (gIn.a.pressed) {
    if (gCursor == 6) {
      if (gSave.lastSeatCount == 0) {
        gSound.play(Sfx::Locked);
        toast("Pick a player");
      } else {
        gSound.play(Sfx::Select);
        gCursor = 0;
        enter(Screen::Course);
      }
    } else {
      toggleSeat(static_cast<uint8_t>(gCursor));
      gSound.play(Sfx::Select);
    }
  }
  if (gIn.b.pressed) enter(Screen::Title);
}

int openHoles(int habitat) {
  int open = 0;
  for (int i = 0; i < kHoleCount; ++i)
    if (kHoles[i].habitat == habitat && holeOpen(i)) ++open;
  return open;
}

void drawCourse() {
  menuBackground(110);
  card(4, 4, 127, 26, kPaper, 6);
  F.text(kFontTitle, 68, 23, "Where to?", kInk, Align::Center);
  for (int h = 0; h < kHabitatCount; ++h) {
    const int y = 40 + h * 56;
    const int open = openHoles(h);
    const bool cur = gCursor == h;
    if (cur) roundRect(3, y - 3, 129, 51, 8, kRed);
    card(6, y, 123, 45, open ? kPaper : kPaperEdge, 7);
    F.fillRect(6, y + 4, 6, 37, kHabitats[h].accent);
    F.text(kFontTitle, 18, y + 22, kHabitats[h].name, open ? kInk : kInkSoft);
    char s[32];
    if (open) snprintf(s, sizeof(s), "%d hole%s", open, open == 1 ? "" : "s");
    else snprintf(s, sizeof(s), "coming soon");
    F.text(kFontSmall, 18, y + 37, s, kInkSoft);
  }
}

void updateCourse() {
  int dx, dy;
  readNav(dx, dy);
  if (dy) {
    gCursor = (gCursor + dy + kHabitatCount) % kHabitatCount;
    gSound.play(Sfx::Move);
  }
  if (gIn.a.pressed) {
    if (openHoles(gCursor) == 0) {
      gSound.play(Sfx::Locked);
      toast("Not open yet!");
    } else {
      gSound.play(Sfx::Select);
      RoundSave& r = gSave.round;
      r.seatCount = gSave.lastSeatCount;
      for (int s = 0; s < kMaxSeats; ++s) r.seats[s] = gSave.lastSeats[s];
      beginRound(static_cast<uint8_t>(gCursor));
    }
  }
  if (gIn.b.pressed) enter(Screen::Players);
}

void drawResume() {
  menuBackground(110);
  card(8, 40, 119, 54, kPaper, 7);
  fitText(68, 64, 111, "Keep playing?", kInk, Align::Center, kFontTitle, kFontBody);
  char s[40];
  snprintf(s, sizeof(s), "%s, hole %d", kHabitats[gSave.round.habitat].name, gSave.round.holeIndex + 1);
  F.text(kFontSmall, 68, 82, s, kInkSoft, Align::Center);
  choice(112, 32, "Keep going", gCursor == 0);
  choice(156, 32, "New game", gCursor == 1, true);
  hint("tilt to move", "big button picks");
}

// Tilt moves, and so does the side button (a shortcut); the big button picks.
int menuMove(int count) {
  int dx, dy;
  readNav(dx, dy);
  int to = gCursor;
  if (dy) to += dy;
  if (gIn.b.pressed) to = (gCursor + 1) % count;
  if (to < 0 || to >= count || to == gCursor) return gCursor;
  gSound.play(Sfx::Move);
  return to;
}

void updateResume() {
  gCursor = menuMove(2);
  if (gIn.a.pressed && gScreenT > 0.3f) {
    gSound.play(Sfx::Select);
    if (gCursor == 0) {
      resumeRound();
    } else {
      gSave.round.active = 0;
      writeSave();
      enter(Screen::Title);
    }
  }
}

void drawHoleCard() {
  drawPlayScene();
  dimFrame(120);
  const int x = 6, w = 123;
  card(x, 6, w, 228, kPaper, 8);
  char s[48];
  snprintf(s, sizeof(s), "Hole %d", P.holeIdx + 1);
  F.text(kFontSmall, 68, 20, s, kInkSoft, Align::Center);
  fitText(68, 40, w - 8, P.hole->name, kInk, Align::Center, kFontTitle, kFontBody);
  // Scores.
  int y = 50;
  for (int st = 0; st < seatCount(); ++st) {
    const uint8_t pl = seatPlayer(st);
    const uint8_t sc = gSave.round.scores[st][P.holeIdx];
    F.fillRect(x + 6, y + 3, 4, 12, kPlayers[pl].color);
    F.text(kFontBody, x + 14, y + 14, kPlayers[pl].name, kInk);
    snprintf(s, sizeof(s), "%d", sc);
    F.text(kFontBody, x + w - 8, y + 14, s, sc <= P.hole->par ? kRed : kInk, Align::Right);
    const uint8_t f = P.flags[st];
    if (f & 4) F.text(kFontSmall, x + w - 22, y + 13, "beat " PET_MAKER_NAME "!", kGold, Align::Right);
    else if (f & 2) F.text(kFontSmall, x + w - 22, y + 13, "record!", kGold, Align::Right);
    y += 17;
  }
  // the Maker's record for this hole, when there is one.
  {
    snprintf(s, sizeof(s), "%s's best: %d", PET_MAKER_NAME, makerRecord(P.holeId));
    F.text(kFontSmall, 68, y + 10, s, kInkSoft, Align::Center);
    y += 14;
  }
  // The field note: the hole's animal and its fact.
  y = y < 128 ? 128 : y + 4;
  F.fillRect(x + 10, y - 6, w - 20, 1, kPaperEdge);
  F.text(kFontSmall, 68, y + 6, "FIELD NOTE", kRed, Align::Center);
  if (P.animalCount) {
    const gfx::Image& img = kAnimalArt[P.hole->animals[0].kind];
    F.shade(68 + 2, y + 40, img.w * 0.8f, 3.5f, 100);
    F.blitScaled(img, 68, y + 40, 1.6f, 1.6f, 0.5f, 1.0f);
  }
  lines(kFontBody, 68, y + 58, P.hole->fact, kInk, Align::Center, 14);
  pressA(68, 222, P.holeIdx + 1 < P.holeCount ? "Next hole" : "Scores");
}

void updateHoleCard() {
  if (gIn.a.pressed && gScreenT > 0.6f) {
    gSound.play(Sfx::Select);
    if (P.holeIdx + 1 < P.holeCount) {
      ++P.holeIdx;
      enter(Screen::Play);
      startHole(true);
    } else {
      // The round is over: find the winner.
      int best = 9999, winner = 0;
      for (int st = 0; st < seatCount(); ++st) {
        int tot = 0;
        for (int h = 0; h < P.holeCount; ++h) tot += gSave.round.scores[st][h];
        if (tot < best) {
          best = tot;
          winner = st;
        }
      }
      gSave.round.active = 0;
      writeSave();
      emit(Event::RoundWon, seatPlayer(winner));
      gSound.play(Sfx::Fanfare);
      enter(Screen::RoundEnd);
    }
  }
}

void drawRoundEnd() {
  drawPlayScene();
  dimFrame(130);
  card(6, 10, 123, 220, kPaper, 8);
  F.text(kFontTitle, 68, 36, "Round over!", kInk, Align::Center);
  int totals[kMaxSeats] = {};
  int best = 9999;
  for (int st = 0; st < seatCount(); ++st) {
    for (int h = 0; h < P.holeCount; ++h) totals[st] += gSave.round.scores[st][h];
    if (totals[st] < best) best = totals[st];
  }
  int par = 0;
  for (int h = 0; h < P.holeCount; ++h) par += kHoles[P.holes[h]].par;
  char s[32];
  snprintf(s, sizeof(s), "%d holes, par %d", P.holeCount, par);
  F.text(kFontSmall, 68, 52, s, kInkSoft, Align::Center);
  int y = 64;
  for (int st = 0; st < seatCount(); ++st) {
    const uint8_t pl = seatPlayer(st);
    const bool win = totals[st] == best;
    card(14, y, 107, 30, win ? kPlayers[pl].color : kPaper, 6);
    F.text(kFontBody, 22, y + 20, kPlayers[pl].name, win ? kWhite : kInk);
    snprintf(s, sizeof(s), "%d", totals[st]);
    F.text(kFontTitle, 114, y + 22, s, win ? kWhite : kInk, Align::Right);
    y += 36;
  }
  pressA(68, 214, "Done");
}

void updateRoundEnd() {
  if (gIn.a.pressed && gScreenT > 0.8f) {
    gSound.play(Sfx::Select);
    enter(Screen::Title);
  }
}

// Pause during a round: the side button, or the big button held on an emptied power bar.
Screen gPausedFrom = Screen::Play;
void drawPause() {
  drawPlayScene();
  drawHud();
  dimFrame(130);
  card(10, 44, 115, 160, kPaper, 8);
  F.text(kFontTitle, 68, 70, "Paused", kInk, Align::Center);
  const char* opts[3] = {"Keep playing", "Back to robot", "Stop the round"};
  for (int i = 0; i < 3; ++i) choice(84 + i * 38, 28, opts[i], gCursor == i, i == 2);
  hint("tilt to move", "big button picks");
}

// Opened by the side button, or by holding the big button on an emptied power bar. Tilt (or the side button) moves;
// the big button picks. "Back to robot" keeps the round saved, so the next visit offers to carry on; "Stop the round"
// ends it.
void updatePause() {
  gCursor = menuMove(3);
  if (!gIn.a.pressed || gScreenT < 0.3f) return;
  gSound.play(Sfx::Select);
  if (gCursor == 0) {
    gScreen = gPausedFrom;
  } else if (gCursor == 1) {
    writeSave();
    gQuit = true;
  } else {
    gSave.round.active = 0;
    writeSave();
    enter(Screen::Title);
  }
}

void drawToast() {
  if (gToastT <= 0) return;
  const int w = gfx::Frame::textWidth(kFontSmall, gToast) + 16;
  card(68 - w / 2, 206, w, 18, kInk, 8);
  F.text(kFontSmall, 68, 219, gToast, kWhite, Align::Center);
}

bool gReady = false;

}  // namespace

// --- Public API ----------------------------------------------------------------------------------------------------

void beginRaw(uint16_t* pixels, const Host& host) {
  gHost = host;
  F.attach(pixels);
  if (!gPage) gPage = static_cast<uint16_t*>(bigAlloc(gfx::W * gfx::H * 2));
  if (!gPrev) gPrev = static_cast<uint16_t*>(bigAlloc(gfx::W * gfx::H * 2));
  if (!gSurf) gSurf = static_cast<uint8_t*>(bigAlloc(gfx::W * gfx::H));
  gReady = gPage && gPrev && gSurf;
  gMenuBgDim = -1;
  gSound.attach(host.tone);
  loadSave();
  gCursor = 0;
  enter(gSave.round.active ? Screen::Resume : Screen::Title);
}

#ifdef ARDUINO
void begin(M5Canvas& canvas, const Host& host) { beginRaw(static_cast<uint16_t*>(canvas.getBuffer()), host); }
#endif

bool update(const Input& in) {
  if (!gReady) return false;
  gIn = in;
  filterButton(gIn.b, gFilterB);  // A (G37) doesn't glitch, and a quick tap of A must always count
  // How far the robot turned since the last frame. After a gap (the host may stop calling for a while), the turn
  // made meanwhile isn't an aim, so it's dropped.
  gYawStep = in.dt > 0.1f ? 0 : in.yaw - gLastYaw;
  gLastYaw = in.yaw;
  if (gIn.dt > kDt) gIn.dt = kDt;
  if (gIn.dt < 0) gIn.dt = 0;
  gMs = in.ms;
  gTime += gIn.dt;
  gScreenT += gIn.dt;
  if (gToastT > 0) gToastT -= gIn.dt;
  gSound.update(in.ms);

  switch (gScreen) {
    case Screen::Title:
      updateTitle();
      if (gQuit) {  // back to the robot
        gQuit = false;
        return false;
      }
      if (gScreen == Screen::Title) drawTitle();
      break;
    case Screen::Resume:
      updateResume();
      if (gScreen == Screen::Resume) drawResume();
      break;
    case Screen::Players:
      updatePlayers();
      if (gScreen == Screen::Players) drawPlayers();
      break;
    case Screen::Course:
      updateCourse();
      if (gScreen == Screen::Course) drawCourse();
      break;
    case Screen::Play:
      if (gIn.b.pressed && P.phase != Phase::PageTurn) gPauseAsked = true;
      else updatePlay(gIn.dt);
      if (gPauseAsked) {
        gPauseAsked = false;
        if (P.phase == Phase::Charge) P.phase = Phase::Aim;  // a putt half charged isn't taken on the way back
        gPausedFrom = Screen::Play;
        enter(Screen::Pause);
        drawPause();
        break;
      }
      if (gScreen == Screen::Play) drawPlay();
      break;
    case Screen::HoleCard:
      updateHoleCard();
      if (gScreen == Screen::HoleCard) drawHoleCard();
      break;
    case Screen::RoundEnd:
      updateRoundEnd();
      if (gScreen == Screen::RoundEnd) drawRoundEnd();
      break;
    case Screen::Pause:
      updatePause();
      if (gQuit) {  // back to the robot; the round stays saved
        gQuit = false;
        return false;
      }
      if (gScreen == Screen::Pause) drawPause();
      break;
  }
  // A screen that changed this frame draws next frame; make sure this frame isn't left half-drawn.
  drawToast();
  return true;
}

void resetProgress() { clearProgress(); }

void resetAll() {
  clearProgress();
  clearMaker();
}

}  // namespace game
