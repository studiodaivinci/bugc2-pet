// Face test firmware: plays the 53 face scenes (firmware/lib/face) on the robot, to check them on the real screen and
// time them. All 53 were kept on 2026-11-09; the look is Marble.
//
// Safety: the base keeps its last motor speed, even across a reflash, so the first thing setup() does is write 0 to
// all four motor registers (the same bit-banged write as firmware/bench). Nothing here ever runs a motor.
//
// On the robot: A (front) = next scene, hold A = previous scene, B (side) = tap (pats, the egg), hold B = switch
// between playing through every scene and repeating this one, power button (one short press) = the robot's button.
// In "Poke it", turning the robot over and shaking it are detected; pickup and putdown are serial commands for now.
//
// Commands, over USB serial (115200) or the Wi-Fi shell: list | play <id or number> | next | prev | restart | auto |
// loop | tap | button | event <name> | bright <0-255> | fps <n, 0 = no cap> | stats [on|off] | bench | prof | micro |
// cores 1|2 | live on|off | imu | info. "live on" makes scenes wait for real input, as the robot firmware will.
// "bench" plays every scene once and prints how long each frame takes to draw and send to the screen; "prof" splits
// the current scene's draw time by stage; "micro" times the basic maths operations.
//
// Wi-Fi: the same shell and update protocol as firmware/bench (port 3333, "auth <token>", "update <size> <md5>"), with
// the network and token the robot already stores in Preferences namespace "robot" (read only, never changed). So
// `python tools/bench.py --update` puts the robot firmware back without a cable, and
// `python firmware/face-test/upload_wifi.py` installs this one.
#include <ESPmDNS.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>
#include <driver/gpio.h>
#include <rom/ets_sys.h>

#include <atomic>

#include "face_player.h"

namespace {

// --- Motor stop, as in firmware/bench (bit-banged I2C on G0/G26, base at 0x38) --------------------------------------

constexpr gpio_num_t kSda = GPIO_NUM_0;
constexpr gpio_num_t kScl = GPIO_NUM_26;
constexpr uint8_t kBugAddr = 0x38;

inline void tick() { ets_delay_us(5); }
inline void sdaSet(bool high) { gpio_set_level(kSda, high); }
inline bool sdaGet() { return gpio_get_level(kSda); }
inline void sclLow() { gpio_set_level(kScl, 0); }
inline void sclHigh() {
  gpio_set_level(kScl, 1);
  const uint32_t started = micros();
  while (!gpio_get_level(kScl) && micros() - started < 5000) {
  }
}
void busStart() {
  sdaSet(true);
  sclHigh();
  tick();
  sdaSet(false);
  tick();
  sclLow();
  tick();
}
void busStop() {
  sdaSet(false);
  tick();
  sclHigh();
  tick();
  sdaSet(true);
  tick();
}
bool busWriteByte(uint8_t value) {
  for (int bit = 7; bit >= 0; --bit) {
    sdaSet((value >> bit) & 1);
    tick();
    sclHigh();
    tick();
    sclLow();
  }
  sdaSet(true);
  tick();
  sclHigh();
  const bool ack = !sdaGet();
  tick();
  sclLow();
  tick();
  return ack;
}
void busBegin() {
  for (gpio_num_t pin : {kSda, kScl}) {
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_level(pin, 1);
  }
  for (int i = 0; i < 9; ++i) {
    tick();
    sclLow();
    tick();
    sclHigh();
  }
  busStop();
}
bool stopMotors() {
  const uint8_t zeros[4] = {0, 0, 0, 0};
  for (int attempt = 0; attempt < 3; ++attempt) {
    busStart();
    bool ok = busWriteByte(kBugAddr << 1) && busWriteByte(0x00);
    for (int i = 0; ok && i < 4; ++i) ok = busWriteByte(zeros[i]);
    busStop();
    if (ok) return true;
    delay(2);
  }
  return false;
}

// --- Console: USB and the Wi-Fi client get the same output ----------------------------------------------------------

WiFiServer shellServer(3333);
WiFiClient shellClient;
String shellToken;
bool shellAuthed = false;
bool gWifiOn = false, gServing = false;

class Console : public Print {
 public:
  using Print::write;
  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t* data, size_t n) override {
    Serial.write(data, n);
    if (shellAuthed && shellClient.connected()) shellClient.write(data, n);
    return n;
  }
};
Console console;

