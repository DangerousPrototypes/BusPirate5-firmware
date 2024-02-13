/*
 * This file is part of the Bus Pirate project (http://code.google.com/p/the-bus-pirate/).
 *
 * Written and maintained by the Bus Pirate project.
 *
 * To the extent possible under law, the project has
 * waived all copyright and related or neighboring rights to Bus Pirate. This
 * work is published from United States.
 *
 * For details see: http://creativecommons.org/publicdomain/zero/1.0/.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

/* Binary access modes for Bus Pirate scripting */

//#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "queue.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "bio.h"
#include "system_config.h"
#include "bytecode.h" //needed because modes.h has some functions that use it TODO: move all the opt args and bytecode stuff to a single helper file
#include "pirate.h"
#include "opt_args.h" //needed for same reason as bytecode and needs same fix
#include "commands.h"
#include "modes.h"
#include "mode/binio.h"
#include "pullups.h"
#include "psu.h"
#include "amux.h"
#include "sump.h"
#include "binio_helpers.h"
#include "tusb.h"

unsigned char binBBpindirectionset(unsigned char inByte);
unsigned char binBBpinset(unsigned char inByte);
void binBBversion(void);
void binSelfTest(unsigned char jumperTest);
void binReset(void);
unsigned char getRXbyte(void);

/*
Bitbang is like a player piano or bitmap. The 1 and 0 represent the pins. 
So for the four Bus Pirate pins we use the the bits as follows:
COMMAND|POWER|PULLUP|AUX|CS|MISO|CLK|MOSI.

The Bus pirate also responds to each write with a read byte showing the current state of the pins.

The bits control the state of each of those pins when COMMAND=1. 
When COMMAND=0 then up to 127 command codes can be entered on the lower bits.
0x00 resets the Bus Pirate to bitbang mode.

Data:
1xxxxxxx //COMMAND|POWER|PULLUP|AUX|MOSI|CLK|MISO|CS

Commands:
00000000 //Reset to raw BB mode, get raw BB version string
00000001 //enter rawSPI mode
00000010 //enter raw I2C mode
00000011 //enter raw UART mode
00000100 // enter raw 1-wire
00000101 //enter raw wire mode
00000110 // enter openOCD
00000111 // pic programming mode
00001111 //reset, return to user terminal
00010000 //short self test
00010001 //full self test with jumpers
00010010 // setup PWM
00010011 // clear PWM
00010100 // ADC measurement

// Added JM  Only with BP4
00010101 // ADC ....
00010110 // ADC Stop
00011000 // XSVF Player
00011111 // Debug text to terminal display
// End added JM
//
010xxxxx //set input(1)/output(0) pin state (returns pin read)
 */
void binBBversion(void) 
{
    //const char version_string[]="BBIO1";
    script_print("BBIO1");
}

void script_enabled(void)
{
    printf("\r\nScripting mode enabled. Terminal locked.\r\n");
}

void script_disabled(void)
{
    printf("\r\nTerminal unlocked.\r\n");     //fall through to prompt 
}

bool script_entry(void)
{
    static uint8_t binmodecnt=0;
    char c;

    if(tud_cdc_n_available(1)) return true; else return false;

    while(tud_cdc_n_available(1))
    {

        switch(c)
        {
            case 0x00:
                    script_enabled();
                    system_config.binmode=true;
                    sump_logic_analyzer();
                    system_config.binmode=false;
                    script_disabled();  
                              
                binmodecnt++;
                if(binmodecnt>=20)
                {   
                    system_config.binmode=true;
                    binmodecnt=0;
                    return true;
                }
                break;                
            case 0x02: //test for SUMP client
                if(binmodecnt >= 5) 
                {
                    script_enabled();
                    system_config.binmode=true;
                    sump_logic_analyzer();
                    system_config.binmode=false;
                    script_disabled();
                } 
                binmodecnt = 0; //reset counter
                break;
            default:
                binmodecnt = 0;
                break;
        }

    }  

    return false;
}

