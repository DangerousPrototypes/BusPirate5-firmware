# flash_management
Flash management stack consisting of a flash translation layer ([dhara](https://github.com/dlbeer/dhara)) and an SPI NAND driver. Uses an STM32L432KCUX MCU connected to a Micron MT29F1G01ABAFDWB SPI NAND SLC flash chip.

This project is intended to be the "minimum implementation" needed to tie a FAT filesystem, flash translation layer, and low-level flash driver together. Most areas of the source code are heavily commented (probably over commented) in an effort to make it easy as possible for people unfamiliar with ONFI & flash translation concepts to follow.

## project structure
```
src
├── cmsis
│   └── (...)
├── dhara
│   └── (...)
├── fatfs
│   └── (...)
├── modules
│   ├── fifo.h
│   ├── led.h/c
│   ├── mem.h/c
│   ├── nand_ftl_diskio.h/c
│   ├── shell.h/c
│   ├── shell_cmd.h/c
│   ├── spi.h/c
│   ├── spi_nand.h/c
│   ├── sys_time.h/c
│   └── uart.h/c
├── st
│   └── (...)
├── main.c
├── startup_stm32l432kc.c
├── stm32l432kc.ld
├── stm32l432kc_it.c
└── syscalls.c
```
- **cmsis/** - Cortex Microcontroller Software Interface Standard files.
- **dhara/** - Dhara NAND flash translation layer ([see here](https://github.com/dlbeer/dhara)).
- **fatfs/** - ChaN FAT file system library ([see here](http://elm-chan.org/fsw/ff/00index_e.html)).
- **modules/**
    - **fifo.h** - Barebones header-only FIFO implementation (for raw bytes).
    - **led.h/c** - Barebones LED driver; used on startup to notify the user that code is running.
    - **mem.h/c** - Dumb memory allocator for gaining access to a single buffer thats the length of an SPI NAND page (to avoid putting this buffer on the stack or duplicating in static definitions - where possible). This is written as a generic "mem" module so that it could be expanded into a real heap allocator without updates to the calling code.
    - **nand_ftl_diskio.h/c** - Implements the disk IO functions used by the FAT file system. Disk IO is a nice abstraction as USB MSC read/write & get size functions can call directly into this layer (be careful with mutual exclusion between FATFS and USB MSC if both are implemented in your project).
    - **shell.h/c** - Barebones shell functionality for interacting with the device over a serial connection such as USB CDC or UART (only UART is implemented in this project).
    - **shell_cmd.h/c** - Defines the shell commands used in the project.
    - **spi.h/c** - Barebones synchronous SPI driver.
    - **spi_nand.h/c** - Low-level SPI NAND driver. This is written specifically to support the MT29F for simplicity (rather than having a generic core driver + chip specific drivers).
    - **sys_time.h/c** - Uses the sys tick to generate a 1ms time base; exposes convenience functions such as get time, delay, is elapsed, etc.
    - **uart.h/c** - Barebones synchronous UART driver.
- **st/** - ST low-level driver files (only files used by the project are present).
- **main.c** - Main application.
- **startup_stm32l432kc.c** - Defines weak exception handlers, calls CMSIS & libc init functions, initializes bss and data sections, calls main application.
- **stm32l432kc.ld** - Linker script -- differs from ST's default linker script in that the stack is placed at bottom of RAM so that stack overflows cause an exception rather than silently overwriting data (thanks uncle Miro).
- **stm32l432kc_it.c** - All overrides for exception handlers. All faults just turn on the LED (if able).
- **syscalls.c** - Lib c sys calls.

## usage
All interaction is handled through the shell (currently) which uses a UART backend. If you're using a nucleo board you can simply plug in to USB and use the virtual com port.
### shell commands
#### utility
- help
#### raw flash interaction
- read_page
- write_page
- erase_block
- get_bbt
- mark_bb
- page_is_free
- copy_page
- erase_all
#### file system interaction
- write_file
- read_file
- list_dir
- file_size

## future improvements
- More shell commands for interacting with the FAT filesystem, especially format. This is needed to recover from FS errors that may occur when using raw flash commands from the shell.
- Add USB MSC. I'd really like to avoid using ST's HAL, so the easiest pathway here is probably porting [TinyUSB](https://github.com/hathach/tinyusb) to the STM32L4 (F4 is already supported).
- Example datalogging application.

## acknowledgements
- https://github.com/dlbeer/dhara - Dhara NAND flash translation layer.
- http://elm-chan.org/fsw/ff/00index_e.html - ChaN FAT file system library.
- https://www.state-machine.com/quickstart/ - Miro Samek's "Modern Embedded Systems Programming Course". Great course for beginners, but also a great reference for writing your own linker and startup files.
- https://interrupt.memfault.com/blog/tag/zero-to-main - A series of blog posts detailing how to bootstrap C applications on cortex-m MCU's.