void wifiBegin() {
  Preferences prefs;
  prefs.begin("robot", true);  // read only: the robot's own settings
  const String ssid = prefs.getString("ssid", "");
  const String pass = prefs.getString("pass", "");
  shellToken = prefs.getString("token", "");
  prefs.end();
  if (ssid.isEmpty() || shellToken.isEmpty()) {
    Serial.println("no Wi-Fi settings stored; USB only");
    return;
  }
  WiFi.setHostname("bugc2-pet");
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid.c_str(), pass.c_str());
  gWifiOn = true;
}

void releaseSecondFrame();

// Receives a firmware image over the authenticated Wi-Fi connection (same as firmware/bench).
void cmdUpdate(size_t size, const char* md5) {
  stopMotors();
  releaseSecondFrame();  // the update needs the memory, and it is the way back to the robot firmware
  if (!Update.begin(size)) {
    console.println("ERR update begin");
    return;
  }
  Update.setMD5(md5);
  M5.Display.fillScreen(TFT_NAVY);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextSize(2);
  M5.Display.drawString("UPDATING", 10, 110);
  console.println("SEND");
  static uint8_t chunk[1436];
  size_t received = 0;
  uint32_t lastData = millis();
  while (received < size) {
    const int count = shellClient.read(chunk, min(sizeof(chunk), size - received));
    if (count > 0) {
      if (Update.write(chunk, count) != static_cast<size_t>(count)) break;
      received += count;
      lastData = millis();
    } else if (!shellClient.connected() || millis() - lastData > 10000) {
      break;
    } else {
      delay(1);
    }
  }
  if (received < size || !Update.end(true)) {
    Update.abort();
    console.printf("ERR update failed: %s\n", Update.errorString());
    return;
  }
  console.println("updated, restarting");
  console.println("OK");
  delay(300);
  ESP.restart();
}

// --- The face ------------------------------------------------------------------------------------------------------

// Two frames, each two halves of 135 x 120 (two whole frames don't fit in one piece of fast RAM). A task on core 0
// sends one to the screen while the face draws the next into the other. Without room for the second frame, the
// one frame is sent and waited for.
constexpr size_t kHalfBytes = face::Screen::W * face::Screen::HALF * sizeof(uint16_t);
uint16_t* gFrame[2][2] = {{nullptr, nullptr}, {nullptr, nullptr}};  // [frame][top, bottom]
int gBack = 0;
M5Canvas gLabel(&M5.Display);  // the scene's name, drawn here and copied onto the frame's top rows
face::Player player;

TaskHandle_t gPusher = nullptr;
std::atomic<bool> gPushBusy{false};
volatile int gPushFrame = 0;

