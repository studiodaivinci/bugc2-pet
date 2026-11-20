#include "dances.h"

#include <string.h>

namespace moves {
namespace {

namespace snd = sound;
constexpr snd::Sfx SAME = snd::NONE;  // no new sound: the one before plays on

// Travelling (2026-11-11: each dance moves about a 1-2 foot space, like the animal it mimics): DRIVE
// and STRAFE steps go about 30 cm/s at speed 30, so a dance's travelling adds up to a foot or two, and most come
// back to where they began. The owl doesn't travel: it perches, and its head does the moving.
//
// The cheetah stalks forward, pounces, then chases round and round, takes a breath, chases again, coasts and pants.
// Timed to the screen (face_scenes.cpp, Cheetah): the whoosh runs 0.05-3.4 s after the cue with a dip for the
// breath, "again!" at 2.5 s, the panting from 3.8 s. Spin speed 45, timed: 1.5 s at about 45
// (speed 70 logged 5 g jolts for little extra spin).
const Step kCheetah[] = {
    {"stalk", DRIVE, 30, 1, 600, 0, 0, L_REVUP, 0, snd::REVUP},
    {"pounce", DRIVE, 42, 1, 300, 0, 0, L_STREAK, 0, snd::ZOOM},
    {"sprint", SPIN, 45, 1, 1300, 0, 0, L_STREAK, 0, SAME},
    {"breath", LIGHTS, 0, 0, 300, 0, 0, L_COAST, 0, snd::SKID},
    {"again", SPIN, 45, 1, 900, 0, 0, L_STREAK, 0, snd::ZOOM},
    {"coast", LIGHTS, 0, 0, 400, 0, 0, L_COAST, 0, snd::SKID},
    {"pant", LIGHTS, 0, 0, 2000, 0, 0, L_PANT, 0, snd::PANT},
};
// The honeybee's waggle dance (2026-11-11, after pet 0.6's figure of eight moved forward too much: waggle, step
// forward a little, waggle again, for the whole dance). It
// waggles on the spot (shaketest's 8 Hz at 40, the waggle of pet 0.5), takes one small step forward (0.25 s at 30:
// a few centimetres), and waggles again: five waggles and four steps, about a foot in all, the last waggle a little
// longer. The steps steer back to the heading of the first one, so the waggles' drift doesn't bend the line. The
// screen dances 0.15-4.7 s after the cue; the bloom lasts to the scene's end.
const Step kHoneybee[] = {
    {"bzz", LIGHTS, 0, 0, 500, 0, 0, L_BZZ, 0, snd::BZZ},
    {"waggle", WAGGLE, 40, 1, 600, 60, 0, L_WAGGLE, 0, snd::WIGGLE},
    {"step", DRIVE, 30, 1, 250, 0, 0, L_SWING, 0, snd::SWOOP},
    {"waggle", WAGGLE, 40, 1, 600, 60, 0, L_WAGGLE, 0, snd::WIGGLE},
    {"step", DRIVE, 30, 1, 250, 0, 0, L_SWING, 0, snd::SWOOP},
    {"waggle", WAGGLE, 40, 1, 600, 60, 0, L_WAGGLE, 0, snd::WIGGLE},
    {"step", DRIVE, 30, 1, 250, 0, 0, L_SWING, 0, snd::SWOOP},
    {"waggle", WAGGLE, 40, 1, 600, 60, 0, L_WAGGLE, 0, snd::WIGGLE},
    {"step", DRIVE, 30, 1, 250, 0, 0, L_SWING, 0, snd::SWOOP},
    {"waggle", WAGGLE, 40, 1, 780, 60, 0, L_WAGGLE, 0, snd::WIGGLE},
    {"bloom", LIGHTS, 0, 0, 1200, 0, 0, L_BLOOM, 5900, snd::BLOOM},
};
// The hummingbird visits two flowers and shows off (reworked 2026-11-11). It hovers
// (shaketest's 33 Hz buzz: 15 ms reversals, a light buzz with no net turn, HARDWARE.md), darts forward and to the
// left to a flower and sips there still buzzing, flies backwards out of it (hummingbirds are the only birds that
// can), darts to a flower on the right and sips, backs out, then does a courtship dive (a fast spin), brakes,
// hovers, and glows pink, full of nectar. The screen: face_scenes.cpp, Hummingbird (darts 1.1 and 2.6 s, sips 1.4
// and 2.9 s, the dive 3.8 s, happy from 5.1 s; the scene ends at 6.6 s).
const Step kHummingbird[] = {
    {"hum", LIGHTS, 0, 0, 300, 0, 0, L_HUM, 0, snd::HUM},
    {"hover", WAGGLE, 40, 1, 800, 15, 0, L_HUM, 0, SAME},
    {"dart", DRIVE, 42, 1, 300, 0, 0, L_DART, 0, snd::DART, 45, 0},
    {"sip", WAGGLE, 40, 1, 900, 15, 0, L_NECTAR, 0, snd::SIP},
    {"backout", DRIVE, 42, -1, 300, 0, 0, L_DART, 0, snd::ZIPBACK, 45, 0},
    {"dart", DRIVE, 42, 1, 300, 0, 0, L_DART, 0, snd::DART, -45, 0},
    {"sip", WAGGLE, 40, 1, 600, 15, 0, L_NECTAR, 0, snd::SIP},
    {"backout", DRIVE, 42, -1, 300, 0, 0, L_DART, 0, snd::ZIPBACK, -45, 0},
    {"dive", SPIN, 45, 1, 700, 0, 0, L_DIVE, 0, snd::DIVE},
    {"brake", LIGHTS, 0, 0, 250, 0, 0, L_DIVE, 0, SAME},
    {"hover", WAGGLE, 40, 1, 350, 15, 0, L_HUM, 0, snd::HUM},
    {"happy", LIGHTS, 0, 0, 1500, 0, 0, L_SIP, 6600, snd::CHIRP},
};
// The spider is scared, twice: it skitters forward (a wiggling drive), trembles (17 Hz, 30 ms reversals), jumps
// back at "eek!" (scene 2.2 s), trembles, scrambles forward again at the second "eek!" (3.98 s), trembles until the
// face calms down (5.76 s), then sighs. Trembling at speed 40, not lower: 25 no longer moves it at all (README), and
// a shiver that doesn't move trips the turn check.
const Step kSpider[] = {
    {"creep", DRIVE, 32, 1, 350, 30, 18, L_CREEP, 0, snd::TIPTOE},
    {"shiver", WAGGLE, 40, 1, 400, 30, 0, L_TREMBLE, 0, snd::TREMBLE},
    {"eek", DRIVE, 40, -1, 480, 60, 25, L_EEK, 0, snd::EEK},
    {"shiver", WAGGLE, 40, 1, 1300, 30, 0, L_TREMBLE, 0, snd::TREMBLE},
    {"eek", DRIVE, 40, 1, 480, 60, 25, L_EEK, 0, snd::EEK},
    {"shiver", WAGGLE, 40, 1, 1300, 30, 0, L_TREMBLE, 0, snd::TREMBLE},
    {"phew", LIGHTS, 0, 0, 1300, 0, 0, L_PHEW, 5600, snd::PHEW},
};
// The owl: the 270° happens in its eyes (so no turn-to-an-angle is needed). The body stares, blinks with the
// face (scene 1.9 s), turns as the head swings away (2.3 s) and back as it comes round the other side (3.2 s), hoots,
// then looks round the other way (4.1 s, back at 5.0 s) and hoots twice. The scene ends at 6.8 s.
const Step kOwl[] = {
    {"stare", LIGHTS, 0, 0, 450, 0, 0, L_STARE, 0, SAME},
    {"blink", LIGHTS, 0, 0, 200, 0, 0, L_BLINK, 0, snd::BLINK},
    {"stare", LIGHTS, 0, 0, 200, 0, 0, L_STARE, 850, SAME},
    {"look", TURN, 35, 1, 400, 0, 0, L_LOOK, 0, snd::CREAK},
    {"hold", LIGHTS, 0, 1, 900, 0, 0, L_LOOK, 1750, SAME},
    {"back", TURN, 35, -1, 400, 0, 0, L_LOOK, 0, snd::CREAK},
    {"hoo", LIGHTS, 0, 0, 500, 0, 0, L_HOO, 2650, snd::HOO},
    {"look", TURN, 35, -1, 400, 0, 0, L_LOOK, 0, snd::CREAK},
    {"hold", LIGHTS, 0, -1, 900, 0, 0, L_LOOK, 3550, SAME},
    {"back", TURN, 35, 1, 400, 0, 0, L_LOOK, 0, snd::CREAK},
    {"hoo", LIGHTS, 0, 0, 1500, 0, 0, L_HOO, 5350, snd::HOOT},
};
// The spinner dolphin swims in on a curve, then three leaps, each a spin with a splash, as the screen counts "1!",
// "2!", "3!" (scene 2.2, 3.1 and 4.0 s), then the big leap, spinning 1.2 s as the screen counts on to "7!" (4.8-6.0
// s), a big splash, and it swims off on a curve through the bubbles until the scene ends (7.9 s). The small leaps
// 0.5 s each: on 2026-11-11 the gyro stopped them at 470-540 ms.
const Step kDolphin[] = {
    {"swim", DRIVE, 30, 1, 750, 0, 0, L_SWIM, 0, snd::CHATTER, 0, 90},
    {"spin", TURN, 45, 1, 500, 0, 0, L_SPRAY, 0, snd::LEAP},
    {"splash", LIGHTS, 0, 0, 900, 0, 0, L_SPLASH, 1650, snd::SPLASH},
    {"spin", TURN, 45, 1, 500, 0, 0, L_SPRAY, 0, snd::LEAP},
    {"splash", LIGHTS, 0, 0, 900, 0, 0, L_SPLASH, 2550, snd::SPLASH},
    {"spin", TURN, 45, 1, 500, 0, 0, L_SPRAY, 0, snd::LEAP},
    {"splash", LIGHTS, 0, 0, 800, 0, 0, L_SPLASH, 3350, snd::SPLASH},
    {"bigleap", SPIN, 45, 1, 1200, 0, 0, L_SPRAY, 0, snd::BIGLEAP},
    {"splash", LIGHTS, 0, 0, 800, 0, 0, L_SPLASH, 5100, snd::SPLASH},
    {"swim", DRIVE, 30, 1, 800, 0, 0, L_BUBBLES, 0, snd::BUBBLES, 0, -90},
    {"bubbles", LIGHTS, 0, 0, 550, 0, 0, L_BUBBLES, 6450, SAME},
};
// The whirligig: the long spin, with a light show (2026-11-11: a fast spin for a good while, the
// lights and screen making a pattern as it spins; then made a bit longer). It paddles half way round a small circle as it winds up, speeds up in three
// steps, whirls about 5.4 s at the cheetah's 45 (about 27 turns), slows, and is dizzy until the scene ends (11.4 s).
// While it spins, the lights are fixed to the room, not to the robot (roomLocked): a rainbow ring that stands still,
// then three comets frozen in the air, then a galaxy turning slowly backwards. Timed to the scene: face_scenes.cpp,
// Whirligig.
const Step kWhirligig[] = {
    {"paddle", DRIVE, 30, 1, 800, 0, 0, L_WINDUP, 0, snd::WINDUP, 0, 225},  // half way round a small circle
    {"rev1", SPIN, 30, 1, 350, 0, 0, L_HALO, 0, snd::REV},
    {"rev2", SPIN, 37, 1, 350, 0, 0, L_HALO, 0, SAME},
    {"rev3", SPIN, 42, 1, 300, 0, 0, L_HALO, 0, SAME},
    {"halo", SPIN, 45, 1, 2000, 0, 0, L_HALO, 0, snd::WHIRL},
    {"comets", SPIN, 45, 1, 1700, 0, 0, L_COMETS, 0, snd::COMETS},
    {"galaxy", SPIN, 45, 1, 1700, 0, 0, L_GALAXY, 0, snd::GALAXY},
    {"slow", SPIN, 30, 1, 500, 0, 0, L_SLOWHALO, 0, snd::SLOWDOWN},
    {"dizzy", LIGHTS, 0, 0, 2250, 0, 0, L_DIZZY, 9950, snd::DIZZY},
};

// The trial (the shell's "trial"): each way of travelling, out and back, with a rest between, to see on the floor
// how far and how straight each goes. Forward and back are the bench's tested moves; left and right had never run.
const Step kTrial[] = {
    {"ready", LIGHTS, 0, 0, 800, 0, 0, L_WINDUP, 0, snd::PEEP},
    {"fwd", DRIVE, 30, 1, 700, 0, 0, L_SWIM, 0, snd::CLICK},
    {"rest", LIGHTS, 0, 0, 600, 0, 0, L_BLINK, 0, SAME},
    {"back", DRIVE, 30, -1, 700, 0, 0, L_SWIM, 0, snd::CLICK},
    {"rest", LIGHTS, 0, 0, 1200, 0, 0, L_BLINK, 0, snd::PEEP},
    {"left", STRAFE, 30, 1, 700, 0, 0, L_HUM, 0, snd::CLICK},
    {"rest", LIGHTS, 0, 0, 600, 0, 0, L_BLINK, 0, SAME},
    {"right", STRAFE, 30, -1, 700, 0, 0, L_HUM, 0, snd::CLICK},
    {"rest", LIGHTS, 0, 0, 1200, 0, 0, L_BLINK, 0, snd::PEEP},
    {"left40", STRAFE, 40, 1, 700, 0, 0, L_EEK, 0, snd::CLICK},
    {"rest", LIGHTS, 0, 0, 600, 0, 0, L_BLINK, 0, SAME},
    {"right40", STRAFE, 40, -1, 700, 0, 0, L_EEK, 0, snd::CLICK},
    {"rest", LIGHTS, 0, 0, 1200, 0, 0, L_BLINK, 0, snd::PEEP},
    {"veer", DRIVE, 30, 1, 600, 0, 0, L_DART, 0, snd::CLICK, 45, 0},  // how fast can it swing to a new heading?
    {"rest", LIGHTS, 0, 0, 600, 0, 0, L_BLINK, 0, SAME},
    {"unveer", DRIVE, 30, -1, 600, 0, 0, L_DART, 0, snd::CLICK, 45, 0},
    {"rest", LIGHTS, 0, 0, 1200, 0, 0, L_BLINK, 0, snd::PEEP},
    {"circle", DRIVE, 30, 1, 1600, 0, 0, L_HALO, 0, snd::CLICK, 0, 225},  // once round, about 15 cm across
};

#define PROGRAM(s) {s, sizeof(s) / sizeof(Step)}
const Program kPrograms[DANCE_COUNT] = {PROGRAM(kCheetah), PROGRAM(kHoneybee), PROGRAM(kHummingbird),
                                        PROGRAM(kSpider),  PROGRAM(kOwl),      PROGRAM(kDolphin),
                                        PROGRAM(kWhirligig)};
const Program kTrialProgram = PROGRAM(kTrial);
#undef PROGRAM
Step gLab[kLabSteps];
Program gLabProgram = {gLab, 0};
const char* const kNames[DANCE_COUNT] = {"cheetah", "honeybee", "hummingbird", "spider",
                                         "owl",     "dolphin",  "whirligig"};

}  // namespace

const Program& program(Dance d) { return d < DANCE_COUNT ? kPrograms[d] : d == LAB ? gLabProgram : kTrialProgram; }

void setLab(const Step* steps, int n) {
  gLabProgram.n = n < kLabSteps ? n : kLabSteps;
  for (int i = 0; i < gLabProgram.n; ++i) gLab[i] = steps[i];
}

float motorMs(Dance d) {
  float ms = 0;
  for (int i = 0; i < program(d).n; ++i)
    if (program(d).steps[i].kind != LIGHTS) ms += program(d).steps[i].ms;
  return ms;
}

const char* name(Dance d) { return d < DANCE_COUNT ? kNames[d] : d == TRIAL ? "trial" : d == LAB ? "trick" : "?"; }

bool find(const char* n, Dance& d) {
  for (int i = 0; i < DANCE_COUNT; ++i) {
    if (strcmp(n, kNames[i]) == 0) {
      d = static_cast<Dance>(i);
      return true;
    }
  }
  return false;
}

}  // namespace moves
