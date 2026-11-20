// Bench firmware: a command shell for bring-up tests, over USB or Wi-Fi.
//
// The motor bus (G0/G26) is bit-banged rather than using an I2C controller because:
//   - G0 is also the mic's PDM clock, so the pin must be cleanly handed to I2S and back.
//   - The ESP32 has two I2C controllers and we have three buses (IMU, motors, Grove).
//
// Safety: the base has no failsafe and holds the last speed it was given, so motors stop first
// thing at boot, only run on an explicit command, and every run has a hard time limit.
//
// Wi-Fi details are stored in NVS by tools/wifi_setup.py, over USB only. A Wi-Fi client must send
// the token before anything else, and new firmware can be installed through that same connection.

#include <ESPmDNS.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>
#include <driver/gpio.h>
#include <rom/ets_sys.h>

namespace {

constexpr gpio_num_t kSda = GPIO_NUM_0;
constexpr gpio_num_t kScl = GPIO_NUM_26;
constexpr uint8_t kBugAddr = 0x38;
constexpr uint8_t kSonicAddr = 0x57;

constexpr int kMaxSpeed = 80;         // Library allows 100. Capped for bench work.
constexpr uint32_t kMaxRunMs = 3000;  // Hard limit on any single run.
constexpr uint32_t kSonicHz = 100000;
constexpr uint16_t kShellPort = 3333;
constexpr const char* kHostname = "bugc2-pet";

// --- Console: USB and the Wi-Fi client get the same output ------------------------------------

WiFiServer shellServer(kShellPort);
WiFiClient shellClient;
String shellToken;
bool shellAuthed = false;
bool wifiEnabled = false;
bool commandFromWifi = false;

class Console : public Print {
 public:
  using Print::write;
  size_t write(uint8_t byte) override { return write(&byte, 1); }
  size_t write(const uint8_t* data, size_t size) override {
    Serial.write(data, size);
    if (shellAuthed && shellClient.connected()) shellClient.write(data, size);
    return size;
  }
};
Console console;

// --- Bit-banged I2C for the motor bus --------------------------------------------------------

inline void tick() { ets_delay_us(5); }  // About 100 kHz.
inline void sdaSet(bool high) { gpio_set_level(kSda, high); }
inline bool sdaGet() { return gpio_get_level(kSda); }
inline void sclLow() { gpio_set_level(kScl, 0); }

// The base firmware enables clock stretching, so wait for SCL to actually rise.
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
  sdaSet(true);  // Release for ACK.
  tick();
  sclHigh();
  const bool ack = !sdaGet();
  tick();
  sclLow();
  tick();
  return ack;
}

uint8_t busReadByte(bool ack) {
  uint8_t value = 0;
  sdaSet(true);
  for (int bit = 7; bit >= 0; --bit) {
    tick();
    sclHigh();
    value = (value << 1) | (sdaGet() ? 1 : 0);
    tick();
    sclLow();
  }
  sdaSet(!ack);
  tick();
  sclHigh();
  tick();
  sclLow();
  sdaSet(true);
  tick();
  return value;
}

// Takes the pins back, including from I2S after a mic capture, and unsticks the slave.
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

bool busProbe(uint8_t address) {
  busStart();
  const bool ack = busWriteByte(address << 1);
  busStop();
  return ack;
}

bool bugWrite(uint8_t reg, const uint8_t* data, size_t length) {
  busStart();
  bool ok = busWriteByte(kBugAddr << 1) && busWriteByte(reg);
  for (size_t i = 0; ok && i < length; ++i) ok = busWriteByte(data[i]);
  busStop();
  return ok;
}

// The base prepares its reply when it sees the STOP after the register byte, then reads.
bool bugRead(uint8_t reg, uint8_t* data, size_t length, uint32_t settleMs) {
  busStart();
  const bool ok = busWriteByte(kBugAddr << 1) && busWriteByte(reg);
  busStop();
  if (!ok) return false;
  delay(settleMs);
  busStart();
  if (!busWriteByte((kBugAddr << 1) | 1)) {
    busStop();
    return false;
  }
  for (size_t i = 0; i < length; ++i) data[i] = busReadByte(i + 1 < length);
  busStop();
  return true;
}

// --- Motors ----------------------------------------------------------------------------------

uint32_t motorDeadline = 0;  // 0 means nothing is running.
bool stopPending = false;    // A stop the base never acknowledged. Retried until it is.

bool setMotors(int a, int b, int c, int d) {
  const uint8_t speeds[4] = {
      static_cast<uint8_t>(static_cast<int8_t>(constrain(a, -kMaxSpeed, kMaxSpeed))),
      static_cast<uint8_t>(static_cast<int8_t>(constrain(b, -kMaxSpeed, kMaxSpeed))),
      static_cast<uint8_t>(static_cast<int8_t>(constrain(c, -kMaxSpeed, kMaxSpeed))),
      static_cast<uint8_t>(static_cast<int8_t>(constrain(d, -kMaxSpeed, kMaxSpeed))),
  };
  for (int attempt = 0; attempt < 3; ++attempt) {
    if (bugWrite(0x00, speeds, 4)) return true;
    delay(2);
  }
  return false;
}

void stopAll() {
  motorDeadline = 0;
  stopPending = !setMotors(0, 0, 0, 0);
}

// The Stick is held on the base only by its header pins and can slide off them. The base then
// keeps its last speed, so keep sending the stop, with an alarm, until it gets through.
void retryStop() {
  static uint32_t lastAlarm = 0;
  stopPending = !setMotors(0, 0, 0, 0);
  if (!stopPending) {
    console.println("stop delivered");
    M5.Display.fillScreen(TFT_BLACK);
    return;
  }
  if (millis() - lastAlarm >= 1000) {
    lastAlarm = millis();
    M5.Speaker.tone(3000, 200);
    M5.Display.fillScreen(TFT_RED);
    M5.Display.setCursor(8, 8);
    M5.Display.print("BASE LOST");
    M5.Display.setCursor(8, 40);
    M5.Display.print("Push the Stick in");
    console.println("base not answering, stop not delivered");
  }
}

void startRun(int a, int b, int c, int d, uint32_t ms) {
  ms = min(ms, kMaxRunMs);
  if (setMotors(a, b, c, d)) {
    motorDeadline = millis() + ms;
    if (motorDeadline == 0) motorDeadline = 1;
    console.printf("run %d %d %d %d for %u ms\n", a, b, c, d, ms);
  } else {
    console.println("motor write failed");
  }
}

bool moveVector(const char* name, int s, int out[4]) {
  struct Pattern {
    const char* name;
    int sign[4];
  };
  static const Pattern patterns[] = {
      {"forward", {1, -1, 1, -1}}, {"back", {-1, 1, -1, 1}},  {"left", {-1, -1, 1, 1}},
      {"right", {1, 1, -1, -1}},   {"rotate", {1, 1, 1, 1}},  {"rrotate", {-1, -1, -1, -1}},
  };
  for (const Pattern& p : patterns) {
    if (strcmp(p.name, name) == 0) {
      for (int i = 0; i < 4; ++i) out[i] = p.sign[i] * s;
      return true;
    }
  }
  return false;
}

// --- Tests -----------------------------------------------------------------------------------

void printScan(const char* label, const bool* found) {
  console.printf("%s:", label);
  bool any = false;
  for (int addr = 8; addr < 120; ++addr) {
    if (found[addr]) {
      console.printf(" 0x%02X", addr);
      any = true;
    }
  }
  console.println(any ? "" : " none");
}

void cmdScan() {
  bool found[120];
  M5.In_I2C.scanID(found);
  printScan("internal G21/G22", found);
  M5.Ex_I2C.scanID(found);
  printScan("grove G32/G33", found);
  for (int addr = 8; addr < 120; ++addr) found[addr] = busProbe(addr);
  printScan("motor bus G0/G26", found);
}

void cmdBattery() {
  uint8_t raw[2] = {0, 0};
  if (bugRead(0x30, raw, 2, 10)) {
    const uint16_t adc = raw[0] | (raw[1] << 8);
    console.printf("base raw=%u volts=%.2f\n", adc, (adc / 4095.0f * 3.3f) * 2.960784f);
  } else {
    console.println("base battery read failed");
  }
  console.printf("stick mV=%d level=%d%%\n", M5.Power.getBatteryVoltage(),
                M5.Power.getBatteryLevel());
}

void cmdVersion() {
  uint8_t version = 0, address = 0;
  const bool a = bugRead(0xFE, &version, 1, 2);
  const bool b = bugRead(0xFF, &address, 1, 2);
  console.printf("base fw=%d (%s) addr=0x%02X (%s)\n", version, a ? "ok" : "fail", address,
                b ? "ok" : "fail");
}

void cmdSpeeds() {
  int8_t speeds[4] = {0, 0, 0, 0};
  for (int ch = 0; ch < 4; ++ch) bugRead(ch, reinterpret_cast<uint8_t*>(&speeds[ch]), 1, 1);
  console.printf("speeds %d %d %d %d\n", speeds[0], speeds[1], speeds[2], speeds[3]);
}

