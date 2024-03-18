//
// SUMP LA
//
//#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "queue.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "bio.h"
#include "system_config.h"
#include "bytecode.h" //needed because modes.h has some functions that use it TODO: move all the opt args and bytecode stuff to a single helper file
#include "opt_args.h" //needed for same reason as bytecode and needs same fix
#include "modes.h"
#include "mode/binio.h"
#include "hardware/clocks.h"
#include "psu.h"
#include "sump.h"
#include "binio_helpers.h"
#include "lib/gusmanbla/LogicAnalyzer_Structs.h"
#include "lib/gusmanbla/LogicAnalyzer.h"

//commandset
//http://www.sump.org/projects/analyzer/protocol/
#define SUMP_RESET 	0x00
#define SUMP_RUN	0x01
#define SUMP_ID		0x02
#define SUMP_DESC	0x04
#define SUMP_XON	0x11
#define SUMP_XOFF 	0x13
#define SUMP_DIV 	0x80
#define SUMP_CNT	0x81
#define SUMP_FLAGS	0x82
#define SUMP_TRIG	0xc0
#define SUMP_TRIG_VALS 0xc1

static enum _LAstate {
	LA_IDLE = 0,
	LA_ARMED,
} LAstate = LA_IDLE;

static unsigned char sumpPadBytes;
static unsigned int sumpSamples;


void sump_logic_analyzer(void)
{
	char c;

	script_reset(); //reset pins, etc

	//3.3volt power supply?

	sump_reset();

	sump_command(SUMP_ID);

	uint32_t sysclk = clock_get_hz(clk_sys) / 4; //SAMPLING_DIVIDER; 

	//printf("%.8x ", sysclk);

	while(1)
	{
		if(bin_rx_fifo_try_get(&c))
		{
			//printf("%.2x ", c);
			if(sump_command(c)) return;
		}
		
		if(sump_service()) return; //exit at end of sampling
	}
}

void sump_reset(void)
{
	//reset the SUMP state machine
	sumpSamples=BIG_BUFFER_SIZE;
	sumpPadBytes=0;
	LAstate=LA_IDLE;
}

// device name string
//sample memory (21) 0x1000 = 4096, 0x2 00 00 = 131072
//sample rate (23) sysclk = clock_get_hz(clk_sys) / 4; //SAMPLING_DIVIDER; 1dc d650
//number of probes (40)
//protocol version (41)
//http://dangerousprototypes.com/docs/The_Logic_Sniffer%27s_extended_SUMP_protocol
static const char sump_description[]={0x01,'B','P','v','5',0x00, 0x21, 0x00, 0x02, 0x00, 0x00, 0x23, 0x07, 0x73, 0x59, 0x40, 0x40, 0x08, 0x41, 0x02, 0x00};	

