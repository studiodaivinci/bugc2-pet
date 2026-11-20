// The command shell over USB and Wi-Fi, and firmware updates over Wi-Fi.
//
// Wi-Fi uses the network and token in Preferences namespace "robot" (written by tools/wifi_setup.py through the bench
// firmware), with the same shell and update protocol as firmware/bench: port 3333, "auth <token>", then
// "update <size> <md5>". So tools/bench.py --update puts the bench firmware back without a cable, and
// firmware/pet/upload_wifi.py installs this one. Power saving stays off: with it on, the remote's pin (G36) and the
// side button (G39) see false edges (docs/HARDWARE.md).
#pragma once

#include <Arduino.h>

extern Print& console;  // USB serial, plus the Wi-Fi client once it has logged in

namespace net {

using Handler = void (*)(const String& line);

void begin(bool wifi, void (*beforeUpdate)());  // beforeUpdate: stop sound, wait for the screen
void poll(Handler handle);                       // reads whole lines from USB and Wi-Fi and hands them over
bool wifiOn();
String address();  // the IP, or "off" / "connecting"
void off();        // radio off until the next restart
bool forget();     // erases the Wi-Fi network and token from "robot"; the golf records and the pet's save stay

}  // namespace net