// M5Unified subtracts its stored gyro offset in 32-bit integer maths (IMU_Class.inl, getImuData),
// which overflows near the 2000 deg/s limit and flips the sign of fast spins. So setup() clears
// that offset, and the bias is measured here instead while the robot sits still.
float gyroBias[3] = {0, 0, 0};
constexpr float kGyroClip = 1990;  // The sensor saturates at 2000 deg/s.

// Bias-corrected rates. Sets clipped if the yaw axis reached the sensor's limit.
void readGyro(float& gx, float& gy, float& gz, bool* clipped = nullptr) {
  M5.Imu.getGyro(&gx, &gy, &gz);
  if (clipped != nullptr && fabsf(gz) > kGyroClip) *clipped = true;
  gx -= gyroBias[0];
  gy -= gyroBias[1];
  gz -= gyroBias[2];
}

struct BiasMeter {
  double sum[3] = {0, 0, 0};
  float low = 1e9f, high = -1e9f;
  int count = 0;

  void add() {
    float g[3];
    M5.Imu.getGyro(&g[0], &g[1], &g[2]);
    for (int i = 0; i < 3; ++i) sum[i] += g[i];
    low = min(low, g[2]);
    high = max(high, g[2]);
    ++count;
  }

  // Keeps the previous bias if there were too few samples or the robot was moving.
  void apply() {
    if (count < 100 || high - low > 10) {
      console.printf("gyro bias kept: samples=%d spread=%.1f\n", count, high - low);
      return;
    }
    for (int i = 0; i < 3; ++i) gyroBias[i] = sum[i] / count;
    console.printf("gyro bias %.1f %.1f %.1f\n", gyroBias[0], gyroBias[1], gyroBias[2]);
  }
};

void cmdImu(int samples) {
  for (int i = 0; i < samples; ++i) {
    while (!M5.Imu.update()) delay(1);
    float ax, ay, az, gx, gy, gz;
    M5.Imu.getAccel(&ax, &ay, &az);
    readGyro(gx, gy, gz);
    console.printf("a %.2f %.2f %.2f g %.1f %.1f %.1f\n", ax, ay, az, gx, gy, gz);
    delay(100);
  }
}

// Runs the motors while integrating the gyro, and keeps integrating through the coast-down.
void cmdImuRun(int a, int b, int c, int d, uint32_t ms) {
  ms = min(ms, kMaxRunMs);
  float yaw = 0, peakZ = 0, peakAccel = 0, tiltX = 0, tiltY = 0;
  if (!setMotors(a, b, c, d)) {
    console.println("motor write failed");
    return;
  }
  const uint32_t started = millis();
  uint32_t previous = micros();
  bool stopped = false, estop = false;
  uint32_t stopAt = started + ms;
  while (millis() < stopAt + (stopped ? 400 : 0)) {
    M5.update();
    if (!stopped && (M5.BtnA.wasPressed() || M5.BtnB.wasPressed())) {
      estop = true;
      stopAt = millis();
    }
    if (!stopped && millis() >= stopAt) {
      stopAll();
      stopped = true;
    }
    if (M5.Imu.update()) {
      float ax, ay, az, gx, gy, gz;
      M5.Imu.getAccel(&ax, &ay, &az);
      readGyro(gx, gy, gz);
      const uint32_t now = micros();
      const float dt = (now - previous) / 1e6f;
      previous = now;
      yaw += gz * dt;
      tiltX = max(tiltX, fabsf(gx));
      tiltY = max(tiltY, fabsf(gy));
      peakZ = max(peakZ, fabsf(gz));
      peakAccel = max(peakAccel, sqrtf(ax * ax + ay * ay + az * az));
    }
    delay(1);
  }
  stopAll();
  console.printf("yaw=%.1f deg peakZ=%.1f dps peakX=%.1f peakY=%.1f peakAccel=%.2f g%s\n", yaw,
                peakZ, tiltX, tiltY, peakAccel, estop ? " ESTOP" : "");
}

bool sonicRead(float& mm, uint32_t waitMs) {
  uint8_t bytes[3] = {0, 0, 0};
  if (!M5.Ex_I2C.start(kSonicAddr, false, kSonicHz)) {
    M5.Ex_I2C.stop();
    return false;
  }
  M5.Ex_I2C.write(0x01);
  M5.Ex_I2C.stop();
  delay(waitMs);
  if (!M5.Ex_I2C.start(kSonicAddr, true, kSonicHz)) {
    M5.Ex_I2C.stop();
    return false;
  }
  M5.Ex_I2C.read(bytes, 3, true);
  M5.Ex_I2C.stop();
  mm = ((uint32_t(bytes[0]) << 16) | (uint32_t(bytes[1]) << 8) | bytes[2]) / 1000.0f;
  return true;
}

void cmdSonic(int samples, uint32_t waitMs) {
  for (int i = 0; i < samples; ++i) {
    float mm = 0;
    const uint32_t started = millis();
    if (sonicRead(mm, waitMs)) {
      console.printf("sonic %.1f mm (%u ms)\n", mm, millis() - started);
    } else {
      console.println("sonic no response");
    }
  }
}

void showWord(const char* top, const char* word, uint32_t color);
void showBench();

// Hands G0 to the mic and back, then proves the motor bus survived. With a label (a word to
// say), the screen shows it for a moment, a beep means "now", and the word stays up in green
// while it records.
void cmdMic(float seconds, const char* label) {
  stopAll();
  seconds = constrain(seconds, 0.5f, 10.0f);
  const size_t samples = 16000 * seconds;
  auto* buffer = static_cast<int16_t*>(ps_malloc(samples * sizeof(int16_t)));
  if (buffer == nullptr) {
    console.println("no PSRAM");
    return;
  }
  if (label != nullptr) {
    showWord("NEXT", label, TFT_NAVY);
    delay(700);
    M5.Speaker.tone(2000, 80);
    delay(150);
  }
  M5.Speaker.end();
  auto cfg = M5.Mic.config();
  cfg.sample_rate = 16000;
  M5.Mic.config(cfg);
  const bool micOk = M5.Mic.begin();
  if (label != nullptr) {
    showWord("SAY IT", label, TFT_DARKGREEN);
  } else {
    M5.Display.fillScreen(TFT_DARKGREEN);
    M5.Display.setCursor(8, 8);
    M5.Display.print("LISTENING");
  }
  console.printf("mic begin=%d\n", micOk);
  M5.Mic.record(buffer, samples, 16000);
  while (M5.Mic.isRecording()) delay(5);
  M5.Mic.end();

  busBegin();
  uint8_t version = 0;
  const bool alive = bugRead(0xFE, &version, 1, 2);
  const bool stoppedOk = setMotors(0, 0, 0, 0);
  M5.Speaker.begin();
  showBench();
  console.printf("after mic: base alive=%d fw=%d stop=%d\n", alive, version, stoppedOk);

  console.printf("AUDIO %u\n", static_cast<unsigned>(samples * sizeof(int16_t)));
  Serial.flush();
  console.write(reinterpret_cast<uint8_t*>(buffer), samples * sizeof(int16_t));
  Serial.flush();
  console.println();
  free(buffer);
}

// The base's IR receiver (SL0038GD) pulls G36 low while it hears a remote's 38 kHz light. On the
// ESP32, G36 also sees false edges while Wi-Fi power saving or the ADC is on (Espressif errata
// 3.11), so Wi-Fi sleep is off while listening. A remote press is a burst of 16 or more edges
// with no gap over 20 ms; smaller groups count as noise.
constexpr uint32_t kIrMaxEdges = 2048;
constexpr uint32_t kIrGapUs = 20000;
constexpr uint32_t kIrBurstEdges = 16;
volatile uint32_t irEdges = 0;
volatile uint32_t irLast = 0;
volatile uint16_t irWidths[kIrMaxEdges];

void IRAM_ATTR irIsr() {
  const uint32_t now = micros();
  if (irEdges < kIrMaxEdges) {
    irWidths[irEdges] = static_cast<uint16_t>(min<uint32_t>(now - irLast, 65535));
  }
  irLast = now;
  irEdges = irEdges + 1;
}

void showIrCount(int bursts) {
  M5.Display.fillScreen(bursts > 0 ? TFT_DARKGREEN : TFT_NAVY);
  M5.Display.setTextColor(TFT_WHITE, bursts > 0 ? TFT_DARKGREEN : TFT_NAVY);
  M5.Display.setCursor(8, 8);
  M5.Display.print("IR LISTEN");
  M5.Display.setTextSize(6);
  M5.Display.setCursor(8, 50);
  M5.Display.printf("%d", bursts);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
}