bool script_mode(void) 
{
    static unsigned char inByte;
    unsigned int i;
    char c;

    system_config.binmode=true;
    sump_logic_analyzer();
    system_config.binmode=false;
    return false;

    // co-op multitask while checking for the binmode flag
    // then take over and block the user terminal
    //if(!script_entry()) return false;

    script_enabled();
 
    binReset();
    binBBversion(); //send mode name and version

    while(1) 
    {
        inByte = getRXbyte();

        if ((inByte & 0b10000000) == 0) 
        {   //if command bit cleared, process command
            if (inByte == 0) 
            {   //reset, send BB version
                binBBversion();
            } 
            else if (inByte == 1) 
            {   //goto SPI mode
                binReset();
#ifdef TBP_USE_HWSPI
                binSPI(); //go into rawSPI loop
#endif
                binReset();
                binBBversion(); //say name on return
            } else if (inByte == 2) {//goto I2C mode
                binReset();
#ifdef TBP_USE_HWI2C
                binI2C();
#endif
                binReset();
                binBBversion(); //say name on return
            } else if (inByte == 3) {//goto UART mode
                binReset();
#ifdef TBP_USE_HWUART
                binUART();
#endif
                binReset();
                binBBversion(); //say name on return
            } else if (inByte == 4) {//goto 1WIRE mode
                binReset();
#ifdef TBP_USE_1WIRE
                bin1WIRE();
#endif
                binReset();
                binBBversion(); //say name on return
            } else if (inByte == 5) {//goto RAW WIRE mode
                binReset();
                //binwire();
                binReset();
                binBBversion(); //say name on return
            } else if (inByte == 6) {//goto OpenOCD mode
                binReset();
#ifdef TBP_USE_OPENOCD
                binOpenOCD();
#endif
                binReset();
                binBBversion(); //say name on return
            } else if (inByte == 7) {//goto pic mode
                binReset();
#ifdef TBP_USE_PIC
                binpic();
#endif
                binReset();
                binBBversion(); //say name on return
            } 
            else if (inByte == 0b1111) 
            {//return to terminal
                bin_tx_fifo_put(1);
                system_config.binmode=false;
                binReset(); //disable any hardware
                system_bio_claim(false, BP_ADC_PROBE, BP_PIN_MODE, 0);
                system_bio_claim(false, BP_AUX0, BP_PIN_MODE, 0);
                system_bio_claim(false, BP_MOSI, BP_PIN_MODE, 0);
                system_bio_claim(false, BP_CLK, BP_PIN_MODE, 0);
                system_bio_claim(false, BP_MISO, BP_PIN_MODE, 0);
                system_bio_claim(false, BP_CS, BP_PIN_MODE, 0);		
                script_disabled();
                return true;
                //while(queue_available_bytes()) //old version waits for empty TX before exit
                //self test is only for v2go and v3
            } else if (inByte == 0b10000) {//short self test
                //binSelfTest(0);
            } else if (inByte == 0b10001) {//full self test with jumpers
                //binSelfTest(1);
            } else if (inByte == 0b10010) {//setup PWM

                //cleanup timers from FREQ measure
                //T2CON = 0; //16 bit mode
                //T4CON = 0;
                //OC5CON = 0; //clear PWM settings

                //BP_AUX_RPOUT = OC5_IO; //setup pin

                //get one byte
                i = getRXbyte();
                //if (i & 0b10) T2CONbits.TCKPS1 = 1; //set prescalers
                //if (i & 0b1) T2CONbits.TCKPS0 = 1;

                //get two bytes
                i = (getRXbyte() << 8);
                i |= getRXbyte();
                //OC5R = i; //Write duty cycle to both registers
                //OC5RS = i;
                //OC5CON = 0x6; // PWM mode on OC, Fault pin disabled

                //get two bytes
                i = (getRXbyte() << 8);
                i |= getRXbyte();
                //PR2 = i; // write period

                //T2CONbits.TON = 1; // Start Timer2
                bin_tx_fifo_put(1);
            } else if (inByte == 0b10011) {//clear PWM
                //T2CON = 0; // stop Timer2
                //OC5CON = 0;
                //BP_AUX_RPOUT = 0; //remove output from AUX pin
                bin_tx_fifo_put(1);
                //ADC only for v1, v2, v3
            } 
            else if (inByte == 0b10100) 
            {   //ADC reading (x/1024)*6.6volts
                i = hw_adc_bio(BP_ADC_PROBE); //take measurement
                i=i>>2; //remove 2 bit of resolution to match old PIC 10 bit resolution
                bin_tx_fifo_put((i >> 8)); //send upper 8 bits
                bin_tx_fifo_put(i); //send lower 8 bits
            } 
            else if (inByte == 0b10101) 
            {   //ADC reading (x/1024)*6.6volts
                while (1) 
                {
                    i = hw_adc_bio(BP_ADC_PROBE); //take measurement
                    while(bin_tx_not_empty()); //this doesn't work as well with the USB stack
                    bin_tx_fifo_put((i >> 8)); //send upper 8 bits
                    bin_tx_fifo_put(i); //send lower 8 bits

                    if(bin_rx_fifo_try_get(&c)) 
                    {//any key pressed, exit
                        break;
                    }
                }
			}
            else if (inByte==0b10110)
            { //binary frequency count access
				unsigned long l;
				//l=bpBinFreq();
				bin_tx_fifo_put((l>>(8*3)));
				bin_tx_fifo_put((l>>(8*2)));
				bin_tx_fifo_put((l>>(8*1)));
				bin_tx_fifo_put((l));
	
            } 
            else if (inByte == 0b11000) 
            {   //XSVF Player to program CPLD
                //BP_VREGEN = 1;
                printf("XSV1");
		        //jtag();
            } 
            else if(inByte==0b11111)
            {   // Send null terminated string to terminal
                while(true)
                {
                    inByte=getRXbyte();
                    if(inByte==0x00)
                    {
                        bin_tx_fifo_put(1);
                        break;
                    }
                    printf("%c", inByte);
                }
            }
            else if ((inByte >> 5)&0b010) 
            {   //set pin direction, return read
                bin_tx_fifo_put(binBBpindirectionset(inByte));
            } 
            else 
            {   //unknown command, error
                bin_tx_fifo_put(0);
            }

        } 
        else 
        {   //data for pins
            bin_tx_fifo_put(binBBpinset(inByte));
        }//if
    }//while
}//function

