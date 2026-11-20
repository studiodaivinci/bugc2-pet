# Pop-Up Golf

The screen game for the robot: top-down mini golf on the robot's 135x240 screen, played in the hand.
This file covers what the game is and why, how to play, the holes, the Maker's records, the contract the
robot firmware calls, and how to test it. The game lives in `firmware/lib/game/`, `firmware/game-test/`
and `art/game/`. Player names come from `firmware/lib/petconfig/src/pet_config.h`.

## The choice and why

- **Mini golf.** Golf works in short turns, so pass-and-play suits 1 to 4
  players (the kid, three more named players, the Maker, a guest). Little kids and teenagers can all play it, and the youngest can just whack the ball.
- **Animal habitats.** Each hole is a habitat page with a real animal that moves about the
  course and gets in the way, plus a fact card at the end of the hole.
- **The look: a pop-up paper field guide.** Each hole is a cut-paper page, and its animal pops up out of the
  page. The ball, cup and red flag stay the highest-contrast things on screen. The art direction, references and
  colour script are in `art/game/design/DESIGN.md`. All art is local AI (Qwen-Image), painted over a guide drawn
  from the same geometry the physics uses, so what you see is what the ball hits.
- **Everything is original.** No characters, names, art, music or levels from other games or shows. The animals
  are real animals.

## How to play

Hold the robot flat in your hand with the screen up, the way it sits on the floor.

| What | How |
|---|---|
| Start | Title page: **Play** or **Back to robot**. Tilt to move, big button to pick |
| Pick players | Tilt to move, big button to pick (up to four). Then **Go!** |
| Pick a course | Tilt to move, big button to pick |
| Aim | **Turn the robot** the way it turns on the floor. The aim line follows its front (1.5x: a 60° turn swings the aim 90°) |
| Putt | Hold the big button. The power bar fills, then falls once; let go to putt. Let go after it has emptied: no stroke |
| Pause | **Keep holding the big button after the bar has emptied** (about 3.5 s in all): a "keep holding: menu" capsule fills, then the menu opens. Or the side button |
| Pause menu | Keep playing, Back to robot, Stop the round. Tilt to move, big button to pick |
| Carry on a saved round | On the next visit: Keep going or New game. Tilt to move, big button to pick |

**The big button and tilt do everything.** The side button is only a shortcut: we didn't find it on the robot as
built (flush, the same orange as the case; `docs/HARDWARE.md`, "Buttons in use"). It still pauses, moves in the
pause and carry-on menus, goes back a page in the player and course menus, and a short press on the title goes
back to the robot. In every menu the choice the big button will pick wears the red big-button prompt, with a
drawing of the big button, so the red moving is the cursor. The buttons have no letters, so prompts draw the big
button or say "big button".

Before 2026-11-10 the power bar kept cycling while the button was held. Now it runs once, so a long hold is free
to mean "menu". Check the feel on the robot.

The aim line traces off the walls. Where it meets a rail, a bump mark in the player's colour sits on the rail's
face, and fainter dots show the bounce, so aiming into a nearby wall never looks like no aim at all.

**Aim mode.** Turning is the default. Holding the side button for 2 s on the title page (a hidden test switch)
changes to tilt (lean left or right to swing the line), and back again. We tried both on 2026-11-09: tilt was unclear,
and turning worked well. Turning stays the default.

## Holes

No date unlocks. The gift build has three holes, and new holes come with new builds. The course menu shows the next habitats as "coming soon".

| Id | Hole | Par | Habitat | Animal |
|---|---|---|---|---|
| 0 | Tortoise Trot | 2 | Meadow | tortoise |
| 1 | Bunny Bend | 3 | Meadow | rabbit |
| 2 | Hedgehog Hill | 3 | Meadow | hedgehog |

A hole's id is its place in `kHoles`, and the saves index scores by it, so **new holes are only ever appended**.
Holes are authored in `art/game/tools/holes.py` (geometry, guide image, C++ data); `art/game/tools/pack.py`
packs the art. Planned next: Pond (frog, duck, beaver), then Arctic (penguin, seal, polar bear, with ice).

