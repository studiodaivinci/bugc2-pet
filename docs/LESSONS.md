# Lessons

What we learned turning an M5Stack BugC2 with an M5StickC Plus2 (plus a Unit Ultrasonic-I2C) into a small pet:
dances, a Trick Lab taught with a TV remote, a face on the screen and a golf game. Claude Code wrote the firmware;
we tested it on one robot over about ten days in 2026-11. Pin maps, registers and raw test tables are in
[HARDWARE.md](HARDWARE.md). "Speed" is the base's motor command, -100 to 100. Yaw is positive counter-clockwise,
seen from above.

## Before you start

- **Pull the battery tab.** A plastic tab insulated the base's cell. It read 0 V until we removed it, then 3.98 V.
  Our first motor runs were on USB power alone without our knowing.
- **Seat the Stick fully on the header pins.** It can slide about 1 cm, held by friction alone, and the pins are
  its only link to the motors. If it loses the data pins but keeps power, the base keeps running the last speed.
- **Program through the Stick's USB-C.** The base's USB-C is power only. Back up the factory firmware first.
- **Never write base registers 0xFD or 0xFF** (firmware-update entry, saved I2C address).
- **Don't trust M5Unified's stored gyro offset.** At rest ours read -43, -58 and +16 deg/s; the chip's bias is
  about 1 deg/s. Its 32-bit subtraction also overflows near the gyro's limit and flips the sign of fast clockwise
  spins. We clear it and measure the bias while still (`firmware/pet/src/motion.cpp`, `measureBias`).

## Driving

**Out of the box it only spins.** On hardwood at speed 70, forward, back, left and right were all spins of 4 to 5
turns a second. Speed 30 showed the same pattern (HARDWARE.md, "Cable-free run", "Slower run").

**The cause is grip, not wiring.** Run alone at speed 50, only 3 of 8 motor-direction steps moved it, though all
could be heard. Motors 0 and 3 (front left, rear right) didn't move it; motor 1 grips one way only. It probably rests
on the other diagonal like a wobbly table (unconfirmed). The channels follow the original BugC order, not the
BugC2 pin map ("Single motors on the floor", "Leg map").

**Gyro steering makes it drive.** Unsteered forward: 550° of net turn in 1 s. Steered: +11° in 2.5 s, about
50 cm/s at speed 40. A turn is added to every motor, `(s+t, -s+t, s+t, -s+t)`, resized 50 times a second from
heading error, spin rate and a learned trim (gains 0.2, 0.04, 0.2; cap 40). It still wobbles 50 to 70° each way
about once a second. Softer gains were worse (`moves.cpp`, the `kSteer` constants; "Next tests", `steertest`).

**Slow driving holds the facing, not the direction of travel.** Floor tracking of video showed it sliding along a
line 20 to 25° off its body at speed 25; at speed 30 it switched mid-move to 10 to 20° the other way, so paths
bowed. Likely uneven grip between the feet, which the gyro can't sense. Each stop twisted it 15 to 20°, so travel
now eases in and out over 150 ms (`kRampMs`).

**New foot tips halved the wobble.** The old tips looked fine, but four new spares halved the heading swing,
straightened paths and cut the slide angle to +5 to +16°. **Speed 25 no longer moves it** (4 cm in 1.2 s, down
from about a metre). **Speed 30 is the new slow, about 30 cm/s.** The usable band is about five counts wide.

**Shaking in place works; shaking while steering doesn't help.** Reversing all motors every 60 ms wiggles and creeps
about 3 cm in 1.5 s; every 15 ms is a buzz with no travel. Keep shakes short: reversing hammers the gearboxes.

**What we shipped:** no straight driving. Dances travel a foot or two, steered, at speeds 30 to 42. Their steps
ended 20 to 110° off heading, which looked fine in a dance.

## Spins and the gyro

- **The gyro clips at about 2000 deg/s** (+1983 counter-clockwise, -2000 clockwise), reached within 0.3 s of a spin
  at speed 40. Angles through a fast spin read low, and a heading taken before one is lost.
- **So spins run by time.** The cheetah's 1.6 s at speed 45 logged 6.6 turns, so likely more. A 1 s spin at 42 is
  about three turns. No counted spins, no turning to an angle; the travel heading resets after a spin (`refOk`).
