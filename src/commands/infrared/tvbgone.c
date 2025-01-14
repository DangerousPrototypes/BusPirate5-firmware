/*
This is a simple player for TVBGONE power codes.
Our player is simple because PIC C18 has easy 
reads from program memory. 

This code was written based on the description 
of the data packing method published on the Adafruit 
website. It should be a clean, black-box rewrite, but 
we'll release it as CC 2.5 Attrib & Share Alike 
out of respect for the original authors.

PIC C18 Player (c) Ian Lesnet 2009
for use with IR Toy v1.0 hardware.
http://dangerousprototypes.com

With credits to:

TV-B-Gone Firmware version 1.2
for use with ATtiny85v and v1.2 hardware
(c) Mitch Altman + Limor Fried 2009
Last edits, August 16 2009

With some code from:
Kevin Timmerman & Damien Good 7-Dec-07

Distributed under Creative Commons 2.5 -- Attib & Share Alike

Ported to PIC (18F2550) by Ian Lesnet 2009

Ported to RP2040 by Ian Lesnet 2024 (see you in another 15 years?)
*/

#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "tvbgone-codes.h" //include TVBGone code data
#include "bytecode.h"
#include "command_struct.h"
#include "mode/infrared-struct.h"
#include "mode/infrared.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "pirate/bio.h"


const unsigned char num_NAcodes = NUM_NA_CODES; //NUM_ELEM(NApowerCodes);

//These are all the variables we need to 
// slurp down the TVBGONE POWER codes and get 
// the actuall delay values.
static struct _tvbgoneparser{
	unsigned char codecnt;
	unsigned char paircnt;
	unsigned char samplebitcnt;
	unsigned char samplebytecnt;
	unsigned char bittracker;
	unsigned char timetableindex;	
	unsigned int onTime;
	unsigned int offTime;
	unsigned char numpairs;
}tvbg;

volatile void delayint10US(unsigned int delay);


// return 0 success, 1 too low, 2 too high
static uint8_t pwm_freq_find(
    float* freq_hz_value, float* pwm_hz_actual, float* pwm_ns_actual, uint32_t* pwm_divider, uint32_t* pwm_top) {
// calculate PWM values
// reverse calculate actual frequency/period
// from: https://forums.raspberrypi.com/viewtopic.php?t=317593#p1901340
#define TOP_MAX 65534
#define DIV_MIN ((0x01 << 4) + 0x0) // 0x01.0
#define DIV_MAX ((0xFF << 4) + 0xF) // 0xFF.F
    uint32_t clock = 125000000;
    // Calculate a div value for frequency desired
    uint32_t div = (clock << 4) / *freq_hz_value / (TOP_MAX + 1);
    if (div < DIV_MIN) {
        div = DIV_MIN;
    }
    // Determine what period that gives us
    uint32_t period = (clock << 4) / div / *freq_hz_value;
    // We may have had a rounding error when calculating div so it may
    // be lower than it should be, which in turn causes the period to
    // be higher than it should be, higher than can be used. In which
    // case we increase the div until the period becomes usable.
    while ((period > (TOP_MAX + 1)) && (div <= DIV_MAX)) {
        period = (clock << 4) / ++div / *freq_hz_value;
    }
    // Check if the result is usable
    if (period <= 1) {
        return 2; // too high
    } else if (div > DIV_MAX) {
        return 1; // too low
    } else {
        // Determine the top value we will be setting
        uint32_t top = period - 1;
        // Determine what output frequency that will generate
        *pwm_hz_actual = (float)(clock << 4) / div / (top + 1);
        *pwm_ns_actual = ((float)1 / (float)(*pwm_hz_actual)) * (float)1000000000;
        *pwm_divider = div;
        *pwm_top = top;
        // Report the results
        // printf("Freq = %f\t",         freq);
        // printf("Top = %ld\t",         top);
        // printf("Div = 0x%02lX.%lX\t", div >> 4, div & 0xF);
    }

    return 0;
}


