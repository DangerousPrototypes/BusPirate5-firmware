/*
 * Copyright (c) 2021 Stefan Althöfer
 * SPDX-License-Identifier: BSD-3-Clause (as indicated in the PIO file)
 * https://github.com/stefanalt/RP2040-PIO-1-Wire-Master
 *
 * 1-Wire is a tradmark of Maxim Integrated
 * 2023 Modified by Ian Lesnet for Bus Pirate 5 buffered IO
 *
 * OWxxxx functions might fall under Maxxim copyriht.
 *
 * Demo code for PIO 1-Wire interface
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "opt_args.h"
#include "hardware/pio.h"
#include "pirate/hw1wire_pio.h"
#include "ui/ui_help.h"

static const char* const usage[] = {
    "scan\t[-h(elp)]",
    "Scan 1-Wire address space: scan",
};

static const struct ui_help_options options[] = {
    { 1, "", T_HELP_1WIRE_SCAN }, // command help
    { 0, "-h", T_HELP_FLAG },     // help
};

// device list from: http://owfs.sourceforge.net/commands.html
void ds1wire_id(unsigned char famID) {
    switch (famID) {
        // check for device type
        case 0x01:
            printf("DS1990A Silicon Serial Number");
            break;
        case 0x02:
            printf("DS1991 multikey 1153bit secure");
            break;
        case 0x04:
            printf("DS1994 econoram time chip");
            break;
        case 0x05:
            printf("Addressable Switch");
            break;
        case 0x06:
            printf("DS1993 4K memory ibutton");
            break;
        case 0x08:
            printf("DS1992 1K memory ibutton");
            break;
        case 0x09:
            printf("DS1982 1K add-only memory");
            break;
        case 0x0A:
            printf("DS1995 16K memory ibutton");
            break;
        case 0x0B:
            printf("DS1985 16K add-only memory");
            break;
        case 0x0C:
            printf("DS1996 64K memory ibutton");
            break;
        case 0x0F:
            printf("DS1986 64K add-only memory");
            break;
        case 0x10:
            printf("DS1920 high precision digital thermometer");
            break;
        case 0x12:
            printf("Dual switch + 1K RAM");
            break;
        case 0x14:
            printf("DS1971 256byte EEPROM");
            break;
        case 0x1A:
            printf("DS1963L 4K Monetary");
            break;
        case 0x1C:
            printf("4K EEPROM withPIO");
            break;
        case 0x1D:
            printf("4K RAM with counter");
            break;
        case 0x1F:
            printf("Microlan coupler");
            break;
        case 0x20:
            printf("Quad ADC");
            break;
        case 0x21:
            printf("DS1921 Thermachron");
            break;
        case 0x22:
            printf("Econo Digital Thermometer");
            break;
        case 0x23:
            printf("4K EEPROM");
            break;
        case 0x24:
            printf("Time chip");
            break;
        case 0x26:
            printf("Smart battery monitor");
            break;
        case 0x27:
            printf("Time chip with interrupt");
            break;
        case 0x28:
            printf("DS18B20 digital thermometer");
            break;
        case 0x29:
            printf("8-channel addressable switch");
            break;
        case 0x2C:
            printf("Digital potentiometer");
            break;
        case 0x2D:
            printf("DS2431 1K EEPROM");
            break;
        case 0x2E:
            printf("battery monitor and charge controller");
            break;
        case 0x30:
            printf("Precision li+ battery monitor");
            break;
        case 0x31:
            printf("Rechargable lithium protection IC");
            break;
        case 0x33:
            printf("DS1961S 1k protected EEPROM with SHA-1");
            break;
        case 0x36:
            printf("High precision coulomb counter");
            break;
        case 0x37:
            printf("DS1977 Password protected 32K EEPROM");
            break;
        case 0x41:
            printf("DS1922/3 Temperature Logger 8K mem");
            break;
        case 0x51:
            printf("Multichemistry battery fuel gauge");
            break;
        case 0x84:
            printf("Dual port plus time");
            break;
        case 0x89:
            printf("DS1982U 48bit node address chip");
            break;
        case 0x8B:
            printf("DS1985U 16K add-only uniqueware");
            break;
        case 0x8F:
            printf("DS1986U 64K add-only uniqueware");
            break;
        default:
            printf("Unknown device");
    }
}

/* ROM Search test */
#define TRUE 1
#define FALSE 0
void onewire_test_romsearch(struct command_result* res) {
    // check help
    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
        return;
    }

    int i;
    int ret;
    int devcount;
    struct owobj search_owobj;

    /* Full  romsearch */
    printf("1-Wire ROM search:\r\n");
    char* romno;
    ret = OWFirst(&search_owobj);
    devcount = 0;
    while (ret == TRUE) {
        devcount++;
        printf("%d:", devcount);
        for (i = 0; i < 8; i++) {
            printf(" %.2x", search_owobj.ROM_NO[i]);
        }
        printf(" (");
        ds1wire_id(search_owobj.ROM_NO[0]);
        printf(")\r\n");
        ret = OWNext(&search_owobj);
    }
    if (devcount == 0) {
        printf("No devices found\r\n");
    }
}