# Inspiration

What other people have built, judged for a young kid: does it work on this robot, can a kid see it and
do it, and does it make them feel they taught the robot something?

Researched 2026-11-06. "Read" means the source page was opened. "Summary" means only a search
summary was seen. YouTube pages returned titles only, so no video here has been watched.

## Main finding

BugC projects online are remote-control demos. No voice or memory project turned up for BugC, in
English or Japanese. The ideas worth borrowing come from other robots.

## Adopted into the plan

| Idea | Source | What we take | Checked |
|---|---|---|---|
| Exaggerate, correct, save | [Alter3 paper](https://arxiv.org/pdf/2312.06571.pdf) | The model first writes an exaggerated description, then turns it into movement. Verbal feedback revises it. Refined motions are saved to a JSON database with labels and reused. Same shape as "scared spider" and "change my crazy dance". | Read |
| Mood state | [Cozmo, The Robot Report](https://www.therobotreport.com/cozmo-ai-robot-brings-wall-e-to-life/) | "A drop in braveness" at cliffs, "a boost in confidence" after stacking cubes. Animators tested each sequence on a physical prototype. "We want to make him feel alive and not like a big if-then-else statement." Note: Cozmo's losing streaks don't change its long-term personality; ours should. | Read |
| Flee to follow arc | [Braitenberg vehicles](https://colin-price.wbs.uni.worc.ac.uk/DifferentialDriveRobots/CBP_Chapters_Release/Ch6_Braitenberg_Vehicles.pdf), [distance-band follower](https://www.hackster.io/diy_burner/how-to-make-arduino-human-following-robot-32ca7d) | Sensor-to-motor wiring that reads as fear or love. Reverse when close, approach in a middle band. One learned number can slide between them. No kids' version found, so we design our own. | Summary |
| Local speech-to-text in development | [Rocky desk robot](https://github.com/EMIL-code-getin/desk-robot) | ESP32 streams audio over Wi-Fi to a Python brain running faster-whisper locally, with Anthropic as an LLM option. Free, and the kid's voice stays on the PC during development. | Read |
| Body keeps working if the brain dies | [PetBot](https://github.com/Vetri2425/petbot) | "If Linux crashes, the pet still blinks and reacts." Confirms our on-robot / cloud split. | Read |

## Later ideas

| Idea | Source | New parts | Notes |
|---|---|---|---|
| Drive it, name it, replay it | [Curlybot](https://www-prod.media.mit.edu/projects/curlybot/overview/) (MIT, ages 4+) | Any TV remote | Curlybot is pushed by hand. Ours has geared motors, so drive by IR remote and record. Works offline. |
| Drawing | [Logo turtle, 1969](https://cyberneticzoo.com/cyberneticanimals/1969-the-logo-turtle-seymour-papert-marvin-minsky-et-al-american/) | LEGO, washable marker, paper | Floor turtles usually carried a pen. Teaching Logo a new word is what "remember it" is. Open-loop, so shapes will be wobbly. Marker drag is untested. |
| Learns to drive by trial and error | [Arduino crawler](https://scholar.ppu.edu/items/d69d1fda-87fe-4b63-878a-843ab4596810) | None | Q-learning with an ultrasonic sensor as the reward took 5 to 10 minutes. Ours: tries motor patterns facing a wall and keeps what gets it closer. Real learning a kid can watch. |
| Personality from upbringing | [Moflin](https://www.thestar.com.my/aseanplus/aseanplus-news/2025/08/03/ai-robot-pet-hit-in-japan-as-personalities-vary-based-on-upbringing) | None | Commercial proof of the "raise your own" idea, maturing over weeks. Moflin has no reset; we should have one. |
| Send tricks from your house | Our idea | None | Tricks are small data, so a new one can arrive over the relay as a surprise. |
| Joystick controller | [vkichline/BugC](https://github.com/vkichline/BugC) | Second Stick, Joystick Hat | ESP-NOW remote control. The most reusable BugC code found. |
| Robot sumo | [SimpleSumo](https://instructables.com/SimpleSumo-Educational-Fighting-Robots) | Second robot | Real sport with a kid-friendly on-ramp. Needs a ring. |
| Follow a heat source | [M5StickC thermal tracker](https://www.youtube.com/watch?v=AIlpmhLpslI) | Thermal Camera 2 Unit | Title only, not watched. Could give it a sense of direction. |
| Trick cards | [Sphero indi](https://www.mysescutah.gov/page/sphero-indi-robot-kit) | RFID unit, cards | Screenless coding with physical tiles. Same "won't work on carpet" complaint we expect. |

## Longer term: the kid and AI write new behaviours

- Project Fetch: [Phase two](https://www.anthropic.com/news/project-fetch-phase-two) and
  [the original study](https://www.anthropic.com/research/project-fetch-robot-dog). Claude's biggest
  win was connecting to hardware. Its weakest area was precise closed-loop physical control. This
  backs keeping Claude out of real-time motion. (Read)
- [KidCode](https://github.com/statico/kidcode): wraps Claude Code for kids with a G-rated prompt and
  undo snapshots. Archived 2026-10-24, and it runs Claude with permissions off, so take the ideas
  (undo, G-rated prompt) and not the setup. (Read)
- [An 8-year-old and a parent built an app in three hours](https://startupfortune.com/an-8-year-old-built-an-app-in-three-hours-heres-what-that-tells-us/)
  with AI tools. (Summary)
- [ChotuRobo](https://github.com/vishalmysore/choturobo): MCP server connecting Claude to an ESP32
  robot. Small and possibly unmaintained, but the pattern could let Claude Code test on the robot
  directly. (Summary)
- Stefania Druga's [thesis](https://dspace.mit.edu/handle/1721.1/120691) on 7-to-14-year-olds, and
  [MIT coverage](https://www-prod.media.mit.edu/articles/should-children-form-emotional-bonds-with-robots/).
  Children of 6 to 10 tend to believe robots are smarter than they are. Programming and teaching
  them shifts that. Small studies. (Summary)

## Warnings

- [Moxie](https://www.axios.com/2024/12/10/moxie-kids-robot-shuts-down): a robot of about $800 for
  ages 5 to 10. It depended on the cloud, so it stopped working when the company closed. Keep the
  core on the robot and have a plan for the relay outliving our attention. (Summary)
- [PIRG on AI toys](https://www.wlox.com/2025/12/12/consumer-safety-report-warns-disturbing-responses-ai-powered-toys/):
  the Kumma bear gave unsafe answers in long chats and reportedly kept listening without a button.
  Reasons for no free chat and for push-to-talk. PIRG also says AI toys shouldn't be marketed as
  emotional companions for children. (Summary)
- [Project Fetch](https://www.anthropic.com/research/project-fetch-robot-dog): one team commanded the
  robot at 1 m/s with a table in range, and the organizer cut power. Limits belong in code. (Read)

## Useful references, not adopted

| Project | Why not |
|---|---|
| [xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) | Popular MCP voice firmware. Listed chips are ESP32-C3, S3 and P4, not our PICO-V3, and it's built for chat. The "motors as tools" design is a good reference. |
| [AI Stack-chan](https://github.com/610t/AI_StackChan_Ex), [official StackChan](https://github.com/m5stack/StackChan) | The closest M5 AI community, with function calling. Mostly a talking head on Core2 or CoreS3. Useful for M5 voice code. |
| [m5stack-avatar](https://github.com/stack-chan/m5stack-avatar) | Face library on M5Unified. Frozen at v0.10.0, and StickC isn't mentioned. Our eyes need custom design anyway. |
| [RoboEyes](https://github.com/FluxGarage/RoboEyes) | Good mood-eye reference. OLED-focused and GPL-3.0. |
| [EmoBot](https://github.com/rounakrkr/EmoBot) | Same architecture as ours (WebSocket PCM, cloud Whisper, LLM). Good sanity check. Chat-based, no stars. |
| Furby + ChatGPT videos ([example](https://www.youtube.com/watch?v=I8npAzUIYI8)) | Fun, and shows what personality does for a toy. Free LLM speech is what we're avoiding. |

## YouTube leads (titles only, not watched)

- [So cute, a tiny robot!! (BugC)](https://www.youtube.com/watch?v=U-zz74wpFtc)
- [BugC camina](https://www.youtube.com/watch?v=z1-eRpyuLYY)
- [BugC with a PC game controller](https://www.youtube.com/watch?v=TBBzF7eJd4g)
- [I Built a Robot Dog That Actually Feels Alive](https://www.youtube.com/watch?v=ZCQ0IS3Zf7o)
- [How I made this Voice-Controlled Robot Dog from Scratch](https://www.youtube.com/watch?v=QQHyo7KlaQk)
- [Heat-following robot with M5StickC](https://www.youtube.com/watch?v=AIlpmhLpslI)
