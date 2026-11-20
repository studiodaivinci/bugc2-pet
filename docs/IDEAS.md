# Ideas: what it could learn next

Our video ends on three questions: can it draw? Can it see in the dark? Can it turn off the TV? None of them is built
yet. This is where we'd start on each, and the other ideas we kept. Other people's robots that gave us ideas are in
[INSPIRATION.md](INSPIRATION.md).

## The three questions

- **Can it draw?** The base has LEGO-compatible holes, so a LEGO pen holder turns it into a slow Logo turtle. The
  hard part is going straight: at slow speeds its path slides off its body's line and bows
  ([LESSONS.md, "Driving"](LESSONS.md#driving)), so a drawing will wander until the gyro steering is tuned for it
  (`softtest` in the bench firmware is the place to start).
- **Can it see in the dark?** The Unit Ultrasonic-I2C as a sonar eye: a bat power. It reads 2 to 450 cm over I2C
  and answered steadily on the bench ([HARDWARE.md, "Sensor"](HARDWARE.md#sensor-unit-ultrasonic-i2c) and
  ["Sensor mount plan"](HARDWARE.md#sensor-mount-plan)). The face already has a bat power to borrow
  (`art/face/scenes.js`, `bat`). We left the sensor off the gift build: it's hard to attach, and its weight may change
  how the robot moves.
- **Can it turn off the TV?** The Stick has an IR transmitter, so the robot could learn a TV's power code and send it.
  The receiver and NEC decoding already work (`firmware/pet/src/ir.cpp`), but TV remotes often use their own address
  or another protocol ([HARDWARE.md, "IR receiver"](HARDWARE.md#ir-receiver)). The transmitter itself is untested.

## Other ideas we kept

- **LEGO bodies.** Hats, shells and faces built on the base's LEGO holes.
- **Stretch on wake-up.** A wake-up wiggle that doubles as gyro calibration for whatever floor it's on.
- **Drive it, name it, replay it.** Teaching by showing instead of telling. Trick Lab is a first version: remote
  presses become a trick that plays back with a name.
- **Learn to drive by trial and error.** Ultrasonic distance to a wall as the reward: try motor patterns, keep what
  gets closer. Real learning you can watch.
- **Trick mail.** Tricks are small data, so a new one could be sent to the robot from another house.
- **Spin art.** The screen sits at the centre of the spin. Patterns made for spinning (spirals, colour wheels that
  blend, Benham's disc and its illusory colours) need no timing. A real picture needs the backlight flashed once per
  turn, timed by the gyro, so it looks frozen like a zoetrope. That flickers a few times a second: check flashing
  lights are OK for whoever is watching (photosensitive epilepsy). Untested.
- **Voice tricks and bravery** were planned and cut; why, and what they'd need, is in
  [LESSONS.md, "What we cut, and why"](LESSONS.md#what-we-cut-and-why).
