# Terminal Games

Bus Pirate packs a full arcade of electronics-themed terminal games playable right over the serial connection. Neon colors, real-time action, and enough blinking cursors to make your scope jealous. All games run in any standard VT100-compatible terminal.

> **Tip:** For the best experience set your terminal to at least 80×24 and enable ANSI color mode (`color` command).

---

## Arcade

White-knuckle action at terminal speed.

### Snake — `snake`

Pilot a colossal circuit-trace serpent through cyberspace, devouring food pellets to grow ever longer. The board wraps toroidally — fly off one edge and you come screaming out the other. One wrong turn into your own tail and it's game over.

| | |
|---|---|
| **Run** | `snake` |
| **Controls** | WASD or arrow keys to steer, **q** quit |
| **Objective** | Eat as much food as you can without self-collision |

### Bricks — `bricks`

Wield an electrified paddle and smash through a towering wall of electronic components. Capacitors explode, resistors shatter, ICs crack in showers of sparks. Don't let the ball hit the floor.

| | |
|---|---|
| **Run** | `bricks` |
| **Controls** | **A**/**D** or left/right arrows to move paddle, **q** quit |
| **Objective** | Clear all bricks without losing the ball |

### Worm — `worm`

BSD worm gone nuclear. Food pellets are numbered 1–9 — eat a 9 and your worm explodes in length like a freight train. Plan ahead or get buried in your own tail.

| | |
|---|---|
| **Run** | `worm` |
| **Controls** | WASD or arrow keys to steer, **q** quit |
| **Objective** | Eat numbered food and survive as long as possible |

### Wire Trace — `trace`

Race your probe through a labyrinth of glowing copper PCB traces. Collect solder blobs like treasure, dodge screaming magic smoke monsters with glowing eyes, and punch through vias to teleport across the board. The maze grows deadlier every level.

| | |
|---|---|
| **Run** | `trace` |
| **Controls** | WASD or arrow keys to move, **q** quit |
| **Objective** | Reach the goal (G) from the start (S), collect solder blobs for bonus points, avoid sparks |

### Protocol Invaders — `invaders`

Formations of corrupted data packets — `0xDEAD`, `NAK`, `CRC!` — descend from the sky toward your crumbling capacitor barriers. Fire your beam cannon upward and blast them into pixel explosions before they land. The barriers are degrading. The packets are winning. Almost.

| | |
|---|---|
| **Run** | `invaders` |
| **Controls** | Left/right arrows to move, **space** to fire, **q** quit |
| **Objective** | Destroy all enemy packets before they land |

---

## Side-Scrollers & Runners

The world is moving. You react or you're dead.

### PCB Run — `pcbrun`

Hurtle across the surface of a massive PCB at breakneck speed. Resistors, capacitors, and ICs come screaming toward you — jump or get flattened. Speed increases until your reflexes give out.

| | |
|---|---|
| **Run** | `pcbrun` |
| **Controls** | **Space** or **Up** to jump, **q** quit |
| **Objective** | Survive as long as possible |

### Signal Rider — `sigride`

Surf a rolling wall of oscilloscope waveform energy. Dodge erupting EMI spike towers, crackling ground fault geysers, and noise artifacts that strike like lightning. Grab clean signal pickups to keep your score climbing.

| | |
|---|---|
| **Run** | `sigride` |
| **Controls** | Up/down arrows to change lane, left/right to nudge, **q** quit |
| **Objective** | Ride the signal as far as possible without hitting hazards |

### Crossflash — `xflash`

Dodge between roaring lanes of protocol traffic on a logic analyzer battlefield — SPI clocks like freight trains, I2C bursts like electric eels, UART packets like screaming rockets. Reach the golden target columns or get clocked.

| | |
|---|---|
| **Run** | `xflash` |
| **Controls** | Arrow keys to move, **q** quit |
| **Objective** | Cross all bus lanes and reach every target column |

---

## Platformer

### Stack Overflow — `stkover`

Bounce skyward through an endless vertical gauntlet of PCB platforms — decoupling caps, test points, solder pads, moving headers stretching up through a neon stratosphere. Your probe auto-bounces on landing; steer left and right to climb. Miss a platform and you plummet into the void.

| | |
|---|---|
| **Run** | `stkover` |
| **Controls** | Left/right arrows to move (auto-jump on landing), **q** quit |
| **Objective** | Climb as high as you can |

---

## Roguelike

### Rogue Probe — `rogue`

Descend into a procedurally generated dungeon carved from silicon inside an IC die. Battle towering latch-up demons crackling with ESD lightning, explore rooms connected by glowing circuit traces, find the firmware dump, and fight your way out through the bond pad. Turn-based. Permadeath. No mercy.

| | |
|---|---|
| **Run** | `rogue` |
| **Controls** | WASD or arrow keys to move/attack, **space** to wait, **q** quit |
| **Objective** | Find the firmware dump on the final floor and escape alive |

---

## Puzzles

### 2048 — `2048`

Slide numbered tiles across a 4×4 grid in a spiral of mathematical transcendence. Matching tiles collide, merge, and double. Reach the blazing 2048 tile to win — or keep pushing into the infinite.

| | |
|---|---|
| **Run** | `2048` |
| **Controls** | WASD or arrow keys to slide, **r** restart, **q** quit |
| **Objective** | Merge tiles to reach 2048 |

### Mine Sweep — `mines`

Sweep an enormous minefield honeycombed with hidden explosives. Numbers are your only clue — each reveals how many mines lurk in adjacent cells. One wrong click and the whole board detonates. Cold sweat. Pure logic.

| | |
|---|---|
| **Run** | `mines` |
| **Controls** | WASD or arrow keys to move cursor, **space**/**enter** to reveal, **f** to flag, **r** restart, **q** quit |
| **Objective** | Reveal all safe cells without hitting a mine |

