# Hardware facts

What we own, from primary sources (firmware source, schematic, library, M5Stack docs) and the
photos in `docs/photos/`. See "Bench results" for what has been confirmed on the robot itself.

## What it is

**M5Stack BugC2 with M5StickC Plus2**, SKU K033-C, plus a **Unit Ultrasonic-I2C** (U098-B1).
The SKU label is unreadable in the photos, but `esptool` confirms the chip: ESP32-PICO-V3-02 rev 3.1,
8 MB flash, 2 MB PSRAM. USB serial is a WCH CH9102 (USB vendor 0x1A86).

## How it moves

The legs do not articulate. There are no servos or joints.

- Each corner is a fixed, spring-suspended leg holding one DC gear motor.
- The motor shaft sticks out of the foot with a silicone tip on it (visible in the photos). The
  tip spins against the floor like a tiny angled wheel.
- The library treats the four legs as an **X-drive** with no walking gait. At speed 70 on hardwood
  it spins instead of driving. See "Cable-free run".
- No encoders. Speed and distance are open-loop. Turns can be closed-loop with the gyro.
- Leave the clear film on the springs on. M5Stack warns the springs may detach without it.
- The small black parts in the bag are spare silicone tips (12 ship in the box).

Motor patterns from `M5HatBugC.cpp`, channels `(0, 1, 2, 3)`:

| Move | Speeds |
|---|---|
| Forward | `(s, -s, s, -s)` |
| Backward | `(-s, s, -s, s)` |
| Left | `(-s, -s, s, s)` |
| Right | `(s, s, -s, -s)` |
| Rotate | `(s, s, s, s)` |
| Reverse rotate | `(-s, -s, -s, -s)` |

## Base (BugC2)

- STM32F030F4, I2C slave at **0x38**, on Stick pins **SDA G0, SCL G26**.
- Four L9110S H-bridges (schematic U1, U2, U4, U5), run from the battery rail.
- Two WS2812C RGB LEDs. **Both light as commanded** (2026-11-08: `rgb 0 255 0 0` red, `rgb 1 0 0 255`
  blue). Which index is which side wasn't checked.
- SL0038GD IR receiver wired to **G36** through the header, so the Stick decodes it directly.
  **Works** (2026-11-08, see "IR receiver" below). G36 sees false edges while Wi-Fi power saving
  is on, so turn Wi-Fi sleep off while listening, or keep Wi-Fi off. The side button (G39) has the same
  problem: with Wi-Fi power saving on it reads phantom presses, which quit the golf game in a playtest
  (2026-11-09); `WiFi.setSleep(false)` cured it.
- 16340 cell, 700 mAh. TP4057 charger.
- Base USB-C is **power only** (schematic shows VBUS and CC, no data lines).
  Programming uses the Stick's own USB-C port.
- **The Stick can slide about 1 cm along its length** (not sideways or up). The base's header pins
  are at the Stick's screen end, so sliding moves it on and off the pins, which are its only link to
  the motors. Friction alone holds it. Spinning hasn't moved it, but a child could. If it comes off
  mid-run, the base keeps its last speed. The bench firmware now shows BASE LOST and beeps every
  second, retrying the stop until it gets through. Push it fully in before every test.
  The schematics narrow this (read 2026-11-09, untested): the base's motor supply
  is switched by the Stick's 3.3 V, so a Stick switched off or fully off the pins stops the motors.
  The last speed is kept when the Stick hangs, or slides just far enough to lose the two data pins
  but not power. A spacer and strap would stop that.

| Register | Access | Meaning |
|---|---|---|
| `0x00`-`0x03` | R/W | Motor speed, int8. Library clamps to -100..100. Firmware PWM period is 100 counts. |
| `0x10` | W | RGB: `[index, R, G, B]`. Index 0 left, 1 right. |
| `0x20` | R | ADC 8-bit |
| `0x30` | R | ADC 12-bit, little-endian. Battery V = `raw / 4095 * 3.3 * 2.960784` |
| `0xFD` | W | Tears down I2C, apparently firmware-update entry. **Never write.** |
| `0xFE` | R | Firmware version |
| `0xFF` | R/W | I2C address, saved to flash. **Never write.** |

## Controller (M5StickC Plus2)

| Part | Detail |
|---|---|
| SoC | ESP32-PICO-V3-02, 8 MB flash, 2 MB PSRAM, 2.4 GHz Wi-Fi only |
| Display | 1.14" colour TFT, 135x240, ST7789V2, backlight G27 |
| IMU | MPU6886 on internal I2C (SDA G21, SCL G22) |
| Mic | SPM1423 PDM, **CLK G0**, DATA G34 |
| Sound | Passive buzzer, G2. Tones only, no speaker. |
| IR | Transmit LED on G19, shared with the red LED |
| Buttons | A G37, B G39, C G35 (also wake) |
| Power | HOLD G4, battery sense G38, 200 mAh internal cell |
| Grove | SDA G32 (yellow), SCL G33 (white), 5V, GND. Free: it sits beside the Stick's USB-C. |

