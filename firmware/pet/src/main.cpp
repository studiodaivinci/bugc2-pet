// The pet firmware for the M5Stack BugC2 + M5StickC Plus2.
//
// The gift (the egg and the hatching, Happy Birthday with the kid's name, the living face that reacts to being
// handled, and golf), and the dances: seven animal powers, each with its own moves, lights, sounds and colour
// (dances.cpp; moves.cpp, the motor guard). The base gets a stop at boot and every five seconds while nothing is dancing. A build with PET_MOTORS=0
// dances with the lights and the screen only.
//
// Dance: big button, tilt to Dance, big button: the dance picker, a deck of the animals' cards (menu.cpp). Tilt to an
// animal, or stay on Surprise! (a dance that wasn't the last one), and press the big button. The animal's card and
// fact show (big skips ahead), then "Put me down!". On the floor, flat and still for a second, it goes. Any button
// stops it ("Freeze!"), and so does lifting or tipping it.
//
// The buttons: big (the front button under the screen, G37), side (G39), wake (G35, the power
// button; holding it six seconds is a hardware power cut, so play never asks for long holds of it).
// The big button does nearly everything (docs/HARDWARE.md, "Buttons in use"). The power button is hard to press, with
// a cable plugged in beside it, so nothing needs it except switching on; the wake extras here are spares. The side
// button is flush and easy to miss.
// Which way is up: rotation 0, the golf game's up, for everything (docs/HARDWARE.md, "Which way is up").
//
// Home is the face. Picking it up, putting it down, shaking it, turning it over and tapping it get reactions, and held
// at a tilt its eyes go googly. Big = the menu, side = a pat, wake = love.
// Menu: tilt moves (as in golf; side is a shortcut), big = pick, when it's let go. Sound switches every sound on or
// off (the dances' and golf's too) and stays in the menu. A minute untouched goes home.
// Trick Lab (Tricks in the menu, or any button on the remote while the face is showing): lab.h. Four remote buttons
// are moves, done at once and added to a strip; GO plays the strip and then shows its silly name; Freeze stops, or
// wipes it. The latest trick stays the GO trick, through switching off too, until a new animal press starts another.
// The remote's buttons are built in; the shell's "ir learn" teaches it another remote.
// Home from anywhere: hold the big button a second, in the menu, the picker or a dance's card; a ring round the
// face's eyes fills while it's held (golf has its own way back: hold the big button there for its pause menu).
// Hatching: any press, tap or shake cracks the egg; after 8 s without one it wobbles and peeps, and after 25 s it
// cracks by itself. At "Are you... <name>?" the kid presses the big button. An egg nobody touches for 3 minutes stops
// peeping and goes dark until it's touched (it would otherwise peep all night).
// Naps: left alone and still for 3 minutes, it falls asleep and the screen goes off. Any button, or picking it up,
// wakes it. After 30 minutes asleep it sleeps deeply: dark, nearly everything off, and the BIG button wakes it (a
// restart, about two seconds). In the egg, that press is the first crack, so the hatching starts with the kid's first
// push of the big button. Asleep, it looks at its battery every three hours, and after 36 hours (or sooner, on a low
// battery) it switches fully off; then only the power button, or plugging it in, starts it. See sleepDeep().
//
// Shell over USB (921600 baud) or Wi-Fi (dev build): "help" lists the commands. Every reply ends with OK or ERR.
#include <M5Unified.h>
#include <Preferences.h>
#include <driver/gpio.h>
#include <esp_random.h>
#include <esp_sleep.h>

#include "base.h"
#include "batlog.h"
#include "face_player.h"
#include "frames.h"
#include "game.h"
#include "ir.h"
#include "lab.h"
#include "menu.h"
#include "motion.h"
#include "moves.h"
#include "net.h"
#include "sound.h"

#ifndef PET_MOTORS
#define PET_MOTORS 0
#endif
#ifndef PET_WIFI_AT_BOOT
#define PET_WIFI_AT_BOOT 0
#endif

