// PC harness for the game library: plays a scripted session and saves frames, or solves holes.
//
//   sim script.txt out_dir      run a script (see below), saving frames as BMP into out_dir
//   sim --solve                 for each hole: hole-in-one chances and the best strokes from the tee
//
// Script lines (one per line, # comments):
//   days N          what daysSinceHatch() returns (default -1: everything open)
//   wait S          let S seconds pass at 30 fps
//   a / b           press and release a button
//   hold S          hold A for S seconds, then release
//   adown / aup     press A and keep it down / let it go
//   tilt X Y        accelerometer x/y in g, held until changed (z is computed)
//   yaw D           the integrated yaw, degrees
//   shot NAME       save the current frame as NAME.bmp
//   reset           clear all saves
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "../src/course.h"
#include "../src/game.h"
#include "../src/physics.h"

namespace {

uint16_t gFrame[game::kWidth * game::kHeight];
int gDays = -1;
game::Input gIn;
uint32_t gMs = 0;

void tone(uint16_t hz, uint16_t ms) { (void)hz; (void)ms; }
int days() { return gDays; }
void event(game::Event e, uint8_t p) {
  static const char* names[] = {"HoleInOne", "UnderPar", "NewRecord", "BeatMaker", "BallInWater", "RoundWon",
                                "TiedMaker"};
  printf("  event %s player %d at %.1fs\n", names[static_cast<int>(e)], p, gMs / 1000.0);
}

void saveBmp(const char* path) {
  const int w = game::kWidth, h = game::kHeight;
  const int row = (w * 3 + 3) & ~3;
  const int size = 54 + row * h;
  std::vector<uint8_t> b(size, 0);
  auto u32 = [&](int o, uint32_t v) { b[o] = v; b[o + 1] = v >> 8; b[o + 2] = v >> 16; b[o + 3] = v >> 24; };
  b[0] = 'B'; b[1] = 'M'; u32(2, size); u32(10, 54); u32(14, 40); u32(18, w); u32(22, h);
  b[26] = 1; b[28] = 24; u32(34, row * h);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const uint16_t s = gFrame[y * w + x];
      const uint16_t c = static_cast<uint16_t>((s >> 8) | (s << 8));
      const uint8_t r = ((c >> 11) & 31) * 255 / 31, g = ((c >> 5) & 63) * 255 / 63, bl = (c & 31) * 255 / 31;
      const int o = 54 + (h - 1 - y) * row + x * 3;
      b[o] = bl; b[o + 1] = g; b[o + 2] = r;
    }
  FILE* f = fopen(path, "wb");
  if (!f) return;
  fwrite(b.data(), 1, b.size(), f);
  fclose(f);
}

bool gAlive = true;  // false once update() has said "back to the robot", until the next begin

bool step(float dt) {
  gMs += static_cast<uint32_t>(dt * 1000);
  gIn.ms = gMs;
  gIn.dt = dt;
  const bool alive = gAlive && game::update(gIn);
  if (!alive && gAlive) printf("update() returned false: back to the robot\n");
  gAlive = alive;
  gIn.a.pressed = gIn.a.released = gIn.b.pressed = gIn.b.released = false;
  return alive;
}

void press(game::Button& b) {  // a 0.1 s press, like a real thumb
  b.pressed = true;
  b.held = true;
  for (int i = 0; i < 3; ++i) step(1 / 30.0f);
  b.held = false;
  b.released = true;
  step(1 / 30.0f);
}