// Listens for ms, counting remote presses on screen as they arrive, then prints the idle level,
// the noise and the first three bursts' edge widths in microseconds.
void cmdIr(uint32_t ms) {
  ms = constrain(ms, 1000u, 60000u);
  stopAll();
  pinMode(GPIO_NUM_25, INPUT);  // Shares a header pin with G36.
  pinMode(GPIO_NUM_36, INPUT);
  const auto sleepWas = WiFi.getSleep();
  if (wifiEnabled) WiFi.setSleep(false);
  delay(50);

  // A powered receiver holds its output high while it hears nothing; a floating pin wanders.
  int high = 0;
  for (int i = 0; i < 500; ++i) {
    high += digitalRead(GPIO_NUM_36);
    delayMicroseconds(400);
  }

  irEdges = 0;
  irLast = micros();
  attachInterrupt(GPIO_NUM_36, irIsr, CHANGE);
  int liveBursts = 0;
  uint32_t seen = 0, lastBurstAt = 0;
  showIrCount(0);
  const uint32_t started = millis();
  while (millis() - started < ms) {
    delay(50);
    const uint32_t edges = irEdges;
    if (edges - seen >= kIrBurstEdges && millis() - lastBurstAt > 250) {
      lastBurstAt = millis();
      showIrCount(++liveBursts);
    }
    seen = edges;
  }
  detachInterrupt(GPIO_NUM_36);
  if (wifiEnabled) WiFi.setSleep(sleepWas);

  const uint32_t edges = irEdges;
  const uint32_t stored = min<uint32_t>(edges, kIrMaxEdges);
  console.printf("ir idle high=%d%% edges=%lu (%.1f/s)%s\n", high / 5,
                 static_cast<unsigned long>(edges), edges * 1000.0f / ms,
                 edges > kIrMaxEdges ? " buffer full" : "");
  int bursts = 0;
  uint32_t noise = 0, groupStart = 0;
  for (uint32_t i = 1; i <= stored; ++i) {
    if (i < stored && irWidths[i] <= kIrGapUs) continue;
    const uint32_t size = i - groupStart;  // Edges groupStart to i - 1.
    if (size >= kIrBurstEdges) {
      ++bursts;
      if (bursts <= 3) {
        console.printf("burst %d: %lu edges, widths:", bursts, static_cast<unsigned long>(size));
        for (uint32_t j = groupStart + 1; j < i && j <= groupStart + 40; ++j) {
          console.printf(" %u", irWidths[j]);
        }
        console.println();
      }
    } else {
      noise += size;
    }
    groupStart = i;
  }
  console.printf("ir bursts=%d live=%d noise edges=%lu\n", bursts, liveBursts,
                 static_cast<unsigned long>(noise));
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(8, 8);
  M5.Display.print("BENCH");
}

// --- Cable-free demo ---------------------------------------------------------------------------

struct DemoResult {
  char name[16];
  int speed;
  float yaw;
  float peakX;
  float peakY;
  float peakZ;
  float peakAccel;
  float volts;
  bool aborted;
  bool clipped;  // Yaw hit the gyro's limit, so the yaw total is too small.
  float shake;   // RMS of the acceleration's distance from 1 g: how hard it shakes.
};
constexpr int kMaxDemo = 32;
DemoResult demoLog[kMaxDemo];
int demoCount = 0;

float readBaseVolts() {
  uint8_t raw[2] = {0, 0};
  if (!bugRead(0x30, raw, 2, 10)) return -1;
  return ((raw[0] | (raw[1] << 8)) / 4095.0f * 3.3f) * 2.960784f;
}

void showLabel(const char* text, uint32_t color) {
  M5.Display.fillScreen(color);
  M5.Display.setCursor(8, 8);
  M5.Display.print(text);
}

// Large centred text, readable in a video filmed from above.
void showBig(const char* text, uint32_t color) {
  M5.Display.fillScreen(color);
  M5.Display.setTextColor(TFT_WHITE, color);
  M5.Display.setTextSize(6);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(text, M5.Display.width() / 2, M5.Display.height() / 2);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
}

// True if the running demo should stop: a button, any input, or a lost Wi-Fi connection.
bool interrupted() {
  M5.update();
  bool hit = M5.BtnA.wasPressed() || M5.BtnB.wasPressed();
  while (Serial.available()) {
    Serial.read();
    hit = true;
  }
  if (shellServer.hasClient()) hit = true;
  while (shellClient.connected() && shellClient.available()) {
    shellClient.read();
    hit = true;
  }
  if (commandFromWifi && !shellClient.connected()) hit = true;
  return hit;
}

// One move while logging the IMU. With flipMs set, every motor reverses that often, which makes it
// shake. Returns false if it was interrupted.
bool demoMove(const char* name, int speed, const int v[4], uint32_t ms, DemoResult& result,
              uint32_t flipMs = 0) {
  result = {};
  snprintf(result.name, sizeof(result.name), "%s", name);
  result.speed = speed;
  if (!setMotors(v[0], v[1], v[2], v[3])) return false;
  const uint32_t started = millis();
  uint32_t previous = micros(), nextFlip = started + flipMs;
  int sign = 1;
  double shakeSum = 0;
  int shakeCount = 0;
  while (millis() - started < ms) {
    if (interrupted()) {
      result.aborted = true;
      break;
    }
    if (flipMs > 0 && static_cast<int32_t>(millis() - nextFlip) >= 0) {
      nextFlip += flipMs;
      sign = -sign;
      if (!setMotors(sign * v[0], sign * v[1], sign * v[2], sign * v[3])) {
        result.aborted = true;
        break;
      }
    }
    if (M5.Imu.update()) {
      float ax, ay, az, gx, gy, gz;
      M5.Imu.getAccel(&ax, &ay, &az);
      readGyro(gx, gy, gz, &result.clipped);
      const uint32_t now = micros();
      result.yaw += gz * (now - previous) / 1e6f;
      previous = now;
      result.peakX = max(result.peakX, fabsf(gx));
      result.peakY = max(result.peakY, fabsf(gy));
      result.peakZ = max(result.peakZ, fabsf(gz));
      const float g = sqrtf(ax * ax + ay * ay + az * az);
      result.peakAccel = max(result.peakAccel, g);
      shakeSum += (g - 1) * (g - 1);
      ++shakeCount;
    }
    delay(1);
  }
  stopAll();
  result.shake = shakeCount > 0 ? sqrtf(shakeSum / shakeCount) : 0;
  result.volts = readBaseVolts();
  return !result.aborted;
}

// Waits, staying stoppable. Returns false if interrupted.
bool waitOrCancel(uint32_t ms) {
  const uint32_t started = millis();
  while (millis() - started < ms) {
    if (interrupted()) return false;
    delay(5);
  }
  return true;
}

// Runs one logged move, then pauses. Returns false if the run should end.
bool demoStep(const char* name, int speed, const int v[4], uint32_t ms) {
  char label[24];
  snprintf(label, sizeof(label), "%d %s", speed, name);
  showLabel(label, TFT_DARKGREEN);
  M5.Speaker.tone(1600, 60);
  const bool finished = demoMove(name, speed, v, ms, demoLog[demoCount]);
  demoCount++;
  return finished && waitOrCancel(700);
}

// Beeps down so motion never surprises anyone, and measures the gyro bias over the last three
// seconds, while the robot sits still. Returns false if cancelled.
bool countdown(int seconds) {
  BiasMeter meter;
  for (int count = seconds; count > 0; --count) {
    char text[8];
    snprintf(text, sizeof(text), "%d", count);
    showBig(text, TFT_NAVY);
    M5.Speaker.tone(1000, 80);
    const uint32_t started = millis();
    while (millis() - started < 1000) {
      if (interrupted()) return false;
      if (count <= 3 && M5.Imu.update()) meter.add();
      delay(2);
    }
  }
  meter.apply();
  return true;
}

void finishRun() {
  stopAll();
  showLabel("DONE", TFT_BLACK);
  delay(800);
  M5.Display.fillScreen(TFT_BLACK);
}

// The six named moves at one speed. Under 10 seconds, so a drifting robot can't get far.
void runDemo(int speed) {
  static const char* const moves[] = {"forward", "back", "left", "right", "rotate", "rrotate"};
  demoCount = 0;
  if (countdown(3)) {
    for (const char* move : moves) {
      int v[4];
      moveVector(move, speed, v);
      if (!demoStep(move, speed, v, 600)) break;
    }
  }
  finishRun();
}

// Each motor alone in both directions.
void runMotorTest(int speed) {
  demoCount = 0;
  if (countdown(3)) {
    for (int motor = 0; motor < 4; ++motor) {
      for (int sign : {1, -1}) {
        int v[4] = {0, 0, 0, 0};
        v[motor] = sign * speed;
        char name[16];
        snprintf(name, sizeof(name), "motor%d%c", motor, sign > 0 ? '+' : '-');
        if (!demoStep(name, speed, v, 1000)) {
          finishRun();
          return;
        }
      }
    }
  }
  finishRun();
}

