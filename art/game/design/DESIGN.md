# Pop-Up Golf: art direction

Decided 2026-11-08 after a wide exploration (below). Every image for the game inherits this sheet.

## Dial
- **Medium:** game art direction plus a batch of local AI images (Qwen-Image 2.1).
- **Register:** 3 for the course and menus, 4 for the title page, 2 for the HUD.
- **Scope:** a system (about 10 holes, sprites, menus) made as one batch with a shared prompt spine.
- **Boldness:** bold. Toned down only where a young kid needs clarity: one star per screen, words on plates.

## Concept
**The golf game shown as a pop-up nature field guide: every hole is a cut-paper page of a habitat where the paper
animals hop to life, so the player feels the wonder of a book that plays back.**

- **Essence:** crafted, joyful, alive. In the first three seconds: "a tiny paper world in my hands".
- **Collision:** Lumino City's handmade paper world (BAFTA Artistic Achievement 2015) + Golf Peaks' one-view readable
  board + the WPA posters' one colour family per place + the habitat dioramas of a natural history museum.
- **Twist:** the animals pop up. When a hole opens, its animal unfolds from flat like a pop-up book, then hops
  around as a paper puppet (no walk cycles). The next hole arrives with a page turn.

## Star, seed and rhymes
- **Star of every hole:** its animal, the subject of the page. Largest character, most colour.
- **Highest contrast:** the ball (white, dark outline, shadow), the cup and the red flag, because play depends on them.
- **Rhymes:**
  - Paper layers with a soft shadow under each, everywhere: the course, the animals, every UI card.
  - Punched dots, from the sand traps: the aim dots, the stroke dots and the progress dots.
  - The red paper flag: red is kept for the cup and for "your turn / press A".
  - Book parts: the hole card is a page, the fact is the field guide's caption, the players are bookmark tabs, and
    holes change with a page turn.

## Colour script (one paper family per habitat)
| Habitat | Holes | Course card | Around it | Accent |
|---|---|---|---|---|
| Meadow | 1-3 (first day) | spring green | deep greens, buttercup, poppy | red flag |
| Pond | 4-6 | spring green | teal and blue water, lily pink | lily pink |
| Arctic | 7-9 | pale ice blue (slippery) | deep blue sea, white floes | berry red |
| Bonus | 10 | to decide | | |

Ball: white with a dark outline on every page; the player's colour shows in the aim dots and their tab.

## Prompt spine (local AI, qwen-turbo, then qwen for finals)
- **Hole pages** (`--ref` the layout guide from `art/game/tools/holes.py`): "Repaint this color-coded layout map as an
  overhead photograph looking straight down at a layered cut-paper pop-up book page showing a mini golf hole
  [habitat]. Keep the exact layout and position of every shape. [colour mapping per shape]. Each layer casts a soft
  shadow on the one below, soft light from the top left, crisp clean edges, bright colours, no animals, no text, no
  people."
- **Animals** (`--rgba`): "A cute [animal] made as a flat layered cut-paper illustration for a children's pop-up nature
  book: built from three or four layers of cut coloured card stacked on top of each other, each layer casting a small
  soft shadow on the one below, crisp clean scissor-cut edges, visible paper fibre texture, simple bold shapes, [colours],
  a big friendly face with large eyes. Seen from a high three-quarter view from above. Flat paper layers only, nothing
  folded, nothing three-dimensional. A single character, isolated on a transparent background."
- Avoid "papercraft" (folded blocky models) and "toy" (round 2).

## The exploration (2026-11-08)
A board of 24 references (other people's work, so not included). Every route repainted the same layout guide
and was judged at the robot's true 135x240.

1. Round 1, three routes: cartoon, toy box, WPA poster. None was right, so the search went wider.
2. Ten worlds: toy box, museum diorama, blacklight glow, bioluminescent night, clay,
   cut paper, felt, bug's-eye garden, pixel art, pool floats. Glow, floats and paper led; museum and night were
   murky, the bug's-eye garden noisy, pixel art generic. The night route swapped two shapes of the layout.
3. Finalists in more habitats with separate animal sprites. Glow went
   flat and loud off green. Floats held best at true size (Claude's pick). Paper was the most elegant.
4. **Pop-up paper was chosen.** The paper animals then needed a new prompt: "papercraft" gave blocky folded toys;
   "flat layered cut-paper illustration ... nothing folded" gave the right animals.

Things to watch: a white ball on the pale arctic card has little contrast (outline it); paper animals must stay
flat-layered; older kids should not find it babyish, so keep the colours rich and the type grown-up.