void tvbgone_player(struct command_result *res){

	//clean up the PIO stuff:
	infrared_cleanup_temp();

	uint slice_num = pwm_gpio_to_slice_num(bio2bufiopin[BIO4]); 
	uint chan_num = pwm_gpio_to_channel(bio2bufiopin[BIO4]);
	pwm_set_enabled(slice_num, false);
	gpio_set_function(bio2bufiopin[BIO4], GPIO_FUNC_PWM);

	printf("TVBGONE player\r\nPlaying %d TV off codes:\r\n", num_NAcodes);

	for(tvbg.codecnt=0; tvbg.codecnt<num_NAcodes; tvbg.codecnt++){
		//setup indicator LED

		//show some kind of progress indicator
		printf("\r%d", tvbg.codecnt);

		//get and set PWM frequency
		if(NApowerCodes[tvbg.codecnt]->timer_val>0){ //setup PWM with this value
			//calculate PWM value from raw frequency
			float pwm_hz_reqest_temp=(float)NApowerCodes[tvbg.codecnt]->timer_val;
			float pwm_hz_actual_temp;
			float pwm_ns_actual;
			uint32_t pwm_divider;
			uint32_t pwm_top;
			pwm_freq_find(
				&pwm_hz_reqest_temp,
				&pwm_hz_actual_temp,
				&pwm_ns_actual,
				&pwm_divider,
				&pwm_top 
			);
			//set PWM
			pwm_set_clkdiv_int_frac(slice_num, pwm_divider >> 4, pwm_divider & 0b1111);
			pwm_set_wrap(slice_num, pwm_top);
			pwm_set_chan_level(slice_num, chan_num, pwm_top>>1); //50%
			// enable output
			gpio_set_function(bio2bufiopin[BIO4], GPIO_FUNC_PWM);
			pwm_set_enabled(slice_num, true);
		}else{//some don't use PWM, setup for direct on/off
			//these don't exist in our dataset...
			continue;
		}

		//setup the bit tracker and sample counter offset
		tvbg.bittracker=0b10000000;	
		tvbg.samplebytecnt=0;

		//there are numpairs pairs of bitcompression length in *samples that refer to value pairs in *times
		for(tvbg.paircnt=0; tvbg.paircnt<NApowerCodes[tvbg.codecnt]->numpairs; tvbg.paircnt++){
			
			//get the next index to the the times table values
			tvbg.timetableindex=0;	
			for(tvbg.samplebitcnt=0; tvbg.samplebitcnt<NApowerCodes[tvbg.codecnt]->bitcompression; tvbg.samplebitcnt++){
				tvbg.timetableindex=(tvbg.timetableindex<<1);
				if((NApowerCodes[tvbg.codecnt]->samples[tvbg.samplebytecnt] & tvbg.bittracker)!=0) 
					tvbg.timetableindex|=0b1; //set first bit
				tvbg.bittracker=(tvbg.bittracker>>1);
				if(tvbg.bittracker==0){
					tvbg.bittracker=0b10000000;
					tvbg.samplebytecnt++;
				}				
			}
		
			//adjust the index, there are two variables for each entry
			tvbg.timetableindex*=2; //multiply by two, the compiler knows each is an int (2 bytes)

			//PWM for ON time
			//IRTX_TRIS&=(~IRTX_PIN);//IR LED output
			//make GPIO output
			gpio_set_dir(bio2bufiopin[BIO4], GPIO_OUT);
			delayint10US(NApowerCodes[tvbg.codecnt]->times[tvbg.timetableindex]);//on time is first table entry

			//pause PWM for OFF time
			//IRTX_TRIS|=IRTX_PIN;//IR LED input (no PWM)
			//make GPIO input
			gpio_set_dir(bio2bufiopin[BIO4], GPIO_IN);
			delayint10US(NApowerCodes[tvbg.codecnt]->times[(tvbg.timetableindex+1)]);//off time is next entry

		}//for pairs loop
		
		//LEDon();//blink LED
		delayint10US(25000);//delay 250ms between codes
		//LEDoff();

	}//for codes loop

	printf("\r\nDone!\r\n");
    
	pwm_set_enabled(slice_num, false);
	gpio_set_function(bio2bufiopin[BIO4], GPIO_FUNC_SIO);
    gpio_set_dir(bio2bufiopin[BIO4], GPIO_IN);

	//resume normal IR mode
	infrared_setup_resume();


}

//
//
//	Simple blocking delay using Timer1
//	Polls interrupt to see when 10us have passed, repeats.
//	Hand tune T1_10usOffset in HardwareProfile.h
//
//
//volatile void delayint10US(unsigned int delay);
volatile void delayint10US(unsigned int delay){
	busy_wait_us(delay*10);
}

//the hardest part of this project is keeping the pointers straight
//this is a test that shows how to get the actual values from each element
//volatile static unsigned char timer, pairs, bits, sample;
//volatile static unsigned int times;
//timer=NApowerCodes[i]->timer_val;
//pairs=NApowerCodes[i]->numpairs;
//bits=NApowerCodes[i]->bitcompression;
//sample=NApowerCodes[i]->samples[0];
//times=NApowerCodes[i]->times[0];