namespace {

constexpr const char* kVersion = PET_MOTORS ? "pet 0.11: face, hatching, song, golf, seven dances, Trick Lab with the remote (motors on)"
                                            : "pet 0.11: face, hatching, song, golf, seven dances, Trick Lab with the remote (no motors: lights only)";

// --- Timings and thresholds ----------------------------------------------------------------------------------------
constexpr uint32_t kNapAfterMs = 3 * 60 * 1000;    // still and untouched this long at home: a nap
constexpr uint32_t kDozeOffMs = 30 * 60 * 1000;    // asleep this long: switch off
constexpr uint32_t kMenuIdleMs = 60 * 1000;
constexpr uint32_t kGolfIdleMs = 90 * 1000;        // docs/GAME.md: the host stops calling after 90 idle seconds
constexpr uint32_t kFloorWaitMs = 45 * 1000;       // a dance nobody puts on the floor gives up and goes home
constexpr int kCracksToHatch = 5;
constexpr uint32_t kHintMs = 8000;                 // no tap this long: the egg wobbles and peeps
constexpr uint32_t kSelfCrackMs = 25000;           // no tap this long: it cracks by itself (live scenes only)
constexpr uint32_t kHelloWaitMs = 40000;           // nobody presses the big button: it carries on
constexpr int kHungryMv = 3550;                    // Stick battery, placeholders until the run-down log
constexpr int kEmptyMv = 3400;
constexpr uint32_t kHungryEveryMs = 10 * 60 * 1000;
constexpr uint32_t kFramePeriodUs = 33333;         // 30 fps at most
constexpr uint32_t kSecondFrameAtMs = 8000;        // Wi-Fi has connected (or given up) by now
constexpr size_t kKeepFreeBytes = 50000;           // internal RAM left for Wi-Fi's buffers, locks and the rest

// --- Saved state (Preferences "pet") -------------------------------------------------------------------------------
struct Save {
  bool hatched = false;
  int32_t hatchDay = -1;  // days since 1970 on the clock when it hatched; -1 if the clock wasn't set
  int32_t lastDay = -1;   // the latest day the clock has shown; the count never goes backwards
  uint8_t volume = 128;
  bool muted = false;
  bool motors = true;  // "motors off" in the shell: dances use the lights and the screen only (a desk, a quiet night)
  int8_t lastDance = -1;  // Surprise! never picks this one
  uint32_t boots = 0;
};
Save gSave;

void loadSave() {
  Preferences p;
  p.begin("pet", true);
  gSave.hatched = p.getBool("hatched", false);
  gSave.hatchDay = p.getInt("hatchDay", -1);
  gSave.lastDay = p.getInt("lastDay", -1);
  gSave.volume = p.getUChar("volume", 128);
  gSave.muted = p.getBool("muted", false);
  gSave.motors = p.getBool("motors", true);
  gSave.lastDance = p.getChar("lastDance", -1);
  gSave.boots = p.getUInt("boots", 0);
  p.end();
}

void writeSave() {
  Preferences p;
  if (!p.begin("pet", false)) return;
  p.putBool("hatched", gSave.hatched);
  p.putInt("hatchDay", gSave.hatchDay);
  p.putInt("lastDay", gSave.lastDay);
  p.putUChar("volume", gSave.volume);
  p.putBool("muted", gSave.muted);
  p.putBool("motors", gSave.motors);
  p.putChar("lastDance", gSave.lastDance);
  p.putUInt("boots", gSave.boots);
  p.end();
}

// --- The clock (BM8563 on the internal bus). It can be lost when the Stick's battery goes flat -----------------------
int32_t daysFromCivil(int y, unsigned m, unsigned d) {  // Howard Hinnant's algorithm
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int32_t>(doe) - 719468;
}

bool readClock(m5::rtc_datetime_t& dt, bool& low) {
  motion::lockBus();
  low = M5.Rtc.getVoltLow();
  const bool ok = M5.Rtc.getDateTime(&dt);
  motion::unlockBus();
  return ok;
}

int32_t clockDay() {
  m5::rtc_datetime_t dt;
  bool low = true;
  if (!readClock(dt, low) || low) return -1;
  if (dt.date.year < 2026 || dt.date.year > 2099 || dt.date.month < 1 || dt.date.month > 12 || dt.date.date < 1 ||
      dt.date.date > 31)
    return -1;
  return daysFromCivil(dt.date.year, dt.date.month, dt.date.date);
}

int weekDay(int y, int m, int d) {  // 0 = Sunday (Sakamoto)
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

// Days since hatching for golf: < 0 before hatching (the Maker's scores become records), then never negative and
// never going backwards, even when the clock is lost.
int daysSinceHatch() {
  if (!gSave.hatched) return -1;
  const int32_t today = clockDay();
  if (today >= 0 && today > gSave.lastDay) {
    gSave.lastDay = today;
    writeSave();
  }
  if (gSave.hatchDay < 0 || gSave.lastDay < gSave.hatchDay) return 0;
  return gSave.lastDay - gSave.hatchDay;
}

// --- The face ------------------------------------------------------------------------------------------------------
face::Player player;
const char* gSceneId = "";
uint64_t gLastUs = 0;
int gBright = 160;  // the backlight at full scene brightness; scenes dim it through Screen::dim
int gLastBright = -1;

bool playScene(const char* id) {
  const int i = face::findScene(id);
  if (i < 0) return false;
  player.play(i);
  player.reseed(esp_random());  // a different play each time: the scene's, the face's and the particles' randoms
  gSceneId = face::SCENES[i].id;
  return true;
}
float sceneTime() { return static_cast<float>(player.S.t); }
float sceneLength() {  // the player's, not the catalogue's: a power waits for "putdown" before its trick
  return player.info() ? static_cast<float>(player.length()) : 0;
}

// Screen orientation. "Up" for the whole robot is the golf game's up: rotation 0 (docs/HARDWARE.md, "Which way is
// up"). Lying flat, the face uses it too: it used to turn 180° there, so the face and golf pointed opposite ways
// (2026-11-10). Held upright the other way round, the face turns 180° to stay upright for whoever holds it.
// Gravity on the screen, in rotation 0, is (kGravX * ax, kGravY * ay): the signs match the golf game's tilt
// (docs/GAME.md) and are set with "axes" if wrong.
constexpr int kUpRotation = 0;
float gGravX = -1, gGravY = 1;
int gRot = 0, gRotWant = 0;
uint32_t gRotSince = 0;

void setRotation(int r) {
  if (r == gRot) return;
  frames::wait();
  M5.Display.setRotation(r);
  gRot = r;
}

void updateRotation(const motion::Sample& s, uint32_t now, bool faceUp) {
  int want = gRot;
  if (!faceUp) {
    setRotation(kUpRotation);
    return;
  }
  const float down = gGravY * s.ay;  // how far gravity points to the bottom of the screen, in rotation 0
  if (s.az > 0.75f) want = kUpRotation;  // lying flat: golf's up
  else if (down > 0.35f) want = 0;
  else if (down < -0.35f) want = 2;
  if (want != gRotWant) {
    gRotWant = want;
    gRotSince = now;
  }
  if (want != gRot && now - gRotSince > 400) setRotation(want);
}

// Googly eyes while it's held at a tilt: the pupils hang the way gravity pulls.
bool gGooglyOn = false;
uint32_t gLastReactMs = 0;

void updateGoogly(const motion::Sample& s, uint32_t now) {
  face::Face& f = player.S.face;
  const bool tilted = motion::held() && !motion::flipped() && s.az < 0.8f;
  if (tilted && now - gLastReactMs > 2500) {
    float gx = gGravX * s.ax, gy = gGravY * s.ay;
    if (gRot == 2) {
      gx = -gx;
      gy = -gy;
    }
    f.googly = true;
    f.autoLook = false;
    f.grav[0] = gx * 1.3f;
    f.grav[1] = gy * 1.3f;
    gGooglyOn = true;
  } else if (gGooglyOn) {
    f.googly = false;
    f.autoLook = true;
    f.grav[0] = 0;
    f.grav[1] = 1;
    gGooglyOn = false;
  }
}

// --- The base lights -----------------------------------------------------------------------------------------------
struct Flash {
  uint32_t until = 0;
  uint8_t r = 0, g = 0, b = 0;
  bool rainbow = false;
};
Flash gFlash;

void flash(uint8_t r, uint8_t g, uint8_t b, uint32_t ms) { gFlash = {millis() + ms, r, g, b, false}; }
void flashRainbow(uint32_t ms) {
  gFlash = Flash{};
  gFlash.until = millis() + ms;
  gFlash.rainbow = true;
}

void hsv(float h, float v, uint8_t& r, uint8_t& g, uint8_t& b) {
  h = fmodf(h, 1) * 6;
  const int i = static_cast<int>(h);
  const float f = h - i, q = 1 - f;
  float rr = 0, gg = 0, bb = 0;
  switch (i) {
    case 0: rr = 1; gg = f; break;
    case 1: rr = q; gg = 1; break;
    case 2: gg = 1; bb = f; break;
    case 3: gg = q; bb = 1; break;
    case 4: rr = f; bb = 1; break;
    default: rr = 1; bb = q; break;
  }
  r = static_cast<uint8_t>(rr * v);
  g = static_cast<uint8_t>(gg * v);
  b = static_cast<uint8_t>(bb * v);
}

void rainbowLeds(float t, float v) {
  uint8_t r, g, b;
  hsv(t * 0.6f, v, r, g, b);
  base::setLed(0, r, g, b);
  hsv(t * 0.6f + 0.5f, v, r, g, b);
  base::setLed(1, r, g, b);
}

// --- States --------------------------------------------------------------------------------------------------------
enum class St : uint8_t { Egg, Crack, Hatch, Hello, Birthday, Home, React, Menu, Golf, Nap, Doze, Wake, Demo, Power, Pick, Lab };
const char* const kStateNames[] = {"egg", "crack", "hatch", "hello", "birthday", "home", "react", "menu",
                                   "golf", "nap", "doze", "wake", "demo", "power", "pick", "lab"};
St gSt = St::Egg;
uint32_t gStAt = 0;
uint32_t gLastActive = 0;  // the last button, handling event or movement

// Per-state working values.
int gCracks = 0;
uint32_t gLastCrackMs = 0, gLastHintMs = 0;
bool gHatchSound = false;
int gHelloPhase = 0;
uint32_t gHelloAt = 0, gThumpAt = 0;
bool gHoldScene = false;
bool gSongStarted = false;
uint32_t gSongAt = 0;
float gReactUntil = 0;  // scene seconds; 0 = the scene's own length
uint32_t gHelpAt = 0;
uint32_t gHungryAt = 0;
int gMenuSel = 0;
uint32_t gSoundPopAt = 0;  // the Sound switch was pressed: its speaker bounces
uint32_t gDozeAt = 0;

// The big button in the menu, the picker and a dance's card: a tap acts when it's let go, so that holding it can go
// home (2026-11-11: a shortcut back to the face). Held a second, a ring round the face's eyes fills
// and it goes home. Only a press that started in this state counts: the press that opened a screen does nothing there.
constexpr uint32_t kHomeHoldMs = 1000, kHomeRingMs = 250;
uint32_t gBigDownAt = 0;  // when the big button went down; 0 while it's up
uint32_t gHomeHoldMs = 0;  // how long it's been held this time, if that counts here; for the ring
moves::Dance gDance = moves::CHEETAH;
bool gDanceDry = false;       // the shell's "dance <animal> dry": lights and screen, motors off
bool gFloorSent = false;      // the scene has been told it's on the floor
volatile bool gTrickCue = false;  // the scene's trick has started (face::onCue, from inside player.step())

// Switches off. On the battery that cuts the stick's power, and the power button starts it again. On USB power it can't
// cut its own power, so the library puts it into deep sleep instead, with nothing armed to wake it: it stayed dark on
// its cable until a reset (2026-11-10, after a doze). The big button is armed first, so a press restarts it. The base
// has its own cell and keeps the last motor speeds, so the motors are stopped first, whatever was running.
void switchOff() {
  moves::stop("switching off");
  base::stopMotors();
  base::ledsOff();
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_37, 0);  // the big button: pressed = low
  M5.Power.powerOff();
}

// Sleeping deeply. Switched off, only the power button starts the Stick again, and that button is hard to press
// (2026-11-11: we had to plug it in). So after its nap it goes into the chip's deep sleep instead, with the pin that
// keeps the Stick's power on (G4, "HOLD") held high through the sleep, and the big button armed to wake it. A wake is a
// restart. The cost: the Stick's 3.3 V stays on, so the base stays powered too, and both cells drain slowly (not yet
// measured: docs/LESSONS.md, "Open questions"). So it wakes itself every few hours without lighting anything, looks at its
// battery, and after kSleepChunks of them, or on a low battery, lets go of the power for real.
constexpr gpio_num_t kLatchPin = GPIO_NUM_4, kBacklightPin = GPIO_NUM_27, kBigButtonPin = GPIO_NUM_37;
constexpr int kBatteryAdcPin = 38;             // half the cell's voltage (M5Unified reads the same pin)
constexpr uint32_t kSleepChunkS = 3 * 3600;
constexpr uint16_t kSleepChunks = 12;          // 36 hours
constexpr int kSleepLowMv = 3500;
RTC_DATA_ATTR uint16_t gSleepChunk = 0;        // kept through deep sleep: how many chunks it has slept...
RTC_DATA_ATTR uint16_t gSleepChunks = kSleepChunks;  // ...of how many...
RTC_DATA_ATTR uint32_t gSleepChunkS = kSleepChunkS;  // ...each this long (the shell's "sleep <s> <n>" shortens them)

// Arms the big button and the timer, and sleeps. Needs nothing set up, so the silent timer wake can call it too.
[[noreturn]] void sleepNow() {
  digitalWrite(kLatchPin, HIGH);
  pinMode(kLatchPin, OUTPUT);
  digitalWrite(kLatchPin, HIGH);
  gpio_hold_en(kLatchPin);       // the power stays on...
  gpio_hold_en(kBacklightPin);   // ...and the backlight stays as it is: off
  gpio_deep_sleep_hold_en();
  esp_sleep_enable_ext0_wakeup(kBigButtonPin, 0);  // pressed = low
  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(gSleepChunkS) * 1000000ULL);
  esp_deep_sleep_start();
  for (;;) {}
}

void sleepDeep(uint32_t chunkS = kSleepChunkS, uint16_t chunks = kSleepChunks) {
  moves::stop("sleeping");
  base::stopMotors();
  base::ledsOff();
  sound::stop();
  writeSave();
  frames::wait();
  M5.Display.setBrightness(0);
  M5.Display.sleep();
  M5.Display.waitDisplay();
  motion::lockBus();
  M5.Imu.sleep();
  motion::unlockBus();
  gSleepChunk = 0;
  gSleepChunks = chunks;
  gSleepChunkS = chunkS;
  Serial.flush();
  sleepNow();
}

// The timer woke it, asleep: nothing is lit and nothing set up. Sleep on, or let go of the power.
[[noreturn]] void sleepCheck() {
  ++gSleepChunk;
  const int mv = analogReadMilliVolts(kBatteryAdcPin) * 2;
  if (gSleepChunk < gSleepChunks && !(mv > 2000 && mv < kSleepLowMv)) sleepNow();
  gpio_hold_dis(kLatchPin);
  pinMode(kLatchPin, OUTPUT);
  digitalWrite(kLatchPin, LOW);  // on the battery, this is off
  delay(500);
  // Still here: it's on a cable. Sleep with only the big button armed (the backlight stays held off).
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_sleep_enable_ext0_wakeup(kBigButtonPin, 0);
  esp_deep_sleep_start();
  for (;;) {}
}

// Tilt moves the menu's choice, as in golf's menus (docs/GAME.md): tipping the bottom of the screen down moves it
// down, measured from how the robot was held when the menu opened, one step per tilt. The side button is a shortcut
// (it's easy to miss: docs/HARDWARE.md, "Buttons in use").
// A step needs a clear tip, held: about 27 degrees for 0.15 s (at 17 degrees with no hold it changed too
// easily and too often). Golf's menus use the same numbers (game.cpp, readNav).
constexpr float kTipG = 0.45f, kTipRearmG = 0.2f;
constexpr uint32_t kTipHoldMs = 150;
float gMenuBaseY = 0;
bool gMenuLatch = false;
int gMenuTipDir = 0;
uint32_t gMenuTipAt = 0;

int menuTilt(const motion::Sample& s, uint32_t now) {
  const float ay = gGravY * s.ay;
  if (now - gStAt < 400) {  // learn how it's held
    gMenuBaseY = ay;
    gMenuLatch = false;
    gMenuTipDir = 0;
    return 0;
  }
  const float ty = ay - gMenuBaseY;
  if (fabsf(ty) < kTipRearmG) {  // back near the start: ready for the next step, and follow a slow change of grip
    gMenuLatch = false;
    gMenuTipDir = 0;
    gMenuBaseY += (ay - gMenuBaseY) * 0.03f;
    return 0;
  }
  if (gMenuLatch) return 0;
  const int dir = fabsf(ty) < kTipG ? 0 : ty > 0 ? 1 : -1;
  if (dir != gMenuTipDir) {  // a new tip (or none): start timing it
    gMenuTipDir = dir;
    gMenuTipAt = now;
    return 0;
  }
  if (dir == 0 || now - gMenuTipAt < kTipHoldMs) return 0;
  gMenuLatch = true;
  return dir;
}

void enter(St s);

// Inputs gathered once a frame.
struct Inputs {
  bool big = false, side = false, wake = false;  // pressed this frame
  bool bigHeld = false;
  bool bigTap = false;     // let go after a short press that started in this state (the menus act on this)
  uint32_t bigHoldMs = 0;  // held this long, by a press that started in this state
  motion::Event ev[8];
  int nev = 0;
  uint32_t irCode = 0;       // a button on a remote, pressed this frame: its code (any NEC remote); 0: none
  int role = lab::NO_ROLE;   // ...and what that button means, if it's one it has been taught
  bool any() const { return big || side || wake || nev > 0 || role >= 0; }
  bool has(motion::Event e) const {
    for (int i = 0; i < nev; ++i)
      if (ev[i] == e) return true;
    return false;
  }
};
bool gSimBig = false, gSimSide = false, gSimWake = false;