void sendFrame(int f) {
  // DMA: the SPI hardware keeps sending while this core does rows for the face (the row worker outranks this task).
  // The sleeps let core 0's idle task run while a half goes out (one takes about 6.5 ms). Without them the row worker
  // and this task waiting in endWrite keep core 0 busy, and with Wi-Fi off the task watchdog restarts the robot after
  // 5 s (found in firmware/pet, 2026-11-10).
  M5.Display.startWrite();
  M5.Display.pushImageDMA(0, 0, face::Screen::W, face::Screen::HALF,
                          reinterpret_cast<const lgfx::swap565_t*>(gFrame[f][0]));
  vTaskDelay(pdMS_TO_TICKS(5));
  M5.Display.pushImageDMA(0, face::Screen::HALF, face::Screen::W, face::Screen::HALF,
                          reinterpret_cast<const lgfx::swap565_t*>(gFrame[f][1]));
  vTaskDelay(pdMS_TO_TICKS(5));
  M5.Display.endWrite();  // waits for the rest of the transfer
}
void pushTask(void*) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    sendFrame(gPushFrame);
    gPushBusy.store(false, std::memory_order_release);
  }
}
void waitPush() {
  while (gPushBusy.load(std::memory_order_acquire)) delay(0);
}
void releaseSecondFrame() {
  waitPush();
  if (!gFrame[1][0]) return;
  heap_caps_free(gFrame[1][0]);
  heap_caps_free(gFrame[1][1]);
  gFrame[1][0] = gFrame[1][1] = nullptr;
  gBack = 0;
}
uint16_t* allocHalf(bool psram);
// The second frame is a luxury, so it is taken last: after Wi-Fi (about 55 KB of internal RAM) and anything else the
// host starts (sound took 17 KB in firmware/pet), and only if 60 KB is still left. Taken first, it left firmware/pet
// too little for its first ADC read and the robot crash-looped (2026-11-10).
void takeSecondFrame() {
  if (gFrame[1][0] || heap_caps_get_free_size(MALLOC_CAP_INTERNAL) <= 2 * kHalfBytes + 60000) {
    if (!gFrame[1][0]) Serial.println("one frame only (no room for two)");
    return;
  }
  gFrame[1][0] = allocHalf(false);
  gFrame[1][1] = allocHalf(false);
  if (!gFrame[1][0] || !gFrame[1][1]) {
    heap_caps_free(gFrame[1][0]);
    heap_caps_free(gFrame[1][1]);
    gFrame[1][0] = gFrame[1][1] = nullptr;
    Serial.println("one frame only (no room for two)");
  }
}
uint16_t* allocHalf(bool psram) {
  return static_cast<uint16_t*>(
      heap_caps_malloc(kHalfBytes, psram ? (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : (MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)));
}

// --- The second core -----------------------------------------------------------------------------------------------
// The Arduino loop (and so the face) runs on core 1. A worker on core 0 takes the odd rows of every big fill.
TaskHandle_t gWorker = nullptr;
volatile face::RowJob gJob = nullptr;
void* volatile gJobCtx = nullptr;
std::atomic<bool> gJobDone{true};

void workerTask(void*) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    gJob(gJobCtx, 1);
    gJobDone.store(true, std::memory_order_release);
  }
}
void splitOverCores(face::RowJob job, void* ctx) {
  gJob = job;
  gJobCtx = ctx;
  gJobDone.store(false, std::memory_order_release);
  xTaskNotifyGive(gWorker);
  job(ctx, 0);
  while (!gJobDone.load(std::memory_order_acquire)) {
  }
}

enum class Mode { Auto, Loop, Bench };
Mode gMode = Mode::Auto;
int gBright = 160;     // the backlight with the scene at full brightness; scenes dim it through Screen::dim
int gLastBright = -1;  // what the panel was last set to
uint32_t gFramePeriodUs = 33333;  // 30 fps cap ("fps 0" removes it, to see how fast it can go)
uint32_t gLabelUntil = 0;         // show the scene's number and name until then (ms)
uint32_t gNextStop = 0;
uint64_t gLastUs = 0;

// Frame timing. "window" is the last 3 s (printed with "stats"); "scene" is the whole current scene.
struct Timing {
  uint32_t frames = 0, slow = 0, maxDrawUs = 0, startMs = 0;
  uint64_t stepUs = 0, drawUs = 0, pushUs = 0;
  void add(uint32_t step, uint32_t draw, uint32_t push) {
    ++frames;
    stepUs += step;
    drawUs += draw;
    pushUs += push;
    if (draw > maxDrawUs) maxDrawUs = draw;
    if (step + draw + push > 33333) ++slow;
  }
  float fps() const {
    const uint32_t ms = millis() - startMs;
    return ms ? frames * 1000.0f / ms : 0;
  }
};
Timing gWindow, gScene;
bool gStatsOn = false;

struct BenchRow {
  float drawAvg, drawMax, push, fps;
  uint16_t frames, slow;
};
BenchRow gBench[64];

void printTiming(const char* label, const Timing& t) {
  if (!t.frames) return;
  console.printf("%-18s %4lu fr  fps %4.1f  draw avg %5.1f max %5.1f ms  step %4.1f  push %4.1f ms  slow %lu\n", label,
                 (unsigned long)t.frames, t.fps(), t.drawUs / 1000.0f / t.frames, t.maxDrawUs / 1000.0f,
                 t.stepUs / 1000.0f / t.frames, t.pushUs / 1000.0f / t.frames, (unsigned long)t.slow);
}

// Ends the current scene's timing: prints it with "stats" on, and keeps it for the bench table.
void closeSceneTiming() {
  const face::SceneInfo* in = player.info();
  if (!in || !gScene.frames) return;
  if (gStatsOn || gMode == Mode::Bench) printTiming(in->id, gScene);
  if (gMode == Mode::Bench && player.index < 64) {
    const Timing& t = gScene;
    gBench[player.index] = BenchRow{t.drawUs / 1000.0f / t.frames, t.maxDrawUs / 1000.0f, t.pushUs / 1000.0f / t.frames,
                                    t.fps(), (uint16_t)t.frames, (uint16_t)t.slow};
  }
}

