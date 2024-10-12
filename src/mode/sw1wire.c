

// took 1wire implementation from old buspirate
// fiddled a bit with the timings to make it work on the NG version

#include <stdint.h>
#include <libopencm3/stm32/gpio.h>
#include "buspirateNG.h"
#include "1WIRE.h"
#include "cdcacm.h"
#include "UI.h"

// the roster stores the first OW_DEV_ROSTER_SLOTS 1-wire addresses found during a ROM SEARCH command
// these addresses are available as MACROs for quick address entry
#define OW_DEV_ROSTER_SLOTS 10 // how many devices max to store addresses as MACROs
struct _OWID {
    //      unsigned char familyID; //to lazy to do it right, for now...
    unsigned char id[8];
    //      unsigned char crc;
};

struct _OWIDREG {
    unsigned char num;
    struct _OWID dev[OW_DEV_ROSTER_SLOTS];
};

struct _OWIDREG OWroster;

#define TRUE 1
#define FALSE 0

// because 1wire uses bit times, setting the data line high or low with (_-) has no effect
// we have to save the desired bus state, and then clock in the proper value during a clock(^)
static unsigned char DS1wireDataState = 0; // data bits are low by default.

// global search state,
// these lovely globals are provided courtesy of MAXIM's code
// need to be put in a struct....
unsigned char ROM_NO[8];
unsigned char SearchChar = 0xf0; // toggle between ROM and ALARM search types
unsigned char LastDiscrepancy;
unsigned char LastFamilyDiscrepancy;
unsigned char LastDeviceFlag;
unsigned char crc8;