// --- Golf ----------------------------------------------------------------------------------------------------------
M5Canvas gGolfCanvas(&M5.Display);
bool gGolfCanvasOk = false;
int gGolfBest = -1;  // the best thing that happened this visit, for the reaction afterwards
uint32_t gGolfLastInput = 0, gGolfLastMs = 0;

void hostTone(uint16_t hz, uint16_t ms) { sound::tone(hz, ms); }
int hostDays() { return daysSinceHatch(); }
void hostEvent(game::Event e, uint8_t who) {
  static const char* names[] = {"HoleInOne", "UnderPar",    "NewRecord",    "BeatMaker",
                                "BallInWater", "RoundWon", "TiedMaker"};
  const int i = static_cast<int>(e);
  console.printf("golf %s player %d\n", i < 7 ? names[i] : "?", who);
  switch (e) {
    case game::Event::HoleInOne: flashRainbow(2500); break;
    case game::Event::BallInWater: flash(0, 40, 120, 600); break;
    case game::Event::BeatMaker:
    case game::Event::NewRecord:
    case game::Event::TiedMaker: flash(0, 120, 40, 1200); break;
    case game::Event::RoundWon: flashRainbow(1500); break;
    default: flash(60, 60, 0, 400); break;
  }
  // Ranks for the reaction afterwards: a hole in one beats beating the Maker beats a record...
  static const int rank[] = {5, 1, 3, 4, 0, 2, 2};
  if (i < 7 && (gGolfBest < 0 || rank[i] > rank[gGolfBest])) gGolfBest = i;
}

void startGolf() {
  frames::wait();
  setRotation(kUpRotation);
  base::ledsOff();
  if (!gGolfCanvasOk) {
    gGolfCanvas.setColorDepth(16);
    gGolfCanvas.setPsram(true);
    gGolfCanvasOk = gGolfCanvas.createSprite(game::kWidth, game::kHeight);
  }
  if (!gGolfCanvasOk) {
    console.println("golf: no canvas");
    enter(St::Home);
    return;
  }
  game::Host host;
  host.tone = hostTone;
  host.daysSinceHatch = hostDays;
  host.event = hostEvent;
  game::begin(gGolfCanvas, host);
  gGolfBest = -1;
  gGolfLastInput = gGolfLastMs = millis();
}

// Returns false when the game is over.
bool golfFrame(uint32_t now) {
  game::Input in;
  in.ms = now;
  in.dt = (now - gGolfLastMs) / 1000.0f;
  gGolfLastMs = now;
  in.a.pressed = M5.BtnA.wasPressed();
  in.a.held = M5.BtnA.isPressed();
  in.a.released = M5.BtnA.wasReleased();
  in.b.pressed = M5.BtnB.wasPressed();
  in.b.held = M5.BtnB.isPressed();
  in.b.released = M5.BtnB.wasReleased();
  const motion::Sample s = motion::latest();
  in.ax = s.ax;
  in.ay = s.ay;
  in.az = s.az;
  in.gz = s.gz;
  in.yaw = s.yaw;
  if (in.a.held || in.b.held || fabsf(s.gz) > 20) gGolfLastInput = now;
  const bool more = game::update(in);
  gGolfCanvas.pushSprite(0, 0);
  if (now - gGolfLastInput > kGolfIdleMs) return false;  // the round stays saved for next time
  return more;
}

// --- The menu (menu.cpp) ------------------------------------------------------------------------------------------
face::Screen gMenuScreen;

void drawMenu(uint32_t now) {
  gMenuScreen.setFrame(frames::top(), frames::bottom());
  const float pop = gSoundPopAt && now - gSoundPopAt < 400 ? (now - gSoundPopAt) / 400.0f : 0;
  menu::draw(gMenuScreen, gMenuSel, now / 1000.0f, !gSave.muted, pop);
}

// The ring that fills while the big button is held for home (gHomeHoldMs), over whatever is on the screen.
void drawHomeRing(face::Screen& scr, uint32_t now) {
  if (gHomeHoldMs <= kHomeRingMs) return;
  menu::drawHomeRing(scr, static_cast<float>(gHomeHoldMs - kHomeRingMs) / (kHomeHoldMs - kHomeRingMs), now / 1000.0f);
}

// --- The dance picker (menu.cpp, drawPicker) -------------------------------------------------------------------------
// Surprise! shuffles through the animals for about a second, slowing like a prize wheel, and lands on a dance that
// wasn't the last one (saved, so a restart doesn't repeat it either). A named card bounces once. Either way the
// dance's scene then opens with that animal's card.
constexpr uint32_t kShuffleMs = 1100, kShuffleFirstMs = 40, kLandMs = 650;
constexpr float kShuffleSlow = 1.22f;  // each flip this much longer than the one before
menu::PickView gPick;
uint32_t gPickAt = 0;  // when the big button picked; 0 while choosing
uint32_t gShuffleNext = 0, gLandAt = 0;
float gShuffleGap = 0;
bool gShuffling = false;
moves::Dance gPicked = moves::CHEETAH;

moves::Dance surpriseDance() {
  const int last = gSave.lastDance;
  const bool hasLast = last >= 0 && last < moves::DANCE_COUNT;
  int r = static_cast<int>(esp_random() % (hasLast ? moves::DANCE_COUNT - 1 : moves::DANCE_COUNT));
  if (hasLast && r >= last) ++r;
  return static_cast<moves::Dance>(r);
}

void drawPicker(uint32_t now) {
  gMenuScreen.setFrame(frames::top(), frames::bottom());
  gPick.pop = gPickAt && !gShuffling ? (now - gLandAt) / 350.0f : 0;
  menu::drawPicker(gMenuScreen, gPick, now / 1000.0f);
}

// --- Trick Lab (lab.h) and the remote (ir.h) ------------------------------------------------------------------------
// The remote's six buttons are built in: the codes of the remote we used (a cheap six-button heater remote, taught
// 2026-11-11), so that one works out of the box. Yours sends other codes: teach it with the shell's "ir learn". The lesson
// ("My remote": press the button for the cheetah, the spider...) is still here for a replacement remote, any that
// speaks NEC, but only the shell's "ir learn" opens it. A taught remote and the latest trick are saved in "pet".
constexpr uint32_t kRemote[lab::ROLE_COUNT] = {0xf807ff00, 0xfa05ff00, 0x36c9ff00,   // dash, scoot, spin,
                                               0x37c8ff00, 0x3bc4ff00, 0xfe01ff00};  // wiggle, go, freeze
constexpr uint32_t kLabIdleMs = 2 * 60 * 1000;  // nobody pressing: back to the face
constexpr uint32_t kLabWaitMs = 2500;           // a move that can't start (still settling) is done with lights only
constexpr uint32_t kLabHeldMs = 1500;           // held this long: it's being held, so lights only at once
constexpr uint32_t kLabGoWaitMs = 20000;        // GO waits this long to be put on the floor
constexpr uint32_t kLabPopMs = 300;

uint32_t gIrCodes[lab::ROLE_COUNT] = {0, 0, 0, 0, 0, 0};
bool gIrMapped = false;
lab::Trick gLast;  // the latest trick: what GO plays, until a new animal press starts another. Saved

void loadLab() {
  Preferences p;
  p.begin("pet", true);
  if (p.getBytes("ircodes", gIrCodes, sizeof(gIrCodes)) != sizeof(gIrCodes)) memcpy(gIrCodes, kRemote, sizeof(gIrCodes));
  gIrMapped = true;
  uint8_t buf[1 + lab::kMaxSteps];
  if (p.getBytes("trick", buf, sizeof(buf)) == sizeof(buf) && buf[0] <= lab::kMaxSteps) {
    gLast.n = buf[0];
    for (int i = 0; i < lab::kMaxSteps; ++i) gLast.steps[i] = buf[1 + i] < lab::MOVE_COUNT ? buf[1 + i] : 0;
  }
  p.end();
}
void writeRemote() {
  Preferences p;
  if (!p.begin("pet", false)) return;
  if (gIrMapped) p.putBytes("ircodes", gIrCodes, sizeof(gIrCodes));
  else p.remove("ircodes");
  p.end();
}
void writeTrick() {
  Preferences p;
  if (!p.begin("pet", false)) return;
  uint8_t buf[1 + lab::kMaxSteps];
  buf[0] = gLast.n;
  memcpy(buf + 1, gLast.steps, lab::kMaxSteps);
  p.putBytes("trick", buf, sizeof(buf));
  p.remove("tricks");  // pet 0.8's four kept tricks
  p.end();
}
int roleOf(uint32_t code) {
  if (!gIrMapped) return lab::NO_ROLE;
  for (int i = 0; i < lab::ROLE_COUNT; ++i)
    if (gIrCodes[i] == code) return i;
  return lab::NO_ROLE;
}

enum class LabRun : uint8_t { None, Move, Play };
lab::Trick gTrick;         // the strip
int gLabDone = 0;          // how many of its moves have been done since they were pressed
bool gLabFresh = false;    // it has been played, or remembered from before: the next move starts a new strip
bool gLabGo = false;       // GO is waiting for the floor, or for the motors' rest
uint32_t gLabGoAt = 0, gLabRunAt = 0, gLabPopAt = 0, gLabPendAt = 0, gLabHeldAt = 0;
float gLabRestFrom = 0;
LabRun gLabRun = LabRun::None;
int gLabWord = -1;
lab::Status gLabStatus = lab::S_EMPTY;  // GO's wait, for the screen
bool gLearning = false;
int gLearnRole = 0;
uint32_t gLearnCodes[lab::ROLE_COUNT];
uint32_t gLearnPopAt = 0, gLearnTakenAt = 0;
int gSimRole = lab::NO_ROLE;  // the shell's "rc"

void forgetTrick() {
  gLast = lab::Trick{};
  writeTrick();
}
void printTrick(const lab::Trick& tr) {
  const char *adj, *noun;
  lab::nameOf(tr, adj, noun);
  console.printf("trick %s %s:", adj, noun);
  for (int i = 0; i < tr.n; ++i) console.printf(" %s", lab::roleKey(tr.steps[i]));
  console.println();
}

void labClear() {
  gTrick = lab::Trick{};
  gLabDone = 0;
  gLabFresh = false;
  gLabGo = false;
  gLabWord = -1;
  gLabPendAt = 0;
}
void startLearn(uint32_t now) {
  gLearning = true;
  gLearnRole = 0;
  gLearnPopAt = now;
  gLearnTakenAt = 0;
  console.println("my remote: press the button for each of dash, scoot, spin, wiggle, go, freeze");
}

