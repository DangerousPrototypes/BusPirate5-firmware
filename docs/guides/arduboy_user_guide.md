# Arduboy Emulator Mode — User Guide

> Play Arduboy games directly in your Bus Pirate terminal. No extra hardware needed.

---

## What Is This?

The Arduboy emulator mode turns your Bus Pirate into a tiny game console. It runs Arduboy games (.hex files) using a software emulator of the ATmega32U4 CPU and SSD1306 OLED display, rendering the 128×64 pixel output as ANSI art in your serial terminal.

**Requirements:**
- Bus Pirate 5, 5XL, 6, or 7 (firmware with `BP_USE_ARDUBOY` enabled)
- A terminal emulator with **truecolor** (24-bit color) and **Unicode** support
- `.hex` game files copied to the Bus Pirate's flash storage

### Recommended Terminal Emulators

| Terminal | Platform | Truecolor | Unicode | Notes |
|----------|----------|-----------|---------|-------|
| **Windows Terminal** | Windows | ✅ | ✅ | Best Windows option |
| **PuTTY** | Windows | ❌ | ⚠️ | Will not render correctly |
| **iTerm2** | macOS | ✅ | ✅ | Works great |
| **Terminal.app** | macOS | ❌ | ✅ | No truecolor, degraded |
| **Alacritty** | All | ✅ | ✅ | Fast, recommended |
| **kitty** | Linux/macOS | ✅ | ✅ | Fast, recommended |
| **GNOME Terminal** | Linux | ✅ | ✅ | Works well |
| **minicom** | Linux | ⚠️ | ⚠️ | May have issues |

For best results, set your terminal to **at least 132 columns × 40 rows**.

---

## Getting Game Files

Arduboy games are distributed as compiled `.hex` files (Intel HEX format). Sources:

- **Arduboy community**: [community.arduboy.com](https://community.arduboy.com/) — game jams, homebrew
- **Erwin's Arduboy Collection**: Large curated collection of `.hex` files
- **GitHub**: Many games are open-source — look for releases with `.hex` downloads
- **Compile your own**: Use the Arduino IDE with Arduboy2 library; choose "Export compiled Binary" to get a `.hex` file

> **Note:** Only `.hex` files are supported — not `.arduboy` packages. If you have a `.arduboy` file, it's a ZIP archive; extract it and use the `.hex` inside.

---

## Loading Games onto Bus Pirate

The Bus Pirate appears as a USB mass storage drive when connected to your computer:

1. **Connect** your Bus Pirate via USB
2. **Open** the Bus Pirate drive that appears on your computer
3. **Copy** `.hex` files to the drive root (or any directory)
4. **Eject** safely before switching to the terminal

Alternatively, use the Bus Pirate's built-in `cat` or file transfer commands.

---

## Entering Arduboy Mode

From the Bus Pirate command prompt:

```
HiZ> m
```

Select **ARDUBOY** from the mode list. The Bus Pirate will prompt for a `.hex` filename:

```
Arduboy Emulator Mode
Place .hex files on the Bus Pirate flash drive.
Enter filename to load: mygame.hex
```

Type the filename and press Enter. If the file loads successfully:

```
Loaded 28432 bytes from 'mygame.hex'
Program loaded into emulated flash
Type 'play' to start the game, 'info' for details.
ARDUBOY>
```

You can also press Enter without a filename and load later with the `load` command.

---

## Mode Commands

Once in Arduboy mode, three commands are available:

### `play`

Launches the game in fullscreen terminal mode. The terminal switches to an alternate screen buffer, hides the cursor, and renders the Arduboy display as ANSI art.

```
ARDUBOY> play
```

The game runs until you press **ESC** or **Ctrl+Q**.

### `load`

Load a different `.hex` file without leaving Arduboy mode:

```
ARDUBOY> load
Loading new ROM...
Enter .hex filename: othergame.hex
Loaded 15204 bytes from 'othergame.hex'
ROM loaded. Type 'play' to start.
```

### `info`

Display emulator status and loaded ROM information:

```
ARDUBOY> info
Arduboy Emulator
  ROM: mygame.hex
  CPU: ATmega32U4 (emulated)
  Flash: 32768 bytes
  SRAM:  2560 bytes
  EEPROM: 1024 bytes
  Display: 128x64 → terminal (ANSI)
  Cycles/frame: 266666 (~60fps @ 16MHz)
```

### `help`

Show the mode help screen with command list and controls:

```
ARDUBOY> help
```

---

## In-Game Controls

While a game is running (`play` command active):

| Key | Arduboy Button | Notes |
|-----|---------------|-------|
| **↑** Arrow Up | D-pad UP | |
| **↓** Arrow Down | D-pad DOWN | |
| **←** Arrow Left | D-pad LEFT | |
| **→** Arrow Right | D-pad RIGHT | |
| **Z** or **N** | A button | Two keys for comfort |
| **X** or **M** | B button | Two keys for comfort |
| **ESC** | — | Exit game, return to prompt |
| **Ctrl+Q** | — | Exit game (alternate) |

### Control Tips

- **Keys are case-insensitive** — `z` and `Z` both work
- **Multiple buttons** can be held simultaneously (e.g., diagonal movement + fire)
- **Input is polled each frame** (~60fps) — short taps register as single-frame presses
- Some games expect you to **hold** buttons rather than tap them
- If a game seems unresponsive, try pressing a button to get past a title screen

### Why Z/X and N/M?

These keys are chosen because:
- **Z/X** are next to each other on QWERTY, QWERTZ, and AZERTY keyboards — natural for left-hand A/B
- **N/M** are next to each other — natural for right-hand play
- Arrow keys + Z/X gives a classic retro gaming layout

---

## Display

The Arduboy's 128×64 monochrome OLED is rendered using Unicode half-block characters (`▀`, `▄`, `█`), packing 2 vertical pixels into each terminal character cell. This produces a **128 × 32** character display.

### Color Palettes

The emulator uses a green phosphor palette by default. Four palettes are built in:

| Palette | Foreground (ON) | Background (OFF) | Look |
|---------|----------------|------------------|------|
| **Green** | Bright green | Dark green | Classic monochrome monitor |
| **White** | Pure white | Black | High contrast |
| **Amber** | Orange-amber | Dark brown | Warm retro CRT |
| **Blue** | Light blue | Dark navy | Cool tone |

> Palette selection is currently compile-time (defaults to Green). A runtime `palette` command is planned for a future update.

### Performance

The status bar below the game display shows:
- **FPS** — actual frames per second rendered
- **ms** — emulated millisecond clock

Target is 60 FPS. If your terminal is slow to render, FPS may drop. Tips:
- Use a fast terminal emulator (Alacritty, kitty, WezTerm)
- Reduce terminal window size to minimize scroll region
- Disable terminal transparency/blur effects
- The dirty-cell renderer only redraws changed pixels, so static screens are very fast

---

## Exiting

### Exit a Running Game

Press **ESC** or **Ctrl+Q** during gameplay. The terminal restores to the normal Bus Pirate prompt:

```
Game exited.
ARDUBOY>
```

### Exit Arduboy Mode

From the Arduboy prompt (not during gameplay), switch back to HiZ:

```
ARDUBOY> m
```

Select mode 1 (HiZ) or any other mode. The emulator memory is freed automatically.

---

## Troubleshooting

### "Error: cannot open 'file.hex'"

- Check the filename is spelled correctly (case-sensitive on FAT filesystem)
- Verify the file exists on the Bus Pirate flash storage
- Try listing files: exit to HiZ, use `ls` to see available files

### "Error: invalid Intel HEX format"

- The file may not be a valid Intel HEX file
- Ensure you copied the `.hex` file, not the `.elf` or `.bin`
- Some `.hex` files from non-Arduboy AVR projects may target a different chip — only ATmega32U4 code will work

### "Error: memory in use by another feature"

- Another mode or feature is using the large memory buffer
- Exit to HiZ first (`m` → 1), then re-enter Arduboy mode

### Display looks garbled

- Your terminal may not support truecolor (24-bit) ANSI codes
- Try a different terminal emulator (see table above)
- Ensure your terminal font includes Unicode block characters (▀▄█)
- Increase terminal window width to at least 132 columns

### Game doesn't respond to input

- Some games need a button press to get past the splash screen
- Arrow keys may not work if your terminal is intercepting them (e.g., for scrollback)
- Try the alternate keys: N/M instead of Z/X, or vice versa

### Game crashes or hangs

- The emulator covers the core AVR instruction set used by Arduboy games. Some games using unusual instructions or hardware features (USB, ADC, advanced timers) may not work
- Games that rely on precise cycle-accurate timing may behave differently
- Interrupts are currently limited — games heavily dependent on timer interrupts may have timing issues
- Try a different game to verify the emulator is working

### FPS is low

- The RP2040 runs the interpreter on Core 0 — complex games with heavy computation may not reach 60fps
- Terminal rendering speed matters — each frame sends ANSI escape sequences over USB serial
- Static screens (menus, pause) render fastest due to dirty-cell tracking

---

## Limitations

| Feature | Status | Notes |
|---------|--------|-------|
| CPU instructions | ✅ Most supported | Core AVR ISA for ATmega32U4 |
| SSD1306 display | ✅ Working | Horizontal addressing mode |
| 6 buttons | ✅ Working | UP/DOWN/LEFT/RIGHT/A/B |
| Timer0 / millis() | ✅ Working | Approximate timing |
| EEPROM save data | ✅ Working | 1KB, volatile (lost on mode exit) |
| Audio | ❌ Stub only | Pins tracked but no sound output |
| USB (device mode) | ❌ Not emulated | Some games check USB state |
| Advanced timers | ❌ Partial | Timer1/3/4 not fully emulated |
| Interrupts | ❌ Limited | Timer0 overflow only |
| Cycle-accurate timing | ❌ Approximate | Instruction-level, not cycle-level |
| EEPROM persistence | ❌ Volatile | Save data is lost when exiting mode |
| LCD backend | 🔜 Planned | Render on Bus Pirate's ST7789 LCD |
| ROM launcher menu | 🔜 Planned | Browse and select games from storage |
| Audio via PWM | 🔜 Planned | Route to Bus Pirate piezo/pin |

---

## Quick Reference Card

```
┌──────────────────────────────────────────┐
│          ARDUBOY EMULATOR                │
│                                          │
│  Enter mode:   m → select ARDUBOY        │
│  Load game:    load                      │
│  Play game:    play                      │
│  Game info:    info                      │
│  Help:         help                      │
│  Exit game:    ESC or Ctrl+Q             │
│  Exit mode:    m → select HiZ            │
│                                          │
│  ┌──────────────────────────────┐        │
│  │  CONTROLS                    │        │
│  │  ↑↓←→ .... D-pad            │        │
│  │  Z / N .... A button         │        │
│  │  X / M .... B button         │        │
│  │  ESC ...... Quit             │        │
│  └──────────────────────────────┘        │
└──────────────────────────────────────────┘
```
