# Using RTT with OpenOCD

RTT allows for faster debug output, using the debug port.
It also works immediately, even if UARTs or USB CDC ports
are not yet setup (or break during debugging, or ...).

This post focuses on using a Raspberry Pi Pico (RP2040)
or Pi Probe (RP2040) with OpenOCD.

Using a JLink is possible, but requires an adapter to
force the VTRef voltage (to 3.3V?).

## TODO

* [ ] create schematic and PCB for JLink debug adapter
* [ ] Prerequisites - add minimum versions, other required programs, etc.
* [ ] Document connections using Pi Probe.
* [ ] Document connections using RP2040 with probe firmware.
* [ ] Document connections using JLink.
* [ ] Determine how to install OpenOCD 0.12.0+dev on Ubuntu 24.04 LTS, for RP2350 support.

## Prerequisites and Presumptions

This is written presuming use of Linux Ubuntu 24.04 LTS.
Also presumes CMake is building the RP2040 firmware into
subdirectory `/build_rp2040`.

* OpenOCD 


## Commands to use / Walkthrough

At least three shell windows required.

### Shell #1 -- OpenOCD server

1. Start OpenOCD

    `openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000"`


For BP5XL / BP6 (RP2350 based boards), a pre-release version
of OpenOCD is required.  For me, it was
necessary to run
from the installed scripts folder. As a result, programming required
a full path to the `elf` firmware binary.  YMMV.


```
pushd ~/.pico-sdk/openocd/0.12.0+dev/scripts

../openocd -f ./interface/cmsis-dap.cfg -f ./target/rp2350.cfg -c "adapter speed 5000"
```

### Shell #2 -- Interacting with OpenOCD

1. Connect to OpenOCD
`telnet localhost 4444`
2. Optionally, program the newly-built binary via OpenOCD
`program ./build_rp2040/src/bus_pirate5_rev10.elf`
3. Reset and halt the cores
`reset halt`
4. deassert reset on core1, then on core0
```
rp2040.core1 arp_reset assert 0
rp2040.core0 arp_reset assert 0
```
5. sleep to allow firmware to initialize RTT structure
`sleep 500`
6. Setup and start `rtt`
```
rtt setup 0x20000000 0x100000 "SEGGER RTT"
rtt start
```
7. Start the rtt server, with one port per channel
`rtt server start 4321 0`

<details><summary>As a single script...</summary><P/>

```
reset halt
rtt stop
program ./build_rp2040/src/bus_pirate5_rev10.elf
reset halt
rp2040.core1 arp_reset assert 0
rp2040.core0 arp_reset assert 0
sleep 500
rtt setup 0x20000000 0x100000 "SEGGER RTT"
rtt start

rtt server start 4321 0

```

</details>

### Shell #2 for RP2350

This requires a pre-release version of OpenOCD 0.12.0

<details><summary>As a single script...</summary><P/>

```
reset halt
rtt stop
program /home/henrygab/build_rp2350/src/bus_pirate6.elf
reset halt
rp2350.dap.core1 arp_reset assert 0
rp2350.dap.core0 arp_reset assert 0
sleep 500
rtt setup 0x20000000 0x100000 "SEGGER RTT"
rtt start

rtt server start 4321 0

```

</details>

### Shells #3 and higher -- View RTT output

Connect to RTT channel 0
`telnet localhost 4321`

Note: Channel 0 is technically bi-directional, but currently only used for output.

## Conclusion

That's it!  RTT output should now be streaming to those last shell(s).