void ONEWIRE_start(void) {
    printf("ONEWIRE start()");
}
void ONEWIRE_startr(void) {
    printf("ONEWIRE startr()");
}
void ONEWIRE_stop(void) {
    printf("ONEWIRE stop()");
}
void ONEWIRE_stopr(void) {
    printf("ONEWIRE stopr()");
}
uint32_t ONEWIRE_send(uint32_t d) {
    OWWriteByte(d);

    return 0;
}
uint32_t ONEWIRE_read(void) {
    return (OWReadByte());
}
void ONEWIRE_clkh(void) {
    printf("ONEWIRE clkh()");
}
void ONEWIRE_clkl(void) {
    printf("ONEWIRE clkl()");
}
void ONEWIRE_dath(void) {
    DS1wireDataState = 1;
    printf(" *next clock (^) will use this value\r\n");
}
void ONEWIRE_datl(void) {
    DS1wireDataState = 0;
    printf(" *next clock (^) will use this value\r\n");
}
uint32_t ONEWIRE_dats(void) {
    return DS1wireDataState;
}
void ONEWIRE_clk(void) {
    OWWriteBit(DS1wireDataState);
}
uint32_t ONEWIRE_bitr(void) {
    return (OWReadBit());
}
uint32_t ONEWIRE_period(void) {
    return 0;
}
void ONEWIRE_macro(uint32_t macro) {
    unsigned char c, j;
    unsigned int i;
    unsigned char devID[8];

    if (macro > 0 && macro < 51) {
        macro--;                     // adjust down one for roster array index
        if (macro >= OWroster.num) { // no device #X on the bus, try ROM SEARCH (0xF0)
            printf("No device, try (ALARM) SEARCH macro first\r\n");
            return;
        }
        // write out the address of the device in the macro
        printf("ADDRESS MACRO %d: ", macro + 1);
        for (j = 0; j < 8; j++) {
            printf("%02X ", OWroster.dev[macro].id[j]);
            OWWriteByte(OWroster.dev[macro].id[j]);
        } // write address
        printf("\r\n");

        return;
    }
    switch (macro) {
        case 0: // menu
            printf(" 0.Macro menu\r\n");
            printf("Macro\t1WIRE address\r\n");
            // write out roster of devices and macros, or SEARCH ROM NOT RUN, TRY (0xf0)
            if (OWroster.num == 0) {
                printf("No device, try (ALARM) SEARCH macro first\r\n");
            } else {
                for (c = 0; c < OWroster.num; c++) {
                    printf(" %d.\t", c + 1);
                    for (j = 0; j < 8; j++) {
                        printf("%02X ", OWroster.dev[c].id[j]);
                    }
                    printf("   *");
                    DS1wireID(OWroster.dev[c].id[0]); // print the device family identity (if known)
                }
            }
            printf("1WIRE ROM COMMAND MACROs:\r\n");
            printf(" 51. READ ROM (0x33) *for single device bus\r\n");
            printf(" 85. MATCH ROM (0x55) *followed by 64bit address\r\n");
            printf(" 204.SKIP ROM (0xCC) *followed by command\r\n");
            printf(" 236.ALARM SEARCH (0xEC)\r\n");
            printf(" 240.SEARCH ROM (0xF0)\r\n");
            printf(" 255.reset bus");
            break;
        // 1WIRE ROM COMMANDS
        case 0xec: // ALARM SEARCH
        case 0xf0: // SEARCH ROM
            SearchChar = macro;
            if (macro == 0xec) {
                printf("ALARM SEARCH (0xEC)\r\n");
            } else { // SEARCH ROM command...
                printf("SEARCH (0xF0)\r\n");
            }

            printf("Macro\t1WIRE address\r\n");
            // find ALL devices
            j = 0;
            c = OWFirst();
            OWroster.num = 0;
            while (c) {
                printf(" %d.\t", j);

                // print address
                for (i = 0; i < 8; i++) {
                    printf("%02X ", ROM_NO[i]);
                }
                printf("\t*");
                DS1wireID(ROM_NO[0]); // print the device family identity (if known)

                // keep the first X number of one wire IDs in a roster
                // so we can refer to them by macro, rather than ID
                if (j < OW_DEV_ROSTER_SLOTS) { // only as many as we have room for
                    for (i = 0; i < 8; i++) {
                        OWroster.dev[OWroster.num].id[i] = ROM_NO[i];
                    }
                    OWroster.num++; // increment the roster count
                }

                j++;

                c = OWNext();
            }

            printf("Device IDs are available by MACRO, see (0).\r\n");
            break;
        case 0x33: // READ ROM
            DS1wireReset();
            printf("READ ROM (0x33): ");
            OWWriteByte(0x33);
            for (i = 0; i < 8; i++) {
                devID[i] = OWReadByte();
                printf("%02X ", devID[i]);
            }
            printf("\r\n");
            DS1wireID(devID[0]);
            break;
        case 0x55: // MATCH ROM
            DS1wireReset();
            printf("MATCH ROM (0x55)\r\n");
            OWWriteByte(0x55);
            break;
        case 0xcc: // SKIP ROM
            DS1wireReset();
            printf("SKIP ROM (0xCC)\r\n");
            OWWriteByte(0xCC);
            break;
        case 0xff:
            DS1wireReset();
            break;
        case 0x100:
            gpio_set(BP_1WIRE_PORT, BP_1WIRE_PIN);
            break;
        case 0x101:
            gpio_clear(BP_1WIRE_PORT, BP_1WIRE_PIN);
            break;
        default:
            printf("No such macro\r\n");
            modeConfig.error = 1;
    }
}
void ONEWIRE_setup(void) {
    printf("ONEWIRE setup()");
}
void ONEWIRE_setup_exc(void) {
    modeConfig.oc = 1; // yes, always opencollector
    OWroster.num = 0;  // clear any old 1-wire bus enumeration rosters

    gpio_set_mode(BP_1WIRE_SENSE_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, BP_1WIRE_SENSE_PIN);
    gpio_set_mode(BP_1WIRE_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_OPENDRAIN, BP_1WIRE_PIN);
    gpio_set(BP_1WIRE_PORT, BP_1WIRE_PIN);
}
void ONEWIRE_cleanup(void) {
    gpio_set_mode(BP_1WIRE_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, BP_1WIRE_PIN);
    gpio_set_mode(BP_1WIRE_SENSE_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, BP_1WIRE_SENSE_PIN);
}
void ONEWIRE_pins(void) {
    printf("pin1\tpin2\tpin3\tpin4");
}
void ONEWIRE_settings(void) {
    printf("1WIRE ()=()");
}