bool sump_command(unsigned char inByte)
{
//	static unsigned char i;
	static unsigned long l;

	static enum _SUMP {
		C_IDLE = 0,
		C_PARAMETERS,
		C_PROCESS,
	} sumpRXstate = C_IDLE;

	static struct _sumpRX {
		unsigned char command[5];
		unsigned char parameters;
		unsigned char parCnt;
	} sumpRX;

	switch(sumpRXstate){ //this is a state machine that grabs the incoming commands one byte at a time

		case C_IDLE:
			switch(inByte){//switch on the current byte
				case SUMP_RESET://reset
					sump_reset();
					break;
				case SUMP_ID://SLA0 or 1 backwards: 1ALS
					script_print("1ALS");
					break;
				case SUMP_RUN://arm the trigger
					//BP_LEDMODE=1;//ARMED, turn on LED


					LAstate=LA_ARMED;
					break;
				case SUMP_DESC:
					script_send(sump_description, sizeof(sump_description));
					//script_print()"\01BPv5\00\21\00\00\10\00\23\00\0f\42\40\20\00\00\00\08\41\02\00";
					/*bin_tx_fifo_put(0x01);	//1
					script_print("BPv5"); //5
					bin_tx_fifo_put(0x00);	//6
					bin_tx_fifo_put(0x21); //7
					bin_tx_fifo_put(0x00); 
					bin_tx_fifo_put(0x00);
					bin_tx_fifo_put(0x10);
					bin_tx_fifo_put(0x00);
					bin_tx_fifo_put(0x23);
					bin_tx_fifo_put(0x00);
					bin_tx_fifo_put(0x0f);
					bin_tx_fifo_put(0x42);
					bin_tx_fifo_put(0x40);
					bin_tx_fifo_put(0x40);	
					bin_tx_fifo_put(0x08);	
					bin_tx_fifo_put(0x42);	
					bin_tx_fifo_put(0x02);	
					bin_tx_fifo_put(0x00);	*/
					break;
				case SUMP_XON://resume send data
				//	xflow=1;
					//break;
				case SUMP_XOFF://pause send data
				//	xflow=0;
					break;
				default://long command
					sumpRX.command[0]=inByte;//store first command byte
					sumpRX.parameters=4; //all long commands are 5 bytes, get 4 parameters
					sumpRX.parCnt=0;//reset the parameter counter
					sumpRXstate=C_PARAMETERS;
					break;
			}
			break;
		case C_PARAMETERS: 
			sumpRX.parCnt++;
			sumpRX.command[sumpRX.parCnt]=inByte;//store each parameter
			if(sumpRX.parCnt<sumpRX.parameters) break; //if not all parameters, quit
		case C_PROCESS: //process the command
			switch(sumpRX.command[0]){

				case SUMP_TRIG: //set CN on these pins
					/*if(sumpRX.command[1] & 0b10000)	CNEN2|=0b1; //AUX
					if(sumpRX.command[1] & 0b1000)  CNEN2|=0b100000;
					if(sumpRX.command[1] & 0b100)   CNEN2|=0b1000000;
					if(sumpRX.command[1] & 0b10)  	CNEN2|=0b10000000;
					if(sumpRX.command[1] & 0b1) 	CNEN2|=0b100000000;*/
/*
				case SUMP_FLAGS:
					sumpPadBytes=0;//if user forgot to uncheck chan groups 2,3,4, we can send padding bytes
					if(sumpRX.command[1] & 0b100000) sumpPadBytes++;
					if(sumpRX.command[1] & 0b10000) sumpPadBytes++;
					if(sumpRX.command[1] & 0b1000) sumpPadBytes++;
					break;
*/
				case SUMP_CNT:		
					req->postSamples = ((((sumpRX.command[4] <<8) | sumpRX.command[3]) +1 ) *4);
					req->preSamples = ((((sumpRX.command[2] <<8) | sumpRX.command[1]) +1 ) *4)-req->postSamples;
					printf("%.4x",req->postSamples );
					/*sumpSamples=sumpRX.command[2];
					sumpSamples<<=8;
					sumpSamples|=sumpRX.command[1];
					sumpSamples=(sumpSamples+1)*4;
					//prevent buffer overruns
					if(sumpSamples>LA_SAMPLE_SIZE) sumpSamples=LA_SAMPLE_SIZE;*/
					break;
				case SUMP_DIV:
					l=sumpRX.command[3];
					l<<=8;
					l|=sumpRX.command[2];
					l<<=8;
					l|=sumpRX.command[1];
					req->frequency = l;
					//convert from SUMP 100MHz clock to our 16MIPs
					//l=((l+1)*16)/100;
					//l=((l+1)*4)/25; 

					break;
			}

			sumpRXstate=C_IDLE;
			break;					
		}
	return 0;
}

bool sump_service(void){
	static unsigned int i;
//	static unsigned char j;

	switch(LAstate){//dump data
		case LA_ARMED: //check interrupt flags
			/*if(IFS1bits.CNIF==0){//no flags
				if(CNEN2) //if no trigger just continue
					break;
			}
	
			//else sample
			T4CONbits.TON=1;//start timer4
			IFS1bits.T5IF=0;//clear interrupt flag//setup timer and wait

			for(i=0;i<sumpSamples;i++){ //take SAMPLE_SIZE samples
				bpConfig.terminalInput[i]=(PORTB>>6); //change to pointer for faster use...
				while(IFS1bits.T5IF==0); //wait for timer4 (timer 5 interrupt)
				IFS1bits.T5IF=0;//clear interrupt flag
			}
			
			CNEN2=0;//change notice off
			T4CON=0; //stop count

			for(i=sumpSamples; i>0; i--){ //send back to SUMP, backwards
				UART1TX(bpConfig.terminalInput[(i-1)]);
				//for(j=0; j<sumpPadBytes; j++) UART1TX(0); //add padding if needed
			}*/
			//TODO: after arming and setup, delay 1000ms before allowing to continue the loop
			sump_reset();
			return 1;//done, exit SUMP
			//break;
		case LA_IDLE:
			break;
	}
	
	return 0; //not done, keep going

}