// "Poke it" reacts to being handled. The robot lies screen up, so upside down is az < -0.6 (as in firmware/bench).
struct Handling {
  float shake = 0;  // smoothed squared distance of the acceleration from 1 g
  uint32_t shakeAt = 0, flipSince = 0, unflipSince = 0;
  bool flipped = false;
  void reset() { *this = Handling(); }
  void update(float ax, float ay, float az, uint32_t now) {
    const float d = sqrtf(ax * ax + ay * ay + az * az) - 1;
    shake = shake * 0.8f + d * d * 0.2f;
    if (shake > 0.5f && now - shakeAt > 3000) {
      shakeAt = now;
      shake = 0;
      console.println("event shake");
      player.event("shake");
    }
    if (!flipped) {
      if (az < -0.6f) {
        if (!flipSince) flipSince = now;
        if (now - flipSince > 400) {
          flipped = true;
          unflipSince = 0;
          console.println("event flip");
          player.event("flip");
        }
      } else {
        flipSince = 0;
      }
    } else {
      if (az > 0.6f) {
        if (!unflipSince) unflipSince = now;
        if (now - unflipSince > 400) {
          flipped = false;
          flipSince = 0;
          console.println("event unflip");
          player.event("unflip");
        }
      } else {
        unflipSince = 0;
      }
    }
  }
};
Handling gHand;
int gPlayIndex = -1;  // the "Poke it" scene

void startScene(int index, bool label) {
  closeSceneTiming();
  const int n = face::SCENE_COUNT;
  player.play(((index % n) + n) % n);
  gScene = Timing();
  gScene.startMs = millis();
  gHand.reset();
  if (label) gLabelUntil = millis() + 1500;
  const face::SceneInfo* in = player.info();
  console.printf("scene %d/%d %s (%s)\n", player.index + 1, n, in->id, in->title);
}

void finishBench() {
  console.println("\nbench: every scene once, at most 30 fps (a frame has 33.3 ms)");
  console.println("scene               draw avg   max   push    fps  slow/frames");
  float worst = 0;
  for (int i = 0; i < face::SCENE_COUNT && i < 64; i++) {
    const BenchRow& b = gBench[i];
    console.printf("%-18s %7.1f %6.1f %6.1f %6.1f  %3u/%u\n", face::SCENES[i].id, b.drawAvg, b.drawMax, b.push, b.fps,
                   b.slow, b.frames);
    if (b.drawMax > worst) worst = b.drawMax;
  }
  console.printf("slowest frame to draw: %.1f ms\n", worst);
  gMode = Mode::Auto;
}

// How long the current scene runs before the next one starts (0 = until something else is played).
float sceneLength() {
  const face::SceneInfo* in = player.info();
  if (!in) return 0;
  if (in->dur > 0) return in->dur;
  if (gMode == Mode::Bench) return 5;
  if (gMode == Mode::Auto) return 12;
  return 0;
}

void drawLabel(uint16_t* top) {
  M5Canvas& canvas = gLabel;
  if (millis() >= gLabelUntil) return;
  const face::SceneInfo* in = player.info();
  char buf[64];
  snprintf(buf, sizeof(buf), "%d/%d %s%s", player.index + 1, face::SCENE_COUNT, in->title,
           gMode == Mode::Loop ? " (loop)" : "");
  canvas.fillRect(0, 0, canvas.width(), 12, TFT_BLACK);
  canvas.setFont(&fonts::Font0);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextDatum(top_center);
  canvas.drawString(buf, canvas.width() / 2, 2);
  memcpy(top, canvas.getBuffer(), face::Screen::W * 12 * sizeof(uint16_t));
}

// "prof": where the draw time goes, averaged over the next 60 frames of the current scene.
int gProfLeft = 0;
uint32_t profMicros() { return micros(); }
void startProfile() {
  memset(face::profUs, 0, sizeof(face::profUs));
  face::profClock = profMicros;
  gProfLeft = 60;
}
void finishProfile() {
  static const char* names[face::PR_N] = {"background", "under", "face", "mid", "particles", "over", "caption", "post",
                                          "  eyes", "  mouth"};
  console.printf("profile of %s, ms per frame:\n", player.info()->id);
  for (int k = 0; k < face::PR_N; k++) console.printf("  %-11s %6.2f\n", names[k], face::profUs[k] / 60000.0f);
  face::profClock = nullptr;
}