/* ***********************************************************************************
   Function: OWReset
   Args[0]: Void
   Return: Made it mimic the old source.
        0 = OK
        1 = Short
        2 = No device.

   Desc: OneWire reset bus procedure. See return values for details.
*********************************************************************************** */
unsigned char OWReset(void) {
    unsigned int Presence = 0;

    gpio_set(BP_1WIRE_PORT, BP_1WIRE_PIN);
    gpio_clear(BP_1WIRE_PORT, BP_1WIRE_PIN); // go low

    // Maxim says a minimum of 480.
    delayus(490);

    gpio_set(BP_1WIRE_PORT, BP_1WIRE_PIN);
    delayus(65);

    if (gpio_get(BP_1WIRE_SENSE_PORT, BP_1WIRE_SENSE_PIN)) { // if lines still high, then no device
        Presence = 2;                                        // if no device then return 2.
    }

    delayus(500);

    if (!gpio_get(BP_1WIRE_SENSE_PORT, BP_1WIRE_SENSE_PIN)) // if lines still low; then theres a short
    {
        return 1;
    }

    return Presence;
}

/* ***********************************************************************************
   Function: OWBit
   Args[0]: uChar [Bit to send. Logic 1 or 0] - or 1 to recieve
   Return: Returns bit value on bus

   Desc: OWBit works as both a sending and reciving of 1 bit value on the OWBus.
         To get a bit value send a logical 1 (OWBit 1) This will pulse the line
         as needed then release the bus and wait a few US before sampling if the
         OW device has sent data.
*********************************************************************************** */
unsigned char OWBit(unsigned char OWbit) {
    gpio_set(BP_1WIRE_PORT, BP_1WIRE_PIN);
    gpio_clear(BP_1WIRE_PORT, BP_1WIRE_PIN);

    delayus(5); // maxim says 6
    if (OWbit) {
        gpio_set(BP_1WIRE_PORT, BP_1WIRE_PIN);
    }

    delayus(8); // maxim says 9

    if (OWbit) { // This is where the magic happens. If a OWbit value of 1 is sent to this function
        OWbit = (gpio_get(BP_1WIRE_SENSE_PORT, BP_1WIRE_SENSE_PIN)
                     ? 1
                     : 0); // well thats the same timing needed to get a value (not just send it) so why not
        delayus(44);       // maxim says 45						// perform both? So sending one will not only send 1 bit; it
                           // will also read one bit
    } else {               // it all depends on what the iDevice is in the mood to do. If its in send mode then
        delayus(44);       // maxim says 45						// it will sends its data, if its in recive mode. Then we
                           // will send ours.
        gpio_set(BP_1WIRE_PORT, BP_1WIRE_PIN); // magical, i know :)
    }

    delayus(9); // maxim says 9

    return OWbit;
}

/* ***********************************************************************************
   Function: OWByte
   Args[0]: uChar [Byte to send to bus] - or 0xFF to recieve
   Return: Returns a byte from the OWBus

   Desc: Like OWBit; OWByte works for both sending and getting. OWTime slots are the same
         for sending and getting; so only one function is needed to perform both tasks.

*********************************************************************************** */
unsigned char OWByte(unsigned char OWbyte) {
    unsigned char i,
        t = 0; // nothing much to say about this function. pretty standard; do this 8 times and collect results.
               // except that sends and GETS data. Sending a value of 0xFF will have the OWBit function return
    for (i = 0; i < 8;
         i++) // bits; this will collect those returns and spit them out. Same with send. It all depends on
    {         // what the iDevice is looking for at the time this command is sent.
        t = OWBit(OWbyte & 1);
        OWbyte >>= 1;
        if (t) {
            OWbyte |= 0x80;
        }
    }
    delayus(10);
    return OWbyte;
}