// Hands the motor guard one move of the strip (only >= 0) or all of it (-1), and starts it. Dry: lights and sound.
bool labStart(int only, bool dry, uint32_t now) {
  moves::Step steps[moves::kLabSteps];
  moves::setLab(steps, lab::build(gTrick, only, steps));
  bool ok = moves::start(moves::LAB, dry, true);
  if (!ok && !dry) {
    console.printf("trick: lights only (%s)\n", moves::why());
    ok = moves::start(moves::LAB, true, true);
  }
  if (ok) {
    gLabRun = only >= 0 ? LabRun::Move : LabRun::Play;
    gLabRunAt = now;
    if (only >= 0) gLabWord = gTrick.steps[only];
  }
  return ok;
}

void labPress(int role, uint32_t now) {
  if (role < lab::MOVE_COUNT) {  // a move: onto the strip, and done as soon as the one before is over
    if (gLabRun == LabRun::Play) return;
    gLabGo = false;
    if (gLabFresh) labClear();
    if (gTrick.n >= lab::kMaxSteps) {
      sound::play(sound::BACK);
      return;
    }
    gTrick.steps[gTrick.n++] = static_cast<uint8_t>(role);
    gLast = gTrick;  // the GO trick from now on
    writeTrick();
    gLabPopAt = now;
    if (gLabRun != LabRun::None) sound::play(sound::CLICK);  // queued behind the move it's doing
  } else if (role == lab::R_GO) {
    if (gLabRun == LabRun::Play) return;
    if (gTrick.n == 0) {  // the strip was wiped: the latest trick comes back
      if (!gLast.n) {
        sound::play(sound::BACK);
        return;
      }
      gTrick = gLast;
      gLabDone = gTrick.n;
      gLabFresh = true;
      gLabPopAt = 0;
    }
    gLabGo = true;
    gLabGoAt = now;
    gLabRestFrom = 0;
    sound::play(sound::PICK);
  } else {  // Freeze: stop; with nothing running, wipe the strip
    if (moves::busy()) {
      moves::stop("freeze");
    } else if (gLabGo) {
      gLabGo = false;
    } else if (gTrick.n) {
      labClear();
      sound::play(sound::BACK);
    } else {
      sound::play(sound::CLICK);
    }
  }
}

void updateLab(const Inputs& in, uint32_t now) {
  if (in.bigHoldMs >= kHomeHoldMs || in.wake) {
    sound::play(sound::BACK);
    enter(St::Home);
    return;
  }
  if (gLearning) {
    if (in.irCode) {
      bool taken = false;
      for (int i = 0; i < gLearnRole; ++i) taken = taken || gLearnCodes[i] == in.irCode;
      if (taken) {
        gLearnTakenAt = now;
        sound::play(sound::BACK);
      } else {
        const face::C3 c = lab::roleColor(gLearnRole);
        flash(c.r * 0.5f, c.g * 0.5f, c.b * 0.5f, 300);
        gLearnCodes[gLearnRole++] = in.irCode;
        gLearnPopAt = now;
        sound::play(sound::PICK);
        if (gLearnRole >= lab::ROLE_COUNT) {
          memcpy(gIrCodes, gLearnCodes, sizeof(gIrCodes));
          gIrMapped = true;
          writeRemote();
          gLearning = false;
          sound::play(sound::CHEER);
          flashRainbow(1200);
          console.println("my remote: learned");
        }
      }
    }
    if (now - gLastActive > kMenuIdleMs) enter(St::Home);
    return;
  }

  // The move or the replay has ended.
  if (gLabRun != LabRun::None && !moves::busy()) {
    const bool early = moves::why()[0] != 0;
    if (gLabRun == LabRun::Move) ++gLabDone;
    else if (!early) {  // played to the end: ta-da, and its name
      gLabFresh = true;
      printTrick(gTrick);
    }
    gLabRun = LabRun::None;
    gLabPendAt = 0;
    if (early) {  // Freeze, a button, lifted or tipped: the moves still waiting stay on the strip, undone
      gLabDone = gTrick.n;
      gLabGo = false;
    }
  }

  // Presses. Any button on the robot stops a move (the guard has already seen it).
  if (moves::busy() && (in.big || in.side)) moves::stop("button");
  if (in.role >= 0) labPress(in.role, now);

  const bool motors = PET_MOTORS && gSave.motors;
  const bool held = motion::held() || motion::flipped() || motion::latest().az < 0.9f;
  if (!held) gLabHeldAt = 0;
  else if (!gLabHeldAt) gLabHeldAt = now ? now : 1;
  gLabStatus = lab::S_BUILD;

  if (gLabRun == LabRun::None && gLabDone < gTrick.n) {
    // The next move pressed. On the floor it waits to stop coasting, and for the motors' rest if they need one
    // (three seconds at most for one move). In the hand, or if it never settles, it's done with lights and sound.
    if (!gLabPendAt) gLabPendAt = now ? now : 1;
    moves::Step steps[moves::kLabSteps];
    moves::setLab(steps, lab::build(gTrick, gLabDone, steps));
    bool go = false, dry = !motors;
    if (dry) {
      go = true;
    } else if (moves::labReady()) {
      const float rest = moves::restMs(moves::LAB);
      go = rest <= 0;
      if (!go) {  // the motors' rest: "Phew!" and the bar, then it does the move
        gLabStatus = lab::S_REST;
        if (gLabRestFrom < rest) gLabRestFrom = rest;
      }
    } else if ((held && now - gLabHeldAt > kLabHeldMs) || now - gLabPendAt > kLabWaitMs) {
      go = dry = true;
    }
    if (go) {
      gLabRestFrom = 0;
      if (!labStart(gLabDone, dry, now)) ++gLabDone;  // it can't even show it: skip it
    }
  } else if (gLabRun == LabRun::None && gLabGo) {
    moves::Step steps[moves::kLabSteps];
    moves::setLab(steps, lab::build(gTrick, -1, steps));
    const float rest = motors ? moves::restMs(moves::LAB) : 0;
    if (!motors) {
      gLabGo = false;
      labStart(-1, true, now);
    } else if (!moves::labReady()) {
      if (held) gLabStatus = lab::S_PUTDOWN;
      if (now - gLabGoAt > kLabGoWaitMs) gLabGo = false;
    } else if (rest > 0) {
      gLabStatus = lab::S_REST;
      if (gLabRestFrom < rest) gLabRestFrom = rest;
    } else {
      gLabGo = false;
      labStart(-1, false, now);
    }
  }
  if (now - gLastActive > kLabIdleMs && !moves::busy()) enter(St::Home);
}

void drawLab(uint32_t now) {
  gMenuScreen.setFrame(frames::top(), frames::bottom());
  if (gLearning) {
    lab::LearnView v;
    v.role = gLearnRole;
    v.pop = (now - gLearnPopAt) / 350.0f;
    v.taken = gLearnTakenAt && now - gLearnTakenAt < 900 ? 1 : 0;
    lab::drawLearn(gMenuScreen, v, now / 1000.0f);
    return;
  }
  lab::View v;
  v.trick = gTrick;
  v.anySaved = gLast.n > 0;
  v.pop = gLabPopAt ? fminf(1.0f, (now - gLabPopAt) / static_cast<float>(kLabPopMs)) : 1;
  v.word = gLabWord;
  const int n = gTrick.n;
  if (gLabRun == LabRun::Play) {
    const int at = lab::stepAt(gTrick, now - gLabRunAt);
    v.status = at >= n ? lab::S_DONE : lab::S_PLAY;
    v.active = at >= 0 && at < n ? at : -1;
    if (v.active >= 0) gLabWord = gTrick.steps[v.active];
    v.word = at >= n ? -1 : gLabWord;
  } else if (gLabRun == LabRun::Move) {
    v.status = lab::S_BUILD;
    v.active = gLabDone;
  } else if (gLabStatus != lab::S_BUILD) {  // waiting for the floor, or for the motors' rest
    v.status = gLabStatus;
    if (gLabStatus == lab::S_REST && gLabRestFrom > 0)
      v.rest = 1 - moves::restMs(moves::LAB) / gLabRestFrom;
  } else if (gLabFresh && n) {
    v.status = lab::S_NAMED;
  } else {
    v.status = n == 0 ? lab::S_EMPTY : n >= lab::kMaxSteps ? lab::S_FULL : lab::S_BUILD;
    if (v.status == lab::S_FULL && gLabDone >= n) v.word = -1;  // nothing left to do: "FULL!"
  }
  lab::draw(gMenuScreen, v, now / 1000.0f);
}

// --- Reactions at home ---------------------------------------------------------------------------------------------
void react(motion::Event e, uint32_t now) {
  gLastReactMs = now;
  switch (e) {
    // Picking it up and putting it down are silent: a beep every time was too much
    case motion::PICKUP: player.event("pickup"); break;
    case motion::PUTDOWN: player.event("putdown"); break;
    case motion::SHAKE: player.event("shake"); sound::play(sound::DIZZY); flashRainbow(1500); break;
    case motion::FLIP:  // the screen faces the floor, so the lights and the buzzer carry "Help!"
      player.event("flip");
      sound::play(sound::HELP);
      gHelpAt = now;
      break;
    case motion::UNFLIP: player.event("unflip"); sound::play(sound::THANKS); flash(0, 90, 30, 500); break;
    case motion::TAP: player.event("pat"); sound::play(sound::GIGGLE); flash(90, 10, 40, 200); break;
    default: break;
  }
}

// --- Entering a state ----------------------------------------------------------------------------------------------
void enter(St s) {
  const St from = gSt;
  gSt = s;
  gStAt = millis();
  console.printf("state %s\n", kStateNames[static_cast<int>(s)]);
  if (from == St::Power) moves::stop("left the dance");
  if (from == St::Lab) moves::stop("left the lab");
  if (from == St::Doze && s != St::Doze) {
    M5.Display.wakeup();
    gLastBright = -1;
  }
  switch (s) {
    case St::Egg:
      playScene("egg-sleep");
      gLastHintMs = millis();
      break;
    case St::Crack:
      playScene("egg-crack");
      gCracks = 0;
      gLastCrackMs = gLastHintMs = millis();
      break;
    case St::Hatch:
      playScene("egg-hatch");
      gHatchSound = false;
      break;
    case St::Hello:
      playScene("hello-kid");
      gHelloPhase = 0;
      gHoldScene = false;
      break;
    case St::Birthday:
      playScene("birthday");
      gSongStarted = false;
      break;
    case St::Home:
      playScene("play");
      gGooglyOn = false;
      break;
    case St::Menu:
      gMenuSel = 0;
      base::ledsOff();
      break;
    case St::Golf: startGolf(); break;
    case St::Nap:
      playScene("nap");
      sound::play(sound::YAWN);
      break;
    case St::Doze:
      frames::wait();
      M5.Display.setBrightness(0);
      M5.Display.sleep();
      base::ledsOff();
      gDozeAt = millis();
      break;
    case St::Wake:
      playScene("wake-up");
      sound::play(sound::WAKE);
      break;
    case St::Power: {  // the animal's card and fact, then "Put me down!" (face_scenes.cpp, Power)
      char id[24];
      snprintf(id, sizeof(id), "power-%s", moves::name(gDance));
      playScene(id);
      gFloorSent = false;
      gTrickCue = false;
      base::ledsOff();
      break;
    }
    case St::Pick:
      gPick = menu::PickView{};
      gPickAt = 0;
      gShuffling = false;
      base::ledsOff();
      break;
    case St::Lab:
      base::ledsOff();
      labClear();
      if (gLast.n) {  // the latest trick, ready for GO
        gTrick = gLast;
        gLabDone = gTrick.n;
        gLabFresh = true;
      }
      gLabRun = LabRun::None;
      gLabStatus = lab::S_BUILD;
      gLabRestFrom = 0;
      gLearning = false;
      gLastActive = millis();  // opened from the shell after a quiet minute, it used to go straight home again
      if (!gIrMapped) startLearn(millis());
      break;
    case St::React:
    case St::Demo: break;  // the caller has chosen the scene
  }
}

