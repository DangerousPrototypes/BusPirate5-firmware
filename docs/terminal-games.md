# Terminal Games

Bus Pirate includes a collection of electronics-themed terminal games playable over the serial connection. All games run in a standard VT100-compatible terminal and use ANSI color when enabled.

> **Tip:** For the best experience set your terminal to at least 80×24 and enable ANSI color mode (`color` command).

---

## Arcade

Classic action games with real-time controls.

### Snake — `snake`

Eat food pellets to grow your probe trace. Don't crash into yourself — the board wraps toroidally so you come out the other side.

| | |
|---|---|
| **Run** | `snake` |
| **Controls** | WASD or arrow keys to steer, **q** quit |
| **Objective** | Eat as much food as you can without self-collision |

### Bricks — `bricks`

Paddle-and-ball brick breaker. Smash rows of components off the board.

| | |
|---|---|
| **Run** | `bricks` |
| **Controls** | **A**/**D** or left/right arrows to move paddle, **q** quit |
| **Objective** | Clear all bricks without losing the ball |

### Worm — `worm`

BSD worm clone. Food is numbered 1–9 — eating it grows the worm by that many segments. Plan ahead!

| | |
|---|---|
| **Run** | `worm` |
| **Controls** | WASD or arrow keys to steer, **q** quit |
| **Objective** | Eat numbered food and survive as long as possible |

### Wire Trace — `trace`

Pac-Man on a PCB. Navigate your probe through a maze of copper traces collecting solder blobs, dodge escaped magic smoke monsters, and use vias to teleport. Maze grows and sparks speed up each level.

| | |
|---|---|
| **Run** | `trace` |
| **Controls** | WASD or arrow keys to move, **q** quit |
| **Objective** | Reach the goal (G) from the start (S), collect solder blobs for bonus points, avoid sparks |

### Protocol Invaders — `invaders`

Malformed packets (`0xDEAD`, `NAK`, `CRC!`) descend in formation. Blast them before they reach your capacitor barriers. Barriers degrade as they absorb hits.

| | |
|---|---|
| **Run** | `invaders` |
| **Controls** | Left/right arrows to move, **space** to fire, **q** quit |
| **Objective** | Destroy all enemy packets before they land |

---

## Side-Scrollers & Runners

Games where the world scrolls and you react.

### PCB Run — `pcbrun`

Endless side-scrolling runner on a PCB trace. Jump over resistors, capacitors, and ICs hurtling toward you. Speed increases over time.

| | |
|---|---|
| **Run** | `pcbrun` |
| **Controls** | **Space** or **Up** to jump, **q** quit |
| **Objective** | Survive as long as possible |

### Signal Rider — `sigride`

Surf across a scrolling oscilloscope display. Dodge noise spikes, EMI bursts, and ground faults by switching lanes. Collect clean signal pickups for points.

| | |
|---|---|
| **Run** | `sigride` |
| **Controls** | Up/down arrows to change lane, left/right to nudge, **q** quit |
| **Objective** | Ride the signal as far as possible without hitting hazards |

### Crossflash — `xflash`

Frogger on a logic analyzer. Cross lanes of scrolling protocol traffic (SPI clocks, I2C data, UART packets) without getting clocked. Reach the target columns to advance.

| | |
|---|---|
| **Run** | `xflash` |
| **Controls** | Arrow keys to move, **q** quit |
| **Objective** | Cross all bus lanes and reach every target column |

---

## Platformer

### Stack Overflow — `stkover`

Vertical-scrolling jump platformer. Your probe auto-bounces on landing — steer left and right to climb an endless stack of PCB platforms (decoupling caps, test points, solder pads, moving headers). Miss a platform and you fall off the bottom.

| | |
|---|---|
| **Run** | `stkover` |
| **Controls** | Left/right arrows to move (auto-jump on landing), **q** quit |
| **Objective** | Climb as high as you can |

---

## Roguelike

### Rogue Probe — `rogue`

Procedurally generated roguelike inside an IC die. Explore rooms connected by silicon traces, battle ESD strikes and latch-up demons, find the firmware dump, and escape through the bond pad. Turn-based with permadeath.

| | |
|---|---|
| **Run** | `rogue` |
| **Controls** | WASD or arrow keys to move/attack, **space** to wait, **q** quit |
| **Objective** | Find the firmware dump on the final floor and escape alive |

---

## Puzzles

### 2048 — `2048`

Slide numbered tiles on a 4×4 grid. Matching tiles merge and double. Reach 2048 to win — or keep going for a high score.

| | |
|---|---|
| **Run** | `2048` |
| **Controls** | WASD or arrow keys to slide, **r** restart, **q** quit |
| **Objective** | Merge tiles to reach 2048 |

### Mine Sweep — `mines`

Classic minesweeper on a 10×10 grid with 10 hidden mines. Numbers reveal how many adjacent mines surround a cell. Flag cells you suspect.

| | |
|---|---|
| **Run** | `mines` |
| **Controls** | WASD or arrow keys to move cursor, **space**/**enter** to reveal, **f** to flag, **r** restart, **q** quit |
| **Objective** | Reveal all safe cells without hitting a mine |

---

## Board & Strategy

### Tic-Tac-Toe — `ttt`

Classic 3×3 grid against an unbeatable minimax AI. Good luck getting anything better than a draw.

| | |
|---|---|
| **Run** | `ttt` |
| **Controls** | **1–9** (numpad layout) to place your mark, **r** restart, **q** quit |
| **Objective** | Get three in a row |

### Drop Four — `drop4`

Drop colored discs into a 7-column board. First to connect four in a row (horizontal, vertical, or diagonal) wins. CPU opponent uses look-ahead AI.

| | |
|---|---|
| **Run** | `drop4` |
| **Controls** | **1–7** to drop in column (or arrows + enter), **r** restart, **q** quit |
| **Objective** | Connect four discs in a row |

### Fleet — `fleet`

Battleship on a 10×10 grid. Five hidden enemy ships — call your shots and sink them all in as few moves as possible.

| | |
|---|---|
| **Run** | `fleet` |
| **Controls** | Arrow keys to aim cursor, **space**/**enter** to fire, **r** restart, **q** quit |
| **Objective** | Sink all 5 ships |

---

## Educational

### Logic Gates — `gates`

A timed quiz. You're shown a truth table and must identify the matching logic expression from four choices. Difficulty increases with cascaded and inverted gates.

| | |
|---|---|
| **Run** | `gates` |
| **Controls** | **1–4** to pick answer, **q** quit |
| **Objective** | Answer as many correctly as possible before time runs out |

### Crypto Crack — `crack`

Speed-drill cipher challenges: Caesar shift, XOR, hex↔ASCII, binary↔decimal. Type the decoded answer before time runs out. Difficulty and cipher variety increase with each level.

| | |
|---|---|
| **Run** | `crack` |
| **Controls** | Type answer + **enter** to submit, **q** quit |
| **Objective** | Decode as many ciphers as possible |

---

## Simulation

### Game of Life — `life`

Conway's Game of Life cellular automaton running fullscreen. Watch patterns evolve, pause to edit, or randomize the board.

| | |
|---|---|
| **Run** | `life` |
| **Controls** | **Space** pause/resume, **r** randomize, **c** clear, **+**/**−** speed, **q** quit |
| **Objective** | Observe and experiment with emergent cellular patterns |

---

## Word Game

### Hangman — `hangman`

Classic hangman with electronics and protocol vocabulary — guess words like MOSFET, BUSPIRATE, CHIPSELECT one letter at a time.

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