void DS1wireReset(void) {
    unsigned char c;

    c = OWReset();
    printf("BUS RESET ");
    if (c == 0) {
        printf(" OK\r\n");
    } else {
        printf("Warning: ");
        if (c & 0b1) {
            printf("*Short or no pull-up \r\n");
        }
        if (c & 0b10) {
            printf("*No device detected \r\n");
        }
    }
}

// device list from: http://owfs.sourceforge.net/commands.html
void DS1wireID(unsigned char famID) {
    switch (famID) { // check for device type
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
            printf("Addresable Switch");
            break;
        case 0x06:
            printf("DS1993 4k memory ibutton");
            break;
        case 0x08:
            printf("DS1992 1k memory ibutton");
            break;
        case 0x09:
            printf("DS1982 1k add-only memory");
            break;
        case 0x0A:
            printf("DS1995 16k memory ibutton");
            break;
        case 0x0B:
            printf("DS1985 16k add-only memory");
            break;
        case 0x0C:
            printf("DS1996 64k memory ibutton");
            break;
        case 0x0F:
            printf("DS1986 64k add-only  memory");
            break;
        case 0x10:
            printf("DS1920 high precision digital thermometer");
            break;
        case 0x12:
            printf("dual addressable switch plus 1k memory");
            break;
        case 0x14:
            printf("DS1971 256 eeprom");
            break;
        case 0x1A:
            printf("DS1963L 4k Monetary");
            break;
        case 0x1C:
            printf("4k EEPROM withPIO");
            break;
        case 0x1D:
            printf("4k ram with counter");
            break;
        case 0x1F:
            printf("microlan coupler");
            break;
        case 0x20:
            printf("quad a/d converter");
            break;
        case 0x21:
            printf("DS1921 Thermachron");
            break;
        case 0x22:
            printf("Econo Digital Thermometer");
            break;
        case 0x23:
            printf("4k eeprom");
            break;
        case 0x24:
            printf("time chip");
            break;
        case 0x26:
            printf("smart battery monitor");
            break;
        case 0x27:
            printf("time chip with interrupt");
            break;
        case 0x28:
            printf("programmable resolution digital thermometer");
            break;
        case 0x29:
            printf("8-channel addressable switch");
            break;
        case 0x2C:
            printf("digital potentiometer");
            break;
        case 0x2D:
            printf("1k eeprom");
            break;
        case 0x2E:
            printf("battery monitor and charge controller");
            break;
        case 0x30:
            printf("high-precision li+ battery monitor");
            break;
        case 0x31:
            printf("efficient addressable single-cell rechargable lithium protection ic");
            break;
        case 0x33:
            printf("DS1961S 1k protected eeprom with SHA-1");
            break;
        case 0x36:
            printf("high precision coulomb counter");
            break;
        case 0x37:
            printf("DS1977 Password protected 32k eeprom");
            break;
        case 0x41:
            printf("DS1922/3 Temperature Logger 8k mem");
            break;
        case 0x51:
            printf("multichemistry battery fuel gauge");
            break;
        case 0x84:
            printf("dual port plus time");
            break;
        case 0x89:
            printf("DS1982U 48 bit node address chip");
            break;
        case 0x8B:
            printf("DS1985U 16k add-only uniqueware");
            break;
        case 0x8F:
            printf("DS1986U 64k add-only uniqueware");
            break;
        default:
            printf("Unknown device");
    }
    printf("\r\n");
}

// the 1-wire search algo taken from:
// http://www.maxim-ic.com/appnotes.cfm/appnote_number/187
// #define TRUE 1 //if !=0
// #define FALSE 0

