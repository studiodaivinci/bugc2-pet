# bugc2-pet

A pet toy for the **M5Stack BugC2 with M5StickC Plus2**: an egg that hatches, a face with moods, seven animal dances,
tricks taught with any cheap TV-style remote, and a mini golf game played by tilting it. Plus everything we learned
getting there, written down so your build starts where ours ended.

We built it in November 2026 as a gift for a kid. Claude Code (Anthropic's coding agent) wrote the firmware; we
tested every step on the robot and on the floor. It works, and it is not finished: the open questions are listed in
[docs/LESSONS.md](docs/LESSONS.md#open-questions--not-yet-verified).

## What it does

- **Hatching.** It starts as an egg. Any press, tap or shake cracks it; it hatches, asks "Are you... (name)?", wants
  its big button held, and sings Happy Birthday.
- **A living face.** 57 animated scenes: eyes that follow tilt, reactions to being picked up, put down, shaken,
  turned over and patted, naps when left alone.
- **Seven animal dances.** Cheetah, honeybee, hummingbird, spider, owl, dolphin and whirligig, each with its own
  moves, sounds, lights and colour, and a true fact about the animal. Every motor run goes through a guard (floor
  check, tip, lift, any button stops it, a motor-time budget).
- **Trick Lab.** Any remote that speaks NEC (most cheap LED-strip and heater remotes do) becomes a controller: four
  buttons are moves, GO plays your sequence back and gives it a silly name, Freeze stops it.
- **Pop-Up Golf.** Top-down mini golf on the 135 x 240 screen for 1 to 4 players, with paper-craft animal holes.
  Whoever builds the robot can set course records before it hatches, for the kid to beat.

## From the video

If you came here from our video, this is where each part of it lives:

| In the video | In the repo |
|---|---|
| "No guide to how it moves." | [HARDWARE.md, "What the documentation says"](docs/HARDWARE.md#what-the-documentation-says) |
| "Walk forward. It spins." (550° in 1 s) | [HARDWARE.md, "Next tests"](docs/HARDWARE.md#next-tests) (`steertest`), [LESSONS.md, "Driving"](docs/LESSONS.md#driving) |
| "The gyro steers... straight down the stairs." | [LESSONS.md, "Driving"](docs/LESSONS.md#driving) and ["Floors, stairs and feet"](docs/LESSONS.md#floors-stairs-and-feet) |
| "Slow it down, and it wanders." | [LESSONS.md, "Driving"](docs/LESSONS.md#driving), [HARDWARE.md, "Slower run"](docs/HARDWARE.md#slower-run) |
| The dances, and the whirligig's spin | [`firmware/pet/src/dances.cpp`](firmware/pet/src/dances.cpp), [LESSONS.md, "Spins and the gyro"](docs/LESSONS.md#spins-and-the-gyro) |
| "A foot fell off." | [LESSONS.md, "Floors, stairs and feet"](docs/LESSONS.md#floors-stairs-and-feet) |
| "The egg peeped all night." | The hatching notes at the top of [`firmware/pet/src/main.cpp`](firmware/pet/src/main.cpp) |
| "Claude got the gyro wrong." | [LESSONS.md, "Spins and the gyro"](docs/LESSONS.md#spins-and-the-gyro) ("Check signs against video") |
| The face | [`art/face/`](art/face/) (open `index.html` through a local web server) and [`firmware/lib/face/`](firmware/lib/face/) |
| Trick Lab | [`firmware/pet/src/lab.h`](firmware/pet/src/lab.h), [LESSONS.md, "The IR remote"](docs/LESSONS.md#the-ir-remote) |
| The golf game | [docs/GAME.md](docs/GAME.md) |
| "Can it draw? Can it see in the dark? Can it turn off the TV?" | [docs/IDEAS.md](docs/IDEAS.md) |

## What you need

| Part | Notes |
|---|---|
| M5Stack BugC2 with M5StickC Plus2 (SKU K033-C) | The robot. ESP32-PICO-V3-02, 8 MB flash, 2 MB PSRAM. |
| A USB-C data cable | For the first install. Later installs can go over Wi-Fi. |
| A cheap IR remote (optional) | For Trick Lab. Any NEC remote; the shell's `ir learn` teaches it. |
| Unit Ultrasonic-I2C (U098-B1, optional) | Not used by the pet firmware. The bench firmware reads it. |

Software: Python 3.10 or newer and PlatformIO (`pip install -r requirements.txt` installs both, plus pyserial).
We built and tested everything on Windows 11. The firmware builds anywhere PlatformIO runs; some tools assume
Windows (`tools/wifi_setup.py` reads the current network with `netsh`, and the art tools look for Chrome and
clang where Windows puts them).

## Before you start

Read [docs/LESSONS.md, "Before you start"](docs/LESSONS.md#before-you-start) (the battery tab, seating the Stick, which
port programs it), then back up the factory firmware: the command is in [docs/HARDWARE.md](docs/HARDWARE.md), under
"Bench results". Block any stairs before a dance: ours fell down two.

## Quick start

1. **Name it.** Put your kid's name (and the golf players') in
   [`firmware/lib/petconfig/src/pet_config.h`](firmware/lib/petconfig/src/pet_config.h).
2. **Install over USB.** Plug the Stick in and run:
   ```
   pio run -d firmware/pet -e dev -t upload
   ```
   PlatformIO finds the port. The `dev` build has Wi-Fi on at boot, for the shell and updates.
3. **Wi-Fi (optional).** `python tools/wifi_setup.py` with the Stick on USB asks for your network in a small window
   and stores it on the robot. After that, `python firmware/pet/upload_wifi.py` installs over Wi-Fi, and
   `python firmware/pet/shell.py info` talks to it (`help` lists every command; they're in `kHelp` in
   [`firmware/pet/src/main.cpp`](firmware/pet/src/main.cpp)).
4. **Teach your remote.** `python firmware/pet/shell.py "ir learn"`, then press the buttons it asks for.
5. **The gift build.** When it's ready to hand over: `pio run -d firmware/pet -e gift -t upload`. Wi-Fi stays off
   unless the side button is held at power-on, which gives the face a second frame buffer, and nothing on the robot
   talks to a network. Before giving it away, `wifi forget` erases the stored Wi-Fi password.

## How to play

The big button does nearly everything; tilting moves through menus. Home is the face. Big button: the menu (Golf,
Dance, Tricks, Sound, Face). Hold the big button a second to go home from anywhere. The side button is a shortcut and
easy to miss; the power button only switches it on. Left alone for 30 minutes, it sleeps deeply, and the big button
wakes it.

## What's here

| Path | What it is |
|---|---|
| [docs/LESSONS.md](docs/LESSONS.md) | What we learned: driving, the gyro's limits, memory, crashes, sleep, the remote, what we cut |
| [docs/HARDWARE.md](docs/HARDWARE.md) | Pin maps, the base's registers, every bench measurement, the leg map |
| [docs/GAME.md](docs/GAME.md) | The golf game: design, holes, records, and the contract the pet firmware calls |
| [docs/IDEAS.md](docs/IDEAS.md) | What it could learn next: drawing, a sonar eye, the TV, and more |
| [docs/INSPIRATION.md](docs/INSPIRATION.md) | Other robots and projects we read, and what we took from them |
| `firmware/pet/` | The pet firmware (dev and gift builds) |
| `firmware/bench/` | The bench firmware: motor, gyro, mic, battery and sensor tests, used to measure everything in HARDWARE.md |
| `firmware/face-test/`, `firmware/game-test/` | Play the face scenes or the golf game alone on the robot |
| `firmware/lib/` | The face engine, the golf game, and `petconfig` (the names) |
| `art/face/` | The face as a browser prototype (open `index.html` through a local web server), its design notes and asset builder |
| `art/game/` | The golf game's art, design notes and asset packer |
| `tools/` | `bench.py` (talk to the robot over USB or Wi-Fi), `wifi_setup.py`, `capture.py` |
| `bench-output/` | Logs from our floor runs, cited in the docs |

The toy art and golf art were made with Qwen-Image 2.1, a local AI image model; the prompts are in `art/face/design/`,
`art/game/holes/prompts.txt` and `art/game/design/DESIGN.md`. Rebuild the firmware's art with
`art/face/tools/build_face_assets.py` and `art/game/tools/pack.py`. See "License" before you use that art for
anything you sell.

## License

MIT, see [LICENSE](LICENSE), for the code, the docs and the photos.

**Not the AI art.** The images in `art/face/assets/`, `art/game/sprites/` and `art/game/holes/`, and the firmware's
compiled copies of them (`firmware/lib/face/src/face_assets_data.cpp`, `firmware/lib/game/src/assets_data.cpp`), were
generated with Qwen-Image 2.1, whose licence (the Qwen Research License) allows research and non-commercial use only.
They aren't covered by the MIT licence. Use them for your own build; for anything commercial, redraw them from the
prompts with a model whose licence allows it, and rebuild the firmware's art.

The fonts in `art/face/fonts/` and `art/game/fonts/` are under the SIL Open Font
License (their licence files sit beside them). M5Stack, BugC2 and M5StickC are M5Stack's names; this project isn't
affiliated with M5Stack.
