#include "net.h"

#include <ESPmDNS.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>

namespace {

WiFiServer gServer(3333);
WiFiClient gClient;
String gToken;
bool gAuthed = false, gWifiOn = false, gServing = false;
void (*gBeforeUpdate)() = nullptr;
String gUsbLine, gWifiLine;

class Console : public Print {
 public:
  using Print::write;
  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t* data, size_t n) override {
    Serial.write(data, n);
    if (gAuthed && gClient.connected()) gClient.write(data, n);
    return n;
  }
};
Console gConsole;

void update(size_t size, const char* md5) {
  if (gBeforeUpdate) gBeforeUpdate();
  if (!Update.begin(size)) {
    console.println("ERR update begin");
    return;
  }
  Update.setMD5(md5);
  M5.Display.setRotation(0);
  M5.Display.setBrightness(120);
  M5.Display.fillScreen(TFT_NAVY);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextSize(2);
  M5.Display.drawString("UPDATING", 10, 110);
  console.println("SEND");
  static uint8_t chunk[1436];
  size_t received = 0;
  uint32_t lastData = millis();
  while (received < size) {
    const int count = gClient.read(chunk, min(sizeof(chunk), size - received));
    if (count > 0) {
      if (Update.write(chunk, count) != static_cast<size_t>(count)) break;
      received += count;
      lastData = millis();
    } else if (!gClient.connected() || millis() - lastData > 10000) {
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

void line(const String& raw, bool fromUsb, net::Handler handle) {
  String l = raw;
  l.trim();
  if (!l.length()) return;
  if (!fromUsb && !gAuthed) {
    if (l.startsWith("auth ") && gToken.length() && l.substring(5) == gToken) {
      gAuthed = true;
      console.println("OK");
    } else {
      gClient.stop();
    }
    return;
  }
  unsigned size = 0;
  char md5[33] = {0};
  if (!fromUsb && sscanf(l.c_str(), "update %u %32s", &size, md5) == 2) {
    update(size, md5);
    return;
  }
  handle(l);
}

}  // namespace

Print& console = gConsole;

namespace net {

void begin(bool wifi, void (*beforeUpdate)()) {
  gBeforeUpdate = beforeUpdate;
  if (!wifi) {
    WiFi.mode(WIFI_OFF);
    return;
  }
  Preferences prefs;
  prefs.begin("robot", true);  // read only
  const String ssid = prefs.getString("ssid", "");
  const String pass = prefs.getString("pass", "");
  gToken = prefs.getString("token", "");
  prefs.end();
  if (ssid.isEmpty() || gToken.isEmpty()) {
    Serial.println("no Wi-Fi settings stored; USB only");
    WiFi.mode(WIFI_OFF);
    return;
  }
  WiFi.setHostname("bugc2-pet");
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid.c_str(), pass.c_str());
  gWifiOn = true;
}

void poll(Handler handle) {
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (gUsbLine.length()) line(gUsbLine, true, handle);
      gUsbLine = "";
    } else if (gUsbLine.length() < 200) {
      gUsbLine += c;
    }
  }
  if (!gWifiOn) return;
  if (!gServing) {
    if (WiFi.status() != WL_CONNECTED) return;
    gServing = true;
    WiFi.setSleep(false);
    gServer.begin();
    gServer.setNoDelay(true);
    MDNS.begin("bugc2-pet");
  }
  if (gServer.hasClient()) {
    gClient.stop();
    gClient = gServer.accept();
    gAuthed = false;
    gWifiLine = "";
  }
  while (gClient.connected() && gClient.available()) {
    const char c = gClient.read();
    if (c == '\n' || c == '\r') {
      if (gWifiLine.length()) line(gWifiLine, false, handle);
      gWifiLine = "";
    } else if (gWifiLine.length() < 200) {
      gWifiLine += c;
    }
  }
}

bool wifiOn() { return gWifiOn; }

String address() {
  if (!gWifiOn) return "off";
  return WiFi.isConnected() ? WiFi.localIP().toString() : String("connecting");
}

void off() {
  if (!gWifiOn) return;
  gAuthed = false;
  gClient.stop();
  gServer.end();
  if (gServing) MDNS.end();  // first: left running, its task frees a packet after the radio has gone and crashes
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  gWifiOn = gServing = false;
}

bool forget() {
  Preferences prefs;
  if (!prefs.begin("robot", false)) return false;
  const bool ok = prefs.remove("ssid") | prefs.remove("pass") | prefs.remove("token");
  prefs.end();
  return ok;
}

}  // namespace net