//--------------------------------------------------------------------------
// Find the 'first' devices on the 1-Wire bus
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : no device present
//
unsigned char OWFirst(void) {
    // reset the search state
    LastDiscrepancy = 0;
    LastDeviceFlag = FALSE;
    LastFamilyDiscrepancy = 0;

    return OWSearch();
}

//--------------------------------------------------------------------------
// Find the 'next' devices on the 1-Wire bus
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : device not found, end of search
//
unsigned char OWNext(void) {
    // leave the search state alone
    return OWSearch();
}

//--------------------------------------------------------------------------
// Perform the 1-Wire Search Algorithm on the 1-Wire bus using the existing
// search state.
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : device not found, end of search
//
unsigned char OWSearch(void) {
    unsigned char id_bit_number;
    unsigned char last_zero, rom_byte_number, search_result;
    unsigned char id_bit, cmp_id_bit;
    unsigned char rom_byte_mask, search_direction;

    // initialize for search
    id_bit_number = 1;
    last_zero = 0;
    rom_byte_number = 0;
    rom_byte_mask = 1;
    search_result = 0;
    crc8 = 0;

    // if the last call was not the last one
    if (!LastDeviceFlag) {
        // 1-Wire reset
        if (OWReset()) {
            // reset the search
            LastDiscrepancy = 0;
            LastDeviceFlag = FALSE;
            LastFamilyDiscrepancy = 0;
            return FALSE;
        }

        // issue the search command
        OWWriteByte(SearchChar); //!!!!!!!!!!!!!!!

        // loop to do the search
        do {
            // read a bit and its complement
            id_bit = OWReadBit();
            cmp_id_bit = OWReadBit();

            // check for no devices on 1-wire
            if ((id_bit == 1) && (cmp_id_bit == 1)) {
                break;
            } else {
                // all devices coupled have 0 or 1
                if (id_bit != cmp_id_bit) {
                    search_direction = id_bit; // bit write value for search
                } else {
                    // if this discrepancy if before the Last Discrepancy
                    // on a previous next then pick the same as last time
                    if (id_bit_number < LastDiscrepancy) {
                        search_direction = ((ROM_NO[rom_byte_number] & rom_byte_mask) > 0);
                    } else {
                        // if equal to last pick 1, if not then pick 0
                        search_direction = (id_bit_number == LastDiscrepancy);
                    }

                    // if 0 was picked then record its position in LastZero
                    if (search_direction == 0) {
                        last_zero = id_bit_number;

                        // check for Last discrepancy in family
                        if (last_zero < 9) {
                            LastFamilyDiscrepancy = last_zero;
                        }
                    }
                }

                // set or clear the bit in the ROM byte rom_byte_number
                // with mask rom_byte_mask
                if (search_direction == 1) {
                    ROM_NO[rom_byte_number] |= rom_byte_mask;
                } else {
                    ROM_NO[rom_byte_number] &= ~rom_byte_mask;
                }

                // serial number search direction write bit
                OWWriteBit(search_direction);

                // increment the byte counter id_bit_number
                // and shift the mask rom_byte_mask
                id_bit_number++;
                rom_byte_mask <<= 1;

                // if the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask
                if (rom_byte_mask == 0) {
                    docrc8(ROM_NO[rom_byte_number]); // accumulate the CRC
                    rom_byte_number++;
                    rom_byte_mask = 1;
                }
            }
        } while (rom_byte_number < 8); // loop until through all ROM bytes 0-7

        // if the search was successful then
        if (!((id_bit_number < 65) || (crc8 != 0))) {
            // search successful so set LastDiscrepancy,LastDeviceFlag,search_result
            LastDiscrepancy = last_zero;

            // check for last device
            if (LastDiscrepancy == 0) {
                LastDeviceFlag = TRUE;
            }

            search_result = TRUE;
        }
    }

    // if no device found then reset counters so next 'search' will be like a first
    if (!search_result || !ROM_NO[0]) {
        LastDiscrepancy = 0;
        LastDeviceFlag = FALSE;
        LastFamilyDiscrepancy = 0;
        search_result = FALSE;
    }

    return search_result;
}