void startPower(moves::Dance d, bool dry) {
  gDance = d;
  gDanceDry = dry;
  if (gSave.lastDance != d) {
    gSave.lastDance = d;
    writeSave();
  }
  enter(St::Power);
}

void onCue(const char* what) {
  if (gSt == St::Power && strcmp(what, "trick") == 0) gTrickCue = true;
}

void printReport(const moves::Report& r) {
  console.printf("dance %s%s: %s, base %.2f V\n", moves::name(r.dance), r.dry ? " (dry)" : "",
                 r.stop[0] ? r.stop : "finished", r.baseV);
  for (int i = 0; i < r.n; ++i) {
    const moves::StepLog& s = r.steps[i];
    console.printf("  %-7s %5u ms  peak %5d deg/s  turned %+6d deg (%.1f turns)", s.name, s.ms, s.peakDps, s.yaw,
                   s.yaw / 360.0f);
    if (s.off >= 0) console.printf("  steered, off by up to %d deg", s.off);
    console.println();
  }
}

void reactScene(const char* id, float until = 0) {
  if (!playScene(id)) return;
  gReactUntil = until;
  enter(St::React);
}

// --- The hatching --------------------------------------------------------------------------------------------------
bool crackInput(const Inputs& in) { return in.big || in.side || in.wake || in.has(motion::TAP) || in.has(motion::SHAKE); }

void crack(uint32_t now) {
  ++gCracks;
  gLastCrackMs = gLastHintMs = now;
  sound::play(sound::CRACK);
  flash(120, 110, 90, 150);
  if (gCracks >= kCracksToHatch) enter(St::Hatch);
  else player.tap();
}

void updateHatching(const Inputs& in, uint32_t now) {
  const float t = sceneTime();
  switch (gSt) {
    case St::Egg:
      // The egg sleeps until someone taps it (the scene keeps going past its length), peeping now and then.
      if (now - gLastHintMs > 10000 && t > 3) {
        gLastHintMs = now;
        player.event("hint");
        sound::play(sound::PEEP);
      }
      if (crackInput(in)) {
        enter(St::Crack);
        crack(now);
      } else if (now - gLastActive > kNapAfterMs) {
        enter(St::Doze);  // nobody about: stop peeping and go dark (a 2026-11-10 night of peeps); a touch wakes it
      }
      break;
    case St::Crack:
      if (crackInput(in)) {
        crack(now);
      } else if (now - gLastCrackMs > kSelfCrackMs) {
        crack(now);  // moves on by itself, slowly: an egg that hatches itself wasn't hatched by the kid
      } else if (now - gLastHintMs > kHintMs) {
        gLastHintMs = now;
        player.event("hint");  // a wobble and a "peep!"
        sound::play(sound::PEEP);
      }
      break;
    case St::Hatch:
      if (!gHatchSound && t > 0.85f) {
        gHatchSound = true;
        sound::play(sound::HATCH);
        flashRainbow(2000);
      }
      if (t >= sceneLength()) enter(St::Hello);
      break;
    case St::Hello:
      if (gHelloPhase == 0) {  // "Are you... " PET_KID_NAME "?"
        const bool asked = t > 1.5f;
        const bool pressed = (asked && in.big) || now - gStAt > kHelloWaitMs;
        if (pressed) {
          gHelloPhase = 1;
          gHelloAt = gThumpAt = now;
          gHoldScene = playScene("hold-button");  // the face library's heartbeat scene
          if (gHoldScene) player.event("hold");
          sound::play(sound::THUMP);
        }
      } else if (gHelloPhase == 1) {  // a heartbeat, then "You're my person!"
        if (now - gThumpAt > 700) {
          gThumpAt = now;
          sound::play(sound::THUMP);
        }
        if (now - gHelloAt > 1400) {
          gHelloPhase = 2;
          gHelloAt = now;
          if (gHoldScene) player.event("done");
          else player.button();  // "You're my person!"
          sound::play(sound::LOVE);
          flash(120, 10, 50, 1200);
        }
      } else if (now - gHelloAt > 3500) {
        enter(St::Birthday);
      }
      break;
    case St::Birthday:
      if (!gSongStarted && t >= 4.0f) {
        gSongStarted = true;
        gSongAt = now;
        sound::play(sound::BIRTHDAY);
      }
      // The song is 10.75 s; hold the confetti a moment after it (or move on anyway, e.g. when muted).
      if (gSongStarted && ((now - gSongAt > 12500 && !sound::busy()) || now - gSongAt > 16000)) {
        gSave.hatched = true;
        gSave.hatchDay = clockDay();
        if (gSave.hatchDay > gSave.lastDay) gSave.lastDay = gSave.hatchDay;
        writeSave();
        console.printf("hatched, day %ld\n", static_cast<long>(gSave.hatchDay));
        enter(St::Home);
      }
      break;
    default: break;
  }
}

// --- One frame of everything else -----------------------------------------------------------------------------------
void updatePet(const Inputs& in, uint32_t now) {
  const float t = sceneTime();
  // A button on the remote while the face is showing: straight into Trick Lab, and the press counts there.
  if (in.role >= 0 && (gSt == St::Home || gSt == St::React || gSt == St::Wake)) enter(St::Lab);
  switch (gSt) {
    case St::Lab: updateLab(in, now); break;
    case St::Home: {
      for (int i = 0; i < in.nev; ++i) react(in.ev[i], now);
      if (in.side) {
        gLastReactMs = now;
        player.event("pat");
        sound::play(sound::GIGGLE);
        flash(90, 10, 40, 200);
      }
      if (in.wake) {
        gLastReactMs = now;
        player.event("button");
        sound::play(sound::LOVE);
        flash(120, 10, 50, 800);
      }
      if (in.big) {
        sound::play(sound::CLICK);
        enter(St::Menu);
        break;
      }
      if (motion::flipped() && now - gHelpAt > 2200) {
        gHelpAt = now;
        sound::play(sound::HELP);
      }
      updateGoogly(motion::latest(), now);
      // No naps during the battery run-down: it measures the face awake.
      if (now - gLastActive > kNapAfterMs && !motion::held() && !batlog::running()) enter(St::Nap);
      break;
    }
    case St::React: {
      const float end = gReactUntil > 0 ? gReactUntil : sceneLength();
      if (in.big) {
        enter(St::Menu);
      } else if (end > 0 && t >= end) {
        enter(St::Home);
      }
      break;
    }
    case St::Menu: {
      const int step = menuTilt(motion::latest(), now);
      if (in.bigHoldMs >= kHomeHoldMs) {
        sound::play(sound::BACK);
        enter(St::Home);
      } else if (in.side) {
        gMenuSel = (gMenuSel + 1) % menu::COUNT;
        sound::play(sound::CLICK);
      } else if (step && gMenuSel + step >= 0 && gMenuSel + step < menu::COUNT) {
        gMenuSel += step;
        gLastActive = now;
        sound::play(sound::CLICK);
      } else if (in.bigTap) {
        if (gMenuSel == menu::GOLF) {
          sound::play(sound::PICK);
          enter(St::Golf);
        } else if (gMenuSel == menu::DANCE) {
          sound::play(sound::PICK);
          enter(St::Pick);
        } else if (gMenuSel == menu::LAB) {
          sound::play(sound::PICK);
          enter(St::Lab);
        } else if (gMenuSel == menu::SOUND) {  // the switch: it stays in the menu
          gSave.muted = !gSave.muted;
          sound::setMuted(gSave.muted);
          writeSave();
          if (!gSave.muted) sound::play(sound::SOUND_ON);
          gSoundPopAt = now;
          console.printf("sound %s\n", gSave.muted ? "off (menu)" : "on (menu)");
        } else {
          sound::play(sound::BACK);
          enter(St::Home);
        }
      } else if (in.wake || now - gLastActive > kMenuIdleMs) {
        enter(St::Home);
      }
      break;
    }
    case St::Pick: {
      static uint32_t lastSlide = 0;  // the deck slides toward the choice
      const float dt = fminf(0.1f, (now - lastSlide) / 1000.0f);
      lastSlide = now;
      gPick.pos += (gPick.sel - gPick.pos) * fminf(1.0f, dt * 14);
      if (in.bigHoldMs >= kHomeHoldMs) {
        sound::play(sound::BACK);
        enter(St::Home);
        break;
      }
      if (gPickAt) {  // picked: the shuffle, the bounce, then the dance
        if (gShuffling && now >= gShuffleNext) {
          gShuffleGap *= kShuffleSlow;
          if (now - gPickAt + gShuffleGap > kShuffleMs) {
            gShuffling = false;
            gPick.show = gPicked;
            gLandAt = now;
            sound::play(sound::PICK);
          } else {
            int next = static_cast<int>(esp_random() % (moves::DANCE_COUNT - 1));
            if (next >= gPick.show) ++next;  // a different animal every flip
            gPick.show = next;
            gShuffleNext = now + static_cast<uint32_t>(gShuffleGap);
            sound::play(sound::CLICK);
          }
        }
        if (!gShuffling && now - gLandAt >= kLandMs) startPower(gPicked, !gSave.motors);
        break;
      }
      const int n = menu::pickCount();
      const int step = menuTilt(motion::latest(), now);
      if (in.side) {
        gPick.sel = (gPick.sel + 1) % n;
        sound::play(sound::CLICK);
      } else if (step) {
        if (gPick.sel + step >= 0 && gPick.sel + step < n) {
          gPick.sel += step;
          sound::play(sound::CLICK);
        } else {
          gPick.pos += step * 0.12f;  // the end of the deck: a little bump
        }
        gLastActive = now;
      } else if (in.bigTap) {
        if (gPick.sel == menu::pickBack()) {
          sound::play(sound::BACK);
          enter(St::Menu);
        } else {
          gPickAt = now;
          gPick.pos = static_cast<float>(gPick.sel);
          if (gPick.sel == menu::kPickSurprise) {
            gPicked = surpriseDance();
            gShuffling = true;
            gShuffleGap = kShuffleFirstMs;
            gShuffleNext = now + kShuffleFirstMs;
            gPick.show = static_cast<int>(esp_random() % moves::DANCE_COUNT);
            sound::play(sound::CLICK);
          } else {
            gPicked = static_cast<moves::Dance>(gPick.sel - 1);
            gPick.show = gPicked;
            gLandAt = now;
            sound::play(sound::PICK);
          }
        }
      } else if (in.wake || now - gLastActive > kMenuIdleMs) {
        enter(St::Home);
      }
      break;
    }
    case St::Golf:
      if (!golfFrame(now)) {
        const bool proud = gGolfBest == 0 || gGolfBest == 2 || gGolfBest == 3 || gGolfBest == 5;
        if (proud) sound::play(sound::CHEER);
        reactScene(proud ? "mood-proud" : "mood-happy");
        if (gSt != St::React) enter(St::Home);
      }
      break;
    case St::Nap:
      if (in.any()) enter(St::Wake);
      else if (t >= 5.5f) enter(St::Doze);  // asleep and dimmed: the screen goes off (the scene wakes itself at 7 s)
      break;
    case St::Doze:
      if (in.big || in.side || in.wake || in.role >= 0 || in.has(motion::PICKUP) || in.has(motion::SHAKE) ||
          in.has(motion::TAP)) {
        enter(gSave.hatched ? St::Wake : St::Egg);
      } else if (now - gDozeAt > kDozeOffMs) {
        console.println("asleep 30 minutes: sleeping deeply (the big button wakes it)");
        delay(100);
        sleepDeep();
      }
      break;
    case St::Wake:
      if (t >= sceneLength() || in.big || in.side) enter(St::Home);
      break;
    case St::Demo:
      if (in.big || in.side || in.wake || (sceneLength() > 0 && t >= sceneLength()))
        enter(gSave.hatched ? St::Home : St::Egg);
      break;
    case St::Power: {
      // Handling events don't get reactions here: a spin looks like a pick-up, a waggle like a shake.
      const bool trickOn = sceneLength() < 1e8f;  // 1e9 until the trick starts
      if (gTrickCue) {
        gTrickCue = false;
        const bool dry = gDanceDry || !PET_MOTORS;
        if (moves::start(gDance, dry)) {
          console.printf("dancing: %s%s\n", moves::name(gDance), dry ? " (dry: lights only)" : "");
        } else {
          console.printf("no dance: %s\n", moves::why());  // the scene plays on; the robot stays put
        }
      }
      if (!gFloorSent && moves::floorReady()) {
        gFloorSent = true;
        player.event("floor");
        console.println("on the floor");
      }
      if (moves::busy() && (in.big || in.side || in.wake || in.role == lab::R_FREEZE)) {
        moves::stop("button");  // the guard has already seen it: "Freeze!" (the remote's Freeze too)
        enter(St::Home);
      } else if (in.bigHoldMs >= kHomeHoldMs) {
        sound::play(sound::BACK);
        enter(St::Home);  // held: changed its mind (a short press can't, while it asks to be put down). The press
                          // that picked Dance in the menu doesn't count, however long it's held
      } else if (in.big && !trickOn) {
        player.button();  // skips ahead through the fact
      } else if (in.big || in.side || in.wake) {
        moves::stop("button");
        enter(St::Home);
      } else if (!gFloorSent && now - gStAt > kFloorWaitMs) {
        enter(St::Home);
      } else if (trickOn && t >= sceneLength() && !moves::busy()) {
        enter(St::Home);
      }
      break;
    }
    default: updateHatching(in, now); break;
  }
}