Physics: rails stop the ball at the painted rail's inner face. Every hole has been solved by brute force in the
simulator (`sim --solve`): from the tee, 0.7 to 3% of putts are holes in one with the animals ignored.

## The Maker's records

Every hole has a Maker's record to tie or beat:

- **Before hatching** (`daysSinceHatch() < 0`), whatever the Maker scores on a hole becomes their record. It's
  the latest score, not the best, so they can retake a record to make it beatable. Records are stored under their
  own key, `"maker"`, so a reset to unhatched keeps them.
- **After hatching** the records are fixed. Playing as the Maker after that counts as an ordinary player.
- A hole with no record set uses a built-in one of par + 1.
- Beating it shows "You beat Maker!" (event `BeatMaker`). Matching it shows "Tied Maker!"
  (event `TiedMaker`). The hole card always shows "Maker's best". (`PET_MAKER_NAME` sets the name.)

**How the Maker sets records:**
1. Install the game test firmware, which reports "before hatching", or use the pet firmware before the egg hatches.
2. Play a round with the Maker as a player. Each hole they finish becomes their record for that hole.
3. Replay a hole to change its record. Aim for records the kid can tie or beat (par or par + 1).
4. Retake them when new holes arrive (reset the robot to unhatched with the pet shell's `egg`).

## The contract (for the robot firmware)

```
API (game.h): begin(canvas, host) | beginRaw(px, host); bool update(const Input&);
  resetProgress(); resetAll(). Nothing else is public.
Loop: call update() once per frame (~30 fps), then push the canvas. update() never blocks, except an NVS
  write of about 200 bytes after each player's hole.
Gaps: the host may skip update() for any time (face cut-in, pause). dt is clamped to 0.05 s, and a turn of
  the robot during a gap of over 0.1 s is not taken as aiming.
Exit: update() returns false when the player picks "Back to robot" (on the title or in the pause menu) or
  presses the side button on the title. The big button alone reaches both. The round in progress stays
  saved. The host may also just stop calling (idle, low battery); no end() is needed.
Enter: call begin() on each visit; it re-reads the saves and offers to carry on a saved round. Keep the
  motors off and the pet's handling reactions muted while the game runs.
Display: the host owns M5.Display. Canvas 135x240, 16-bit (byte-swapped RGB565, as M5Canvas keeps it),
  portrait, in PSRAM. The game redraws the whole frame on every update().
Memory: the game allocates 162 KB of PSRAM at the first begin() (two page buffers and the surface map) and
  keeps it. If that fails, update() returns false at once. Fixed RAM under 1 KB; up to 1 KB of stack in
  begin() and when saving.
Flash: about 170 KB for the library, 130 KB of it art.
Input: ms, dt; big button = a, side button = b, each with pressed/held/released; accel in g (pass the last
  good sample when there's no new one: zeros read as a tilt); gz in deg/s, bias-corrected; yaw in degrees,
  counter-clockwise positive seen from above with the screen up, bias-corrected, integrated from the gyro at
  100 Hz or more. The game never touches the IMU, I2C, GPIO, M5.update() or M5.Display.
Sound: host.tone(hz, ms), non-blocking, newest wins. The host applies volume and mute.
Clock: host.daysSinceHatch(): < 0 only before hatching (the Maker's records writable); anything >= 0
  after. Nothing else depends on it.
Events: host.event(e, player) fires inside update(); queue it and react after. Values, append-only:
  0 HoleInOne, 1 UnderPar, 2 NewRecord, 3 BeatMaker, 4 BallInWater, 5 RoundWon, 6 TiedMaker.
  `player`: 0 Kid, 1 Player2, 2 Maker, 3 Player4, 4 Player5, 5 Guest (`game.h`, `Player`).
Storage: Preferences namespace "game" only. Keys "data" (progress and the aim setting) and "maker" (records).
  Each blob carries a checksum. Layouts only grow at the end, into reserved bytes. An older or shorter
  blob migrates; a newer one (newer build, then an older one installed) is read for what this build knows
  and never overwritten; a damaged one reads as nothing saved. Never erase NVS.
Reset to unhatched: game::resetProgress() (keeps the Maker's records and the aim setting).
Build: lib_extra_dirs = ../lib, -DBOARD_HAS_PSRAM -mfix-esp32-psram-cache-issue, M5Unified.
```

## Notes for the robot firmware

- **Side button and Wi-Fi.** G39 (side button) reads false presses for an instant while Wi-Fi power saving
  is on (Espressif errata 3.11, as with G36 in `docs/HARDWARE.md`). Measured: phantom presses went back a
  screen and quit the game; with `WiFi.setSleep(false)` a full playtest logged none. The game now ignores a
  side-button press seen on one frame only, but keep Wi-Fi sleep off while the game runs if you can.
- **Yaw.** Aiming follows `Input.yaw`, so integrate the gyro fast: `firmware/game-test` reads the IMU every
  3 ms in its own task (`imuTask`) and hands the game the latest values. Once a frame is too coarse. Its bias
  keeps following the reading while the robot is held still, and the game ignores turns slower than 2°/s.
- **Accel.** On a frame with no new IMU sample, pass the last good one. Zeros read as a tilt (a bug in an
  early game-test, fixed).
- **Frame time.** Sending the canvas to the screen takes 13.6 ms (the host's cost). The game's own update
  takes about 21 ms on the menus (the backdrop is cached, and a full-screen copy into PSRAM alone costs 6 to
  8 ms) and is estimated at 13 to 15 ms on a hole. That's over the 10 ms target; a speed pass (redraw only
  what moves) is planned. Measured with `stats` and `bench` in game-test.
- **Quit reactions.** The game fires no event on quitting. Use update() returning false as the cue.
- **No side button needed.** The game can be entered, played, paused and left with the big button and tilt alone
  (since 2026-11-10). The side button stays as a shortcut.

## Testing

**On the robot** (`firmware/game-test`): it stops the motors first thing at boot and never runs them.
Install with `python firmware/game-test/upload_wifi.py` over Wi-Fi (it works from the bench
firmware or the game), or `pio run -t upload` in `firmware/game-test` over USB. It has the same Wi-Fi
update listener as the bench firmware, so `python tools/bench.py --update` installs the bench firmware again.
Commands over USB serial or Wi-Fi (`firmware/game-test/shell.py`):

| Command | Does |
|---|---|
| `days N` | what daysSinceHatch() returns (default -1, before hatching) |
| `reset` / `resetall` | resetProgress() / resetAll() |
| `stats` | frame rate and update and push times every 3 s |
| `keys` | logs every button press and release |
| `imu` | accel, gyro z, yaw and bias for 3 s |
| `bench` | times the drawing pieces |
| `info` | version, memory, Wi-Fi |
| `shot` (Wi-Fi, `shell.py shot`) | saves a screenshot to `art/game/shots/` |

**On the PC** (`firmware/lib/game/pc`, clang): `bash build.sh` builds `out/sim.exe` and `out/save_test.exe`.

| Run | Checks |
|---|---|
| `sim tour.txt <dir>` | every screen, as BMP frames |
| `sim review.txt <dir>` | the aim hint, aiming into a wall, pause, Back to robot, carrying on a round |
| `sim menu.txt <dir>` | the big button alone: title choices, hold to pause, Back to robot, carry on or New game |
| `sim --solve` | each hole can be holed from the tee |
| `save_test` (in an empty folder) | old saves migrate, newer ones stay untouched, damaged ones read as empty |

## Status (2026-11-10)

- Playtested on the robot: menus, turn-to-aim, a par on hole 1.
- Built and tested in the simulator, not yet on the robot:
  - the wall-traced aim line, and rails at the painted face;
  - the design-review fixes: saves that migrate, the Maker record rules and tie, the Resume and Pause menus,
    and the button wording;
  - big button only (2026-11-10): Back to robot on the title, hold the big button to pause, tilt in every menu,
    and a power bar that runs once.
- Next: the speed pass, then polish. Holes 4 to 9 are for later builds.