unsigned char getRXbyte(void) 
{
    char c;
    bin_rx_fifo_get_blocking(&c);
    return c;
}


void binReset(void) 
{    
    modes[system_config.mode].protocol_cleanup();   // switch to HiZ
    modes[0].protocol_setup_exc();			        // disables power supply etc.

    //POWER|PULLUP|AUX|MOSI|CLK|MISO|CS
    static const char pin_labels[][5]=
    {
        "ADC",
        "AUX",
        "MOSI",
        "CLK",
        "MISO",
        "CS"
    };
    system_bio_claim(true, BP_ADC_PROBE, BP_PIN_MODE, pin_labels[0]);
    system_bio_claim(true, BP_AUX0, BP_PIN_MODE, pin_labels[1]);
	system_bio_claim(true, BP_MOSI, BP_PIN_MODE, pin_labels[2]);
	system_bio_claim(true, BP_CLK, BP_PIN_MODE, pin_labels[3]);
	system_bio_claim(true, BP_MISO, BP_PIN_MODE, pin_labels[4]);
	system_bio_claim(true, BP_CS, BP_PIN_MODE, pin_labels[5]);		

}



unsigned char port_read(unsigned char inByte)
{
    inByte &= (~0b00011111);
    if(bio_get(BP_AUX0))inByte |= 0b10000;
    if(bio_get(BP_MOSI))inByte |= 0b1000;
    if(bio_get(BP_CLK))inByte |= 0b100;
    if(bio_get(BP_MISO))inByte |= 0b10;
    if(bio_get(BP_CS))inByte |= 0b1;
    return inByte;
}

void set_pin_direction(uint8_t bio, uint8_t direction)
{
    if(direction)
    {
        bio_input(bio);
    }
    else
    {
        bio_output(bio);
    }
}

unsigned char binBBpindirectionset(unsigned char inByte) 
{
    unsigned char i;
    //setup pin in/out
    set_pin_direction(BP_AUX0, inByte & 0b10000);
    set_pin_direction(BP_MOSI, inByte & 0b1000);
    set_pin_direction(BP_CLK, inByte & 0b100);
    set_pin_direction(BP_MISO, inByte & 0b10);
    set_pin_direction(BP_CS, inByte & 0b1);

    //delay for a brief period
    busy_wait_us(5);
    return port_read(inByte); //return the read
}

unsigned char binBBpinset(unsigned char inByte) 
{

    if(inByte & 0b1000000) 
    {
        psu_set(3.3f, 500, false);
    }
    else 
    {
        psu_reset(); //power off
    }

    if(inByte & 0b100000)
    {
        pullups_enable_exc(); //pullups on
    }
    else
    {
        pullups_cleanup();
    }

    //set pin high/low
    bio_put(BP_AUX0, inByte & 0b10000);
    bio_put(BP_MOSI, inByte & 0b1000);
    bio_put(BP_CLK, inByte & 0b100);
    bio_put(BP_MISO, inByte & 0b10);
    bio_put(BP_CS, inByte & 0b1);

    //delay for a brief period
    busy_wait_us(5);

    //return PORT read
    return port_read(inByte); //return the read
}


void binSelfTest(unsigned char jumperTest) {
/*    static volatile unsigned int tick = 0;
    unsigned char errors, inByte;

    errors = selfTest(0, jumperTest); //silent self-test
    if (errors) BP_LEDMODE = 1; //light MODE LED if errors
    bin_tx_fifo_put(errors); //reply with number of errors

    while (1) {
        //echo incoming bytes + errors
        //tests FTDI chip, UART, retrieves results of test
        if (UART1RXRdy()) {
            inByte = UART1RX(); //check input
            if (inByte != 0xff) {
                bin_tx_fifo_put(inByte + errors);
            } else {
                bin_tx_fifo_put(0x01);
                return; //exit if we get oxff, else send back byte+errors
            }
        }

        if (!errors) {
            if (tick == 0) {
                tick = 0xFFFF;
                BP_LEDMODE ^= 1; //toggle LED
            }
            tick--;
        }

    }
*/
}

