# The robot's face: design notes

Screen animations for the robot (M5StickC Plus2: 135 x 240 TFT, 1.14 inch, RGB565). Made 2026-11-08 for
the gift build. The review page is `index.html`: every scene, drawn live by the same code the firmware was ported
from. Its Love / Keep / Cut buttons only save when the page runs where it has a database; opened locally they do
nothing.

**Picks (2026-11-09):** all 53 scenes were kept. No look was picked, so the firmware uses Marble, the recommended
route. Switching to Glow or Ink
means porting that skin's drawing code (the scenes don't change).

## Files

| File | What it is |
|---|---|
| `engine.js` | The face engine: signed-distance shapes drawn per pixel (anti-aliased, morphable), springs, eyes with lids, gaze, googly physics, mouth, particles, text and sprites, RGB565 output. Plain C-like JS so it ports line for line to the firmware |
| `player.js` | Runs one scene: cues, captions, draw order |
| `scenes.js` | The catalogue: about 50 scenes with priority (core, should, idea) |
| `index.html` | The review page (local: `python -m http.server` in this folder, then open it) |
| `strip.html`, `test.html`, `fonttest.html` | Filmstrips and stills for checking work (`strip.html?ids=a,b&skin=ink&n=7`) |
| `assets/` | Toy art from Qwen-Image 2.1, a local AI image model (prompts in `design/prompts-toys*.txt`) |
| `design/prompts-toys*.txt` | The AI prompts, one shared spine |
| `fonts/` | Bagel Fat One (shouts) and Lilita One (sentences), OFL; the firmware draws them as signed-distance fonts |
| `dump.html` | Renders `?cells=id@t,...&cols=N` with no gaps, for pixel comparison with the firmware |
| `tools/build_face_assets.py` | Writes `firmware/lib/face/src/face_assets_data.cpp` (font SDFs, kerning, sprites) from `fonts/` and `assets/` |
| `tools/compare.py` | Renders scenes in the browser and in the C++ simulator and diffs them: `python art/face/tools/compare.py <ids or all> --n 4 --out <dir>` prints a table and writes JS / C++ / difference x4 bands |

## The dial

- Medium: character animation and game UI on a tiny TFT. Register 3, and 4 for hatching and the birthday.
  Scope: a system (one engine, many scenes). Boldness: bold.
- Essence: **alive, cheeky, warm**. First three seconds: "it's looking at ME."
- Orientation: portrait, and up is the golf game's up (display rotation 0) for everything on the robot. The host
  firmware owns the turning: see `docs/HARDWARE.md` "Which way is up" (2026-11-10). The face library always
  draws in screen coordinates and never sets the display's rotation.
- Buttons: anything on the screen asks for the **big button** ("Hold my button!" is the pattern). Never ask for the
  power button (hard to press, a cable beside it); the side button is flush and easy to miss, so nothing should
  depend on the kid finding it. See `docs/HARDWARE.md` "Buttons in use".

## Concept

**The robot shown as a little creature who can borrow any animal's eyes, so the kid feels it's alive and
it's theirs.** The eyes are the star of every screen. Animal powers transform them (amber cheetah eyes with
tear stripes, compound bee eyes, eight spider eyes, huge owl eyes), so the eyes teach the true fact as well as
show the power.

Collision: LOVOT and aibo's layered eyes (anchor) + Jim Henson's puppet eyes that converge on you, and Ghibli's
soot sprites (cross) + real animal eyes and cat's-eye marbles (wildcard). Twist: the eyes sit in the robot's own
orange skin, not floating on black.

## Three routes (every scene runs in all three; pick in the page)

1. **Glow (safe).** Glowing aqua pills on near-black, no pupils, the whole eye moves. The category's best
   practice: Vector, Cozmo, EMO, EVE, Astro Bot.