---

## Board & Strategy

### Tic-Tac-Toe — `ttt`

Face off across a 3×3 grid against an unstoppable minimax AI — a cold calculating colossus that has already computed the outcome. It knows you can't win. You play anyway.

| | |
|---|---|
| **Run** | `ttt` |
| **Controls** | **1–9** (numpad layout) to place your mark, **r** restart, **q** quit |
| **Objective** | Get three in a row |

### Drop Four — `drop4`

Hurl glowing discs into a 7-column arena in an orbital bombardment of strategy. Connect four in a row — horizontal, vertical, or diagonal — before the CPU opponent's look-ahead AI does it first. The grid hums with competitive electricity.

| | |
|---|---|
| **Run** | `drop4` |
| **Controls** | **1–7** to drop in column (or arrows + enter), **r** restart, **q** quit |
| **Objective** | Connect four discs in a row |

### Fleet — `fleet`

Call coordinates into the void and pray for a hit. Five enemy ships lurk somewhere on a 10×10 ocean grid. Torpedoes blaze through the water. Explosions dot your previous shots. The hunt is almost over — sink them all.

| | |
|---|---|
| **Run** | `fleet` |
| **Controls** | Arrow keys to aim cursor, **space**/**enter** to fire, **r** restart, **q** quit |
| **Objective** | Sink all 5 ships |

---

## Educational

### Logic Gates — `gates`

A countdown timer blazes overhead like a red sun. Truth tables materialize and you must conjure the matching logic expression from four choices — AND, OR, XOR, inverted gates orbiting like arcane sigils. Logic is your only weapon. Difficulty escalates.

| | |
|---|---|
| **Run** | `gates` |
| **Controls** | **1–4** to pick answer, **q** quit |
| **Objective** | Answer as many correctly as possible before time runs out |

### Crypto Crack — `crack`

Intercepted cipher text cascades across your terminal — Caesar wheels, XOR masks, hex streams, binary waterfalls — all screaming to be decoded before the timer hits zero. Your fingers are already moving. Difficulty and cipher variety ramp with each level.

| | |
|---|---|
| **Run** | `crack` |
| **Controls** | Type answer + **enter** to submit, **q** quit |
| **Objective** | Decode as many ciphers as possible |

---

## Simulation

### Game of Life — `life`

Hover above an infinite grid as patterns of glowing life bloom and die beneath your hands — gliders, oscillators, and still lifes emerging from pure mathematics. The universe is alive. It doesn't need your guidance. It only needs to be observed.

| | |
|---|---|
| **Run** | `life` |
| **Controls** | **Space** pause/resume, **r** randomize, **c** clear, **+**/**−** speed, **q** quit |
| **Objective** | Observe and experiment with emergent cellular patterns |

---

## Word Game

### Hangman — `hangman`

The gallows are made of soldering iron and copper wire. Incomplete words glow in oscilloscope traces — MOSFET, BUSPIRATE, CHIPSELECT — with blank spaces waiting to be filled. Every wrong letter adds another component to the scaffold. Choose wisely.

| | |
|---|---|
| **Run** | `hangman` |
| **Controls** | **a–z** to guess letters, **q** quit |
| **Objective** | Guess the word before the drawing is complete |

---

## Quick Reference

| Game | Command | Genre | Controls |
|------|---------|-------|----------|
| Snake | `snake` | Arcade | WASD / arrows |
| Bricks | `bricks` | Arcade | A/D / arrows |
| Worm | `worm` | Arcade | WASD / arrows |
| Wire Trace | `trace` | Arcade | WASD / arrows |
| Protocol Invaders | `invaders` | Arcade | arrows + space |
| PCB Run | `pcbrun` | Runner | space / up |
| Signal Rider | `sigride` | Runner | arrows |
| Crossflash | `xflash` | Runner | arrows |
| Stack Overflow | `stkover` | Platformer | arrows |
| Rogue Probe | `rogue` | Roguelike | WASD / arrows + space |
| 2048 | `2048` | Puzzle | WASD / arrows |
| Mine Sweep | `mines` | Puzzle | WASD / arrows + space + f |
| Tic-Tac-Toe | `ttt` | Board | 1–9 |
| Drop Four | `drop4` | Board | 1–7 / arrows |
| Fleet | `fleet` | Board | arrows + space |
| Logic Gates | `gates` | Educational | 1–4 |
| Crypto Crack | `crack` | Educational | type + enter |
| Game of Life | `life` | Simulation | space, r, c, +/− |
| Hangman | `hangman` | Word | a–z |
