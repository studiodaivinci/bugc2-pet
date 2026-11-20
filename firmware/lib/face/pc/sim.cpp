// PC harness for the face library: renders scenes to images, so the port can be checked against the review page
// (art/face/dump.html draws the same grid from the JS) without the robot. Build: bash firmware/lib/face/pc/build.sh
//
//   sim grid "<id@t,id@t,...>" <cols> <out.ppm>   cells in a grid, 135x240 each, no gaps
//   sim strip "<id,id,...|all>" <n> <out.ppm>     n frames per scene at (i+0.5)/n of its length
//   sim bench ["<id,...>"]                        draw time per frame on this PC (relative costs only)
//   sim list                                      scene ids
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../src/face_player.h"

using namespace face;

namespace {

struct Cell {
  int scene;
  double t;
};

std::vector<std::string> split(const std::string& s, char c) {
  std::vector<std::string> out;
  size_t a = 0;
  while (a <= s.size()) {
    size_t b = s.find(c, a);
    if (b == std::string::npos) b = s.size();
    if (b > a) out.push_back(s.substr(a, b - a));
    a = b + 1;
  }
  return out;
}

int sceneOrDie(const std::string& id) {
  const int i = findScene(id.c_str());
  if (i < 0) {
    fprintf(stderr, "unknown scene %s\n", id.c_str());
    exit(2);
  }
  return i;
}

void writeGrid(const std::vector<Cell>& cells, int cols, const char* path) {
  const int W = Screen::W, H = Screen::H;
  const int rows = ((int)cells.size() + cols - 1) / cols;
  std::vector<uint8_t> img((size_t)W * cols * H * rows * 3, 0);
  static uint16_t frame[Screen::N];
  Player p;
  p.begin(frame);
  for (size_t k = 0; k < cells.size(); k++) {
    p.play(cells[k].scene);
    p.renderAt(cells[k].t);
    const int ox = (int)(k % cols) * W, oy = (int)(k / cols) * H;
    const float dim = p.scr.dim;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        float r, g, b;
        Screen::unpack(frame[y * W + x], r, g, b);
        uint8_t* o = &img[(((size_t)(oy + y) * W * cols) + ox + x) * 3];
        o[0] = (uint8_t)(r * dim);
        o[1] = (uint8_t)(g * dim);
        o[2] = (uint8_t)(b * dim);
      }
  }
  FILE* f = fopen(path, "wb");
  if (!f) { perror(path); exit(1); }
  fprintf(f, "P6\n%d %d\n255\n", W * cols, H * rows);
  fwrite(img.data(), 1, img.size(), f);
  fclose(f);
  printf("wrote %s (%zu cells)\n", path, cells.size());
}

std::vector<int> sceneList(const std::string& arg) {
  std::vector<int> ids;
  if (arg == "all") {
    for (int i = 0; i < SCENE_COUNT; i++) ids.push_back(i);
  } else {
    for (auto& s : split(arg, ',')) ids.push_back(sceneOrDie(s));
  }
  return ids;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: sim grid|strip|bench|list ...\n");
    return 2;
  }
  const std::string cmd = argv[1];
  // FACE_SPLIT=1: run split fills as the robot does (odd rows, then even rows), one after the other. The output must
  // be byte-identical to a normal run; a difference means a per-pixel function depends on the order it runs in.
  if (getenv("FACE_SPLIT")) face::splitRows = [](face::RowJob job, void* ctx) { job(ctx, 1); job(ctx, 0); };
  if (cmd == "list") {
    for (int i = 0; i < SCENE_COUNT; i++) printf("%s\t%s\t%s\t%.1f\n", SCENES[i].id, SCENES[i].group, SCENES[i].title, SCENES[i].dur);
    return 0;
  }
  if (cmd == "grid" && argc >= 5) {
    std::vector<Cell> cells;
    for (auto& c : split(argv[2], ',')) {
      const size_t at = c.find('@');
      cells.push_back(Cell{sceneOrDie(c.substr(0, at)), atof(c.substr(at + 1).c_str())});
    }
    writeGrid(cells, atoi(argv[3]), argv[4]);
    return 0;
  }
  if (cmd == "strip" && argc >= 5) {
    const int n = atoi(argv[3]);
    std::vector<Cell> cells;
    for (int i : sceneList(argv[2])) {
      const float dur = SCENES[i].dur ? SCENES[i].dur : 4;
      for (int k = 0; k < n; k++) cells.push_back(Cell{i, (double)dur * (k + 0.5) / n});
    }
    writeGrid(cells, n, argv[4]);
    return 0;
  }
  if (cmd == "bench") {
    static uint16_t frame[Screen::N];
    Player p;
    p.begin(frame);
    for (int i : sceneList(argc >= 3 ? argv[2] : "all")) {
      p.play(i);
      const float dur = SCENES[i].dur ? SCENES[i].dur : 6;
      const int frames = (int)(dur * 30);
      double drawMs = 0, worst = 0;
      for (int k = 0; k < frames; k++) {
        p.step(1.0f / 30);
        const auto a = std::chrono::steady_clock::now();
        p.draw();
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - a).count();
        drawMs += ms;
        if (ms > worst) worst = ms;
      }
      printf("%-18s draw avg %.3f ms  worst %.3f ms\n", SCENES[i].id, drawMs / frames, worst);
    }
    return 0;
  }
  fprintf(stderr, "bad arguments\n");
  return 2;
}