2. **Marble (bold, recommended).** Glossy toy eyes: iris with fibres, a pupil that dilates and moves, catchlights
   fixed to the light (they don't follow the gaze, which is what makes them read as glass). Painted brown rim, lids
   in the skin colour, a small mouth. The background is the Stick's own orange, so the screen edge disappears into
   the case. Nobody on the board puts robot eyes in skin.
3. **Ink (wild).** A picture-book creature: ink shapes on warm paper whose outlines "boil" (redrawn 8 times a
   second, like hand animation on twos). Charming; the hardest to read across a room.

## Star, rhymes, tokens

- Star: the eyes. Rhymes: the eye's rounded-rect shape returns in the caption plates, the card in the guessing
  slot and the dice; the teal of the iris is the accent everywhere; Bagel Fat One's soft round letters match the eyes.
- Type: **Bagel Fat One** for shouts (WHEEE!, GO!, names), **Lilita One** for sentences (narrow, so more words
  fit in 135 px). Both rasterise to 4-bit anti-aliased bitmap fonts for the firmware. Captions: at most two lines,
  auto-fit to 125 px, popped in with overshoot; the eyes glance down at a new caption, then back to you.
- Colour (Marble): skin `#f59a1b` to `#ec8510`, iris `#13b5ae` (light `#7ff0dc`, dark `#06474d`), pupil
  `#0b0f14`, rim and mouth `#4a1f04`, text `#fff6e5` outlined in rim brown, heart `#ff3b5c`, star `#ffd23a`.
- Motion: everything moving is a spring (eyes stiffness 260, damping 0.62; face 160, 0.6). Blink 160 ms: closes in
  35 %, opens slower. Idle: blink every 1.8 to 5.4 s, 18 % doubles; a glance every 0.6 to 2.8 s, a third of them back
  to eye contact. Reactions answer within one frame.
- Toy art (`assets/`): "chunky painted wooden toy, glossy enamel, rounded simplified carving, soft key light from
  the upper left, isolated, transparent". It rhymes with the golf game's toy-box world.

## Lineage (the reference behind each choice)

- Pupils and catchlights: LOVOT (r04), aibo (r05), cat's-eye marble (r18).
- Animal eyes as powers: gecko eye (r16), owl (r17).
- Eye contact as "home" in the idle glances: Kermit and Henson's focus point (r15).
- Googly eyes: soot sprites (r14).
- Squash, stretch and reaction timing (sneeze, yawn, sulk): Cozmo's animation notes (r21), Vector (r02).
- Eyes opening as the hatching ritual: Daruma (r19). The speckled egg: the egg drawer (r20).
- A pet with moods on a tiny screen: Tamagotchi (r10), Flipper's dolphin (r09); a monitor face with a small
  emoticon set: Stray (r07).

## The firmware port (`firmware/lib/face/`)

All 57 scenes and the Marble look, in C++ (2026-11-09; four gift-build scenes added 2026-11-10). The JS stays the reference: change a scene there first,
check it in the page, then port the change.

- `face_core.h` maths, easing, colour, SDFs; `face_screen` the swapped-RGB565 frame, per-pixel shape, shade, glow,
  sprite and SDF text; `face_engine` springs, the face and particles; `face_player` cues, captions and draw order;
  `face_kit` shared scene helpers; `face_scenes.cpp` the catalogue, one class per scene, in the JS order.
- **Matches the JS pixel for pixel** within text rasterisation (canvas text vs SDF): `compare.py all` gives a mean
  difference under 1 for most cells and under 5 for nearly all. The exceptions are frames that land exactly on a
  cue time (the C++ passes scenes a float clock, so 3.9999999 s rounds to 4.0) and googly's chaotic physics after
  such a frame. On the robot the frame times are never exact, so this never shows.
- Keep the scene clock, the blink and look countdowns and `every()` in double: in float they drift a frame from
  the JS, and every random number after that changes.
- Per-pixel code must stay float: the ESP32 has no double FPU. `fastAtan2` replaces `atan2f` in per-pixel loops.
- The PC simulator: `bash firmware/lib/face/pc/build.sh`, then `sim list`, `sim grid "<id@t,...>" cols out.ppm`,
  `sim bench`. `compare.py` uses it.
- `firmware/face-test/` plays the scenes on the robot (controls and commands at the top of its `main.cpp`;
  `bench` times every scene). Install it with `python firmware/face-test/upload_wifi.py`, then install the bench
  firmware again with `python tools/bench.py --update`.

### Speed on the robot (2026-11-09)

The first port drew idle in 59 ms: the ESP32 does division, `sqrtf`, `floorf`, `fmodf` and `expf` in software,
70-320 cycles each (face-test's `micro` command times them). Now 33 of 53 scenes run at 30 fps, the rest at 25-29,
except hummingbird (19), birthday (20.5), hole-in-one (23) and cheetah (24). What it took, biggest first:
- **Both cores.** `face::splitRows` (set by the host) gives the odd rows of every big fill to core 0. The
  per-pixel functions only read shared state, so the split is exact (`FACE_SPLIT=1 sim ...` checks it byte for byte).
- **The frame is sent while the next one draws.** Two frames, each two 135x120 halves (`Screen::setFrame`): two
  whole 65 KB frames don't fit in one block of internal RAM. A task on core 0 sends one by DMA. Free the second
  frame before an OTA update.
- **The iris is a table** (`Face::iris_`, built once): fibres and rings by position relative to the pupil, in eye
  sizes. No atan2, noise or sqrt per pixel. Rim and fill are one pass (`Screen::rimShade`).
- **No library maths per pixel:** `ffloor`, `ffmod`, `fexp`, `wrapf` without `fmodf`; small shapes are tested
  before the eye's outline; confetti boxes fit the piece. `library.json` adds `-fno-math-errno -freciprocal-math`.
- Hosts copy `firmware/face-test/src/main.cpp`: the worker task, the push task, `scr.setFrame` per frame. Two rules
  learned in firmware/pet (2026-11-10):
  - Take the second frame **last**, after Wi-Fi (about 55 KB of internal RAM) and sound (17 KB), and only with
    60 KB to spare. Taken first, it starved the ADC driver and the robot crash-looped. With Wi-Fi on in the pet, only
    one frame fits.
  - The push task sleeps 5 ms after each DMA half. Without it, core 0 never idles, and with Wi-Fi off the task
    watchdog restarts the robot after 5 s. face-test runs with Wi-Fi on, which hid this.
  - Measured in the pet with both frames: play 30.5 fps, egg-sleep 26.

### Hooks for the robot firmware

`face::live` (scenes wait for real taps, the button, "unflip" and the accelerometer instead of faking them), the
egg's "hint" event, `Player::play(Scene*)` for host-owned scenes and `Player::reseed(seed)`.
`Player::length()` is when the scene ends on `S.t` (hosts use it, not `info()->dur`: an animal power runs as long as
it waits to be put down), and `face::onCue` hears "trick" as an animal power's trick starts, for the motors and sound.

## The gift-build scenes added on 2026-11-10 (57 now)

In live mode each waits for the robot's event; the review page fakes it.

| Scene | What it does | Events from the robot |
|---|---|---|
| `hold-button` | Hatching step 3. A heart at the top fills like a glass while the kid holds the big button (1.3 s, just before "done") and beats on each buzzer thump (0.7 s), with a heart-monitor blip running out both sides; then love eyes, hearts and "You're my person!" | "hold" on the press, "done" 1.4 s later, "release" if they let go first |
| `floor-ask` | Hatching step 5. "Put me on the floor!" at the top, eyes on the floor, a bouncing arrow, little hops; then star eyes and "Watch this!" | "putdown" (or "floor") when it is level and still on the floor |
| `charging` | Munching ("Yum!", nom nom) while the battery icon fills, then content; "Full!" | "full"; the icon shows `face::batteryLevel` (0..1) when set |
| `muted` | "shh!", a wink, and a brass zipper slides across its mouth; a speaker-with-an-X icon and "Quiet mode" | "unmute" unzips it: "La la la!" |

The heart, arrow and speaker icon are stickers in the captions' style (cream, brown outline), so they rhyme with the
text. Checked against the JS with `compare.py` (mean difference under 3.3 per cell, text only).

## Animal facts: first, big, in the hand (2026-11-10)

The facts were 13-15 px (4 to 5 point) for two seconds, at the end of the trick, with
the robot on the floor. Now every power goes:
1. The card (toy and name), then the zap into the animal's eyes. The look comes with them: tear stripes, antennae,
   wings and beak, discs and tufts, ears, the coloured backgrounds.
2. The fact at 22 px (the spider's widest line fits at 21), three short lines at most, a few words a screen, 2.6 s a
   screen. The eyes glance down at each screen and back. The big button skips to the next screen.
3. "Put me down!" at the top, eyes on the floor, the bouncing arrow, until the robot sends "putdown" or "floor"
   ("floor" may come early: then it goes straight on). The bat and the chameleon are hand powers and don't ask.
4. The trick, on its own clock (`trickT`, 1.45 at its start), so its timings didn't change. `onCue("trick")` fires
   then. The old fact at the end is gone; the scene ends `end` seconds of trick clock later.

The facts were split into screens, with two small additions that are still true: the owl's "That's most of the way
round!" (270° of 360°) and the dolphin's "in one jump!" (spinner dolphins make up to seven turns in one leap). The bat's
"by echoes" became "by listening for echoes".
22 px is about 2.3 mm on this screen (0.105 mm a pixel): meant for arm's length (not yet checked on the real
screen), and far too small from standing height with the robot on the floor, so the facts belong in the hand. Demo lengths in the page are 11-14 s; on the robot it's as long as the kid takes.

## The dance picker (2026-11-11)

The ask: a random dance (never the one just done), but also a way for the kid to pick the one they want. Drawn in `firmware/pet/src/menu.cpp` (`drawPicker`), checked on the PC with
`firmware/pet/pc` (the sheet: every card, a slide and a pick).

- Dial: game menu on the tiny TFT, register 3, one screen inside the face system, bold. A small extension, so it uses
  this board and the Marble tokens rather than a new board, and went straight to the bold route.
- Concept: **the picker is a deck of the animals' power cards, each animal inside a glass marble**, because the robot
  borrows the animal's eyes, and the eyes are marbles (the cat's-eye marble, r18). The card the kid picks is the card the
  dance's scene opens with: same name, same burst colour, the toy over the same sunburst.
- Star: the marble with its toy, one per screen (a young kid across the table reads one big animal, not a
  list). The marble rhymes with the eyes: the brown rim, a catchlight fixed to the upper left, a shade on the far side.
- Cards: SURPRISE! first (a gold "?" in Bagel Fat One, its marble running through every animal's colour), then the
  six animals, then BACK (the robot's own eyes looking left). Surprise! is the default: one press, a new dance.
- Surprise! shuffles through the animals for about a second, slowing like a prize wheel, with a click each flip, then
  bounces on the dance it landed on (never the last dance, which is saved). A named card bounces once.
- Supporting: "Dance!" at the top like the menu's "Play!", a dot per card down the right edge with theirs in the iris
  teal (the accent), the hints at the bottom with the last line saying what the big button will do.
- Motion: the deck slides between cards on an ease (rate 14 a second); the end of the deck gives a little bump. The
  base lights glow the marble's colour, rainbow for Surprise!.
- Tokens: marble centre y 104, radius 55, rim 3; names at y 182, Bagel 26 shrinking to fit 124 px; card pitch 175.

## The whirligig: the long spin (2026-11-11)

The ask: a dance that spins fast for a good while, with the lights and screen making a pattern as it spins. The animal is the whirligig beetle: it whirls on ponds, and each eye is split in two, looking above
and below the water. Firmware only (`face_scenes.cpp`, Whirligig); the review page doesn't have it yet.

- The facts: the eyes sit on a pond's waterline (sky above, water below, a rippling foam line), each crossed by a gold
  line, so the split eyes are the picture, not just the words.
- The spin: a spinning screen smears anything that isn't round, so the whirl is **rings that flow outwards**, ribbed
  like a tunnel, in every dance's colour (the picker's marbles), white-hot in the middle: rotation can't blur a
  circle. Spirals (cream and the beetle's lime) show only while it speeds up and slows down, when they can be seen to
  turn. After the first floor run (the bands on the screen looked a bit thin), the rings are 34 px wide (were
  22), shaded flatter, with a dark seam between colours, in the seven dances' colours (below); the spiral has two
  wide arms instead of three. The face hides during the whirl and comes back dizzy (spiral eyes, gold stars circling).
- The base lights during the spin are fixed to the room (`moves.cpp`, roomLocked): colour by compass direction, from
  the gyro, updated every 5 ms, so the two lights draw a still rainbow ring, then three frozen comets, then a galaxy
  turning slowly backwards. They use the same six colours as the screen's rings.
- Toy: local AI (`design/prompts-toys3.txt`), pick 002 (on a ring of ripples, the gold
  split across each eye), trimmed and stored at 200 px like the others. Burst and marble: lime `#a8e63a`, which the
  midnight-blue beetle reads against.

## Seven colours (2026-11-11)

After the first floor run, too many animals shared the same orange. The
cheetah, honeybee, spider and owl all played on the robot's own orange (the Marble background). Each dance animal now
has its own world, fading in as its card leaves (the card still plays on the orange, so every power opens the same
way). The picker's marble, the zap and the base lights use the same colour.

| Animal | Card | Its world |
|---|---|---|
| Cheetah | red `#ff4b3a` | racing red, `#ff5446` to `#b30c2c`; the gold eyes and cream whoosh arcs read on it |
| Honeybee | yellow `#ffd23a` | a honeycomb: pale wax walls over golden honey; the dance path drawn brown |
| Hummingbird | green `#36d17c` | mint and pink (unchanged) |
| Spider | purple `#b678ff` | dusk purple with a web in the top-left corner, silk white; the black eyes read on it |
| Owl | blue `#6f82ff` | a night sky with a crescent moon and twinkling stars; tan ear tufts, and a beak instead of the mouth, which vanished on the dark blue |
| Dolphin | sky `#7fd6ff` | the sea (unchanged) |
| Whirligig | lime `#a8e63a` | the pond (unchanged) |

Round the rainbow (the whirligig's rings and the base lights' wheel): red, yellow, lime, green, sky, blue, purple.

## The Sound switch and the home ring (2026-11-11)

The asks: a menu option that turns all sound off, and a shortcut back to the face.

- The menu has four rows now (Golf, Dance, Sound, Face): centres 62-182, 40 apart, boxes 34 px tall, icons at 0.3,
  labels in Bagel 20. Sound reads **Sound** with the toy speaker, or **Quiet** with it dimmed and crossed out in red,
  like the mute sign on a tablet. Pressing it bounces the speaker; turning sound on chimes. It mutes everything.
- The speaker toy: local AI (`design/prompts-toys4.txt`), pick 001, a teal box with a cream
  cone and a red dome, side on like the speaker icon. Teal is the robot's iris colour.
- Home: hold the big button a second in the menu, the picker or a dance's card. After a quarter of a second a dark
  disc appears over the screen with the face's two eyes in it, and a teal ring fills clockwise round them. Three
  quick presses were the first idea, but each press does its job first and kids mash the button through the facts.
  So taps in the menu and picker act when the button is let go.

## Next

1. Speed up hummingbird, birthday, hole-in-one and cheetah (captions and big sprites are the remaining cost).
   Check the real screen: brightness, the orange against the case, caption size at arm's length. Time the four new
   scenes and the fact screens on the robot (`bench` in face-test).
2. Eight candle flames on the cake (new cake art).