// The three ways to split four motors into two opposing pairs, each both ways round, so a video
// shows which one drives in a line. A is the library's forward/back and B its left/right. C had
// never been run. Each pair goes out and comes back, so the robot ends near where it started.
void runPairTest(int speed) {
  struct Step {
    const char* name;
    int sign[4];
    uint32_t color;
  };
  static const Step steps[] = {
      {"C+", {1, -1, -1, 1}, TFT_DARKGREEN}, {"C-", {-1, 1, 1, -1}, TFT_DARKGREEN},
      {"A+", {1, -1, 1, -1}, TFT_MAROON},    {"A-", {-1, 1, -1, 1}, TFT_MAROON},
      {"B+", {-1, -1, 1, 1}, TFT_DARKCYAN},  {"B-", {1, 1, -1, -1}, TFT_DARKCYAN},
  };
  demoCount = 0;
  if (countdown(5)) {
    for (const Step& step : steps) {
      int v[4];
      for (int i = 0; i < 4; ++i) v[i] = step.sign[i] * speed;
      showBig(step.name, step.color);
      M5.Speaker.tone(1600, 60);
      const bool finished = demoMove(step.name, speed, v, 800, demoLog[demoCount++]);
      if (!finished || !waitOrCancel(700)) break;
    }
  }
  finishRun();
}

bool upsideDown() {
  float ax, ay, az;
  while (!M5.Imu.update()) delay(1);
  M5.Imu.getAccel(&ax, &ay, &az);
  return az < -0.6f;
}

// Leg tests only run with the robot on its back, so it can't drive off.
bool requireUpsideDown() {
  if (upsideDown()) return true;
  console.println("leg tests only run with the robot upside down");
  showLabel("Turn me over", TFT_MAROON);
  delay(1500);
  M5.Display.fillScreen(TFT_BLACK);
  return false;
}

// One motor after a 3-second countdown, so someone can watch which foot turns.
void runOneLeg(int motor, int speed, uint32_t ms) {
  if (motor < 0 || motor > 3) {
    console.println("ERR motor must be 0 to 3");
    return;
  }
  ms = min<uint32_t>(ms, 8000);
  demoCount = 0;
  if (!requireUpsideDown()) return;
  if (countdown(3)) {
    char name[16];
    snprintf(name, sizeof(name), "ch%d", motor);
    showBig("GO", TFT_DARKGREEN);
    int v[4] = {0, 0, 0, 0};
    v[motor] = speed;
    demoMove(name, speed, v, ms, demoLog[demoCount++]);
  }
  finishRun();
}

// All four motors in a row. Before each one it beeps once per channel number plus one: one beep
// is channel 0.
void runLegTest(int speed) {
  demoCount = 0;
  if (!requireUpsideDown()) return;
  if (countdown(3)) {
    for (int motor = 0; motor < 4; ++motor) {
      if (!upsideDown()) {
        console.println("turned upright, stopping");
        break;
      }
      char name[16];
      snprintf(name, sizeof(name), "ch%d", motor);
      char number[4];
      snprintf(number, sizeof(number), "%d", motor + 1);
      showBig(number, TFT_DARKGREEN);
      bool ok = true;
      for (int beep = 0; ok && beep <= motor; ++beep) {
        M5.Speaker.tone(2000, 80);
        ok = waitOrCancel(220);
      }
      int v[4] = {0, 0, 0, 0};
      v[motor] = speed;
      if (!ok || !demoMove(name, speed, v, 2000, demoLog[demoCount++]) || !waitOrCancel(1000)) break;
    }
  }
  finishRun();
}

// --- Steering test ---------------------------------------------------------------------------

// Heading hold for forward and back. Both spin the robot counter-clockwise, so this adds a turn to
// every motor, which drives one side harder than the other, and resizes it from the gyro 50 times
// a second. Positive heading is counter-clockwise; a positive turn pushes clockwise.
// Floor runs at speed 40: these gains drove about 50 cm/s, fairly straight, wobbling 50 to 70 degrees
// each way. Halving kp and ki let it arc about 100 degrees left within 0.7 s.
float steerKp = 0.2f;   // Turn per degree of heading error.
float steerKd = 0.04f;  // Turn per deg/s of spin.
float steerKi = 0.2f;   // Turn per degree-second. Learns the steady correction, the "trim".
constexpr float kMaxTurn = 40;
constexpr uint32_t kSteerTickMs = 20;
constexpr int kTraceMax = 160;

struct TracePoint {
  int16_t heading;  // deg
  int16_t rate;     // deg/s over the last tick
  int8_t turn;
  uint8_t power;    // Percent of the move's power, which soft starts and stops ease in and out.
};

struct SteerTrace {
  char name[16];
  int count;
  float trim;
  TracePoint points[kTraceMax];
};
SteerTrace steerTrace[6];
int steerCount = 0;

float clampf(float value, float limit) { return fmaxf(-limit, fminf(limit, value)); }
int16_t toInt16(float value) { return static_cast<int16_t>(roundf(clampf(value, 32000))); }

// What one steered move does. Every field is set explicitly where it's used.
struct SteerSpec {
  int speed;
  int direction;        // 1 forward, -1 back.
  uint32_t ms;
  float maxTurn;        // Cap on the steering correction.
  int ditherTurn;       // A shake on top: the turn swings by this much...
  int ditherDrive;      // ...and the drive speed by this much...
  uint32_t flipMs;      // ...reversing this often. 0 means no shake.
  uint32_t pulseOnMs;   // Driving in steps: drive this long...
  uint32_t pulseOffMs;  // ...then rest this long, still steering. 0 means drive steadily.
  uint32_t rampMs;      // Soft start and stop: ease in over the first this long, out over the last. 0 is abrupt.
};

SteerSpec plainSpec(int speed, int direction, uint32_t ms) {
  return {speed, direction, ms, kMaxTurn, 0, 0, 0, 0, 0, 0};
}

// How much of the move's power to use: 0 to 1 over the first rampMs, back to 0 over the last rampMs,
// eased at both ends (smoothstep) so nothing jumps. Drive and steering fade together, keeping their
// balance. The slow test showed a twist of 15 to 20 degrees at every abrupt stop, probably the springy
// legs letting go, and a slide direction that switched mid-move, probably weight bouncing between
// pairs of feet.
float rampGain(uint32_t elapsed, const SteerSpec& spec) {
  if (spec.rampMs == 0) return 1;
  const float in = static_cast<float>(elapsed) / spec.rampMs;
  const float out = static_cast<float>(spec.ms > elapsed ? spec.ms - elapsed : 0) / spec.rampMs;
  const float x = fminf(1.0f, fminf(in, out));
  return x * x * (3 - 2 * x);
}

int scaled(int value, float gain) { return static_cast<int>(roundf(value * gain)); }

// Adds up the gyro into a heading. A gap between samples (a screen redraw, say) is filled with the
// next sample's rate, so a heading kept across several moves and pauses doesn't lose the turns made
// in between.
struct HeadingTracker {
  float heading = 0;
  uint32_t last = 0;
  void reset() {
    heading = 0;
    last = micros();
  }
  void add(float gz) {
    const uint32_t now = micros();
    heading += gz * (now - last) / 1e6f;
    last = now;
  }
};
float runHeading = 0;  // Where the last multi-move run ended up facing, from its start.
bool runHeadingValid = false;

// Waits like waitOrCancel, but keeps the heading up to date.
bool waitTracking(uint32_t ms, HeadingTracker& tracker) {
  const uint32_t started = millis();
  while (millis() - started < ms) {
    if (interrupted()) return false;
    if (M5.Imu.update()) {
      float gx, gy, gz;
      readGyro(gx, gy, gz);
      tracker.add(gz);
    }
    delay(1);
  }
  return true;
}