### Which way is up

Decided 2026-11-10: **the screen's up is the golf game's up**, display rotation 0. Everything drawn on the
robot uses it: the face, the menu, golf and anything added later. The face lying flat used to turn 180° to it, so
the face and golf pointed opposite ways. The one exception: held upright the other way round, the face turns 180° to
stay upright for whoever is holding it (`kUpRotation` and `updateRotation` in `firmware/pet/src/main.cpp`).

### Buttons in use

On the robot as built (2026-11-10):
- **Big button** (A, G37, on the front under the screen): easy to press. It should do nearly everything.
- **Power button** (C, G35, on the side): hard to press, because a cable is plugged in right beside it. Use it as
  little as possible: switching on, and nothing play depends on. Holding it 6 s is a hardware power cut.
- **Side button** (B, G39): the same orange as the case and nearly flush, beside the beam at the USB end. We
  didn't spot it at first, so treat it as hard to find. The pet uses it for "next" in the menu and a pat; golf uses it for
  pause and back.

**Switching off on USB power** (found 2026-11-10): the stick switches off by letting go of its power-hold pin, which
only cuts the power on the battery. On a USB cable M5Unified's `powerOff()` falls through to deep sleep with nothing
armed to wake it, so the robot stays dark until a reset (the serial port's RTS line, or the power button). The pet's
`switchOff()` arms the big button first, so a press restarts it.

## Sensor (Unit Ultrasonic-I2C)

RCWL-9620, I2C **0x57**, 2 to 450 cm, ±2%, 60° beam, 56 x 24 x 15.2 mm, 12.2 g.

Read: write `0x01`, wait, read 3 bytes, big-endian, divide by 1000 for mm. The library waits
120 ms (about 8 readings a second). The docs state a 50 ms cycle, so 20 a second may be possible.

## Constraints that shape the design

1. **Mic and motors share G0.** The mic clock is the motor bus's SDA line, so the robot cannot
   listen and command motors at the same moment. Stop, release the bus, record, restore the bus.
   Motors hold their last speed through this, so always stop first.
2. **No motor failsafe.** The STM32 has no watchdog and holds the last speed until told otherwise
   (`i2c_timeout_counter` only resets the I2C peripheral). If the Stick hangs mid-move, the robot
   keeps driving. Firmware must time-limit every move, use the ESP32 watchdog, and send a stop
   first thing at boot.
3. **Three I2C buses, two ESP32 controllers.** IMU, motor bus and Grove each need one. Bit-bang
   one, or remap a controller per transaction.
4. **No cliff sensor.** It will drive off a table. Free roaming is floor only.
5. **No encoders.** Forward distance is a time estimate that varies with battery and surface.
6. **G36 shares a header pin with G25.** Leave G25 as a floating input so IR receive works.

## Bench results

Tested 2026-11-06 with `firmware/bench`, driven by `tools/bench.py`.

| Test | Result |
|---|---|
| Base on motor bus | Answers at 0x38, firmware version 1 |
| Software I2C on G0/G26 | Works, including clock stretching |
| Internal bus | 0x51 (RTC) and 0x68 (IMU) both answer |
| IMU | Reads 1.0 g at rest, gyro noise under 1 deg/s |
| Stick battery | 3.82 V, about 65% |
| Buzzer | Commanded without error (not yet heard) |
| Mic capture | Runs at 16 kHz; 6 of 6 captures returned full audio |
| **Mic / motor bus sharing** | **Solved.** After every capture the base still answered and took commands. No stop was lost. |
| **Motors** | **Work with the base USB-C plugged in.** Reached 140 deg/s at speed 60. Speed 40 barely moves it. Rocks strongly (up to 147 deg/s of tilt). |
| **Rotation asymmetry** | **Confirmed without cables.** Opposite commands spin it the same way. See "Cable-free run". |
| **Base battery** | **Connected.** A plastic insulating tab was blocking the battery contact, which explains the 0 V reading and the 4.1 to 4.7 V on USB. With the tab removed it reads 3.98 V. The motor results above were on USB power alone. Not yet tested under motor load. |
| Ultrasonic | Answers at 0x57 on Grove. At 120 ms: 618 to 623 mm, stable. Rate tops out near 19 Hz; at that rate about 1 reading in 6 is an outlier, so it needs a median filter. |

Also learned:

- The base runs PWM in its main loop and reads the battery inside an interrupt, so reading the
  battery briefly stalls the motors. Don't read it mid-move.
- M5Unified's first few milliseconds of mic audio contain a large startup spike. Skip them.
- M5Unified's stored gyro offset can be badly wrong: at rest it read -43, -58 and +16 deg/s. With
  the offset cleared, the chip's own bias is about 1 deg/s.
- The gyro clips at about 2000 deg/s, and the robot passes that within 0.3 s of a spin at speed 40. Counter-clockwise
  clips at about +1983 and clockwise at exactly -2000. Positive is counter-clockwise from above; M5Unified doesn't
  flip any axis on this board. So angles measured through a fast spin read low, and a heading taken before one is
  lost. (On 2026-11-11 the -2000 readings were first taken for a sign flip in the library, and pet 0.4 and 0.5
  turned them into +2000: `unwrapGz`, in pet 0.5. That was wrong and is gone: every clockwise spin in those
  two builds' reports reads as counter-clockwise.) Which way a spin command turns the robot isn't fixed either: it
  depends on which feet grip.
- The factory firmware contains the string `BugC2_Slave`, so it likely expects a paired remote.
- Back up the factory firmware before you flash anything (it is M5Stack's, so it isn't in this repo):
  `esptool --port <port> read-flash 0 0x800000 factory-stickc-plus2-8MB.bin`. Restore with
  `esptool --port <port> write-flash 0 factory-stickc-plus2-8MB.bin`.

## Cable-free run

Tested 2026-11-06 on hardwood: battery only, sensor unplugged, speed 70, 900 ms per move. Yaw is from
the gyro; positive is counter-clockwise seen from above. The rotate rows are wrong in sign: see below.

| Move | Yaw (deg) | Peak tilt rate (deg/s) | Peak accel (g) | Base V just after |
|---|---|---|---|---|
| forward | +1067 | 169 | 4.6 | 3.80 |
| back | +1208 | 187 | 5.8 | 3.81 |
| left | -1206 | 97 | 4.0 | 3.79 |
| right | -1063 | 152 | 5.1 | 3.80 |
| rotate | +1604 | 157 | 4.9 | 3.86 |
| rrotate | +1702 | 156 | 5.0 | 3.85 |

- Every move is a fast spin with a slower drift, about 4 to 5 turns a second. None drove straight.
- On video, forward and back both spin counter-clockwise, and left and right both spin clockwise.
- rotate and rrotate do reverse. Frames at spin-up and spin-down show rotate clockwise and rrotate
  counter-clockwise, so those two moves work as intended.
- One weak or dead motor can't explain this. If each motor added its own effect, forward + back and
  left + right would be equal. They are +2275 and -2270.
- The commands aren't at fault: the base firmware source swaps the H-bridge pins for negative speeds.
- Fast clockwise spins read as counter-clockwise. rotate logged +1604 while the video shows it turning
  clockwise, with the gyro at its 2000 deg/s limit. Cause found and fixed on 2026-11-07: M5Unified
  subtracts its stored gyro offset in 32-bit integer maths (`IMU_Class.inl`, `getImuData`), which
  overflows near the limit and flips the sign. The firmware now clears that offset and measures the
  bias itself during each countdown. Readings at the limit are flagged `CLIPPED` in `demolog`; their
  yaw is too small but the sign is right.
- The battery held up: 3.98 V before, 3.79 to 3.86 V just after each move, 3.95 V a few minutes later.

### Slower run

Same floor, started and read back over Wi-Fi with no USB cable. 600 ms per move. Yaw in degrees.

**Part of this run is unreliable.** The robot headed for the stairs and was picked up at an unknown
point while the test kept running, so later rows may come from the air. The speed 30 and 40 rows
match the first run. Treat left and right at speed 50 and all the single-motor rows as unconfirmed.

| Speed | forward | back | left | right | rotate | rrotate |
|---|---|---|---|---|---|---|
| 30 | +367 | +535 | -438 | -523 | +760 | +1022 |
| 40 | +595 | +463 | -564 | -160 | +855 | +1054 |
| 50 | +406 | +542 | +94 | -11 | +924 | +1079 |

| Motor alone, speed 50 | + direction | - direction |
|---|---|---|
| 0 | -0.5 | +0.7 |
| 1 | -66.5 | +3.3 |
| 2 | -0.2 | +442 (peak 1983 deg/s) |
| 3 | -9.6 | +1.2 |

- Slowing down does not fix it. The same pattern holds at speed 30, the lowest tested.
- The rotate column has the wrong sign, as in the first run: it is clockwise.

### Single motors on the floor

`motortest 50`, 1 second per step, on hardwood. Yaw in degrees. The video shows the robot standing
on the floor for the whole run, so this is clean data.

| Motor | + direction | - direction |
|---|---|---|
| 0 | -0.4 | +0.6 |
| 1 | -4.7 | +1410 (peak 1983 deg/s) |
| 2 | -1037 (peak 1765 deg/s) | +1156 (peak 1983 deg/s) |
| 3 | -0.2 | +0.4 |

- Below about 2000 deg/s the gyro is trustworthy. The three steps with large yaw are exactly the three
  where the video shows the robot moving, and motors 1- and 2- turn counter-clockwise on video.
- All eight steps can be heard on the video's audio, so all four motors run both ways. Only three
  steps moved the robot. On the others the tip spins without moving it.
- Motor 2 behaves like a normal drive motor: it moves the robot both ways, and reversing it reverses
  the spin. Motor 1 grips in one direction only. Motors 0 and 3 don't move it alone at speed 50.
- Where a motor moves the robot, + is clockwise and - is counter-clockwise, which matches rotate and
  rrotate.
- All four tips and feet look fine by eye, so worn or loose tips are unlikely.
- 2026-11-07, after `slowtest`: all four tips replaced with new spares from the box. The old ones
  showed nothing wrong. Every run after that is on the new tips.
- The two motors that moved it, 1 and 2, are front right and rear left. See "Leg map".

### What the documentation says

M5Stack's pages don't describe the drive type, expected behaviour, surfaces or calibration, and the
BugC2 "Video" section is empty. No owner reports of driving behaviour turned up. Useful fragments:

- The vendor example runs `move()` at speed 50 for 1 second, with on-screen arrows for forward, back,
  left, right and two rotations. Our patterns match its code exactly.
- **The motor order differs between products.** The original BugC register table is front-left,
  front-right, rear-left, rear-right. The BugC2 pin map lists Motor1 to 4 as left front, left rear,
  right rear, right front. The library uses the same `move()` patterns for both.
- With the original order, `forward` runs the left side against the right, which suits mirrored legs.
  With the BugC2 order, `forward` pairs diagonals, and `left` and `right` become the left-against-right
  pattern. The leg map ruled this out: the original order is right.
- The schematic doesn't settle it. Channels 0 to 3 go to connectors P1 to P4, wired the same way, and
  nothing ties a connector to a corner.
- The original BugC motors are listed at 3.7 V, 50 mA, 70 mA stall, 0.81 mm shaft, and "15000-2000rpm"
  as printed. So they may be fast and weak, though BugC2 specs aren't published.
- Pairing patterns tried: `forward` (0,2 against 1,3) and `left` (0,1 against 2,3). The third pairing,
  0,3 against 1,2, has never been run.

Second search, 2026-11-07 (Japanese sites, forums, retailers, GitHub, the factory firmware):

- The factory firmware is a remote-control receiver. Its strings include `BugC2_Slave`,
  `Slave_1_Password`, `ESPNow Init Success` and the IRremoteESP8266 library, so out of the box a
  person steers it from a paired M5 device over ESP-NOW (wireless) or with an IR remote.
  CNX Software lists its uses as "remote motor control, robot control, and an intelligent toy".
- The only owner-written driving code found is vkichline/BugC with BugController, for the original
  BugC with a joystick. Its README says: "It's not a very accurate or powerful robot, but it's fun
  to play with." It drives `(x, -x, x, -x)` for forward and steers like a tank by scaling down
  motors 0 and 2 or 1 and 3. It never uses the sideways or rotate patterns, and a person on the
  joystick corrects the heading.
- No reviews, forum posts or videos describe straight driving. The RobotShop BugC forum page (403) and
  The Pi Hut's reviews (429) refused automated reads and are unread.

### Leg map

Tested 2026-11-07 with `leg <channel> 30 5000`: one motor for 5 seconds, robot held upside down and
watched, one channel per go. The command only runs upside down, checked with the accelerometer, so
it can't drive off. At 2 seconds and speed 40 the foot was too fast to pick out. Corners are as seen
from above with the robot the right way up and the screen end, where the header pins are, as the
front.

| Channel | Leg |
|---|---|
| 0 | front left |
| 1 | front right |
| 2 | rear left |
| 3 | rear right |

- This is the original BugC order, so the library's patterns fit this robot. `forward` runs the left
  legs against the right ones and should drive towards the screen end. `left` and `right` run the
  front legs against the rear ones and should slide it sideways. The BugC2 pin map's order is wrong
  for this unit.
- The two motors that can't move the robot alone, 0 and 3, are diagonal: front left and rear right.
  So it probably rests on the other diagonal like a wobbly table, and those two feet barely grip.
  With only one diagonal gripping, any difference between its two feet becomes a spin. Unconfirmed.
- `legtest 40` runs all four in a row, with one beep before channel 0, two before channel 1, and so
  on. It was too fast to watch, so use `leg` instead.

### Next tests

**Corner check: done.** Pressing each corner, it doesn't rock. Springs could still hide a twist.

**`steertest 40`: can the gyro steer it straight?** Built and run 2026-11-07 on hardwood.

**Result: with steering, it drives.** Net turn was 550° in 1 s for forward without steering, +11° in
2.5 s with it, and -48° in 2.5 s for back with it. On video it takes off in a fairly direct line as
soon as the steered forward starts. From its size in the frame, it crosses about 40 cm in 0.6 s, so
roughly 50 cm/s. It wobbles about 50 to 70° each way, about once a second, so the gains need tuning.
It left the frame, went past the metre of clear floor planned, and fell down two stairs. Nothing
broke. The steered forward logged the run's largest tilt rate (379 deg/s), and the steered back's
1.5 s of steady -18 deg/s was probably the robot pinned against a riser, not smooth driving.
**Driving runs now need the stairs blocked.** Fall detection was considered and dropped: once it
detects a fall, it's too late. Forward and back both spin it counter-clockwise, so the steered moves add a turn to every
motor, `(s+t, -s+t, s+t, -s+t)`. In effect it drives one side harder, the same as tank steering. A
positive turn pushes clockwise, mostly through rear left (motor 2, the strongest foot). It
resizes the turn 50 times a second from heading error, spin rate and a slowly learned trim. The turn
is capped at 40.

The first run had a red unsteered forward (1 s), green steered forward and back (2.5 s each), then a
red unsteered back (1 s). Since then, `steertest <speed> [ms] [dry]` runs only the steered pair:
a 3 s blue countdown, green FWD G, then green BACK G, each `ms` long (default 1500). `steerlog`
prints each move every 40 ms (heading, spin rate, turn) and the trim it learned.
`steergains <kp> <kd> <ki>` changes the gains until the next restart. Defaults are 0.2, 0.04, 0.2.
`dry` runs it all with the motors off.

**Second run, softer gains (0.1, 0.04, 0.1), 1.5 s, speed 40:** worse. It arced forward and to the
left, about 107° in 0.7 s, the way it drifts without steering. Then it ran onto the edge of a carpet
and tipped over (1049 deg/s roll, 12 g). It lay there, so the rest of the run did nothing. The
defaults went back to the first run's gains. **A carpet edge can tip it at speed 40**, which matters
in a home with rugs. If it drives straight,
the learned trim is the start of self-calibration. If the turn sits at 40 the whole time, the
correction is too weak; if it swings between +40 and -40, it is overcorrecting.

**`shaketest 40`: can it shake in place?** Run 2026-11-07 on hardwood. Net yaw over 1.5 s, peak spin
rate, and shake (RMS g):

| Step | Net yaw | Peak spin | Shake |
|---|---|---|---|
| FIGHT | +834° | 1983 deg/s | 0.44 |
| 8 HZ | -27° | 1278 deg/s | 1.19 |
| 17 HZ | -67° | 640 deg/s | 0.80 |
| 33 HZ | 0° | 312 deg/s | 0.22 |
| FB 17 | +63° | 141 deg/s | 0.44 |

FIGHT spun counter-clockwise, as the single-leg data predicted, so it's a spin, not a shake. The
spin shakes get gentler as they get faster. 8 HZ wiggles hard and ends facing nearly the same way,
and 33 HZ is a light buzz with no net turn. FB 17 shakes with the least turning. On video, all of
them stayed in place: FIGHT drifted about 7 cm while spinning, 8 HZ crept about 3 cm, 17 HZ about
1 cm, 33 HZ not at all, and FB 17 under 1 cm. These are estimates from the robot's size in frame.

**`dithertest 40`: does a shake smooth the steering?** Run 2026-11-07. **No.** Heading range per
move, smaller is steadier:

| Pair | Forward | Back | Shake (RMS g) |
|---|---|---|---|
| 1 plain | 59° | 146° | 0.43 |
| 2 turn buzz 33 Hz | 161° | 94° | 0.76, 0.46 |
| 3 drive shake 17 Hz | 152° | 222°, spin clipped | 1.70, with 10 to 12 g jolts and rolls up to 1165 deg/s |

The video changes the drive-shake reading. During 3F the robot was jammed in the corner of a door
frame, which explains the jolts, so pair 3 wasn't a fair test. It also shows the robot never came
back. Over the six moves it went from the open room, along a wall, through a doorway and into a
hallway, several metres in all. Each move holds the heading it starts with, and the robot turns
during the pauses, so a back move doesn't retrace the forward move before it. The fix is one heading
kept across the whole run. The turn buzz was worse going forward and better going back, within
run-to-run noise. Plain steering was steadiest going forward. The robot moved during
the countdown (spread 396 deg/s), so the bias check rightly kept the earlier bias. Original design: Three steered forward/back pairs of 1.2 s each, about 14 s in all:

| Screen | Shake on top of steering |
|---|---|
| green 1F, 1B | none |
| teal 2F, 2B | turn swings +-20, reversing every 15 ms (33 Hz) |
| purple 3F, 3B | drive speed swings +-30, reversing every 30 ms (17 Hz) |

`steerlog` now gives each move's heading range, so the wobble can be compared directly.

Design notes: The steered runs wobble
because the feet don't slide until pushed hard enough, then go all at once. A fast shake ("dither")
might keep them gliding so steering can be gentle. Shaking in place is also a behaviour in itself:
shivering, trembling, purring. The firmware's move routine can now reverse every motor on a timer, and `demolog`
gives a `shake` value, the RMS of the acceleration's distance from 1 g. Each step runs 1.5 s,
about 13 s in all with the 3 s countdown.

| Screen | Speeds `(0, 1, 2, 3)` | Reverses every |
|---|---|---|
| red FIGHT | `(s, -s, -s, s)`, the diagonal pairing | never |
| green 8 HZ | rotate | 60 ms |
| green 17 HZ | rotate | 30 ms |
| green 33 HZ | rotate | 15 ms |
| teal FB 17 | forward | 30 ms |

FIGHT was predicted to shake in place on an ideal drive, but on this robot both gripping feet push
counter-clockwise in that pattern, so it may spin instead. A small net yaw and little travel on
video mean it stayed in place. Keep shakes short: rapid reversing hammers the small gearboxes.

**`slowtest [ms] [dry]`: slower driving that comes back.** Run 2026-11-07 on hardwood. **It works.**
Heading swing per move: speed 25 was 44° forward and 28° back, the steadiest yet; speed 30 was 49°
and 65°; stepping was 106° and 80°, wobblier because each burst is at speed 40. The run ended facing
-1° from its start. On video, measured against a rug corner, each back move retraced its forward
move along the same line and ended near where the pair began. The path isn't straight ahead of the
robot but at an angle to it, on a diagonal. The gyro holds which way it faces but
can't sense sliding, so it crabs at a steady angle. If that angle stays the same, steering to a
heading offset by it would cancel it. The camera moved and the video starts mid-run, so it can't say
whether speed 25 travelled on its own.

**Same video, re-analysed with floor tracking: the facing held, the travel direction didn't.** Every
frame was pinned to the wood grain (SIFT features, one homography per frame to a reference frame), so
the hand-held camera drops out, and the robot was found as the moving blob. The gyro heading matches
the screen's angle on video to within about 8°. Crab = how far the line it travels along is turned
from its body's line, positive counter-clockwise seen from above. Positive means forward slides left
of straight and back slides right of straight back, along the same line:

| Move | Crab | Inside the move | Distance |
|---|---|---|---|
| 25 F | +24° | +14 to +31 | over 50 cm; video starts 0.35 s in |
| 25 B | +22° | +14 to +26; retraced 25 F | about 90 cm |
| 30 F | +6° | -21 to -10, then +5 to +29: a bow, not straight | about 100 cm |
| 30 B | -10° | -25 to -11, then +14; didn't retrace 30 F | about 100 cm |
| step F | +13° | travel direction held within 4° | about 45 cm |
| step B | +16° | within 10°; retraced step F | about 50 cm |

- The facing barely moved during each move; the travel direction did. Steering holds the facing well.
  The problem is which way the feet push.
- Two states: the travel line turned 20 to 25° counter-clockwise from the body, or 10 to 20°
  clockwise. Speed 25 stayed counter-clockwise for both moves.
  Speed 30 switched mid-move, so 30 F's ends lined up (it looked straighter) but the path bowed.
- Wobble alone doesn't bend the path: in the step moves the facing swung about ±17° and the travel
  direction held within 4°.
- Likely cause, unconfirmed: one diagonal of feet (front right, rear left) does most of the pushing
  (motor table above), and the springs let the weight shift between diagonals as it bounces, which
  changes the push direction. More bounce at higher speed, more switching.
- Every stop twists it 15 to 20° clockwise (3° to -14°, 3° to -12°, 2° to -19° over the pauses),
  probably the springs unloading. Exact turns will need a soft stop.
- Distances use the Stick (54 mm) as a ruler, about 2 mm per map pixel, ±15%. Speed 25 still covers
  about a metre per 1.2 s move, so it isn't slow yet.
- The first reading (each back move retraced its forward move) holds for the 25 and step pairs, not
  the 30 pair.

**`slowtest` again on new tips, 2026-11-07 evening.** Same test and hallway, all four tips new, floor
tracking as above. Both videos are about 2 mm per map pixel, checked against the Stick on a pixel
ruler. The screen's glow in the dark video makes the robot's blob look twice its size, so don't use
blob size for scale.

| Move | Old tips: distance, crab | New tips: distance, crab |
|---|---|---|
| 25 F | over 50 cm, +24° | 4 cm, barely moved |
| 25 B | 93 cm, +22° | 4 cm, barely moved |
| 30 F | 102 cm, +6°, a bow: -15 then +25 | 37 cm, +16°, steady +14 to +23 |
| 30 B | 105 cm, -10°, switched | 78 cm, +7°, switched: -21 then +18 |
| step F | 44 cm, +13° | 37 cm, +14° |
| step B | 49 cm, +16° | 39 cm, +5° |

- The tips mattered, though the old ones looked fine. Heading swing about halved (30: 22° and 20°;
  step: 44° and 36°), paths are straighter (start-to-end over path length 0.99 to 1.00, was 0.93 to
  0.99), and it travels less.
- Speed 25 now barely moves it, with almost no shake (0.05 g), so the new tips need more push. Speed
  30 is the new slow, about 30 cm/s forward. Back at 30 went twice as far as forward.
- The slide angle is smaller and mostly one way, +5 to +16°, but 30 B still switched sides early in
  the move. Not yet a constant to subtract.
- Turn while stopped, after the moves that travelled: 31°, -17°, 1° (old tips: -21°, -72°, +24°). The robot moved during the countdown (spread 119 deg/s), so
the earlier bias was kept. Original design: Decision: the robot should move slowly, like a pet, not zip around the house,
and the test space is small. One heading is now kept for the whole run, tracked through pauses and
screen redraws, so each back move steers along the forward move's line. `steerlog` ends with where
the run finished facing. The steering correction is capped at the drive speed so it can't whip
round. 1.2 s per move, about 14 s in all:

| Screen | Drive |
|---|---|
| green 25 F, 25 B | speed 25, steady |
| teal 30 F, 30 B | speed 30, steady |
| purple ST F, ST B | speed 40 in 150 ms steps with 150 ms rests, like a bug scuttling |

Speed 40 barely moved it on USB power early on, but 30 spun it on battery. Below some speed the
motors may stall, so 25 might not move at all.

**`softtest [ms] [dry]`: soft starts and stops.** Built 2026-11-07, dry run passed, not yet run on
the floor. A steered move can now ease in over its first 300 ms and out over its last 300 ms
(smoothstep), with drive and steering scaled together. It's `rampMs` in the move's settings; 0 keeps
the old abrupt moves, so earlier tests are unchanged. One heading for the whole run. `steerlog` now
shows each tick's power % and, for one-heading runs, how far it turned while stopped between moves.
1.2 s per move, about 14 s in all:

| Screen | Drive |
|---|---|
| green 30 F, 30 B | speed 30, soft |
| purple ST F, ST B | speed 40 in 150 ms bursts with 150 ms rests, soft |
| teal 30 F, 30 B | speed 30, soft, repeating the first pair |

Speeds went up from 25 after the new tips stopped speed 25 moving it, so every move matches one in the
new-tips slow test. What to look for: the turn while stopped; whether both speed 30 pairs slide at the
same angle as each other, without the switch 30 B had; whether the bursts hold a direction. With ramps a move covers less ground. Below some power the feet don't
turn at all, so the ramp that matters is shorter than 300 ms.

**`pairtest 50`: which pairing drives in a line.** Built, not yet run. There are three ways to split four motors into
two opposing pairs. Each runs 0.8 seconds one way and then the other, with a large label and colour
on screen. About 14 seconds including a 5-second countdown.

| Label | Screen | Speeds `(0, 1, 2, 3)` | Library name |
|---|---|---|---|
| C+, C- | green | `(s, -s, -s, s)` and reverse | none, never run |
| A+, A- | dark red | `(s, -s, s, -s)` and reverse | forward, back |
| B+, B- | teal | `(-s, -s, s, s)` and reverse | left, right |

For an ideal drive like this one, one pairing goes forward and back, one goes sideways, and one
makes the feet fight each other with little motion. The leg map says that is A, B and C. If they
still spin, the cause is grip, not wiring. Fix the wobble first if the corner check finds one.

We checked runs on phone video; clips of about 17 seconds were enough for one test.

Floor runs must stay short and well away from stairs. It drifts unpredictably and has no drop sensor,
so 2 metres of clear floor was not enough for a 34-second run.

## Wi-Fi bench link

Tested 2026-11-06. Commands, mic audio and a firmware update all worked over Wi-Fi with the USB cable
attached, and a wrong token was refused. A full motor test then ran on battery with no cable.

- `tools/wifi_setup.py`, run once over USB, opens a window for the network name and password and
  stores them on the robot. The ESP32 only sees 2.4 GHz networks.
- `python tools/bench.py --wifi "<command>"` runs any bench command. `demo <speed>` runs the six moves in
  under 10 seconds, and `motortest <speed>` runs each motor alone. `leg` is under "Leg map", and
  `pairtest` and `steertest` are under "Next tests". `demolog` prints the last run. The top button only stops things.
- Bench tools (2026-11-09), all without motors, through `tools/capture.py`:
  - `clock` / `clock set Y M D h m s`: the clock chip (BM8563 at 0x51). `capture.py clockset` sets
    it from the PC to the second. `lowvolt=1` means the chip lost power and its time is untrusted.
  - `imulog <ms> <NAME>`: the IMU at 200 Hz for up to 30 s. The screen shows GET READY and the name,
    then a beep and GO with a countdown, then two beeps. `capture.py gesture <name> [s]` saves a CSV
    in `bench-output/gestures/`.
  - `mic <s> <WORD>`: shows the word, beeps, records with the word up in green.
    `capture.py voice <speaker> <condition>` records the six power names five times each, round
    robin, into `bench-output/voice/<speaker>/<condition>/`.
  - `batrun`: battery run-down with Wi-Fi off, screen on, no motors. Saves both batteries to flash
    every 2 minutes (12 hours at most), so the log survives the battery dying. A button ends it and
    restarts the robot. `batlog` prints the log.
  - `poweroff`: switches the Stick off, if nothing is powering it.
- Plugged into USB while switched off, the Stick stays off and doesn't answer. A USB install
  (`pio run -t upload`) resets it and starts it.
- The robot remembers one Wi-Fi network. On 2026-11-09 the PC was on a phone hotspot, so the
  robot couldn't be reached over Wi-Fi; `wifi_setup.py` over USB switches it to another network.
- `python tools/bench.py --update` builds the firmware and installs it over Wi-Fi.
- The robot's address and token are in `tools/robot.local.json`. Never commit or print that file.
- Any input, a button or a dropped connection stops a running `demo`. Single `m` and `mv` runs stop
  at their time limit.
- Start motion only after someone confirms the robot is on the floor.
- The Wi-Fi password sits in the robot's flash, unencrypted. Erase it (`wifi forget`) before you give the
  robot away.

## Sensor mount plan

**Not fitted in the gift build** (2026-11-09): it's hard to attach, there's no Velcro, and its
weight could change how the robot moves. The plan below is for a later upgrade.

Decision: hook-and-loop (Velcro), sideways like two eyes, on the solid outer face of a black beam.
Pins and an L-bracket were dropped: they add parts, limit the angle and are hard to adjust.

Each long side of the Stick has two stacked beams. The upper beam's holes face up, and its outer
face is solid, about 24 x 8 mm, with a textured finish. The lower beam's holes face outward. A
side button sits right beside the beam on the USB-end side. It's the same orange as the case and nearly flush, so it's easy to miss. Only one long side is photographed. Keep the button reachable.

- That surface is textured, so tape may not grip. If it peels, hot glue the Velcro patch to the
  beam and use the adhesive on the smooth sensor back. The sensor then stays removable.
- This assumes a long side becomes the front. The leg map says the library's `forward` points at
  the screen end instead, so this plan probably moves to that end. Decide once it drives straight.
- Point the sensor's connector towards the Grove port, and tie up the spare cable.
- Don't mount it on top: it blocks the screen and raises the weight.

## IR receiver

Tested 2026-11-08 with a cheap six-button heater remote (CR2025 cell). **The receiver works.**

- The first two runs were all noise: 318 edges in 20 s while pressing, then 1054 in 10 s with no
  remote at all, as single blips several times a second. G36 is one of the two ESP32 pins that
  glitch while Wi-Fi power saving or the ADC is on (Espressif errata 3.11). With Wi-Fi sleep off it
  was silent: 0 edges in 8 s, and the pin read high 100% of the time, so the receiver is powered.
- With presses: 5 bursts of 68 edges, all 5 counted live on screen, no noise. The leftover 48
  edges are the short repeat codes a held button sends.
- The remote speaks **NEC**: 9 ms leader, 4.5 ms space, 560 us marks, 560 or 1690 us spaces.
  Address 0x00 (inverse 0xFF). The command byte differed between buttons, so each button can mean
  something different.
- Address 0x00 is the generic one many cheap remotes use (LED strips, gadgets), so another cheap
  remote in the same house could send the same codes. TV remotes use their own addresses.
- `ir <ms>` (1 to 60 s) turns Wi-Fi sleep off, shows a live press count on screen (blue, green
  once it hears one), then prints the idle level, the noise, and the first three bursts' widths.
- For the pet firmware: decode NEC and check the inverse bytes, which throws away any glitch. The
  ADC battery reads can still glitch G36, so don't trust single edges.

## Not yet verified on the robot

- Whether it can drive straight at lower speeds, and which surfaces work (carpet is the worry)
- How far the Stick can slide before the base stops answering, and whether its position changes how
  it drives (it shifts weight between the front and rear legs)
- Mic quality for a child's voice at a realistic distance
- Whether the Grove port supplies 5V on battery (the schematics say yes)
- Whether the sensor's mounting holes fit LEGO Technic pins (they look like it; docs don't say)
- Whether the base USB-C also charges the Stick. The schematics say yes (it charges both cells; the
  Stick's own port charges only the Stick, through the header pins). Untested
- Real run time. The schematics say the base's 700 mAh cell drives only the motors and the Stick's
  200 mAh cell runs everything else, so about two hours awake (estimate). The docs' "about 2.5 hours"
  is the motors' figure. The `batrun` log logs both cells
- Whether the buzzer is loud enough over a room of kids. Heard across a quiet room 2026-11-08 at
  volume 96 of 255, all four tones (1, 2, 3 and 4 kHz), so there's room to turn it up
- Whether the RTC keeps time while the Stick is off, and after the battery runs flat. It had never
  been set (`lowvolt=1`, no valid time) until 2026-11-09, when it was set from the PC. The
  schematics show no backup cell, so a flat Stick should lose the time

## Sources

- BugC2 docs: https://docs.m5stack.com/en/app/BUGC2
- Library and examples: https://github.com/m5stack/M5Hat-BugC
- STM32 firmware: https://github.com/m5stack/M5Hat-BugC-Internal-FW
- Schematic: https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/541/SCH_BugC2_V1.0.pdf
- StickC Plus2 docs: https://docs.m5stack.com/en/core/M5StickC%20PLUS2
- Ultrasonic docs: https://docs.m5stack.com/en/unit/Unit_Ultrasonic-I2C
- Ultrasonic library: https://github.com/m5stack/M5Unit-Sonic
