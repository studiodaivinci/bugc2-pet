#include "sfx.h"

#include <math.h>
#include <string.h>

namespace sound {
namespace {

// Notes, an octave up from the usual: the buzzer is much louder above 1 kHz.
constexpr uint16_t G5 = 784, A5 = 880, B5 = 988, C6 = 1047, D6 = 1175, E6 = 1319, F6 = 1397, G6 = 1568, A6 = 1760,
                   B6 = 1976, C7 = 2093, D7 = 2349, E7 = 2637, G7 = 3136, C8 = 4186;

// Happy Birthday in 3/4, a crotchet = 400 ms. "Hap-py" is a dotted quaver and a semiquaver. Line 3 is "dear (name)", two notes.
const Note kBirthday[] = {
    {G5, 300}, {G5, 100}, {A5, 400}, {G5, 400}, {C6, 400}, {B5, 800}, {0, 150},
    {G5, 300}, {G5, 100}, {A5, 400}, {G5, 400}, {D6, 400}, {C6, 800}, {0, 150},
    {G5, 300}, {G5, 100}, {G6, 400}, {E6, 400}, {C6, 400}, {B5, 400}, {A5, 800}, {0, 150},
    {F6, 300}, {F6, 100}, {E6, 400}, {C6, 400}, {D6, 400}, {C6, 1100}, {0, 0}};

const Note kCrack[] = {{2200, 12}, {0, 25}, {1500, 14}, {0, 20}, {2600, 10}, {0, 0}};
const Note kPeep[] = {{2800, 50}, {0, 70}, {3200, 70}, {0, 0}};
const Note kHatch[] = {{C6, 90}, {E6, 90}, {G6, 90}, {C7, 120}, {0, 40}, {E7, 90}, {G7, 260}, {0, 0}};
const Note kThump[] = {{700, 45}, {0, 90}, {600, 60}, {0, 0}};
const Note kLove[] = {{G6, 60}, {A6, 60}, {B6, 60}, {C7, 60}, {D7, 160}, {0, 0}};
const Note kHelp[] = {{2400, 90}, {0, 70}, {2400, 90}, {0, 0}};
const Note kThanks[] = {{E6, 80}, {A6, 140}, {0, 0}};
const Note kGiggle[] = {{C7, 40}, {0, 30}, {D7, 40}, {0, 30}, {E7, 70}, {0, 0}};
const Note kClick[] = {{3000, 8}, {0, 0}};
const Note kPick[] = {{G6, 40}, {C7, 90}, {0, 0}};
const Note kBack[] = {{G6, 40}, {D6, 90}, {0, 0}};
const Note kWake[] = {{C6, 80}, {G6, 80}, {C7, 140}, {0, 0}};
const Note kHungry[] = {{600, 150}, {0, 80}, {500, 260}, {0, 0}};
const Note kCheer[] = {{G6, 80}, {C7, 80}, {E7, 80}, {G7, 220}, {0, 0}};
const Note kSad[] = {{E6, 160}, {D6, 160}, {C6, 320}, {0, 0}};
const Note kSoundOn[] = {{C7, 70}, {E7, 70}, {G7, 70}, {C8, 180}, {0, 0}};

// Dance sounds that need no building. Each is timed to its step (dances.cpp has the steps' lengths).
const Note kBloom[] = {{C7, 80}, {E7, 80}, {G7, 80}, {C8, 260}, {0, 70}, {G7, 40}, {C8, 140}, {0, 0}};  // a flower opens
const Note kTiptoe[] = {{3200, 6}, {0, 82}, {2800, 6}, {0, 82}, {3400, 6}, {0, 82}, {3000, 6}, {0, 0}};  // 8 little feet
const Note kBlink[] = {{2600, 14}, {0, 50}, {2300, 14}, {0, 0}};

const Note* const kFixed[] = {
    kCrack, kPeep, kHatch, kThump, kLove, nullptr, nullptr, nullptr, kHelp, kThanks, kGiggle,
    kClick, kPick, kBack, nullptr, kWake, kHungry, kCheer, kBirthday, kSad,
    kSoundOn,
    nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, kBloom,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    kTiptoe, nullptr, nullptr,
    kBlink, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr};

const char* const kNames[] = {
    "crack", "peep", "hatch", "thump", "love", "wheee", "phew", "dizzy", "help", "thanks", "giggle",
    "click", "pick", "back", "yawn", "wake", "hungry", "cheer", "birthday", "sad",
    "soundon",
    "revup", "zoom", "skid", "pant",
    "bzz", "wiggle", "swoop", "bloom",
    "hum", "dart", "sip", "dive", "chirp", "zipback",
    "tiptoe", "tremble", "eek",
    "blink", "creak", "hoo", "hoot",
    "chatter", "leap", "splash", "bubbles", "bigleap",
    "windup", "rev", "whirl", "comets", "galaxy", "slowdown",
    "tdash", "tscoot", "tspin", "twiggle"};
static_assert(sizeof(kFixed) / sizeof(kFixed[0]) == SFX_COUNT, "a sound missing from kFixed");
static_assert(sizeof(kNames) / sizeof(kNames[0]) == SFX_COUNT, "a sound missing from kNames");

// The built sounds share one pool.
constexpr int kPool = 2200;
Note gPool[kPool];
int gUsed = 0;
const Note* gBuilt[SFX_COUNT] = {};

// Writes one sequence into the pool.
struct W {
  Sfx id;
  int start;
  explicit W(Sfx s) : id(s), start(gUsed) {}
  void n(float hz, float ms) {
    if (gUsed < kPool - 1 && ms >= 1) gPool[gUsed++] = {static_cast<uint16_t>(hz), static_cast<uint16_t>(ms)};
  }
  void rest(float ms) { n(0, ms); }
  // `steps` notes of `ms` each, gliding from one pitch to another (evenly in pitch, as the ear hears it)
  void sweep(int steps, float from, float to, float ms) {
    for (int i = 0; i < steps; ++i) n(from * powf(to / from, steps > 1 ? static_cast<float>(i) / (steps - 1) : 0), ms);
  }
  ~W() {  // the end mark (a full pool cuts the last sound short rather than overrun)
    if (gUsed >= kPool) gUsed = kPool - 1;
    gPool[gUsed++] = {0, 0};
    gBuilt[id] = gPool + (start < gUsed ? start : gUsed - 1);
  }
};

uint32_t gSeed = 0x9e3779b9u;
float rnd() {  // 0..1, the same every build
  gSeed = gSeed * 1664525u + 1013904223u;
  return (gSeed >> 8) / 16777216.0f;
}

}  // namespace

void build() {
  if (gUsed) return;
  const float kTau = 6.2831853f;
  // The everyday ones
  { W w(WHEEE); w.sweep(15, 900, 2600, 22); }
  { W w(PHEW); w.sweep(11, 1800, 700, 32); }
  { W w(DIZZY); for (int i = 0; i < 16; ++i) w.n(i % 2 ? 1500 - i * 20 : 1200 - i * 20, 55); }
  { W w(YAWN); w.sweep(13, 1300, 600, 50); }

  // Cheetah. REVUP (600 ms): an engine revving, brrm faster and higher, as the lights blink faster.
  {
    W w(REVUP);
    for (float t = 0; t < 590;) {
      const float k = t / 600, on = 40 - 28 * k, off = 30 - 24 * k;
      w.n(700 * powf(1800 / 700.0f, k), on);
      w.rest(off);
      t += on + off;
    }
  }
  // ZOOM (the 1.6 s spin): a high whine climbing as it goes, with a fast wobble.
  { W w(ZOOM); for (int i = 0; i < 80; ++i) w.n(1900 + 700 * i / 79.0f + 160 * sinf(i * kTau / 6), 20); }
  { W w(SKID); w.sweep(20, 2600, 900, 20); }  // coasting to a stop (400 ms)
  // PANT (2 s): huh-hah, in time with the face's panting (a breath every 286 ms)
  { W w(PANT); for (int i = 0; i < 7; ++i) { w.n(1250, 50); w.n(950, 40); w.rest(195); } }

  // Honeybee. BZZ (500 ms): two buzzes with the two yellow blinks (0-90 and 180-270 ms).
  {
    W w(BZZ);
    for (int b = 0; b < 2; ++b) {
      for (int i = 0; i < 9; ++i) w.n(i % 2 ? 1200 : 1050, 10);
      w.rest(90);
    }
  }
  // WIGGLE (600 ms): buzzing that jumps up and down with each 60 ms waggle
  {
    W w(WIGGLE);
    for (int s = 0; s < 10; ++s)
      for (int i = 0; i < 6; ++i) w.n((s % 2 ? 1350 : 1100) + (i % 2) * 120, 10);
  }
  { W w(SWOOP); w.sweep(11, 1200, 2000, 20); w.sweep(11, 2000, 1300, 20); }  // looping back round (450 ms)

  // Hummingbird. HUM (1.2 s): a fast buzzy hum, drifting up and down as it hovers.
  { W w(HUM); for (int i = 0; i < 120; ++i) w.n(1550 + 80 * sinf(i * kTau / 60) + (i % 2) * 150, 10); }
  { W w(DART); w.sweep(9, 1600, 4000, 10); w.n(3600, 30); }  // zip! (300 ms step)
  { W w(ZIPBACK); w.sweep(9, 4000, 1600, 10); w.n(1900, 30); }  // flying backwards out of the flower
  // SIP (at a flower, 0.9 s): slurp, slurp, slurp
  {
    W w(SIP);
    for (int i = 0; i < 3; ++i) {
      w.sweep(12, 2200 - 100 * i, 1200, 15);
      w.rest(80);
    }
  }
  // DIVE (0.7 s): the courtship dive, swooping down and back up, zoooo-WHEE
  { W w(DIVE); w.sweep(15, 3400, 1300, 20); w.sweep(20, 1300, 3800, 20); }
  // CHIRP (happy, 1.5 s): tweet-tweet, tweet-tweet, mmm!
  {
    W w(CHIRP);
    for (int i = 0; i < 2; ++i) {
      w.n(G7, 45);
      w.rest(35);
      w.n(C8, 60);
      w.rest(i ? 300 : 160);
    }
    w.n(G6, 90);
    w.n(C7, 220);
  }

  // Spider. TREMBLE (2.3 s): a nervous trill with every 30 ms shiver, calming down as it goes.
  {
    W w(TREMBLE);
    for (int i = 0; i < 77; ++i) w.n(1950 - 450 * i / 76.0f + (i % 2) * 150, 30);
  }
  // EEK (480 ms): eek! eek!
  {
    W w(EEK);
    w.sweep(12, 2400, 4200, 10);
    w.n(4200, 60);
    w.rest(50);
    w.sweep(10, 2800, 4400, 10);
    w.n(4400, 60);
  }

  // Owl. CREAK (400 ms): the head turning, a slow ratchet.
  { W w(CREAK); for (int i = 0; i < 10; ++i) { w.n(1300 + 50 * i, 10); w.rest(30); } }
  { W w(HOO); w.sweep(14, 1100, 930, 25); }  // one hoo (0.5 s step)
  // HOOT (1.4 s): hoo... hoo-hoo, each dropping a little
  {
    W w(HOOT);
    w.sweep(14, 1100, 930, 25);
    w.rest(130);
    w.sweep(7, 1050, 960, 25);
    w.rest(60);
    w.sweep(14, 1100, 930, 25);
  }

  // Dolphin. CHATTER (750 ms): clicks, then a whistle up and down.
  {
    W w(CHATTER);
    for (int i = 0; i < 8; ++i) { w.n(4200, 5); w.rest(22); }
    w.sweep(10, 2200, 3600, 20);
    w.sweep(10, 3600, 2500, 20);
  }
  { W w(LEAP); w.sweep(25, 2000, 4200, 20); }  // wheee, through one leap (500 ms)
  // BIGLEAP (1.2 s): whee-tip! four times, on the screen's counts 4, 5, 6, 7 (0.05, 0.35, 0.65 and 0.95 s in)
  {
    W w(BIGLEAP);
    w.rest(50);
    for (int i = 0; i < 4; ++i) {
      w.sweep(5, 2000 + 200 * i, 3600 + 200 * i, 20);
      w.n(4000 + 150 * i, 60);
      w.rest(140);
    }
  }
  // SPLASH: a burst of noise, then droplets
  {
    W w(SPLASH);
    for (int i = 0; i < 30; ++i) w.n(1500 + 3500 * rnd(), 8);
    for (int i = 0; i < 8; ++i) { w.n(2500 + 2000 * rnd(), 8); w.rest(20 + 40 * rnd()); }
  }
  // BUBBLES (1.7 s): bloop... bloop, each a different size
  {
    W w(BUBBLES);
    for (int i = 0; i < 7; ++i) {
      const float lo = 800 + 300 * rnd();
      w.sweep(5, lo, lo * 1.9f, 12);
      w.rest(120 + 110 * rnd());
    }
  }

  // Whirligig. WINDUP (800 ms): clockwork winding, the clicks coming faster.
  {
    W w(WINDUP);
    for (float t = 0; t < 780;) {
      const float k = t / 800, gap = 110 - 88 * k;
      w.n(2500 + 800 * k, 6);
      w.rest(gap);
      t += 6 + gap;
    }
  }
  { W w(REV); w.sweep(50, 800, 2300, 20); }  // speeding up through rev1-rev3 (1 s)
  // Trick Lab's moves (lab.cpp, kMoves): the animals' own sounds, cut to the length of each move.
  // T_DASH (700 ms): the cheetah's whine, climbing. T_SCOOT (600 ms): eek! eek! eek!
  { W w(T_DASH); for (int i = 0; i < 35; ++i) w.n(1700 + 1300 * i / 34.0f + 160 * sinf(i * kTau / 6), 20); }
  {
    W w(T_SCOOT);
    for (int i = 0; i < 3; ++i) {
      w.sweep(8, 2400 + 200 * i, 4200 + 100 * i, 10);
      w.n(4200 + 100 * i, 50);
      if (i < 2) w.rest(75);
    }
  }
  // T_SPIN (1 s): the whirligig's siren going round. T_WIGGLE (840 ms): the bee's buzz, hopping with each waggle.
  { W w(T_SPIN); for (int i = 0; i < 50; ++i) { const int p = i % 12; w.n(2000 * powf(1.4f, (p < 6 ? p : 12 - p) / 6.0f), 20); } }
  {
    W w(T_WIGGLE);
    for (int s = 0; s < 14; ++s)
      for (int i = 0; i < 6; ++i) w.n((s % 2 ? 1350 : 1100) + (i % 2) * 120, 10);
  }
  // WHIRL (the halo, 2 s): a siren going round, wee-oo wee-oo
  { W w(WHIRL); for (int i = 0; i < 100; ++i) { const int p = i % 12; w.n(2000 * powf(1.4f, (p < 6 ? p : 12 - p) / 6.0f), 20); } }
  // COMETS (1.7 s): a lower whirl with a bright ping for each comet going by
  {
    W w(COMETS);
    for (int i = 0; i < 85; ++i) {
      const int p = i % 12;
      w.n(i % 21 < 2 ? C8 : 1800 * powf(1.33f, (p < 6 ? p : 12 - p) / 6.0f), 20);
    }
  }
  // GALAXY (1.7 s): a twinkling arpeggio rolling round and round
  {
    W w(GALAXY);
    const uint16_t arp[6] = {C7, E7, G7, C8, G7, E7};
    for (int i = 0; i < 42; ++i) w.n(arp[i % 6], 40);
  }
  { W w(SLOWDOWN); w.sweep(25, 2400, 700, 20); }  // winding down (500 ms)
}

const Note* sequence(Sfx s) {
  if (s < 0 || s >= SFX_COUNT) return nullptr;
  return kFixed[s] ? kFixed[s] : gBuilt[s];
}

uint32_t lengthMs(Sfx s) {
  uint32_t ms = 0;
  for (const Note* n = sequence(s); n && n->ms; ++n) ms += n->ms;
  return ms;
}

const char* name(Sfx s) { return s >= 0 && s < SFX_COUNT ? kNames[s] : "?"; }

int find(const char* n) {
  for (int i = 0; i < SFX_COUNT; ++i)
    if (strcmp(n, kNames[i]) == 0) return i;
  return -1;
}

}  // namespace sound