- **Speed barely sets spin rate:** about 1700 to 1890 deg/s from speed 30 to 70 (an estimate). Speed 70 gave 5 g
  jolts for little extra, so the cheetah uses 45. Which way a spin goes also depends on which feet grip.
- **Check signs against video.** We read the clockwise -2000 readings as a library sign flip and "fixed" them; pet
  0.4 and 0.5 then logged every clockwise spin as counter-clockwise. That fix is gone.

## Coasting

- **Honeybee (pet 0.2):** each 150° turn "finished" after 110 to 190 ms, carried there by the waggle before it, then
  turned another 270° in a 150 ms settle. Coasting set the shape more than the program did. A turn now counts only
  rotation its own way (`moves.cpp`, the `TURN` check in `play`).
- **Cheetah (pet 0.5):** about 600° in the 300 ms after its sprint, with the gyro at its limit. Trick Lab waits
  800 ms after a spin, and until it turns slower than 25 deg/s (`lab.cpp`, `kAfterSpinMs`; `moves.cpp`, `labReady`).

## The motor guard

The base has no watchdog and holds the last speed until told otherwise: if the Stick hangs, the robot keeps going.
Every motor run goes through one guard task (`firmware/pet/src/moves.cpp`).

- **Before any motor runs** (`start`, `floorReady`): a press asked for it; not held or upside down; flat (z above
  0.9 g) and stiller than a still hand for 1 s; the base answers and its cell is above 3.45 V; motor time is left.
- **While running** (`play`): stop on any button, the main loop silent 500 ms, a tip (smoothed z below 0.6 g), or a
  spin that stops turning for 250 ms (lifted, or carpet). Speeds are re-sent every 200 ms; idle, a stop every 5 s.
- **Motor time:** 20 s a minute, refilling (`kBudgetMs`). It began at 10 s; a six-move trick and its replay need
  about 12 s, so we doubled it. What the gearboxes can take is unknown.
- **Its own moves looked like handling.** Its spin read as a pick-up, so Trick Lab marks its own motion
  (`motion::selfMoving`). The hummingbird's hum broke the floor check 0.3 s in, so just before the first move it
  checks only flat and not held (`stillDown`).
- **Dropped:** a stop on hard jolts (the motors would trip it) and fall detection (too late once detected).
- **Unfixed:** about 1 dance in 10 went back to the face without moving. Likely the 1 s floor count resetting.

## Floors, stairs and feet

- **Block the stairs.** On its first steered run it overshot a metre of clear floor and fell down two stairs,
  unharmed. 2 m wasn't enough for a 34 s test. No cliff sensor: floors only.
- **Carpet edges tip it:** at speed 40 it arced onto one and rolled over (1049 deg/s, 12 g).
- **Dances need about 60 cm of clear floor all round.**
- **The feet come off.** One silicone tip fell off after two days of dances. We glued all four with clear 100%
  silicone sealant (24 h cure); grip was unchanged. Leave the clear film on the springs. The 12 spare tips are a
  choking size for toddlers.

## Memory and the screen

- A frame (135 x 240, 16-bit) is about 64 KB, as two 32 KB halves in internal DMA RAM.
- **Wi-Fi costs about 55 KB of internal RAM: the second frame buffer.** Wi-Fi on, one frame (the egg ran 19 fps);
  off, two (egg 26, home face 30, menu 21). The `gift` build keeps Wi-Fi off unless the side button is held at boot
  (`PET_WIFI_AT_BOOT`). The second frame needs 50,000 bytes left free (`frames.cpp`, `addSecond`).

## Crash loops and their fixes

- **Boot loop:** the second frame, taken before Wi-Fi and sound, starved them. It is now taken last, 8 s after boot
  (`main.cpp`, `tryTwoFrames`).
- **Crash on switching Wi-Fi off:** mDNS outlived the radio. `MDNS.end()` now runs first (`net.cpp`, `off`).
- **Watchdog restart after 5 s:** with Wi-Fi off the face drew flat out and core 0 never idled. The display task
  now sleeps through each half-frame DMA transfer (`frames.cpp`, `sendFrame`).

## Switching off and deep sleep