int runScript(const char* scriptPath, const char* outDir) {
  FILE* f = fopen(scriptPath, "r");
  if (!f) {
    printf("can't open %s\n", scriptPath);
    return 1;
  }
  game::Host host;
  host.tone = tone;
  host.daysSinceHatch = days;
  host.event = event;
  gIn.az = 1;
  // The first lines may set days or reset before the game starts.
  std::vector<std::string> lines;
  char buf[256];
  while (fgets(buf, sizeof(buf), f)) lines.push_back(buf);
  fclose(f);
  bool begun = false;
  for (const std::string& line : lines) {
    char cmd[32] = {0};
    char arg[128] = {0};
    float x = 0, y = 0;
    if (sscanf(line.c_str(), "%31s", cmd) != 1 || cmd[0] == '#') continue;
    if (!strcmp(cmd, "days")) {
      sscanf(line.c_str(), "%*s %d", &gDays);
      continue;
    }
    if (!strcmp(cmd, "reset")) {
      remove("save_data.bin");
      remove("save_maker.bin");
      continue;
    }
    if (!begun) {
      game::beginRaw(gFrame, host);
      begun = true;
      step(1 / 30.0f);
    }
    if (!strcmp(cmd, "begin")) {  // the robot coming back to the game: begin() on each visit
      game::beginRaw(gFrame, host);
      gAlive = true;
      step(1 / 30.0f);
    } else if (!strcmp(cmd, "wait")) {
      sscanf(line.c_str(), "%*s %f", &x);
      for (int i = 0; i < static_cast<int>(x * 30 + 0.5f); ++i) step(1 / 30.0f);
    } else if (!strcmp(cmd, "a")) {
      press(gIn.a);
    } else if (!strcmp(cmd, "b")) {
      press(gIn.b);
    } else if (!strcmp(cmd, "adown")) {
      gIn.a.pressed = true;
      gIn.a.held = true;
      step(1 / 30.0f);
    } else if (!strcmp(cmd, "aup")) {
      gIn.a.held = false;
      gIn.a.released = true;
      step(1 / 30.0f);
    } else if (!strcmp(cmd, "hold")) {
      sscanf(line.c_str(), "%*s %f", &x);
      gIn.a.pressed = true;
      gIn.a.held = true;
      for (int i = 0; i < static_cast<int>(x * 30 + 0.5f); ++i) {
        step(1 / 30.0f);
        if (i == 14) {
          char name[300];
          snprintf(name, sizeof(name), "%s/_charging.bmp", outDir);
          saveBmp(name);
        }
      }
      gIn.a.held = false;
      gIn.a.released = true;
      step(1 / 30.0f);
    } else if (!strcmp(cmd, "tilt")) {
      sscanf(line.c_str(), "%*s %f %f", &x, &y);
      gIn.ax = x;
      gIn.ay = y;
      gIn.az = sqrtf(fmaxf(0, 1 - x * x - y * y));
    } else if (!strcmp(cmd, "yaw")) {
      sscanf(line.c_str(), "%*s %f", &x);
      gIn.yaw = x;
    } else if (!strcmp(cmd, "shot")) {
      sscanf(line.c_str(), "%*s %127s", arg);
      char name[300];
      snprintf(name, sizeof(name), "%s/%s.bmp", outDir, arg);
      saveBmp(name);
      printf("shot %s\n", name);
    } else {
      printf("unknown: %s", line.c_str());
    }
  }
  return 0;
}

// Brute force from the tee, animals ignored: which putts drop, and the fewest strokes in a greedy search.
int solve() {
  using namespace game;
  static uint8_t surf[gfx::W * gfx::H];
  for (int hi = 0; hi < kHoleCount; ++hi) {
    const Hole& h = kHoles[hi];
    decodeSurface(h, surf);
    int aces = 0, tries = 0, splash = 0;
    float bestLeft = 1e9f, bestAng = 0, bestPow = 0;
    float bx = 0, by = 0;
    for (int ai = 0; ai < 360; ++ai)
      for (int pi = 1; pi <= 40; ++pi) {
        Ball b;
        b.place(h.teeX, h.teeY);
        b.putt(static_cast<float>(ai), pi / 40.0f);
        ++tries;
        BallEvents ev;
        for (int t = 0; t < 30 * 12 && b.moving; ++t) {
          ev = b.step(1 / 30.0f, h, surf, nullptr, 0);
          if (ev.sunk || ev.splash || ev.lost) break;
        }
        if (ev.sunk) {
          ++aces;
          continue;
        }
        if (ev.splash) {
          ++splash;
          continue;
        }
        const float d = hypotf(b.x - h.cupX, b.y - h.cupY);
        if (d < bestLeft) {
          bestLeft = d;
          bestAng = static_cast<float>(ai);
          bestPow = pi / 40.0f;
          bx = b.x;
          by = b.y;
        }
      }
    printf("hole %d %-14s par %d: aces %d of %d (%.2f%%), splashes %.0f%%, closest miss %.1f px at %.0f deg power "
           "%.2f (ends %.0f,%.0f)\n",
           hi + 1, h.name, h.par, aces, tries, 100.0 * aces / tries, 100.0 * splash / tries, bestLeft, bestAng,
           bestPow, bx, by);
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc >= 2 && !strcmp(argv[1], "--solve")) return solve();
  if (argc < 3) {
    printf("usage: sim script.txt out_dir | sim --solve\n");
    return 1;
  }
  return runScript(argv[1], argv[2]);
}