// The base lights for each state, at most 25 times a second.
void updateLeds(uint32_t now) {
  static uint32_t last = 0;
  if (now - last < 40 || gSt == St::Doze || moves::busy()) return;  // a dance has the lights
  last = now;
  const float t = now / 1000.0f;
  if (now < gFlash.until) {
    if (gFlash.rainbow) rainbowLeds(t * 3, 90);
    else base::setLeds(gFlash.r, gFlash.g, gFlash.b);
    return;
  }
  switch (gSt) {
    case St::Egg:
    case St::Crack: {  // a warm glow, breathing
      const float v = 6 + 16 * (0.5f + 0.5f * sinf(t * 1.8f));
      base::setLeds(v, v * 0.5f, v * 0.12f);
      break;
    }
    case St::Hatch: rainbowLeds(t, 70); break;
    case St::Hello:
      if (gHelloPhase == 1) {  // heartbeat
        const float ph = fmodf((now - gHelloAt) / 700.0f, 1);
        const float v = ph < 0.12f || (ph > 0.25f && ph < 0.37f) ? 140 : 8;
        base::setLeds(v, 0, v * 0.25f);
      } else {
        base::setLeds(14, 2, 6);
      }
      break;
    case St::Birthday:
      if (gSongStarted) {
        const int beat = static_cast<int>((now - gSongAt) / 400);
        uint8_t r, g, b;
        hsv(beat * 0.17f, 110, r, g, b);
        base::setLed(beat % 2, r, g, b);
        hsv(beat * 0.17f + 0.5f, 110, r, g, b);
        base::setLed(1 - beat % 2, r, g, b);
      } else {
        rainbowLeds(t, 40);
      }
      break;
    case St::Home:
      if (motion::flipped()) {
        const bool on = static_cast<int>(t * 4) % 2;
        base::setLeds(on ? 160 : 0, 0, 0);
      } else {
        base::ledsOff();
      }
      break;
    case St::Pick: {  // the marble's colour, softly; brighter once picked. Surprise! is every colour
      const int d = gPick.show >= 0 ? gPick.show : gPick.sel - 1;
      if (d >= 0 && d < moves::DANCE_COUNT) {
        const face::C3 c = menu::danceColor(d);
        const float v = gPick.show >= 0 ? 0.45f : 0.1f + 0.04f * sinf(t * 3);
        base::setLeds(c.r * v, c.g * v, c.b * v);
      } else if (gPick.sel == menu::kPickSurprise) {
        rainbowLeds(t, 24);
      } else {
        base::ledsOff();
      }
      break;
    }
    case St::Lab:
      if (gLearning) {  // the button it's asking for, in its colour
        const face::C3 c = lab::roleColor(gLearnRole);
        const float v = 0.12f + 0.08f * sinf(t * 4);
        base::setLeds(c.r * v, c.g * v, c.b * v);
      } else {
        base::ledsOff();
      }
      break;
    case St::Power: {  // waiting for the floor: a slow gold glow, eager
      if (sceneLength() < 1e8f) {
        base::ledsOff();
        break;
      }
      const float v = 4 + 22 * (0.5f + 0.5f * sinf(t * 4));
      base::setLeds(v, v * 0.6f, 0);
      break;
    }
    default: base::ledsOff(); break;
  }
}

// --- Battery -------------------------------------------------------------------------------------------------------
int gStickMv = 0, gEmptyCount = 0;

void updateBattery(uint32_t now) {
  static uint32_t last = 0;
  if (now - last < 5000) return;
  last = now;
  gStickMv = M5.Power.getBatteryVoltage();
  if (gStickMv > 0 && gStickMv < kEmptyMv) {
    if (++gEmptyCount >= 4) {  // 20 seconds below empty: say goodnight and switch off before the clock is lost
      console.printf("battery empty (%d mV): switching off\n", gStickMv);
      batlog::finalRow();
      sound::play(sound::SAD);
      writeSave();
      delay(1200);
      switchOff();
    }
  } else {
    gEmptyCount = 0;
  }
  if (gSt == St::Home && gStickMv > 0 && gStickMv < kHungryMv && now - gHungryAt > kHungryEveryMs) {
    gHungryAt = now;
    sound::play(sound::HUNGRY);
    reactScene("hungry", 3.5f);  // only the hungry half: the munching half is for the charger
  }
}

// --- Stats ---------------------------------------------------------------------------------------------------------
bool gStatsOn = false, gTraceOn = false;
uint32_t gStatsAt = 0, gFrames = 0;
uint64_t gStepUs = 0, gDrawUs = 0, gPushUs = 0;
uint32_t gMaxDrawUs = 0;

// --- Shell ---------------------------------------------------------------------------------------------------------
void printClock() {
  m5::rtc_datetime_t dt;
  bool low = true;
  readClock(dt, low);
  console.printf("clock %04d-%02d-%02d %02d:%02d:%02d lowvolt=%d\n", dt.date.year, dt.date.month, dt.date.date,
                 dt.time.hours, dt.time.minutes, dt.time.seconds, low ? 1 : 0);
}

void printInfo() {
  const face::SceneInfo* in = player.info();
  console.printf("%s\n", kVersion);
  console.printf("state %s, scene %s, hatched %d, days %d, boots %lu\n",
                 kStateNames[static_cast<int>(gSt)], in ? in->id : "-", gSave.hatched, daysSinceHatch(),
                 static_cast<unsigned long>(gSave.boots));
  console.printf("stick %d mV (%d%%), base %.2f V, volume %d%s, rotation %d, frames %d, motors %s\n",
                 M5.Power.getBatteryVoltage(), static_cast<int>(M5.Power.getBatteryLevel()), base::volts(), static_cast<int>(gSave.volume),
                 gSave.muted ? " (muted)" : "", gRot, frames::count(),
                 !PET_MOTORS ? "not in this build" : gSave.motors ? "on" : "off");
  console.printf("heap %lu (largest %lu), psram %lu, wifi %s\n", static_cast<unsigned long>(ESP.getFreeHeap()),
                 static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL)),
                 static_cast<unsigned long>(ESP.getFreePsram()), net::address().c_str());
  console.printf("remote %s, %lu frames heard (%lu edges), latest trick %d moves\n", gIrMapped ? "learned" : "not learned",
                 static_cast<unsigned long>(ir::frames()), static_cast<unsigned long>(ir::edges()), gLast.n);
  const esp_sleep_wakeup_cause_t woke = esp_sleep_get_wakeup_cause();
  console.printf("started by %s; power latch pin %d\n",
                 woke == ESP_SLEEP_WAKEUP_EXT0 ? "the big button, from deep sleep" : woke == ESP_SLEEP_WAKEUP_TIMER ? "the sleep timer" : "power on or a reset",
                 static_cast<int>(M5.getPin(m5::pin_name_t::power_hold)));
  console.printf("built %s %s\n", __DATE__, __TIME__);
}