- **On USB, "off" was asleep for good.** Releasing the power-hold pin (G4) cuts power only on the battery. On a
  cable, M5Unified's `powerOff()` falls into deep sleep with no wake source. `switchOff()` arms the big button first.
- **Deep sleep, big button to wake.** The power button is hard to press with a cable beside it. After a 30-minute
  nap the pet deep-sleeps with G4 held high and the big button (G37) armed; a press restarts it in about 2 s. It
  wakes silently every 3 hours to read its battery, and after 36 hours, or below 3.5 V, lets go of the power
  (`main.cpp`, `sleepDeep`, `sleepCheck`). A big-button wake worked in our final test.
- **The sleep drain is not measured.** The Stick's 3.3 V stays on, so the base stays powered and its cell drains
  too. The 36 hours is a guess.

## Battery

- Per the schematics the base's 700 mAh cell runs only the motors and the Stick's 200 mAh cell runs the rest:
  about two hours awake (estimate). The base's USB-C charges both cells; the Stick's port charges only the Stick.
- At speed 70 the base cell dipped from 3.98 V to 3.79-3.86 V and was back at 3.95 V minutes later.
- Reading the base's battery briefly stalls its motors. Don't read it mid-move.
- **The thresholds are placeholders:** hungry at 3550 mV and switch-off at 3400 mV on the Stick (`main.cpp`,
  `kHungryMv`, `kEmptyMv`), no dance below 3.45 V on the base (`kBaseLowV`). Our one log was taken on the cable, at
  100% throughout. Run `batrun` off the cable before trusting them.

## The IR remote

- The base's receiver is wired to the Stick's G36. Leave G25 a floating input; it shares that header pin.
- **G36 glitches while Wi-Fi power saving or the ADC is on** (Espressif errata 3.11): 1054 edges in 10 s with no
  remote; 0 in 8 s with `WiFi.setSleep(false)`. The side button (G39) does the same: phantom presses quit golf.
- **Any NEC remote works.** Ours sent NEC, address 0x00. Checking the inverse bytes drops glitches (`ir.cpp`).
  Address 0x00 is common on cheap gadget remotes, so another one may send the same codes.
- In the Trick Lab floor run, all of about 30 presses were decoded. Six codes are built in (`main.cpp`, `kRemote`);
  the shell's `ir learn` teaches another remote.
- Freeze is a convenience, not a safety stop: the robot turns about a third of a circle during one remote frame.

## The mic

- **The mic shares a pin with the motor bus.** Its clock, G0, is the base's SDA. Stop the motors, release the bus,
  record, restore the bus. On the bench 6 of 6 captures were full and the base answered after each
  (`firmware/bench/src/main.cpp`, `cmdMic`). Skip the first few ms: a startup spike. The pet firmware has no mic.

## What we cut, and why

- **Voice tricks through an LLM relay.** An LLM would turn a spoken sentence into a plan in a small fixed move
  vocabulary; a relay holding the API keys validates and clamps it, so the LLM never touches the motors. Cut: the
  robot had to work alone with no internet, the mic shares the motor bus, and a child's voice was untested.
- **Bravery** (shy of a hand at first, calmer after gentle approaches): needs the ultrasonic sensor.
- **Ultrasonic tricks:** the sensor is hard to attach, and its 12.2 g could change how the robot moves. On the bench:
  0x57 on Grove, 618 to 623 mm, stable at 120 ms a reading; near its 19 Hz top rate 1 reading in 6 is an outlier, so
  median-filter it.
- **More drive-straight work** (`softtest`, `pairtest`): travel is what met the stairs and the carpet edge.

## Open questions / not yet verified

- The deep-sleep drain, real run time, and the battery thresholds.
- Steered sideways travel has never run on the floor (the shell's `trial` is built for it); the arc gain
  `kSteerFf` is an untuned guess. `softtest` and `pairtest` never ran on the floor.
- The 1-in-10 dance that doesn't start.
- Tile and carpet: almost every logged run was on hardwood.
- What the gearboxes take at 20 s of motor time a minute.
- How far the Stick can slide before the base stops answering, and whether fully off the pins stops the motors.
- The schematics' claims: the base port charges both cells, the Grove port has 5 V on battery, the clock has no
  backup cell.
- Whether the glued feet last. The IR transmitter (G19) is untested.