// "micro": what the basic operations cost on this chip (the loop's own cost is the first line).
void microBench() {
  constexpr int N = 20000;
  volatile float sink = 0;
  auto run = [&](const char* name, auto fn) {
    float acc = 0;
    const uint32_t t0 = micros();
    for (int i = 0; i < N; i++) acc += fn(1.2345f + i * 0.0001f);
    const uint32_t us = micros() - t0;
    sink = acc;
    console.printf("  %-14s %6.0f cycles\n", name, us * 240.0f / N);
  };
  console.println("micro (240 MHz cycles per call, loop included):");
  run("loop only", [](float v) { return v; });
  run("mul+add", [](float v) { return v * 1.01f + 0.5f; });
  run("divide", [](float v) { return 1.0f / v; });
  run("sqrtf", [](float v) { return sqrtf(v); });
  run("floorf", [](float v) { return floorf(v); });
  run("float->int", [](float v) { return (float)(int)(v * 100); });
  run("sinf", [](float v) { return sinf(v); });
  run("expf", [](float v) { return expf(-v); });
  run("powf", [](float v) { return powf(v, 0.9f); });
  run("fastAtan2", [](float v) { return face::fastAtan2(v - 1.3f, 0.7f); });
  run("smooth", [](float v) { return face::smooth(0.2f, 0.8f, v - 1); });
  run("hash2", [](float v) { return face::hash2((int)(v * 100), 7); });
  run("vnoise", [](float v) { return face::vnoise(v * 9, v * 3); });
  run("pack+unpack", [](float v) {
    float r, g, b;
    face::Screen::unpack(face::Screen::pack(v * 100, v * 50, v * 20), r, g, b);
    return r + g + b;
  });
  (void)sink;
}

void setMode(Mode m) {
  gMode = m;
  gLabelUntil = millis() + 1500;
  console.printf("mode %s\n", m == Mode::Loop ? "loop (repeats this scene)" : "auto (plays through every scene)");
}