// One forward or back move under heading hold. A dry run never starts the motors, so the loop and
// its log can be checked with the robot in a hand. With a shared tracker, it steers back to the
// heading the whole run started with, not the one this move started with.
bool steerMove(const char* name, const SteerSpec& spec, bool dry, DemoResult& result, SteerTrace& trace,
               HeadingTracker* shared = nullptr) {
  result = {};
  snprintf(result.name, sizeof(result.name), "%s", name);
  result.speed = spec.speed;
  snprintf(trace.name, sizeof(trace.name), "%s", name);
  trace.count = 0;
  HeadingTracker local;
  local.reset();
  HeadingTracker& tracker = shared != nullptr ? *shared : local;
  const float startHeading = tracker.heading;
  float trim = 0, tickHeading = tracker.heading, gain = rampGain(0, spec);
  int turn = 0, sign = 1;
  double shakeSum = 0;
  int shakeCount = 0;
  const uint32_t started = millis();
  uint32_t tickAt = micros(), nextTick = started, nextFlip = started + spec.flipMs;
  while (millis() - started < spec.ms) {
    if (interrupted()) {
      result.aborted = true;
      break;
    }
    if (M5.Imu.update()) {
      float ax, ay, az, gx, gy, gz;
      M5.Imu.getAccel(&ax, &ay, &az);
      readGyro(gx, gy, gz, &result.clipped);
      tracker.add(gz);
      result.peakX = max(result.peakX, fabsf(gx));
      result.peakY = max(result.peakY, fabsf(gy));
      result.peakZ = max(result.peakZ, fabsf(gz));
      const float g = sqrtf(ax * ax + ay * ay + az * az);
      result.peakAccel = max(result.peakAccel, g);
      shakeSum += (g - 1) * (g - 1);
      ++shakeCount;
    }
    bool write = false;
    if (spec.flipMs > 0 && static_cast<int32_t>(millis() - nextFlip) >= 0) {
      nextFlip += spec.flipMs;
      sign = -sign;
      write = true;
    }
    if (static_cast<int32_t>(millis() - nextTick) >= 0) {
      nextTick += kSteerTickMs;
      const uint32_t now = micros();
      const float dt = (now - tickAt) / 1e6f;
      const float rate = dt > 0.005f ? (tracker.heading - tickHeading) / dt : 0;
      tickAt = now;
      tickHeading = tracker.heading;
      const float error = remainderf(tracker.heading, 360.0f);  // The short way back, never whole turns.
      trim = clampf(trim + steerKi * error * dt, spec.maxTurn);
      turn = static_cast<int>(roundf(clampf(steerKp * error + steerKd * rate + trim, spec.maxTurn)));
      gain = rampGain(millis() - started, spec);
      if (trace.count < kTraceMax) {
        trace.points[trace.count++] = {toInt16(tracker.heading), toInt16(rate), static_cast<int8_t>(turn),
                                       static_cast<uint8_t>(roundf(gain * 100))};
      }
      write = true;  // Every tick, which also starts and ends each step of a stepping move.
    }
    if (write && !dry) {
      const uint32_t period = spec.pulseOnMs + spec.pulseOffMs;
      const bool driving = spec.pulseOnMs == 0 || (millis() - started) % period < spec.pulseOnMs;
      const int s = driving ? spec.direction * (spec.speed + sign * spec.ditherDrive) : 0;
      const int t = turn + sign * spec.ditherTurn;
      if (!setMotors(scaled(s + t, gain), scaled(-s + t, gain), scaled(s + t, gain), scaled(-s + t, gain))) {
        result.aborted = true;
        break;
      }
    }
    delay(1);
  }
  stopAll();
  result.yaw = tracker.heading - startHeading;
  result.shake = shakeCount > 0 ? sqrtf(shakeSum / shakeCount) : 0;
  result.volts = readBaseVolts();
  trace.trim = trim;
  return !result.aborted;
}

// Steered forward, then steered back, so the robot ends near where it started. Without steering it
// only spins, which the first run showed, so those moves are gone. It drives about 50 cm/s at speed
// 40, so keep moves short and the stairs blocked.
void runSteerTest(int speed, uint32_t ms, bool dry) {
  if (speed < 20 || speed > 60) {
    console.println("ERR speed must be 20 to 60");
    return;
  }
  if (ms < 300 || ms > 2500) {
    console.println("ERR time must be 300 to 2500 ms");
    return;
  }
  struct Step {
    const char* name;
    const char* label;
    int direction;
  };
  static const Step steps[] = {{"F steer", "FWD G", 1}, {"B steer", "BACK G", -1}};
  demoCount = 0;
  steerCount = 0;
  runHeadingValid = false;  // Each move here keeps its own heading.
  if (dry) console.println("dry run: motors stay off");
  if (countdown(3)) {
    for (const Step& step : steps) {
      showBig(step.label, TFT_DARKGREEN);
      M5.Speaker.tone(1600, 60);
      const bool finished = steerMove(step.name, plainSpec(speed, step.direction, ms), dry,
                                      demoLog[demoCount++], steerTrace[steerCount++]);
      if (!finished || !waitOrCancel(700)) break;
    }
  }
  finishRun();
}

// Ways to shake in place without travelling or spinning: for a shiver behaviour, and to see whether
// shaking helps the feet glide so steering can be gentler. A small net yaw means it stayed facing
// the same way; demolog's shake value says how hard it shook. 1.5 s each, about 13 s in all.
void runShakeTest(int speed) {
  if (speed < 20 || speed > 80) {
    console.println("ERR speed must be 20 to 80");
    return;
  }
  struct Step {
    const char* name;
    const char* label;
    int sign[4];
    uint32_t flipMs;  // 0 holds the pattern; otherwise it reverses this often.
    uint32_t color;
  };
  static const Step steps[] = {
      {"fight", "FIGHT", {1, -1, -1, 1}, 0, TFT_MAROON},
      {"spin 8Hz", "8 HZ", {1, 1, 1, 1}, 60, TFT_DARKGREEN},
      {"spin 17Hz", "17 HZ", {1, 1, 1, 1}, 30, TFT_DARKGREEN},
      {"spin 33Hz", "33 HZ", {1, 1, 1, 1}, 15, TFT_DARKGREEN},
      {"fwd/back 17Hz", "FB 17", {1, -1, 1, -1}, 30, TFT_DARKCYAN},
  };
  demoCount = 0;
  if (countdown(3)) {
    for (const Step& step : steps) {
      int v[4];
      for (int i = 0; i < 4; ++i) v[i] = step.sign[i] * speed;
      showBig(step.label, step.color);
      const bool finished = demoMove(step.name, speed, v, 1500, demoLog[demoCount++], step.flipMs);
      if (!finished || !waitOrCancel(700)) break;
    }
  }
  finishRun();
}

// Steering with and without a shake riding on top, to see whether shaking smooths the wobble. Three
// forward/back pairs: 1 plain; 2 a turning shake at 33 Hz, the gentle buzz that stayed put in
// shaketest; 3 a driving shake at 17 Hz, the one that turned least. About 14 s at 1200 ms a move.
void runDitherTest(int speed, uint32_t ms, bool dry) {
  if (speed < 20 || speed > 60) {
    console.println("ERR speed must be 20 to 60");
    return;
  }
  if (ms < 300 || ms > 2500) {
    console.println("ERR time must be 300 to 2500 ms");
    return;
  }
  struct Step {
    const char* name;
    const char* label;
    int direction;
    int ditherTurn;
    int ditherDrive;
    uint32_t flipMs;
    uint32_t color;
  };
  static const Step steps[] = {
      {"1 F plain", "1F", 1, 0, 0, 0, TFT_DARKGREEN},
      {"1 B plain", "1B", -1, 0, 0, 0, TFT_DARKGREEN},
      {"2 F turn33", "2F", 1, 20, 0, 15, TFT_DARKCYAN},
      {"2 B turn33", "2B", -1, 20, 0, 15, TFT_DARKCYAN},
      {"3 F drive17", "3F", 1, 0, 30, 30, TFT_PURPLE},
      {"3 B drive17", "3B", -1, 0, 30, 30, TFT_PURPLE},
  };
  demoCount = 0;
  steerCount = 0;
  runHeadingValid = false;  // Each move here keeps its own heading.
  if (dry) console.println("dry run: motors stay off");
  if (countdown(3)) {
    for (const Step& step : steps) {
      showBig(step.label, step.color);
      const SteerSpec spec = {speed, step.direction, ms, kMaxTurn, step.ditherTurn, step.ditherDrive,
                              step.flipMs, 0, 0, 0};
      const bool finished = steerMove(step.name, spec, dry, demoLog[demoCount++], steerTrace[steerCount++]);
      if (!finished || !waitOrCancel(700)) break;
    }
  }
  finishRun();
}

// One move of a run that keeps one heading throughout. spec.ms is filled in from the command.
struct RunStep {
  const char* name;
  const char* label;
  SteerSpec spec;
  uint32_t color;
};

// Runs the steps with one heading kept for the whole run, through the pauses too, so each back move
// retraces the forward move before it and the robot should end near where it started, facing the
// same way. At most 6 steps, the size of the trace log.
void runOneHeading(const RunStep* steps, int count, uint32_t ms, bool dry) {
  demoCount = 0;
  steerCount = 0;
  runHeadingValid = false;
  if (dry) console.println("dry run: motors stay off");
  if (countdown(3)) {
    HeadingTracker tracker;
    tracker.reset();
    for (int i = 0; i < count && i < 6; ++i) {
      showBig(steps[i].label, steps[i].color);
      SteerSpec spec = steps[i].spec;
      spec.ms = ms;
      const bool finished = steerMove(steps[i].name, spec, dry, demoLog[demoCount++], steerTrace[steerCount++],
                                      &tracker);
      if (!finished || !waitTracking(700, tracker)) break;
    }
    runHeading = tracker.heading;
    runHeadingValid = true;
  }
  finishRun();
}

