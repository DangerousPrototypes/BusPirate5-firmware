#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "dummy1.h"




static uint32_t returnval;

static const char pin_labels[][5]={
	"DUM1",
	"DUM2",
	"DUM3",
	"DUM4"
};

void dummy1_start(void)
{
	printf("-DUMMY1- start()");
}
void dummy1_startr(void)
{
	printf("-DUMMY1- startr()");
}
void dummy1_stop(void)
{
	printf("-DUMMY1- stop()");
}
void dummy1_stopr(void)
{
	printf("-DUMMY1- stopr()");
}
uint32_t dummy1_send(uint32_t d)
{
	printf("--DUMMY1- send(%08X)=%08X", d, returnval);

	returnval=d;

	return d;
}
uint32_t dummy1_read(void)
{
	printf("-DUMMY1- read()=%08X", returnval);
	return returnval;
}
void dummy1_clkh(void)
{
	printf("-DUMMY1- clkh()");
}
void dummy1_clkl(void)
{
	printf("-DUMMY1- clkl()");
}
void dummy1_dath(void)
{
	printf("-DUMMY1- dath()");
}
void dummy1_datl(void)
{
	printf("-DUMMY1- datl()");
}
uint32_t dummy1_dats(void)
{
	printf("-DUMMY1- dats()=%08X", returnval);
	return returnval;
}
void dummy1_clk(void)
{
	printf("-DUMMY1- clk()");
}
uint32_t dummy1_bitr(void)
{
	printf("-DUMMY1- bitr()=%08X", returnval);
	return returnval;
}
uint32_t dummy1_period(void)
{
	if(returnval)
	{
		printf("\r\n\x07");
		printf("Pending something");
		returnval=0;
		return 1;
	}
	return 0;
}
void dummy1_macro(uint32_t macro)
{
	printf("-DUMMY1- macro(%08X)", macro);
}
uint32_t dummy1_setup(void)
{
	printf("-DUMMY1- setup()");
	return 1;
}
uint32_t dummy1_setup_exc(void)
{
	system_bio_claim(true, BIO0, BP_PIN_MODE, pin_labels[0]);
	system_bio_claim(true, BIO1, BP_PIN_MODE, pin_labels[1]);
	system_bio_claim(true, BIO2, BP_PIN_MODE, pin_labels[2]);
	system_bio_claim(true, BIO3, BP_PIN_MODE, pin_labels[3]);			
	printf("-DUMMY1- setup_exc()");
	return 1;
}
void dummy1_cleanup(void)
{
	system_bio_claim(false, BIO0, BP_PIN_MODE,0);
	system_bio_claim(false, BIO1, BP_PIN_MODE,0);
	system_bio_claim(false, BIO2, BP_PIN_MODE,0);
	system_bio_claim(false, BIO3, BP_PIN_MODE,0);	
	printf("-DUMMY1- cleanup()");
}
/*const char *dummy1_pins(void)
{
	return "pin1\tpin2\tpin3\tpin4";
}*/
void dummy1_settings(void)
{
	printf("DUMMY (arg1 arg2)=(%d, %d)", 1, 2);
}

