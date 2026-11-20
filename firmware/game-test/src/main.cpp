// Game test firmware: plays the screen game (firmware/lib/game) on the robot, the way the robot firmware will.
//
// Safety: the base keeps its last motor speed, even across a reflash, so the first thing setup() does is write 0 to
// all four motor registers (the same bit-banged write as firmware/bench). Nothing here ever runs a motor.
//
// Commands, over USB serial (115200) or the Wi-Fi shell: "days N" sets what daysSinceHatch() returns (-1 = everything
// open), "reset" clears progress but keeps the Maker's records, "resetall" clears everything, "imu" prints the
// accelerometer for 3 s, "stats" toggles frame timing.
//
// Wi-Fi: the same shell and update protocol as firmware/bench (port 3333, "auth <token>", "update <size> <md5>"), with
// the network and token the robot already stores in Preferences namespace "robot" (read only, never changed). So
// `python tools/bench.py --update` puts the robot firmware back without a cable, and
// `python firmware/game-test/upload_wifi.py` installs this one.
#include <ESPmDNS.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>
#include <driver/gpio.h>
#include <rom/ets_sys.h>

#include "assets.h"
#include "game.h"
#include "gfx.h"

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

// --- Host for the game ---------------------------------------------------------------------------------------------

M5Canvas canvas(&M5.Display);
int gDays = -1;
float gBias[3] = {0, 0, 0};
bool gPlaying = true;

void hostTone(uint16_t hz, uint16_t ms) { M5.Speaker.tone(hz, ms); }
int hostDays() { return gDays; }
void hostEvent(game::Event e, uint8_t player) {
  static const char* names[] = {"HoleInOne", "UnderPar", "NewRecord", "BeatMaker", "BallInWater", "RoundWon",
                                "TiedMaker"};
  Serial.printf("event %s player %d\n", names[static_cast<int>(e)], player);
}

void measureBias() {
  double sum[3] = {0, 0, 0};
  float lo = 1e9f, hi = -1e9f;
  int n = 0;
  for (const uint32_t t0 = millis(); millis() - t0 < 600;) {
    if (M5.Imu.update()) {
      float g[3];
      M5.Imu.getGyro(&g[0], &g[1], &g[2]);
      for (int i = 0; i < 3; ++i) sum[i] += g[i];
      lo = min(lo, g[2]);
      hi = max(hi, g[2]);
      ++n;
    }
    delay(2);
  }
  if (n >= 100 && hi - lo < 10) {
    for (int i = 0; i < 3; ++i) gBias[i] = sum[i] / n;
    Serial.printf("gyro bias %.2f %.2f %.2f (%d samples)\n", gBias[0], gBias[1], gBias[2], n);
  } else {
    Serial.printf("gyro bias kept at 0: samples=%d spread=%.1f (moving?)\n", n, hi - lo);
  }
}

// The IMU is read about every 3 ms in its own task, so turning the robot is integrated closely, not once a frame.
// While the robot is held still, the gyro's bias keeps following its reading, so the aim doesn't creep.
struct ImuState {
  float ax, ay, az, gz, yaw;
};
ImuState gImu = {0, 0, 1, 0, 0};
portMUX_TYPE gImuMux = portMUX_INITIALIZER_UNLOCKED;

void imuTask(void*) {
  uint32_t last = micros();
  float still = 0;
  for (;;) {
    if (M5.Imu.update()) {
      float ax, ay, az, gx, gy, gz;
      M5.Imu.getAccel(&ax, &ay, &az);
      M5.Imu.getGyro(&gx, &gy, &gz);
      const uint32_t now = micros();
      const float dt = min((now - last) / 1e6f, 0.05f);
      last = now;
      const float rate = gz - gBias[2];
      if (fabsf(rate) < 5.0f) {
        still += dt;
        if (still > 1.0f) gBias[2] += rate * min(1.0f, dt / 5.0f);
      } else {
        still = 0;
      }
      portENTER_CRITICAL(&gImuMux);
      gImu = {ax, ay, az, rate, gImu.yaw + rate * dt};
      portEXIT_CRITICAL(&gImuMux);
    }
    vTaskDelay(pdMS_TO_TICKS(3));
  }
}

ImuState readImu() {
  portENTER_CRITICAL(&gImuMux);
  const ImuState s = gImu;
  portEXIT_CRITICAL(&gImuMux);
  return s;
}

// Timing stats.
uint32_t gFrames = 0, gUpdUs = 0, gPushUs = 0, gMaxUpd = 0, gStatsAt = 0;
bool gStatsOn = false;
bool gKeysOn = false;
uint32_t gNextStop = 0;

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
  // Wi-Fi power saving makes G39 (button B) read false presses (Espressif errata 3.11, docs/HARDWARE.md).
  WiFi.setSleep(false);
  gWifiOn = true;
}