const char* const kHelp =
    "info | state | stats | trace | list | scene <id> | event <pickup|putdown|shake|flip|unflip|tap> |\n"
    "big | side | wake (press a button) | egg (back to unhatched) | hatch (skip the hatching) | golf | nap |\n"
    "pick (the dance picker) | dance [cheetah|honeybee|hummingbird|spider|owl|dolphin|whirligig] [dry] (none: a surprise) |\n"
    "trial (drives each way, to tune travelling) | freeze | motors on|off (off: dances are lights only) | floor |\n"
    "lab (Trick Lab) | rc <dash|scoot|spin|wiggle|go|freeze> (as if pressed on the remote) | trick (the latest) | trick forget |\n"
    "ir (the remote's buttons) | ir learn (teach it another remote) | ir forget (back to the built-in one) |\n"
    "sfx <name> | vol <0-255> | mute | unmute | bright <0-255> | rgb <r> <g> <b> | imu | axes <sx> <sy> |\n"
    "clock | clock set <y> <mo> <d> <h> <mi> <s> | days | batrun (battery run-down: Wi-Fi off, no naps) | batlog |\n"
    "wifi off | wifi forget | reboot | poweroff (fully off) |\n"
    "sleep (deep sleep now, as after a nap: the big button wakes it) | sleep <seconds> <n> (test: n short chunks)";

// batrun restarts into the run-down: Wi-Fi never starts, so the face gets both frames, as in the gift build (switching
// Wi-Fi off later gives back only 14 KB). The flag is cleared at that start, so the start after it has Wi-Fi again
// and can print the log.
void setBatrunFlag(bool on) {
  Preferences p;
  if (p.begin("pet", false)) {
    p.putBool("batrun", on);
    p.end();
  }
}
bool takeBatrunFlag() {
  Preferences p;
  bool on = false;
  if (p.begin("pet", false)) {
    on = p.getBool("batrun", false);
    if (on) p.putBool("batrun", false);
    p.end();
  }
  return on;
}