//--------------------------------------------------------------------------
// Verify the device with the ROM number in ROM_NO buffer is present.
// Return TRUE  : device verified present
//        FALSE : device not present
//
unsigned char OWVerify(void) {
    unsigned char rom_backup[8];
    unsigned char i, rslt, ld_backup, ldf_backup, lfd_backup;

    // keep a backup copy of the current state
    for (i = 0; i < 8; i++) {
        rom_backup[i] = ROM_NO[i];
    }
    ld_backup = LastDiscrepancy;
    ldf_backup = LastDeviceFlag;
    lfd_backup = LastFamilyDiscrepancy;

    // set search to find the same device
    LastDiscrepancy = 64;
    LastDeviceFlag = FALSE;

    if (OWSearch()) {
        // check if same device found
        rslt = TRUE;
        for (i = 0; i < 8; i++) {
            if (rom_backup[i] != ROM_NO[i]) {
                rslt = FALSE;
                break;
            }
        }
    } else {
        rslt = FALSE;
    }

    // restore the search state
    for (i = 0; i < 8; i++) {
        ROM_NO[i] = rom_backup[i];
    }
    LastDiscrepancy = ld_backup;
    LastDeviceFlag = ldf_backup;
    LastFamilyDiscrepancy = lfd_backup;

    // return the result of the verify
    return rslt;
}

// TEST BUILD
static unsigned char dscrc_table[] = {
    0,   94,  188, 226, 97,  63,  221, 131, 194, 156, 126, 32,  163, 253, 31,  65,  157, 195, 33,  127, 252, 162,
    64,  30,  95,  1,   227, 189, 62,  96,  130, 220, 35,  125, 159, 193, 66,  28,  254, 160, 225, 191, 93,  3,
    128, 222, 60,  98,  190, 224, 2,   92,  223, 129, 99,  61,  124, 34,  192, 158, 29,  67,  161, 255, 70,  24,
    250, 164, 39,  121, 155, 197, 132, 218, 56,  102, 229, 187, 89,  7,   219, 133, 103, 57,  186, 228, 6,   88,
    25,  71,  165, 251, 120, 38,  196, 154, 101, 59,  217, 135, 4,   90,  184, 230, 167, 249, 27,  69,  198, 152,
    122, 36,  248, 166, 68,  26,  153, 199, 37,  123, 58,  100, 134, 216, 91,  5,   231, 185, 140, 210, 48,  110,
    237, 179, 81,  15,  78,  16,  242, 172, 47,  113, 147, 205, 17,  79,  173, 243, 112, 46,  204, 146, 211, 141,
    111, 49,  178, 236, 14,  80,  175, 241, 19,  77,  206, 144, 114, 44,  109, 51,  209, 143, 12,  82,  176, 238,
    50,  108, 142, 208, 83,  13,  239, 177, 240, 174, 76,  18,  145, 207, 45,  115, 202, 148, 118, 40,  171, 245,
    23,  73,  8,   86,  180, 234, 105, 55,  213, 139, 87,  9,   235, 181, 54,  104, 138, 212, 149, 203, 41,  119,
    244, 170, 72,  22,  233, 183, 85,  11,  136, 214, 52,  106, 43,  117, 151, 201, 74,  20,  246, 168, 116, 42,
    200, 150, 21,  75,  169, 247, 182, 232, 10,  84,  215, 137, 107, 53
};

//--------------------------------------------------------------------------
// Calculate the CRC8 of the byte value provided with the current
// global 'crc8' value.
// Returns current global crc8 value
//
unsigned char docrc8(unsigned char value) {
    // See Application Note 27
    // TEST BUILD
    crc8 = dscrc_table[crc8 ^ value];
    return crc8;
}