// Receives a firmware image over the authenticated Wi-Fi connection (same as firmware/bench).
void cmdUpdate(size_t size, const char* md5) {
  stopMotors();
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
  if (line.startsWith("days ")) {
    gDays = line.substring(5).toInt();
    console.printf("daysSinceHatch = %d\n", gDays);
  } else if (line == "reset") {
    game::resetProgress();
    console.println("progress cleared (the Maker's records kept)");
  } else if (line == "resetall") {
    game::resetAll();
    console.println("all game saves cleared");
  } else if (line == "imu") {
    for (int i = 0; i < 30; ++i) {
      const ImuState m = readImu();
      console.printf("accel %.2f %.2f %.2f  gyro z %.1f  yaw %.1f  bias %.2f\n", m.ax, m.ay, m.az, m.gz, m.yaw,
                     gBias[2]);
      delay(100);
    }
  } else if (line == "stats") {
    gStatsOn = !gStatsOn;
    console.printf("stats %s\n", gStatsOn ? "on" : "off");
  } else if (line == "bench") {
    // Times the drawing pieces the game uses, into the canvas (PSRAM) and into internal RAM.
    using namespace game;
    const size_t n = gfx::W * gfx::H * 2;
    uint16_t* ps = static_cast<uint16_t*>(heap_caps_malloc(n, MALLOC_CAP_SPIRAM));
    uint16_t* in = static_cast<uint16_t*>(heap_caps_malloc(n, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    uint16_t* cv = static_cast<uint16_t*>(canvas.getBuffer());
    auto timeIt = [](const char* what, auto fn) {
      const uint32_t t0 = micros();
      for (int i = 0; i < 5; ++i) fn();
      console.printf("%-34s %6lu us\n", what, (micros() - t0) / 5);
    };
    gfx::Frame fc(cv), fi(in);
    timeIt("unpack page -> canvas (psram)", [&] { gfx::unpack(kHoleArt[0], cv); });
    if (in) timeIt("unpack page -> internal", [&] { gfx::unpack(kHoleArt[0], in); });
    if (ps) timeIt("copy psram -> canvas", [&] { fc.copyFrom(ps); });
    if (in) timeIt("copy internal -> canvas", [&] { fc.copyFrom(in); });
    if (in) timeIt("memcpy internal -> canvas", [&] { memcpy(cv, in, n); });
    timeIt("dim whole canvas", [&] { fc.blendRect(0, 0, gfx::W, gfx::H, 0, 110); });
    if (in) timeIt("dim whole internal", [&] { fi.blendRect(0, 0, gfx::W, gfx::H, 0, 110); });
    timeIt("fillRect 120x40 canvas", [&] { fc.fillRect(8, 40, 120, 40, 0x1234); });
    timeIt("title text 'Meadow' canvas", [&] { fc.text(kFontTitle, 10, 60, "Meadow", 0); });
    timeIt("small text outlined canvas", [&] { fc.text(kFontSmall, 68, 230, "tilt to move, A to pick", 0xFFFF, gfx::Align::Center, 1, 0); });
    timeIt("animal sprite canvas", [&] { fc.blit(kAnimalArt[0], 40, 100); });
    timeIt("animal sprite scaled canvas", [&] { fc.blitScaled(kAnimalArt[0], 60, 120, 1.1f, 0.9f, 0.5f, 1.0f); });
    timeIt("disc r5 canvas", [&] { fc.disc(60, 60, 5, 0xFFFF); });
    timeIt("pushSprite", [&] { canvas.pushSprite(0, 0); });
    free(ps);
    free(in);
  } else if (line == "keys") {
    gKeysOn = !gKeysOn;
    console.printf("keys %s\n", gKeysOn ? "on" : "off");
  } else if (line == "shot" && !fromUsb) {
    // The screen as it is now: raw 135x240 byte-swapped RGB565, Wi-Fi only (too slow for USB).
    const size_t n = game::kWidth * game::kHeight * 2;
    shellClient.printf("SHOT %u\n", static_cast<unsigned>(n));
    const uint8_t* px = static_cast<const uint8_t*>(canvas.getBuffer());
    for (size_t sent = 0; sent < n;) {
      const size_t w = shellClient.write(px + sent, min<size_t>(1436, n - sent));
      if (!w) break;
      sent += w;
    }
  } else if (line == "info") {
    console.printf("game-test, days %d, heap %u, psram %u, wifi %s\n", gDays, ESP.getFreeHeap(), ESP.getFreePsram(),
                   WiFi.isConnected() ? WiFi.localIP().toString().c_str() : "off");
  } else if (line.length()) {
    console.println("commands: days N | reset | resetall | imu | stats | keys | info | shot (Wi-Fi)");
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

void bootStep(const char* what) {
  Serial.printf("boot %lu ms: %s\n", millis(), what);
  Serial.flush();
}

}  // namespace

void setup() {
  // Before anything else: the base may still hold a speed from before this firmware was installed.
  busBegin();
  const bool stopped = stopMotors();
  Serial.begin(115200);
  bootStep(stopped ? "motors stopped" : "motor stop: no answer from the base");

  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);
  bootStep("M5 begun");
  Serial.printf("board %d, battery %d mV\n", static_cast<int>(M5.getBoard()), M5.Power.getBatteryVoltage());
  stopMotors();
  M5.Display.setRotation(0);  // portrait, 135x240
  M5.Display.fillScreen(TFT_BLACK);
  M5.Speaker.begin();
  M5.Speaker.setVolume(128);
  bootStep("display and speaker");

  canvas.setColorDepth(16);
  canvas.setPsram(true);
  if (!canvas.createSprite(game::kWidth, game::kHeight)) {
    M5.Display.drawString("no canvas", 10, 10);
    Serial.println("canvas allocation failed");
  }
  bootStep("canvas");

  // M5Unified's stored gyro offset can be badly wrong (docs/HARDWARE.md): clear it and measure the bias here.
  M5.Imu.clearOffsetData();
  measureBias();
  xTaskCreatePinnedToCore(imuTask, "imu", 4096, nullptr, 2, nullptr, 0);
  bootStep("gyro");

  game::Host host;
  host.tone = hostTone;
  host.daysSinceHatch = hostDays;
  host.event = hostEvent;
  game::begin(canvas, host);
  bootStep("game");
  wifiBegin();
  bootStep("wifi");
  Serial.println("game-test ready: days -1 (everything open). Type 'help' for commands.");
}

void loop() {
  static uint32_t last = millis();
  const uint32_t frameStart = millis();
  M5.update();
  handleSerial();

  // Belt and braces: re-send the stop every few seconds. Harmless when the motors are already off.
  if (static_cast<int32_t>(frameStart - gNextStop) >= 0) {
    stopMotors();
    gNextStop = frameStart + 5000;
  }

  game::Input in;
  in.ms = frameStart;
  in.dt = (frameStart - last) / 1000.0f;
  last = frameStart;
  in.a.pressed = M5.BtnA.wasPressed();
  in.a.held = M5.BtnA.isPressed();
  in.a.released = M5.BtnA.wasReleased();
  in.b.pressed = M5.BtnB.wasPressed();
  in.b.held = M5.BtnB.isPressed();
  in.b.released = M5.BtnB.wasReleased();
  const ImuState imu = readImu();
  in.ax = imu.ax;
  in.ay = imu.ay;
  in.az = imu.az;
  in.gz = imu.gz;
  in.yaw = imu.yaw;
  if (gKeysOn) {
    if (M5.BtnA.wasPressed()) console.printf("%lu A down\n", frameStart);
    if (M5.BtnA.wasReleased()) console.printf("%lu A up\n", frameStart);
    if (M5.BtnB.wasPressed()) console.printf("%lu B down\n", frameStart);
    if (M5.BtnB.wasReleased()) console.printf("%lu B up\n", frameStart);
  }

  if (gPlaying) {
    const uint32_t t0 = micros();
    gPlaying = game::update(in);
    const uint32_t t1 = micros();
    canvas.pushSprite(0, 0);
    const uint32_t t2 = micros();
    gUpdUs += t1 - t0;
    gPushUs += t2 - t1;
    gMaxUpd = max(gMaxUpd, t1 - t0);
    ++gFrames;
    if (!gPlaying) {
      M5.Display.fillScreen(TFT_BLACK);
      M5.Display.setTextSize(2);
      M5.Display.setTextColor(TFT_WHITE);
      M5.Display.drawString("Back at the", 4, 90);
      M5.Display.drawString("robot. A to", 4, 112);
      M5.Display.drawString("play again", 4, 134);
    }
  } else if (M5.BtnA.wasPressed()) {
    game::Host host;
    host.tone = hostTone;
    host.daysSinceHatch = hostDays;
    host.event = hostEvent;
    game::begin(canvas, host);
    gPlaying = true;
  }

  if (gStatsOn && millis() - gStatsAt > 3000 && gFrames) {
    console.printf("fps %.1f  update avg %lu us max %lu us  push avg %lu us  heap %u psram %u\n",
                  gFrames * 1000.0f / (millis() - gStatsAt), gUpdUs / gFrames, gMaxUpd, gPushUs / gFrames,
                  ESP.getFreeHeap(), ESP.getFreePsram());
    gFrames = gUpdUs = gPushUs = gMaxUpd = 0;
    gStatsAt = millis();
  }

  // About 30 frames a second.
  const uint32_t spent = millis() - frameStart;
  if (spent < 33) delay(33 - spent);
}