// Slower driving for a pet that shouldn't zip around the house. Pairs: speed 25, speed 30, and
// stepping (speed 40 in 150 ms bursts with 150 ms rests, like a bug scuttling). The steering
// correction is capped at the speed so it can't whip round. About 14 s at 1200 ms a move.
void runSlowTest(uint32_t ms, bool dry) {
  if (ms < 300 || ms > 2500) {
    console.println("ERR time must be 300 to 2500 ms");
    return;
  }
  static const RunStep steps[] = {
      {"25 F", "25 F", {25, 1, 0, 25, 0, 0, 0, 0, 0, 0}, TFT_DARKGREEN},
      {"25 B", "25 B", {25, -1, 0, 25, 0, 0, 0, 0, 0, 0}, TFT_DARKGREEN},
      {"30 F", "30 F", {30, 1, 0, 30, 0, 0, 0, 0, 0, 0}, TFT_DARKCYAN},
      {"30 B", "30 B", {30, -1, 0, 30, 0, 0, 0, 0, 0, 0}, TFT_DARKCYAN},
      {"step F", "ST F", {40, 1, 0, 40, 0, 0, 0, 150, 150, 0}, TFT_PURPLE},
      {"step B", "ST B", {40, -1, 0, 40, 0, 0, 0, 150, 150, 0}, TFT_PURPLE},
  };
  runOneHeading(steps, 6, ms, dry);
}

// The slow test again with soft starts and stops: each move eases in over its first 300 ms and out
// over its last 300 ms. On the new tips speed 25 hardly moves it, so speed 30 pairs come first and
// last, to see whether the slide angle repeats, with the slow test's bursts at 40 in between. Compare
// with the slow test on the new tips, move for move, including steerlog's "turned while stopped".
// About 14 s at 1200 ms a move.
void runSoftTest(uint32_t ms, bool dry) {
  constexpr uint32_t kRamp = 300;
  if (ms < 2 * kRamp + 200 || ms > 2500) {
    console.println("ERR time must be 800 to 2500 ms");
    return;
  }
  static const RunStep steps[] = {
      {"30a F", "30 F", {30, 1, 0, 30, 0, 0, 0, 0, 0, kRamp}, TFT_DARKGREEN},
      {"30a B", "30 B", {30, -1, 0, 30, 0, 0, 0, 0, 0, kRamp}, TFT_DARKGREEN},
      {"step F", "ST F", {40, 1, 0, 40, 0, 0, 0, 150, 150, kRamp}, TFT_PURPLE},
      {"step B", "ST B", {40, -1, 0, 40, 0, 0, 0, 150, 150, kRamp}, TFT_PURPLE},
      {"30b F", "30 F", {30, 1, 0, 30, 0, 0, 0, 0, 0, kRamp}, TFT_DARKCYAN},
      {"30b B", "30 B", {30, -1, 0, 30, 0, 0, 0, 0, 0, kRamp}, TFT_DARKCYAN},
  };
  runOneHeading(steps, 6, ms, dry);
}

void cmdSteerLog() {
  console.printf("gains kp=%.3f kd=%.3f ki=%.3f, turn limit %.0f\n", steerKp, steerKd, steerKi, kMaxTurn);
  for (int i = 0; i < steerCount; ++i) {
    const SteerTrace& t = steerTrace[i];
    float sumTurn = 0, sumRate = 0;
    int low = 0, high = 0;
    for (int p = 0; p < t.count; ++p) {
      sumTurn += t.points[p].turn;
      sumRate += fabsf(t.points[p].rate);
      low = min<int>(low, t.points[p].heading);
      high = max<int>(high, t.points[p].heading);
    }
    const int n = max(t.count, 1);
    console.printf("%s: %d ticks, heading %d to %d, mean |rate| %.0f deg/s, mean turn %.1f, final trim %.1f\n",
                   t.name, t.count, low, high, sumRate / n, sumTurn / n, t.trim);
    // With one heading for the whole run, the gap between moves shows how far it turned while stopped.
    if (runHeadingValid && i > 0 && t.count > 0 && steerTrace[i - 1].count > 0) {
      const SteerTrace& before = steerTrace[i - 1];
      console.printf("turned %d deg while stopped before this move\n",
                     t.points[0].heading - before.points[before.count - 1].heading);
    }
    console.println("    ms heading   rate turn power%");
    for (int p = 0; p < t.count; p += 2) {
      const TracePoint& q = t.points[p];
      console.printf("%6u %7d %6d %4d %6u\n", static_cast<unsigned>(p * kSteerTickMs), q.heading, q.rate, q.turn,
                     static_cast<unsigned>(q.power));
    }
  }
  if (runHeadingValid) console.printf("run ended facing %.0f deg from its start\n", runHeading);
  if (steerCount == 0) console.println("no steer run yet");
}

void cmdDemoLog() {
  console.printf("gyro bias %.1f %.1f %.1f\n", gyroBias[0], gyroBias[1], gyroBias[2]);
  for (int i = 0; i < demoCount; ++i) {
    const DemoResult& r = demoLog[i];
    console.printf("%-8s speed=%d yaw=%7.1f peakX=%6.1f peakY=%6.1f peakZ=%7.1f accel=%.2f shake=%.2f volts=%.2f%s%s\n",
                  r.name, r.speed, r.yaw, r.peakX, r.peakY, r.peakZ, r.peakAccel, r.shake, r.volts,
                  r.clipped ? " CLIPPED" : "", r.aborted ? " ABORTED" : "");
  }
  if (demoCount == 0) console.println("no demo run yet");
}

// Live readout on the screen, for battery checks and for trying sensor mounts.
void updateStatus() {
  static uint32_t nextSonic = 0, nextBattery = 0;
  static float volts = 0;
  const uint32_t now = millis();
  if (motorDeadline != 0 || stopPending || now < nextSonic) return;
  nextSonic = now + 200;
  const bool baseOk = busProbe(kBugAddr);
  if (now >= nextBattery) {
    nextBattery = now + 3000;
    volts = readBaseVolts();
  }
  float mm = 0;
  const bool ok = sonicRead(mm, 60);
  M5.Display.setCursor(8, 8);
  M5.Display.printf("BAT %.2fV  ", volts);
  M5.Display.setCursor(8, 40);
  if (ok) {
    M5.Display.printf("DIST %.1f cm  ", mm / 10);
  } else {
    M5.Display.print("DIST --      ");
  }
  M5.Display.setCursor(8, 72);
  if (baseOk) {
    M5.Display.print("Top button = stop");
  } else {
    M5.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5.Display.print("BASE LOST        ");
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  M5.Display.setCursor(8, 104);
  if (WiFi.status() == WL_CONNECTED) {
    M5.Display.printf("%s   ", WiFi.localIP().toString().c_str());
  } else {
    M5.Display.print(wifiEnabled ? "wifi...        " : "no wifi        ");
  }
}

// --- Bench tests: clock, motion log, battery run-down ---------------------------------------

// Day of the week, 0 = Sunday (Sakamoto's method).
int weekDay(int y, int m, int d) {
  static const int offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + offsets[m - 1] + d) % 7;
}

// lowvolt=1 means the clock chip lost power at some point and its time can't be trusted.
void printClock() {
  const auto dt = M5.Rtc.getDateTime();
  console.printf("clock %04d-%02d-%02d %02d:%02d:%02d lowvolt=%d\n", dt.date.year, dt.date.month,
                 dt.date.date, dt.time.hours, dt.time.minutes, dt.time.seconds,
                 M5.Rtc.getVoltLow() ? 1 : 0);
}

void cmdClockSet(int y, int mo, int d, int h, int mi, int s) {
  if (y < 2024 || y > 2099 || mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 ||
      mi > 59 || s < 0 || s > 59) {
    console.println("ERR clock out of range");
    return;
  }
  m5::rtc_datetime_t dt;
  dt.date.year = y;
  dt.date.month = mo;
  dt.date.date = d;
  dt.date.weekDay = weekDay(y, mo, d);
  dt.time.hours = h;
  dt.time.minutes = mi;
  dt.time.seconds = s;
  M5.Rtc.setDateTime(dt);
  printClock();
}

void showWord(const char* top, const char* word, uint32_t color) {
  M5.Display.fillScreen(color);
  M5.Display.setTextColor(TFT_WHITE, color);
  M5.Display.setCursor(8, 8);
  M5.Display.print(top);
  M5.Display.setTextSize(strlen(word) <= 9 ? 4 : 3);  // 240 px across: 9 characters at size 4.
  M5.Display.setCursor(8, 50);
  M5.Display.print(word);
  M5.Display.setTextSize(2);
}

void showBench() {
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(8, 8);
  M5.Display.print("BENCH");
}

// Logs the IMU at 200 Hz for a gesture test. The screen names the gesture: GET READY for 2 s,
// then a beep and GO with a countdown, then two beeps. Prints IMULOG, one row per sample
// (ms, accel g, gyro deg/s), then IMUEND.
struct ImuRow {
  float t, ax, ay, az, gx, gy, gz;
};

