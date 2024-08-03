#!/bin/sh
avrdude -carduino -P /dev/ttyACM0 -p m328p -Uflash:w:main.elf:e