void handleCommand(String line, bool fromUsb) {
  line.trim();
  if (!fromUsb && !shellAuthed) {
    if (line.startsWith("auth ") && shellToken.length() && line.substring(5) == shellToken) {
      shellAuthed = true;
      console.println("OK");
    } else {
      shellClient.stop();
    }
    return;
  }
  unsigned size = 0;
  char md5[33] = {0};
  if (!fromUsb && sscanf(line.c_str(), "update %u %32s", &size, md5) == 2) {
    cmdUpdate(size, md5);
    return;
  }
  if (line == "list") {
    for (int i = 0; i < face::SCENE_COUNT; i++)
      console.printf("%2d %-18s %-14s %4.1f s  %s\n", i + 1, face::SCENES[i].id, face::SCENES[i].group,
                     face::SCENES[i].dur, face::SCENES[i].title);
  } else if (line.startsWith("play ")) {
    const String arg = line.substring(5);
    int i = face::findScene(arg.c_str());
    if (i < 0 && arg.toInt() > 0) i = arg.toInt() - 1;
    if (i < 0 || i >= face::SCENE_COUNT) {
      console.println("no such scene (try 'list')");
    } else {
      startScene(i, true);
    }
  } else if (line == "next") {
    startScene(player.index + 1, true);
  } else if (line == "prev") {
    startScene(player.index - 1, true);
  } else if (line == "restart") {
    startScene(player.index, true);
  } else if (line == "auto") {
    setMode(Mode::Auto);
  } else if (line == "loop") {
    setMode(Mode::Loop);
  } else if (line == "tap") {
    player.tap();
  } else if (line == "button") {
    player.button();
  } else if (line.startsWith("event ")) {
    player.event(line.substring(6).c_str());
  } else if (line.startsWith("bright ")) {
    gBright = constrain(line.substring(7).toInt(), 0, 255);
    console.printf("brightness %d\n", gBright);
  } else if (line.startsWith("fps ")) {
    const int f = line.substring(4).toInt();
    gFramePeriodUs = f > 0 ? 1000000 / f : 0;
    console.printf("frame cap %s\n", f > 0 ? String(f).c_str() : "off");
  } else if (line == "stats" || line == "stats on" || line == "stats off") {
    gStatsOn = line == "stats" ? !gStatsOn : line == "stats on";
    gWindow = Timing();
    gWindow.startMs = millis();
    console.printf("stats %s\n", gStatsOn ? "on" : "off");
  } else if (line == "bench") {
    memset(gBench, 0, sizeof(gBench));
    gMode = Mode::Bench;
    console.println("bench: playing all scenes once (about 6 minutes)");
    gScene = Timing();  // don't count the scene that was playing
    startScene(0, false);
  } else if (line == "live on" || line == "live off") {
    face::live = line == "live on";
    console.printf("live %s (takes effect when a scene starts)\n",
                   face::live ? "on: scenes wait for real taps, the button and events" : "off: scripted demos");
  } else if (line == "cores 1" || line == "cores 2") {
    face::splitRows = (line == "cores 2" && gWorker) ? splitOverCores : nullptr;
    console.printf("drawing on %d core%s\n", face::splitRows ? 2 : 1, face::splitRows ? "s" : "");
  } else if (line == "prof") {
    startProfile();
  } else if (line == "micro") {
    microBench();
  } else if (line == "imu") {
    for (int i = 0; i < 30; ++i) {
      float ax, ay, az;
      M5.Imu.update();
      M5.Imu.getAccel(&ax, &ay, &az);
      console.printf("accel %.2f %.2f %.2f\n", ax, ay, az);
      delay(100);
    }
  } else if (line == "info") {
    const face::SceneInfo* in = player.info();
    const char* mode = gMode == Mode::Loop ? "loop" : gMode == Mode::Bench ? "bench" : "auto";
    console.printf("face-test, scene %s, mode %s, heap %lu (largest %lu), psram %lu, frames %d, cores %d, wifi %s\n",
                   in ? in->id : "-", mode, (unsigned long)ESP.getFreeHeap(),
                   (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
                   (unsigned long)ESP.getFreePsram(), gFrame[1][0] ? 2 : 1, face::splitRows ? 2 : 1,
                   WiFi.isConnected() ? WiFi.localIP().toString().c_str() : "off");
  } else if (line.length()) {
    console.println(
        "commands: list | play <id or number> | next | prev | restart | auto | loop | tap | button | event <name> | "
        "bright <0-255> | fps <n> | stats [on|off] | bench | prof | micro | cores 1|2 | live on|off | imu | info");
  }
}

String gUsbLine, gWifiLine;
void handleSerial() {
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (gUsbLine.length()) handleCommand(gUsbLine, true);
      gUsbLine = "";
    } else if (gUsbLine.length() < 200) {
      gUsbLine += c;
    }
  }
  if (!gWifiOn) return;
  if (!gServing) {
    if (WiFi.status() != WL_CONNECTED) return;
    gServing = true;
    shellServer.begin();
    shellServer.setNoDelay(true);
    MDNS.begin("bugc2-pet");
  }
  if (shellServer.hasClient()) {
    shellClient.stop();
    shellClient = shellServer.accept();
    shellAuthed = false;
    gWifiLine = "";
  }
  while (shellClient.connected() && shellClient.available()) {
    const char c = shellClient.read();
    if (c == '\n' || c == '\r') {
      if (gWifiLine.length()) handleCommand(gWifiLine, false);
      gWifiLine = "";
    } else if (gWifiLine.length() < 200) {
      gWifiLine += c;
    }
  }
}

}  // namespace

void setup() {
  // Before anything else: the base may still hold a speed from before this firmware was installed.
  busBegin();
  stopMotors();

  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);
  stopMotors();
  M5.Display.setRotation(0);  // portrait, 135x240
  M5.Display.fillScreen(TFT_BLACK);

  // Frames in fast internal RAM (every pixel is written each frame). The second is taken in takeSecondFrame(), last.
  gFrame[0][0] = allocHalf(false);
  gFrame[0][1] = allocHalf(false);
  if (!gFrame[0][0] || !gFrame[0][1]) {
    Serial.println("frame in PSRAM");
    if (!gFrame[0][0]) gFrame[0][0] = allocHalf(true);
    if (!gFrame[0][1]) gFrame[0][1] = allocHalf(true);
  }
  if (!gFrame[0][0] || !gFrame[0][1]) {
    M5.Display.drawString("no frame", 10, 10);
    for (;;) delay(1000);
  }
  gLabel.setColorDepth(16);
  gLabel.setPsram(false);
  gLabel.createSprite(face::Screen::W, 12);
  xTaskCreatePinnedToCore(pushTask, "facePush", 4096, nullptr, 3, &gPusher, 0);
  player.begin(gFrame[0][0]);
  player.scr.setFrame(gFrame[0][0], gFrame[0][1]);
  if (xTaskCreatePinnedToCore(workerTask, "faceRows", 6144, nullptr, 5, &gWorker, 0) == pdPASS)
    face::splitRows = splitOverCores;
  gPlayIndex = face::findScene("play");

  wifiBegin();
  takeSecondFrame();
  startScene(0, true);
  gLastUs = micros();
  Serial.println("face-test ready. Type 'help' for commands.");
}

