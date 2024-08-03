CH32V003 Arduino SWIO interface
===============================

This is a very simple implementation of a SWIO interface for the CH32V003
    microcontroller.

It is intended to run on a stock Arduino Uno R3 board running at 16 MHz.


Connections
-----------

The wiring is simple:

*   `AVR PB0` (Arduino pin __8__) goes to the SWIO pin of the CH32V003.

    While it'll work if connected directly, an 1k resistor in series is advised
    to prevent overcurrent, because a bus conflict is intentionally created as a
    part of the RX logic.

*   `AVR PB1` (Arduino pin __9__) is used to switch power to the target.

    Without external devices, the microcontroller's current draw should be
    low enough to be sustained by a simple GPIO.

Building and flashing the AVR
-----------------------------
You'll need avr-gcc and avrdude.
For the flashing script, the interface chosen is `/dev/ttyACM0`.

```
make
./flash.sh
````

Protocol
--------
The protocol runs over UART at 8N1 115200 bps.
    
The stock Arduino board features the ability to reset the AVR using the DTR
    line. You can use that to re-initialize the adapter whenever desired.

When it goes ready, a `!` character is output.

Most commands produce a `+` character to acknowledge their execution.

When arguments are required, they're transmitted as bytes following the command
    byte.

The supported commands are as follows:

* `?` - Test UART communication.
    * Arguments: none
    * Response: `+`

* `p` - Target power on.
    * Arguments: none
    * Response: `+`

* `P` - Target power off.
    * Arguments: none
    * Response: `+`

* `w` - Write target's debug register.
    * Arguments:
        * `1 byte` - debug register number (`0x00` ... `0x7f`)
        * `4 bytes` - value (LSB-first 32-bit int)
    * Response: `+`

* `r` - Read target's debug register.
    * Arguments:
        * `1 byte` - debug register number (`0x00` ... `0x7f`)

    * Response:
        * `4 bytes` - contents of the register.