void cmdImuLog(uint32_t ms, const char* label) {
  ms = constrain(ms, 1000u, 30000u);
  stopAll();
  const size_t capacity = ms / 5 + 10;
  auto* rows = static_cast<ImuRow*>(ps_malloc(capacity * sizeof(ImuRow)));
  if (rows == nullptr) {
    console.println("ERR no PSRAM");
    return;
  }
  showWord("GET READY", label, TFT_NAVY);
  delay(2000);
  M5.Speaker.tone(2000, 120);
  showWord("GO", label, TFT_DARKGREEN);
  size_t n = 0;
  int shown = -1;
  const uint32_t started = micros();
  uint32_t next = started;
  while (micros() - started < ms * 1000 && n < capacity) {
    while (static_cast<int32_t>(micros() - next) < 0) {
    }
    next += 5000;
    M5.Imu.update();
    ImuRow& r = rows[n++];
    r.t = (micros() - started) / 1000.0f;
    M5.Imu.getAccel(&r.ax, &r.ay, &r.az);
    readGyro(r.gx, r.gy, r.gz);
    const int left = (ms - r.t) / 1000 + 1;
    if (left != shown) {
      shown = left;
      M5.Display.setCursor(8, 110);
      M5.Display.printf("%2d s left ", left);
    }
  }
  M5.Speaker.tone(1200, 120);
  delay(200);
  M5.Speaker.tone(1200, 120);
  showBench();
  console.printf("IMULOG %s %u\n", label, static_cast<unsigned>(n));
  for (size_t i = 0; i < n; ++i) {
    const ImuRow& r = rows[i];
    console.printf("%.1f %.3f %.3f %.3f %.1f %.1f %.1f\n", r.t, r.ax, r.ay, r.az, r.gx, r.gy, r.gz);
  }
  console.println("IMUEND");
  free(rows);
}

// Battery run-down: Wi-Fi off (as in the pet's gift build), screen on, no motors. Every 2 minutes it
// saves both batteries to flash, so the log survives the battery running out. A button ends the
// test and restarts the robot, which brings Wi-Fi back. `batlog` prints the log.
struct BatRow {
  uint16_t minute, baseMv, stickMv;
  uint8_t level, baseOk;
};
constexpr int kBatMax = 360;  // 12 hours.
constexpr uint32_t kBatEveryMs = 120000;
BatRow batRows[kBatMax];
int batCount = 0;
bool batRunning = false;
uint32_t batStarted = 0, batNext = 0, batWifiOffAt = 0;

void batSample() {
  if (batCount >= kBatMax) return;
  BatRow& r = batRows[batCount++];
  const float base = readBaseVolts();
  r.minute = (millis() - batStarted) / 60000;
  r.baseMv = base > 0 ? static_cast<uint16_t>(base * 1000) : 0;
  r.stickMv = M5.Power.getBatteryVoltage();
  r.level = M5.Power.getBatteryLevel();
  r.baseOk = base > 0 ? 1 : 0;
  Preferences prefs;
  prefs.begin("batlog", false);
  prefs.putBytes("rows", batRows, batCount * sizeof(BatRow));
  prefs.end();
}

void drawBatScreen() {
  const BatRow& r = batRows[batCount - 1];
  const uint32_t minutes = (millis() - batStarted) / 60000;
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(8, 8);
  M5.Display.printf("BATTERY TEST %lu:%02lu", static_cast<unsigned long>(minutes / 60),
                    static_cast<unsigned long>(minutes % 60));
  M5.Display.setCursor(8, 40);
  M5.Display.printf("base  %.2f V", r.baseMv / 1000.0f);
  M5.Display.setCursor(8, 64);
  M5.Display.printf("stick %.2f V %u%%", r.stickMv / 1000.0f, r.level);
  M5.Display.setCursor(8, 104);
  M5.Display.print("Button = end test");
}

void cmdBatRun() {
  stopAll();
  batCount = 0;
  batRunning = true;
  batStarted = millis();
  batNext = batStarted + kBatEveryMs;
  batWifiOffAt = millis() + 2000;  // Lets this reply reach the laptop first.
  batSample();
  drawBatScreen();
  console.printf("battery test started: base %.2f V, stick %.2f V %u%%. Wi-Fi goes off in 2 s\n",
                 batRows[0].baseMv / 1000.0f, batRows[0].stickMv / 1000.0f, batRows[0].level);
}

void batLoop() {
  if (batWifiOffAt != 0 && static_cast<int32_t>(millis() - batWifiOffAt) >= 0) {
    batWifiOffAt = 0;
    shellClient.stop();
    shellAuthed = false;
    MDNS.end();  // first: the pet firmware crashed when mDNS outlived the radio (2026-11-10). Untested here
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    wifiEnabled = false;
  }
  if (static_cast<int32_t>(millis() - batNext) >= 0) {
    batNext += kBatEveryMs;
    batSample();
    drawBatScreen();
  }
}

void cmdBatLog() {
  static BatRow rows[kBatMax];
  Preferences prefs;
  size_t n = 0;
  if (prefs.begin("batlog", true)) {
    n = min<size_t>(prefs.getBytesLength("rows") / sizeof(BatRow), kBatMax);
    prefs.getBytes("rows", rows, n * sizeof(BatRow));
    prefs.end();
  }
  console.printf("batlog rows=%u every=%lus\n", static_cast<unsigned>(n),
                 static_cast<unsigned long>(kBatEveryMs / 1000));
  for (size_t i = 0; i < n; ++i) {
    console.printf("%4u min  base %.2f V%s  stick %.2f V %u%%\n", rows[i].minute,
                   rows[i].baseMv / 1000.0f, rows[i].baseOk ? "" : " (no answer)",
                   rows[i].stickMv / 1000.0f, rows[i].level);
  }
}

// --- Shell -----------------------------------------------------------------------------------

// True if the words after a command are only numbers and "dry", so a typo can't start a real run.
bool onlyDry(const char* rest) {
  for (const char* p = rest; *p != 0; ++p) {
    if (isalpha(static_cast<unsigned char>(*p)) && strncmp(p, "dry", 3) != 0 &&
        (p == rest || !isalpha(static_cast<unsigned char>(p[-1])))) {
      return false;
    }
  }
  return true;
}

