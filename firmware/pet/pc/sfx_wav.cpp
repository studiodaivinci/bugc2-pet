// Renders the robot's sounds to WAV files, to listen to on the PC (quietly: square waves, like the buzzer), and prints
// each dance's timeline so a sound that's cut short or runs out early shows:
//   bash firmware/pet/pc/build.sh && firmware/pet/pc/out/sfx_wav.exe <out dir>
// sfx-<name>.wav for every sound; dance-<name>.wav for each dance, every step's sound starting with the step, as
// moves.cpp plays them (turns that stop by the gyro are taken at their full time).
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "dances.h"
#include "sfx.h"

namespace {

constexpr int kRate = 22050;
constexpr float kLevel = 0.18f;  // quiet: these are square waves

// One sequence from `at` ms, as the sound task plays it: a note over 60 ms leaves a 12 ms gap; a later sound cuts it.
void render(std::vector<int16_t>& pcm, const sound::Note* seq, uint32_t at, uint32_t cutAt) {
  uint32_t t = at;
  for (const sound::Note* n = seq; n && n->ms && t < cutAt; ++n) {
    const uint32_t len = n->ms > 60 ? n->ms - 12 : n->ms;
    if (n->hz) {
      const size_t a = static_cast<size_t>(t) * kRate / 1000;
      const size_t b = static_cast<size_t>(t + len < cutAt ? t + len : cutAt) * kRate / 1000;
      if (pcm.size() < b) pcm.resize(b, 0);
      for (size_t i = a; i < b; ++i) {
        const double phase = fmod(static_cast<double>(i - a) * n->hz / kRate, 1.0);
        pcm[i] = static_cast<int16_t>((phase < 0.5 ? 1 : -1) * kLevel * 32767);
      }
    }
    t += n->ms;
  }
  const size_t end = static_cast<size_t>(t < cutAt ? t : cutAt) * kRate / 1000;
  if (pcm.size() < end) pcm.resize(end, 0);
}

bool writeWav(const std::string& path, const std::vector<int16_t>& pcm) {
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) return false;
  const uint32_t data = static_cast<uint32_t>(pcm.size() * 2), riff = 36 + data, fmtLen = 16, rate = kRate,
                 bytesPerSec = kRate * 2;
  const uint16_t pcmFmt = 1, channels = 1, align = 2, bits = 16;
  fwrite("RIFF", 1, 4, f);
  fwrite(&riff, 4, 1, f);
  fwrite("WAVEfmt ", 1, 8, f);
  fwrite(&fmtLen, 4, 1, f);
  fwrite(&pcmFmt, 2, 1, f);
  fwrite(&channels, 2, 1, f);
  fwrite(&rate, 4, 1, f);
  fwrite(&bytesPerSec, 4, 1, f);
  fwrite(&align, 2, 1, f);
  fwrite(&bits, 2, 1, f);
  fwrite("data", 1, 4, f);
  fwrite(&data, 4, 1, f);
  fwrite(pcm.data(), 2, pcm.size(), f);
  fclose(f);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";
  sound::build();
  int files = 0;
  for (int i = 0; i < sound::SFX_COUNT; ++i) {
    const auto s = static_cast<sound::Sfx>(i);
    std::vector<int16_t> pcm;
    render(pcm, sound::sequence(s), 0, 1u << 30);
    pcm.resize(pcm.size() + kRate / 5, 0);
    files += writeWav(dir + "/sfx-" + sound::name(s) + ".wav", pcm);
  }

  for (int d = 0; d < moves::DANCE_COUNT; ++d) {
    const moves::Program& p = moves::program(static_cast<moves::Dance>(d));
    // when each step starts (on the dance's clock), as moves.cpp times them
    std::vector<uint32_t> start(p.n + 1);
    uint32_t clock = 0;
    for (int i = 0; i < p.n; ++i) {
      const moves::Step& st = p.steps[i];
      start[i] = clock;
      clock += st.kind == moves::LIGHTS && st.until ? (st.until > clock ? st.until - clock : 0) : st.ms;
    }
    start[p.n] = clock;
    printf("%s (%.2f s):\n", moves::name(static_cast<moves::Dance>(d)), clock / 1000.0);
    std::vector<int16_t> pcm;
    for (int i = 0; i < p.n; ++i) {
      const moves::Step& st = p.steps[i];
      if (st.sfx == sound::NONE) {
        printf("  %5u ms  %-7s  (sound plays on)\n", start[i], st.name);
        continue;
      }
      int next = i + 1;
      while (next < p.n && p.steps[next].sfx == sound::NONE) ++next;
      const uint32_t slot = start[next] - start[i], len = sound::lengthMs(st.sfx);
      printf("  %5u ms  %-7s  %-8s %4u ms of %4u%s\n", start[i], st.name, sound::name(st.sfx), len < slot ? len : slot,
             slot, len > slot ? "  (cut short)" : len + 150 < slot ? "  (quiet at the end)" : "");
      render(pcm, sound::sequence(st.sfx), start[i], next < p.n ? start[next] : 1u << 30);
    }
    pcm.resize(static_cast<size_t>(clock + 300) * kRate / 1000, 0);
    files += writeWav(dir + "/dance-" + moves::name(static_cast<moves::Dance>(d)) + ".wav", pcm);
  }
  printf("wrote %d files to %s\n", files, dir.c_str());
  return 0;
}
