# ds30 Loader for the Bus Pirate v3

Source of the Bus Pirate v3 bootloader v4.5. `binmode/bp3unbrick_bootloader.h` holds its
machine code as data, and the "Bus Pirate v3 unbrick" binmode writes it to the
PIC24FJ64GA002 of a Bus Pirate v3. This firmware does not build it: it builds with
Microchip's assembler and the PIC24FJ64GA002 linker script of that toolchain, and
`settings.inc` selects the Bus Pirate v2go/v3 pins.

The words in `bp3unbrick_bootloader.h` are the bootloader as the combined firmware +
bootloader .hex files carry it. Compared with the standalone bootloader v4.5, the bljump
word at 0xABF8 is the one of v4.4, and 22 words after the code are leftovers of how those
files were put together. They never run.

ds30 Loader, Copyright (c) 2009 Mikael Gustafsson, modified for the Bus Pirate v2go and v3.
GNU General Public License, see `docs/third_party_licenses/gpl-3.0.txt`.