void handle(char* line) {
  char word[16] = {0};
  int a, b, c, d, e, k;
  float f, g, h;
  if (strcmp(line, "ping") == 0) {
    console.println("pong");
  } else if (strcmp(line, "info") == 0) {
    console.printf("built=%s %s board=%d psram=%u heap=%u\n", __DATE__, __TIME__, static_cast<int>(M5.getBoard()),
                  ESP.getPsramSize(), ESP.getFreeHeap());
  } else if (strcmp(line, "scan") == 0) {
    cmdScan();
  } else if (strcmp(line, "bat") == 0) {
    cmdBattery();
  } else if (strcmp(line, "fw") == 0) {
    cmdVersion();
  } else if (strcmp(line, "clock") == 0) {
    printClock();
  } else if (sscanf(line, "clock set %d %d %d %d %d %d", &a, &b, &c, &d, &e, &k) == 6) {
    cmdClockSet(a, b, c, d, e, k);
  } else if (sscanf(line, "imulog %d", &a) == 1) {
    if (sscanf(line, "imulog %d %15s", &a, word) != 2) strcpy(word, "LOG");
    cmdImuLog(a, word);
  } else if (strcmp(line, "batrun") == 0) {
    cmdBatRun();
  } else if (strcmp(line, "batlog") == 0) {
    cmdBatLog();
  } else if (strcmp(line, "poweroff") == 0) {
    stopAll();
    console.println("powering off");
    Serial.flush();
    delay(500);
    M5.Power.powerOff();
  } else if (strcmp(line, "speeds") == 0) {
    cmdSpeeds();
  } else if (strcmp(line, "demolog") == 0) {
    cmdDemoLog();
  } else if (sscanf(line, "demo %d", &a) == 1) {
    runDemo(a);
  } else if (sscanf(line, "motortest %d", &a) == 1) {
    runMotorTest(a);
  } else if (sscanf(line, "pairtest %d", &a) == 1) {
    runPairTest(a);
  } else if (sscanf(line, "legtest %d", &a) == 1) {
    runLegTest(a);
  } else if (sscanf(line, "leg %d %d", &a, &b) == 2) {
    if (sscanf(line, "leg %d %d %d", &a, &b, &c) != 3) c = 2000;
    runOneLeg(a, b, c);
  } else if (sscanf(line, "steertest %d", &a) == 1 || sscanf(line, "dithertest %d", &a) == 1) {
    // steertest|dithertest <speed> [ms] [dry]
    const bool dither = strncmp(line, "dithertest", 10) == 0;
    const char* rest = line + (dither ? 10 : 9);
    const bool dry = strstr(rest, "dry") != nullptr;
    if (sscanf(rest, "%d %d", &a, &b) != 2) b = dither ? 1200 : 1500;
    if (!onlyDry(rest)) {
      console.println("ERR usage: steertest|dithertest <speed> [ms] [dry]");
      return;
    }
    if (dither) {
      runDitherTest(a, b, dry);
    } else {
      runSteerTest(a, b, dry);
    }
  } else if ((strncmp(line, "slowtest", 8) == 0 || strncmp(line, "softtest", 8) == 0) &&
             (line[8] == 0 || line[8] == ' ')) {
    // slowtest|softtest [ms] [dry]
    const char* rest = line + 8;
    if (sscanf(rest, "%d", &b) != 1) b = 1200;
    if (!onlyDry(rest)) {
      console.println("ERR usage: slowtest|softtest [ms] [dry]");
      return;
    }
    if (line[1] == 'o') {
      runSoftTest(b, strstr(rest, "dry") != nullptr);
    } else {
      runSlowTest(b, strstr(rest, "dry") != nullptr);
    }
  } else if (sscanf(line, "shaketest %d", &a) == 1) {
    runShakeTest(a);
  } else if (strcmp(line, "steerlog") == 0) {
    cmdSteerLog();
  } else if (sscanf(line, "steergains %f %f %f", &f, &g, &h) == 3) {
    if (f < 0 || f > 2 || g < 0 || g > 2 || h < 0 || h > 2) {
      console.println("ERR gains must be 0 to 2");
      return;
    }
    steerKp = f;
    steerKd = g;
    steerKi = h;
    console.printf("gains kp=%.3f kd=%.3f ki=%.3f\n", steerKp, steerKd, steerKi);
  } else if (strcmp(line, "ip") == 0) {
    console.printf("ip=%s status=%d rssi=%d\n", WiFi.localIP().toString().c_str(),
                   static_cast<int>(WiFi.status()), WiFi.RSSI());
  } else if (strcmp(line, "stop") == 0) {
    stopAll();
    console.println("stopped");
  } else if (sscanf(line, "m %d %d %d %d %d", &a, &b, &c, &d, &e) == 5) {
    startRun(a, b, c, d, e);
  } else if (sscanf(line, "mv %15s %d %d", word, &a, &b) == 3) {
    int v[4];
    if (moveVector(word, a, v)) {
      startRun(v[0], v[1], v[2], v[3], b);
    } else {
      console.println("unknown move");
    }
  } else if (sscanf(line, "imurun %d %d %d %d %d", &a, &b, &c, &d, &e) == 5) {
    cmdImuRun(a, b, c, d, e);
  } else if (sscanf(line, "imu %d", &a) == 1) {
    cmdImu(a);
  } else if (sscanf(line, "sonic %d %d", &a, &b) == 2) {
    cmdSonic(a, b);
  } else if (sscanf(line, "rgb %d %d %d %d %d", &a, &b, &c, &d, &e) == 4) {
    const uint8_t data[4] = {static_cast<uint8_t>(a), static_cast<uint8_t>(b),
                             static_cast<uint8_t>(c), static_cast<uint8_t>(d)};
    console.println(bugWrite(0x10, data, 4) ? "rgb set" : "rgb failed");
  } else if (sscanf(line, "mic %f", &f) == 1) {
    const bool labelled = sscanf(line, "mic %f %15s", &f, word) == 2;
    cmdMic(f, labelled ? word : nullptr);
  } else if (sscanf(line, "ir %d", &a) == 1) {
    cmdIr(a);
  } else if (sscanf(line, "beep %d %d", &a, &b) == 2) {
    M5.Speaker.tone(a, b);
    console.println("beeped");
  } else {
    console.printf("ERR unknown: %s\n", line);
    return;
  }
  console.println("OK");
}

// --- Wi-Fi --------------------------------------------------------------------------------------

void wifiBegin() {
  Preferences prefs;
  prefs.begin("robot", true);
  const String ssid = prefs.getString("ssid", "");
  const String pass = prefs.getString("pass", "");
  shellToken = prefs.getString("token", "");
  prefs.end();
  if (ssid.isEmpty() || shellToken.isEmpty()) return;
  WiFi.setHostname(kHostname);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid.c_str(), pass.c_str());
  wifiEnabled = true;
}

// SSIDs and passwords can contain spaces, so the setup tool sends them as hex.
String fromHex(const char* hex) {
  String out;
  for (; hex[0] && hex[1]; hex += 2) {
    const char pair[3] = {hex[0], hex[1], 0};
    out += static_cast<char>(strtol(pair, nullptr, 16));
  }
  return out;
}

// USB only, and never echoed: the arguments hold the Wi-Fi password.
void cmdWifiSave(char* args) {
  const char* ssid = strtok(args, " ");
  const char* pass = strtok(nullptr, " ");
  const char* token = strtok(nullptr, " ");
  if (token == nullptr) {
    console.println("ERR wifi format");
    return;
  }
  Preferences prefs;
  prefs.begin("robot", false);
  prefs.putString("ssid", fromHex(ssid));
  prefs.putString("pass", fromHex(pass));
  prefs.putString("token", token);
  prefs.end();
  console.println("wifi saved");
  stopAll();
  delay(100);
  ESP.restart();
}

// Receives a firmware image over the authenticated Wi-Fi connection.
void cmdUpdate(size_t size, const char* md5) {
  stopAll();
  if (!Update.begin(size)) {
    console.println("ERR update begin");
    return;
  }
  Update.setMD5(md5);
  showLabel("UPDATING", TFT_NAVY);
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
    M5.Display.fillScreen(TFT_BLACK);
    console.printf("ERR update failed: %s\n", Update.errorString());
    return;
  }
  console.println("updated, restarting");
  console.println("OK");
  delay(300);
  ESP.restart();
}

void onLine(char* line, bool fromUsb) {
  unsigned size = 0;
  char md5[33] = {0};
  if (fromUsb && strncmp(line, "wifi ", 5) == 0) {
    cmdWifiSave(line + 5);
    return;
  }
  if (!fromUsb && !shellAuthed) {
    if (strncmp(line, "auth ", 5) == 0 && shellToken == (line + 5)) {
      shellAuthed = true;
      console.println("OK");
    } else {
      shellClient.stop();
    }
    return;
  }
  if (!fromUsb && sscanf(line, "update %u %32s", &size, md5) == 2) {
    cmdUpdate(size, md5);
    return;
  }
  commandFromWifi = !fromUsb;
  handle(line);
  commandFromWifi = false;
}

struct LineBuffer {
  char text[256];
  size_t length;
};
LineBuffer usbLine;
LineBuffer wifiLine;

void feed(LineBuffer& buffer, char ch, bool fromUsb) {
  if (ch == '\n' || ch == '\r') {
    if (buffer.length > 0) {
      buffer.text[buffer.length] = 0;
      buffer.length = 0;
      onLine(buffer.text, fromUsb);
    }
  } else if (buffer.length + 1 < sizeof(buffer.text)) {
    buffer.text[buffer.length++] = ch;
  }
}

void pollWifi() {
  static bool serving = false;
  if (!wifiEnabled) return;
  if (!serving) {
    if (WiFi.status() != WL_CONNECTED) return;
    serving = true;
    shellServer.begin();
    shellServer.setNoDelay(true);
    MDNS.begin(kHostname);
  }
  if (shellServer.hasClient()) {
    shellClient.stop();
    shellClient = shellServer.accept();
    shellAuthed = false;
    wifiLine.length = 0;
  }
  while (shellClient.connected() && shellClient.available()) {
    feed(wifiLine, shellClient.read(), false);
  }
}

}  // namespace

void setup() {
  // Before anything else: the base may still hold a speed from before a reset.
  busBegin();
  setMotors(0, 0, 0, 0);

  auto cfg = M5.config();
  cfg.serial_baudrate = 921600;
  M5.begin(cfg);
  M5.Ex_I2C.begin();
  M5.Speaker.begin();
  M5.Speaker.setVolume(96);
  M5.Display.setRotation(1);
  M5.Display.setTextSize(2);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(8, 8);
  M5.Display.print("BENCH");
  stopAll();

  // See readGyro: drop M5Unified's offset and take a first bias reading. Every countdown
  // measures it again.
  M5.Imu.clearOffsetData();
  BiasMeter meter;
  for (const uint32_t started = millis(); millis() - started < 500;) {
    if (M5.Imu.update()) meter.add();
    delay(2);
  }
  meter.apply();
  wifiBegin();
  console.println("READY");
}

void loop() {
  M5.update();
  if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) {
    stopAll();
    console.println("ESTOP");
    if (batRunning) {
      batSample();
      console.println("battery test ended by button; restarting");
      Serial.flush();
      delay(200);
      ESP.restart();
    }
  }
  if (motorDeadline != 0 && static_cast<int32_t>(millis() - motorDeadline) >= 0) {
    stopAll();
    console.println("run finished");
  }
  if (stopPending) retryStop();
  while (Serial.available()) feed(usbLine, Serial.read(), true);
  pollWifi();
  if (batRunning) {
    batLoop();
  } else {
    updateStatus();
  }
  delay(1);
}