void loop() {
  const uint64_t frameStart = micros();
  const uint32_t nowMs = millis();
  M5.update();
  handleSerial();

  // Belt and braces: re-send the stop every few seconds. Harmless when the motors are already off.
  if (static_cast<int32_t>(nowMs - gNextStop) >= 0) {
    stopMotors();
    gNextStop = nowMs + 5000;
  }

  // Buttons.
  if (M5.BtnA.wasHold()) startScene(player.index - 1, true);
  else if (M5.BtnA.wasClicked()) startScene(player.index + 1, true);
  if (M5.BtnB.wasHold()) setMode(gMode == Mode::Loop ? Mode::Auto : Mode::Loop);
  else if (M5.BtnB.wasClicked()) player.tap();
  if (M5.BtnPWR.wasClicked()) {  // scenes take the button one of two ways: "Poke it" as an event, the rest as button()
    player.button();
    player.event("button");
  }

  if ((player.index == gPlayIndex || face::live) && M5.Imu.update()) {  // live: every scene hears flips and shakes
    float ax, ay, az;
    M5.Imu.getAccel(&ax, &ay, &az);
    gHand.update(ax, ay, az, nowMs);
  }

  // Next scene, or the same one again.
  const float len = sceneLength();
  if (len > 0 && player.S.t >= len) {
    if (gMode == Mode::Loop) {
      startScene(player.index, false);
    } else if (gMode == Mode::Bench && player.index == face::SCENE_COUNT - 1) {
      closeSceneTiming();
      gScene = Timing();
      finishBench();
      startScene(0, false);
    } else {
      startScene(player.index + 1, false);
    }
  }

  // One frame: advance by the real time since the last one (capped, so a stall doesn't skip a whole cue list).
  const uint64_t t0 = micros();
  double dt = (t0 - gLastUs) / 1e6;
  gLastUs = t0;
  if (dt > 0.1) dt = 0.1;
  player.step(dt);
  const uint64_t t1 = micros();
  if (!gFrame[1][0]) waitPush();  // one frame: it must have gone out before it is drawn over
  player.scr.setFrame(gFrame[gBack][0], gFrame[gBack][1]);
  player.draw();
  drawLabel(gFrame[gBack][0]);
  const uint64_t t2 = micros();
  const int bright = (int)(gBright * face::clampf(player.scr.dim, 0, 1) + 0.5f);
  if (bright != gLastBright) {
    M5.Display.setBrightness(bright);
    gLastBright = bright;
  }
  waitPush();  // the previous frame has gone out
  gPushFrame = gBack;
  gPushBusy.store(true, std::memory_order_release);
  xTaskNotifyGive(gPusher);
  if (gFrame[1][0]) gBack ^= 1;
  const uint64_t t3 = micros();
  gScene.add(t1 - t0, t2 - t1, t3 - t2);
  if (gProfLeft > 0 && --gProfLeft == 0) finishProfile();
  if (gStatsOn) {
    gWindow.add(t1 - t0, t2 - t1, t3 - t2);
    if (millis() - gWindow.startMs > 3000) {
      printTiming(player.info()->id, gWindow);
      console.printf("heap %lu psram %lu\n", (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getFreePsram());
      gWindow = Timing();
      gWindow.startMs = millis();
    }
  }

  if (gFramePeriodUs) {
    const uint64_t spent = micros() - frameStart;
    if (spent + 1000 < gFramePeriodUs) delay((gFramePeriodUs - spent) / 1000);  // delay() lets the Wi-Fi task run
  }
}
