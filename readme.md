# Bus Pirate 5

![](./img/bp5rev10-cover-angle.jpg)

Bus Pirate 5 is the latest edition of the universal serial interface trusted by hackers since 2008.

Can't get a chip to work? Is it the circuit, code, bad part or a burned out pin? The Bus Pirate sends commands over common serial protocols (1-Wire, I2C, SPI, UART, MIDI, serial LEDs, etc) so you can get to know a chip before prototyping. Updated with tons of new features, talking to chips and probing interfaces is more fun than ever!

## Resources

- [Download the latest firmware build](https://forum.buspirate.com/t/bus-pirate-5-auto-build-main-branch/20/99999)
- [Get help and chat in the forum](https://forum.buspirate.com/)
- [Firmware documentation](https://firmware.buspirate.com/) ([source](https://github.com/DangerousPrototypes/BusPirate5-docs-firmware))
- [Hardware documentation](https://hardware.buspirate.com/) ([source](https://github.com/DangerousPrototypes/BusPirate5-docs-hardware))
- [Hardware repo](https://github.com/DangerousPrototypes/BusPirate5-hardware)
- [Firmware repo](https://github.com/DangerousPrototypes/BusPirate5-firmware)

## VT100 terminal interface

![](./img/teraterm-done.png)

VT100 terminal emulation supports color and a live statusbar view of the voltage and functions on each pin. Type simple commands into the terminal, the Bus Pirate translates them into popular serial protocols and displays the response. Learn how a chip works without touching a line of code.

## Specifications

- Raspberry Pi RP2040 with 128Mbit program flash
- 8 powerful IO pins - Support multiple protocols from 1.2-5volts. Analog voltage measurement and optional 10K pull-ups on all pins
- 1-5volt output power supply - 0-500mA current limit, current sense, resettable fuse and protection circuit
- 1Gbit NAND flash - Store settings and files. Appears as a USB drive.
- LCD - A beautiful 240x320 pixel color IPS (all angle viewing) LCD acts as a pin label, while also showing the voltage on each pin and the current consumption of the programmable power supply unit
- 18 RGB LEDs - It's customary to have an indicator LED, so to check that box we added 16 SK6812 RGB LEDs.
- Just one button - 18 party LEDs but just one button!
- 1-Wire, I2C, SPI, UART, MIDI, serial LEDs supported, more to come!

Bus Pirate 5 is the universal serial interface tool designed by hackers, for hackers. It's crammed full of hardware and firmware features to make probing chips pleasant and easy.  

## Build

This project uses `cmake` as the build system, so building the project only takes 2 steps:
1. project configuration (needs to be ran once, or when you want to change configuration).  
    `cmake -S . -B build -DPICO_SDK_FETCH_FROM_GIT=TRUE`  
    you may want to add the flags `-DPICO_SDK_PATH=/path/to/pico-sdk -DPICO_SDK_FETCH_FROM_GIT=FALSE` if you want to use pico-sdk that is in your local path.  
    > NOTE: WINDOWS users: for some reason the automatic fetch fails because of git submodule, so you will need to clone pico-sdk your self, and then
    > apply the following commands inside the pico-sdk repo:  
    > ```bash
    > git submodule init
    > git submodule update
    > ```
    
2. project build  
    `cmake --build ./build --target bus_pirate5_rev10`  
    you may set `bus_pirate5_rev10` to `bus_pirate5_rev8` if the have the development version.
    
### build using docker
Instructions on the forum provide additional details; however, this repo provides a docker compose image for you to just get running quickly in the event you want to try patching/hacking.
To run a build, perform the following actions on linux:

```sh
# clone the repo
git clone git@github.com:DangerousPrototypes/BusPirate5-firmware.git
cd BusPirate5-firmware

# OPTIONAL: build the environment, or you can just let it pull
# docker compose build

# run a build
UID=$(id -u) GID=$(id -g) docker compose run dev build-clean
# build stuff happens ...
# [100%] Linking CXX executable bus_pirate5_rev10.elf
# Memory region         Used Size  Region Size  %age Used
#            FLASH:      460068 B         2 MB     21.94%
#              RAM:      256168 B       256 KB     97.72%
#        SCRATCH_X:          4 KB         4 KB    100.00%
#        SCRATCH_Y:          0 GB         4 KB      0.00%
# [100%] Built target bus_pirate5_rev10
# your build will be placed in ./build/

# Or drop into the container and run builds manually
UID=$(id -u) GID=$(id -g) docker compose run dev
```

### Building without LGPL3 protected component
**NOTE** by doing the following you may not need to distribute the binaries under LGPL3 license terms. 

To compile the firmware without LGPL3 components, simply add the following flags to the configuration step above:  
`-DUSE_LGPL3=NO -DLEGACY_ANSI_COLOURS_ENABLED=NO`

## Other open source licenses

This project uses code from the following licenses:  
* LGPL3  
* BSD 3 clause 

More information on the licenses and components being used can be found [here](docs/licenses.md).  

## Contributing
Please see [contributing.md](docs/contributing.md)