// With Wi-Fi on there's room for one frame; the gift build (Wi-Fi off) gets two (frames.h).
void tryTwoFrames() {
  frames::wait();
  const bool two = frames::addSecond(kKeepFreeBytes);
  console.printf("second frame %s, internal RAM free %lu\n", two ? "on" : "off (not enough memory)",
                 static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
}

void handleCommand(const String& line) {
  const char* c = line.c_str();
  int a = 0, b = 0, d = 0, e = 0, f = 0, g = 0;
  float x = 0, y = 0;
  char word[24] = {0};
  bool ok = true;
  if (line == "help") {
    console.println(kHelp);
  } else if (line == "info" || line == "fw") {
    printInfo();
  } else if (line == "state") {
    console.printf("state %s, scene %s at %.1f s\n", kStateNames[static_cast<int>(gSt)], gSceneId, sceneTime());
  } else if (line == "stats") {
    gStatsOn = !gStatsOn;
    gStatsAt = millis();
    gFrames = 0;
    gStepUs = gDrawUs = gPushUs = 0;
    gMaxDrawUs = 0;
    console.printf("stats %s\n", gStatsOn ? "on" : "off");
  } else if (line == "trace") {
    gTraceOn = !gTraceOn;
    console.printf("trace %s\n", gTraceOn ? "on" : "off");
  } else if (line == "list") {
    for (int i = 0; i < face::SCENE_COUNT; ++i)
      console.printf("%-18s %4.1f s  %s\n", face::SCENES[i].id, face::SCENES[i].dur, face::SCENES[i].title);
  } else if (sscanf(c, "scene %23s", word) == 1) {
    if (playScene(word)) {
      if (gSt == St::Doze) M5.Display.wakeup();
      enter(St::Demo);
    } else {
      console.println("ERR no such scene (try 'list')");
      ok = false;
    }
  } else if (sscanf(c, "event %23s", word) == 1) {
    const motion::Event ev = motion::find(word);
    if (ev == motion::NONE) {
      console.println("ERR events: pickup putdown shake flip unflip tap");
      ok = false;
    } else {
      motion::inject(ev);
    }
  } else if (line == "big") {
    gSimBig = true;
  } else if (line == "side") {
    gSimSide = true;
  } else if (line == "wake") {
    gSimWake = true;
  } else if (line == "egg") {
    gSave.hatched = false;
    gSave.hatchDay = -1;
    writeSave();
    game::resetProgress();  // the players' scores go; the Maker's records stay
    forgetTrick();          // and the trick from testing; the remote's buttons stay taught
    enter(St::Egg);
    console.println("back in the egg (golf progress and the latest trick cleared; the Maker's records and the remote kept)");
  } else if (line == "hatch") {
    gSave.hatched = true;
    gSave.hatchDay = clockDay();
    if (gSave.hatchDay > gSave.lastDay) gSave.lastDay = gSave.hatchDay;
    writeSave();
    enter(St::Home);
  } else if (line == "golf") {
    enter(St::Golf);
  } else if (line == "pick") {
    if (gSt == St::Doze) M5.Display.wakeup();
    enter(St::Pick);
  } else if (line == "dance" || line.startsWith("dance ")) {
    moves::Dance d = surpriseDance();
    bool dry = !gSave.motors;
    char w2[12] = {0};
    const int got = sscanf(c, "dance %23s %11s", word, w2);
    for (const char* w : {got >= 1 ? word : "", got >= 2 ? w2 : ""}) {
      if (!*w) continue;
      if (strcmp(w, "dry") == 0) dry = true;
      else if (!moves::find(w, d)) ok = false;
    }
    if (!ok) {
      console.println("ERR dance [cheetah|honeybee|hummingbird|spider|owl|dolphin|whirligig] [dry]");
    } else {
      if (gSt == St::Doze) M5.Display.wakeup();
      startPower(d, dry);
      console.printf("%s%s: put it on the floor\n", moves::name(d), dry || !PET_MOTORS ? " (dry: lights only)" : "");
    }
  } else if (line == "trial") {
    // Drives each way, out and back (dances.cpp, kTrial). No scene: it starts at once, if the floor check passes.
    if (moves::start(moves::TRIAL, !gSave.motors))
      console.println("trial: forward, back, left, right, left and right at 40, a veer, a circle");
    else
      console.printf("no trial: %s\n", moves::why());
  } else if (line == "freeze") {
    moves::stop("shell");
  } else if (line == "lab" || line == "ir learn") {
    if (gSt == St::Doze) M5.Display.wakeup();
    enter(St::Lab);
    if (line == "ir learn" && !gLearning) startLearn(millis());
  } else if (sscanf(c, "rc %23s", word) == 1) {
    int role = lab::NO_ROLE;
    for (int i = 0; i < lab::ROLE_COUNT; ++i)
      if (strcmp(word, lab::roleKey(i)) == 0) role = i;
    if (role < 0) {
      console.println("ERR rc <dash|scoot|spin|wiggle|go|freeze>");
      ok = false;
    } else {
      if (gSt != St::Lab) {
        if (gSt == St::Doze) M5.Display.wakeup();
        enter(St::Lab);
      }
      gLearning = false;  // the shell's presses need no remote
      gSimRole = role;
    }
  } else if (line == "trick") {
    if (gLast.n) printTrick(gLast);
    else console.println("no trick yet");
  } else if (line == "trick forget") {
    forgetTrick();
    if (gSt == St::Lab) labClear();
    console.println("latest trick forgotten");
  } else if (line == "ir") {
    for (int i = 0; i < lab::ROLE_COUNT; ++i)
      console.printf("%-7s %08lx\n", lab::roleKey(i), static_cast<unsigned long>(gIrMapped ? gIrCodes[i] : 0));
    console.printf("%s; %lu frames heard, %lu edges\n", gIrMapped ? "learned" : "not learned",
                   static_cast<unsigned long>(ir::frames()), static_cast<unsigned long>(ir::edges()));
  } else if (line == "ir forget") {
    gIrMapped = false;  // writeRemote() erases the taught one
    writeRemote();
    memcpy(gIrCodes, kRemote, sizeof(gIrCodes));
    gIrMapped = true;
    console.println("back to the built-in remote ('ir learn' teaches it another)");
  } else if (line == "motors on" || line == "motors off") {
    gSave.motors = line == "motors on";
    writeSave();
    console.printf("motors %s%s\n", gSave.motors ? "on" : "off (dances are lights only)",
                   PET_MOTORS ? "" : " (this build has no motors)");
  } else if (line == "floor") {
    console.printf("floor check %s: held %d, flipped %d, settled %.1f s, az %.2f\n",
                   moves::floorReady() ? "passes" : "fails", motion::held(), motion::flipped(),
                   motion::settledSeconds(), motion::latest().az);
  } else if (line == "nap") {
    enter(St::Nap);
  } else if (sscanf(c, "sfx %23s", word) == 1) {
    const int s = sound::find(word);
    if (s < 0) {
      console.print("ERR sounds:");
      for (int i = 0; i < sound::SFX_COUNT; ++i) console.printf(" %s", sound::name(static_cast<sound::Sfx>(i)));
      console.println();
      ok = false;
    } else {
      sound::play(static_cast<sound::Sfx>(s));
    }
  } else if (sscanf(c, "vol %d", &a) == 1) {
    gSave.volume = constrain(a, 0, 255);
    sound::setVolume(gSave.volume);
    writeSave();
    console.printf("volume %d\n", gSave.volume);
  } else if (line == "mute" || line == "unmute") {
    gSave.muted = line == "mute";
    sound::setMuted(gSave.muted);
    writeSave();
    console.printf("%s\n", gSave.muted ? "muted" : "sound on");
  } else if (sscanf(c, "bright %d", &a) == 1) {
    gBright = constrain(a, 0, 255);
    gLastBright = -1;
  } else if (sscanf(c, "rgb %d %d %d", &a, &b, &d) == 3) {
    flash(constrain(a, 0, 255), constrain(b, 0, 255), constrain(d, 0, 255), 3000);
  } else if (line == "imu") {
    for (int i = 0; i < 20; ++i) {
      const motion::Sample s = motion::latest();
      console.printf("accel %.2f %.2f %.2f  gyro %.1f %.1f %.1f  yaw %.1f  held %d\n", s.ax, s.ay, s.az, s.gx, s.gy,
                     s.gz, s.yaw, motion::held());
      delay(100);
    }
  } else if (sscanf(c, "axes %f %f", &x, &y) == 2) {
    gGravX = x < 0 ? -1 : 1;
    gGravY = y < 0 ? -1 : 1;
    console.printf("screen gravity = (%+.0f * ax, %+.0f * ay)\n", gGravX, gGravY);
  } else if (sscanf(c, "clock set %d %d %d %d %d %d", &a, &b, &d, &e, &f, &g) == 6) {
    if (a < 2000 || a > 2099 || b < 1 || b > 12 || d < 1 || d > 31 || e < 0 || e > 23 || f < 0 || f > 59 || g < 0 ||
        g > 59) {
      console.println("ERR clock set <year> <month> <day> <hour> <minute> <second>");
      ok = false;
    } else {
      m5::rtc_datetime_t dt;
      dt.date.year = a;
      dt.date.month = b;
      dt.date.date = d;
      dt.date.weekDay = weekDay(a, b, d);
      dt.time.hours = e;
      dt.time.minutes = f;
      dt.time.seconds = g;
      motion::lockBus();
      M5.Rtc.setDateTime(dt);
      motion::unlockBus();
      printClock();
    }
  } else if (line == "clock") {
    printClock();
  } else if (line == "days") {
    console.printf("days since hatching %d (hatch day %ld, last day %ld, clock day %ld)\n", daysSinceHatch(),
                   static_cast<long>(gSave.hatchDay), static_cast<long>(gSave.lastDay),
                   static_cast<long>(clockDay()));
  } else if (line == "batrun") {
    if (!gSave.hatched) {
      console.println("ERR hatch it first: the run-down measures the face at home (type 'hatch', then 'egg' after)");
      ok = false;
    } else {
      setBatrunFlag(true);
      console.println("restarting into the run-down (Wi-Fi off). The start after it has Wi-Fi again: 'batlog' prints it");
      console.println("OK");
      delay(300);
      ESP.restart();
    }
  } else if (line == "batlog") {
    batlog::print();
  } else if (line == "wifi off") {
    console.println("Wi-Fi off until the next restart");
    net::off();
    tryTwoFrames();
  } else if (line == "wifi forget") {
    console.println(net::forget() ? "Wi-Fi network and token erased" : "nothing to erase");
  } else if (line == "reboot") {
    console.println("restarting");
    console.println("OK");
    delay(200);
    ESP.restart();
  } else if (line == "sleep" || sscanf(c, "sleep %d %d", &a, &b) == 2) {
    const bool test = line != "sleep";
    if (test && (a < 5 || b < 1)) {
      console.println("ERR sleep <seconds, 5 or more> <chunks, 1 or more>");
      ok = false;
    } else {
      console.println("sleeping deeply: the big button wakes it");
      console.println("OK");
      delay(300);
      if (test) sleepDeep(a, b);
      else sleepDeep();
    }
  } else if (line == "poweroff") {
    console.println("switching off");
    console.println("OK");
    writeSave();
    delay(300);
    switchOff();
  } else {
    console.println("ERR unknown command (try 'help')");
    ok = false;
  }
  if (ok) console.println("OK");
}

void beforeUpdate() {
  moves::stop("update");
  sound::stop();
  base::ledsOff();
  frames::releaseSecond();
  setRotation(kUpRotation);
}

// Internal RAM left after each step of setup, for the boot log (the second frame needs about 115 KB).
void ramNote(const char* step) {
  Serial.printf("ram after %-7s %6lu\n", step, static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
}

}  // namespace

void setup() {
  // Asleep, and the timer woke it: look at the battery and sleep on, lighting nothing (sleepDeep()).
  const esp_sleep_wakeup_cause_t woke = esp_sleep_get_wakeup_cause();
  if (woke == ESP_SLEEP_WAKEUP_TIMER) sleepCheck();
  // Awake for real: the power pin and the backlight, held through a deep sleep, are the firmware's again.
  digitalWrite(kLatchPin, HIGH);
  pinMode(kLatchPin, OUTPUT);
  digitalWrite(kLatchPin, HIGH);
  gpio_hold_dis(kLatchPin);
  gpio_hold_dis(kBacklightPin);
  gpio_deep_sleep_hold_dis();

  // Before anything else: the base may still hold a speed from before this firmware was installed.
  base::begin();

  auto cfg = M5.config();
  cfg.serial_baudrate = 921600;
  M5.begin(cfg);
  base::stopMotors();
  M5.Display.setRotation(kUpRotation);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setBrightness(gBright);
  ramNote("M5");

  loadSave();
  ++gSave.boots;
  writeSave();
  loadLab();
  ir::begin();

  if (!frames::begin()) {
    M5.Display.drawString("no frame memory", 4, 110);
    for (;;) delay(1000);
  }
  player.begin(frames::top());
  player.scr.setFrame(frames::top(), frames::bottom());
  face::live = true;  // scenes wait for real taps and presses instead of playing their demo
  ramNote("frames");

  motion::begin();
  moves::begin();
  face::onCue = onCue;
  ramNote("motion");
  sound::begin(gSave.volume);
  sound::setMuted(gSave.muted);
  ramNote("sound");

  M5.update();
  const bool batrun = takeBatrunFlag();
  const bool wifi = !batrun && (PET_WIFI_AT_BOOT || M5.BtnB.isPressed());  // gift build: hold side at boot
  net::begin(wifi, beforeUpdate);
  ramNote("wifi");

  gLastActive = millis();
  gLastUs = micros();
  Serial.printf("%s, ready (internal RAM free %lu). Type 'help'.\n", kVersion,
                static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
  enter(gSave.hatched ? St::Wake : St::Egg);
  if (!gSave.hatched && woke == ESP_SLEEP_WAKEUP_EXT0) {  // the big button woke the egg: that push is its first crack
    enter(St::Crack);
    crack(millis());
  }
  if (batrun && gSave.hatched) batlog::start();
}

void loop() {
  const uint64_t frameStart = micros();
  const uint32_t now = millis();
  moves::feed();
  M5.update();
  net::poll(handleCommand);

  // Belt and braces: re-send the stop every few seconds while nothing dances. Harmless when the motors are off.
  static uint32_t nextStop = 0;
  if (static_cast<int32_t>(now - nextStop) >= 0 && !moves::busy()) {
    base::stopMotors();
    nextStop = now + 5000;
  }
  moves::Report report;
  if (moves::takeReport(report)) printReport(report);

  // Inputs.
  Inputs in;
  in.big = M5.BtnA.wasPressed() || gSimBig;
  in.side = M5.BtnB.wasPressed() || gSimSide;
  in.wake = M5.BtnPWR.wasClicked() || gSimWake;
  in.bigHeld = M5.BtnA.isPressed();
  if (M5.BtnA.wasPressed()) gBigDownAt = now ? now : 1;
  const bool bigHere = gBigDownAt > gStAt;  // pressed since this state began
  if (M5.BtnA.wasReleased()) {
    in.bigTap = bigHere && now - gBigDownAt < kHomeHoldMs;
    gBigDownAt = 0;
  }
  in.bigTap = in.bigTap || gSimBig;  // the shell's "big" is a tap
  in.bigHoldMs = gBigDownAt && bigHere && in.bigHeld ? now - gBigDownAt : 0;
  gHomeHoldMs = gSt == St::Menu || gSt == St::Pick || gSt == St::Power || gSt == St::Lab ? in.bigHoldMs : 0;
  gSimBig = gSimSide = gSimWake = false;
  if (ir::next(in.irCode)) {
    in.role = roleOf(in.irCode);
    console.printf("remote %08lx %s\n", static_cast<unsigned long>(in.irCode),
                   in.role >= 0 ? lab::roleKey(in.role) : "(not one it knows)");
  }
  if (gSimRole >= 0) {
    in.role = gSimRole;
    gSimRole = lab::NO_ROLE;
  }
  motion::Event ev;
  while (motion::next(ev)) {
    if (in.nev < 8) in.ev[in.nev++] = ev;
    console.printf("event %s\n", motion::name(ev));
  }
  if (in.any() || motion::held() || motion::stillSeconds() < 1) gLastActive = now;
  if (in.irCode && gSt == St::Lab) gLastActive = now;  // teaching it a remote it doesn't know yet
  if (in.big || in.side || in.wake)
    console.printf("press %s%s%s\n", in.big ? "big " : "", in.side ? "side " : "", in.wake ? "wake" : "");

  const motion::Sample s = motion::latest();
  const bool faceShown = gSt != St::Menu && gSt != St::Pick && gSt != St::Lab && gSt != St::Golf && gSt != St::Doze;
  if (gSt != St::Doze) updateRotation(s, now, faceShown);

  updatePet(in, now);
  updateBattery(now);
  batlog::loop(now);
  updateLeds(now);

  if (gTraceOn) {
    static uint32_t lastTrace = 0;
    if (now - lastTrace >= 100) {  // 10 a second; the peaks cover what happened in between
      lastTrace = now;
      const motion::Trace tr = motion::trace();
      console.printf("trace |a| %.2f (%.2f-%.2f) az %+.2f |g| %5.1f pk %5.1f still %4.1f shake %.2f pk %.2f "
                     "jerk pk %.2f tilt %4.1f held %d flipped %d\n",
                     tr.amag, tr.aMin, tr.aMax, tr.az, tr.gmag, tr.gPeak, tr.still, tr.shake, tr.shakePeak,
                     tr.jerkPeak, tr.tilt, tr.held, tr.flipped);
    }
  }

  // The face's second frame, once Wi-Fi has connected and sound has started and both have taken their memory.
  static bool secondTried = false;
  if (!secondTried && now > kSecondFrameAtMs) {
    secondTried = true;
    tryTwoFrames();
  }

  // Draw.
  if (gSt == St::Doze) {
    gLastUs = micros();
    delay(40);
    return;
  }
  if (gSt == St::Golf) {
    gLastUs = micros();  // golf drew and sent its own frame in updatePet
  } else {
    const uint64_t t0 = micros();
    double dt = (t0 - gLastUs) / 1e6;
    gLastUs = t0;
    uint64_t t1 = t0;
    if (gSt == St::Menu || gSt == St::Pick || gSt == St::Lab) {
      frames::ready();
      if (gSt == St::Menu) drawMenu(now);
      else if (gSt == St::Pick) drawPicker(now);
      else drawLab(now);
      drawHomeRing(gMenuScreen, now);
    } else {
      player.step(dt);
      t1 = micros();
      frames::ready();
      player.scr.setFrame(frames::top(), frames::bottom());
      player.draw();
      drawHomeRing(player.scr, now);
    }
    const uint64_t t2 = micros();
    const float dim = gSt == St::Menu || gSt == St::Pick || gSt == St::Lab ? 1.0f : face::clampf(player.scr.dim, 0, 1);
    const int bright = static_cast<int>(gBright * dim + 0.5f);
    if (bright != gLastBright) {
      M5.Display.setBrightness(bright);
      gLastBright = bright;
    }
    frames::present();
    const uint64_t t3 = micros();
    if (gStatsOn) {
      ++gFrames;
      gStepUs += t1 - t0;
      gDrawUs += t2 - t1;
      gPushUs += t3 - t2;
      gMaxDrawUs = max(gMaxDrawUs, static_cast<uint32_t>(t2 - t1));
    }
  }
  if (gStatsOn && now - gStatsAt > 3000) {
    if (gFrames) {
      console.printf("%-12s %5.1f fps  step %4.1f  draw avg %5.1f max %5.1f  push %4.1f ms  heap %lu\n", gSceneId,
                     gFrames * 1000.0f / (now - gStatsAt), gStepUs / 1000.0f / gFrames, gDrawUs / 1000.0f / gFrames,
                     gMaxDrawUs / 1000.0f, gPushUs / 1000.0f / gFrames, static_cast<unsigned long>(ESP.getFreeHeap()));
    }
    gStatsAt = now;
    gFrames = 0;
    gStepUs = gDrawUs = gPushUs = 0;
    gMaxDrawUs = 0;
  }

  const uint64_t spent = micros() - frameStart;
  if (spent + 1000 < kFramePeriodUs) delay((kFramePeriodUs - spent) / 1000);  // delay() lets the Wi-Fi task run
}
