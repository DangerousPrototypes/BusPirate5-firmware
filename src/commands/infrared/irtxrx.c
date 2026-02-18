#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "bytecode.h"
#include "command_struct.h"
#include "mode/infrared-struct.h"
#include "mode/infrared.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "pirate/bio.h"
#include "lib/bp_args/bp_cmd.h"
// #include "ui/ui_prompt.h" // User prompts and menu system
// #include "ui/ui_const.h"  // Constants and strings
#include "fatfs/ff.h"       // File system related
#include "pirate/storage.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "pirate/irio_pio.h"
#include "usb_rx.h"

static const char* const usage_tx[] = {
    "irtx [aIR packet] [-f <file>]",
	"aIR format:%s $<modulation freq (kHz)>:<MARK1>,<SPACE1>,...<MARKn>,<SPACEn>,;",
	"Transmit:%s irtx $38:900,1800,900,65535,;",
	"Transmit from file:%s irtx -f example.air",
};

static const bp_command_opt_t irtx_opts[] = {
	{ "file", 'f', BP_ARG_REQUIRED, "file", T_HELP_IRTX_FILE_FLAG },
	{ 0 }
};

static const bp_command_positional_t irtx_positionals[] = {
	{ "aIR packet", "packet", 0, false },
	{ 0 }
};

const bp_command_def_t irtx_def = {
	.name         = "irtx",
	.description  = T_IR_CMD_IRTX,
	.actions      = NULL,
	.action_count = 0,
	.opts         = irtx_opts,
	.positionals      = irtx_positionals,
	.positional_count = 1,
	.usage        = usage_tx,
	.usage_count  = count_of(usage_tx),
};

static const char* const usage_rx[] = {
    "irrx [-f <file>] [-s <sensor>]",
	"aIR format:%s $<modulation freq (kHz)>:<MARK1>,<SPACE1>,...<MARKn>,<SPACEn>,;",
	"Receive (interactive):%s irrx",
	"Receive, save to file (interactive):%s irrx -f example.air",
	"Receive, specify sensor (interactive):%s irrx -s 56D",
	"Sensors:%s 38kHz barrier (38B), 36-40kHz/56kHz demodulator (38D*/56D)",
	"*default",
};

static const bp_command_opt_t irrx_opts[] = {
	{ "file",   'f', BP_ARG_REQUIRED, "file",   T_HELP_IRRX_FILE_FLAG },
	{ "sensor", 's', BP_ARG_REQUIRED, "sensor", T_HELP_IRRX_SENSOR_FLAG },
	{ 0 }
};

const bp_command_def_t irrx_def = {
	.name         = "irrx",
	.description  = T_IR_CMD_IRRX,
	.actions      = NULL,
	.action_count = 0,
	.opts         = irrx_opts,
	.usage        = usage_rx,
	.usage_count  = count_of(usage_rx),
};

//returns true (success) false (failed)
//TODO: check for cnt overflow! this is a potential buffer overflow
bool irtx_transmit(char* buffer){
	//parse the csv formatted values into 16 bit value pairs
	uint32_t data[128];
	uint16_t datacnt=0;
	uint8_t mod_freq=0;
	uint16_t cnt=0;

	while(buffer[cnt]!='$'){
		if(buffer[cnt]==0x00){
			printf("Unable to find start of data ($) in AIR packet\r\n");
			return false;
		}
		cnt++;

	}
	cnt++;

	//parse the ascii into an 8 bit value
	while(buffer[cnt]!=':'){
		if(buffer[cnt]==0x00){
			printf("Unable to find modulation frequency (:) in AIR packet\r\n");
			return false;
		}
		mod_freq*=10;
		mod_freq+=buffer[cnt]-0x30;
		cnt++;
	} 
	cnt++;

	//parse the ascii values into 16 bit values, shove in a 32bit word
	//would be much nicer to use pointers here, but it would be a glob of unreadable slop
	bool mark=true;
	while(true){
		uint16_t value=0;
		while(buffer[cnt]!=','){
			if(buffer[cnt]==0x00||buffer[cnt]<'0'||buffer[cnt]>'9'){
				printf("Unable to find end of MARK/SPACE data (,) in AIR packet\r\n");
				return false;
			}
			value*=10;
			value+=buffer[cnt]-0x30;
			cnt++;
		}
		if(mark){
			data[datacnt] = (value<<16);//upper 16 bits are the mark
			mark = false;
		}else{
			data[datacnt] |= value;
			datacnt++;
			if(datacnt>=count_of(data)){
				printf("Too many MARK/SPACE pairs in AIR packet, max 127\r\n");
				return false;
			}			
			mark = true;
		}
		cnt++; //if 0x00 will be tossed in the next loop
		if(buffer[cnt]==';'){
			//found end of AIR packet
            if(!mark){ //end with a SPACE if not included in AIR packet
                data[datacnt] |= 0xffff;
                datacnt++;
                if(datacnt>=count_of(data)){
                    printf("Too many MARK/SPACE pairs in AIR packet, max 127\r\n");
					return false;
                }                
            }			
			break;
		}
	}

	//debug: show loaded data packet
	//printf("Read: %s", buffer);
	printf("\r\n%s$%u:", ui_term_color_info(), mod_freq);
	for(uint16_t i=0; i<datacnt; i++){
		printf("%u,%u,", data[i]>>16, data[i]&0xffff);
	}
	printf(";%s\r\n\r\n", ui_term_color_reset());		
	printf("Parsed AIR packet: modulation frequency %dkHz, %d MARK/SPACE pairs\r\nTransmitting...", mod_freq, datacnt);
	irio_pio_tx_frame_write((float)(mod_freq*1000), datacnt, data);
	printf("done\r\n");
	return true;
}

void irtx_handler(struct command_result *res){
    if (bp_cmd_help_check(&irtx_def, res->help_flag)) {
        return;
    }

	char buffer[512];

	//if -f flag, transmit from file
	char file[13];
	if(bp_cmd_get_string(&irtx_def, 'f', file, sizeof(file))){
		//get the filename
		printf("Transmitting from file %s\r\n", file);
		//open the file
		FIL file_handle;
		FRESULT result;
		result = f_open(&file_handle, file, FA_READ);
		if (result != FR_OK) {
			printf("Error opening file %s for reading\r\n", file);
			res->error = true;
			return;
		}
		//read the file
		//UINT bytes_read;
		//TCHAR* bufptr;
		//result = f_read(&file_handle, buffer, sizeof(buffer), &bytes_read);
		infrared_cleanup_temp(); //tear down current IR PIO programs
		irio_pio_tx_init(bio2bufiopin[BIO4], 38000); //setup IR PIO programs, actual freq will be set in irtx_packet
		while(f_gets(buffer, sizeof(buffer), &file_handle)){
			/*if(bufptr ==0) {
				printf("Error reading file %s\r\n", file);
				res->error = true;
				return;
			}*/
			if(!irtx_transmit(buffer)){
				printf("Error parsing AIR packet\r\n");
				res->error = true;
				break;
			}
		}	
		irio_pio_tx_deinit(bio2bufiopin[BIO4]); //tear down IR PIO programs
		infrared_setup_resume(); //reinit IR PIO programs	
		//close the file
		result = f_close(&file_handle);
		if (result != FR_OK) {
			printf("Error closing file %s\r\n", file);
			res->error = true;
			return;
		}

		return;
	
	}else{
		//try to parse from the command line
		if(bp_cmd_get_positional_string(&irtx_def, 1, buffer, sizeof(buffer))){
			printf("\r\nTransmitting from command line\r\n");
			//here's the deal: the command line parser is removing the final ';'
			// as it is a seperation character for multiple commands
			// so we need to add it back in
			if(strlen(buffer)<sizeof(buffer)-1){
				buffer[strlen(buffer)]=';';
				buffer[strlen(buffer)+1]=0x00;
			}else{
				printf("AIR packet too long, max 512 characters\r\n");
				res->error = true;
				return;
			}
			
			//printf("%s", buffer);
			infrared_cleanup_temp(); //tear down current IR PIO programs
			irio_pio_tx_init(bio2bufiopin[BIO4], 38000); //setup IR PIO programs, actual freq will be set in irtx_packet
			if(!irtx_transmit(buffer)){
				printf("Error parsing AIR packet\r\n");
				res->error = true;
			}
			irio_pio_tx_deinit(bio2bufiopin[BIO4]); //tear down IR PIO programs
			infrared_setup_resume(); //reinit IR PIO programs
			return;
		}
	}

	printf("Nothing to do, showing help\r\n");
	//nothing to do, show help
	bp_cmd_help_show(&irtx_def);
}

void irrx_handler(struct command_result *res){
	if (bp_cmd_help_check(&irrx_def, res->help_flag)) {
        return;
    }

	//check for file flag
	//if no file, go into interactive mode?
	FIL file_handle;
	FRESULT result;
	bool save_file=false;
	char file[13];
	if(bp_cmd_get_string(&irrx_def, 'f', file, sizeof(file))){
		printf("Saving to file %s\r\n", file);
		save_file=true;
		//open file
		result = f_open(&file_handle, file, FA_WRITE | FA_CREATE_ALWAYS);
		if (result != FR_OK) {
			printf("Error opening file %s for writing\r\n", file);
			res->error = true;
			return;
		}
	}else{
		printf("No file specified, packets cannot be saved\r\n");
	}

	//list of demodulator pin options
	// 38kHz barrier
	// 36-40kHz demodulator
	// 56kHz modulator
	typedef struct _demod_sensors{
		uint description;
		uint8_t bio;
		char flag_opt[6];
	} demod_sensors;
	static const demod_sensors ir_rx_pins[] = {
		{ T_IR_RX_SENSOR_MENU_BARRIER, BIO3, "38b" },
		{ T_IR_RX_SENSOR_MENU_38K_DEMOD, BIO5, "38d" },
		{ T_IR_RX_SENSOR_MENU_56K_DEMOD, BIO7, "56d" }
	};

	//default to 38kHz demod
	uint8_t rx_sensor=1;
	char sensor[6];
	if(bp_cmd_get_string(&irrx_def, 's', sensor, sizeof(sensor))){
		//find the sensor
		strlwr(sensor);
		for(uint8_t i=0; i<count_of(ir_rx_pins); i++){
			if(strcmp(sensor, ir_rx_pins[i].flag_opt)==0){
				rx_sensor=i;
				break;
			}
		}
	}

	printf("RX sensor: %s\r\n", GET_T(ir_rx_pins[rx_sensor].description));

	//tear down current IR PIO programs
	infrared_cleanup_temp();
	//setup IR PIO programs
	irio_pio_rx_init(bio2bufiopin[ir_rx_pins[rx_sensor].bio]);
	irio_pio_tx_init(bio2bufiopin[BIO4], 36000);

	while(true){
		uint32_t buffer[128];
		uint16_t pairs=0;
		float mod_freq;
		uint16_t us;
		char air_buffer[512];
		//wait for complete IR packet from irio_pio
		printf("\r\nListening for IR packets (x to exit)...\r\n");
		//drain the FIFO so we can sync and not get garbage
		irio_pio_rxtx_drain_fifo();
		//display captured packet
		while(true){
			if(irio_pio_rx_frame_buf(&mod_freq, &us, &pairs, buffer)) break;
			// any key to exit
			char c;
		    if (rx_fifo_try_get(&c)) {
				if(c=='x'){
					printf("Exiting...\r\n");
					goto exit_irrx_handler;
				}
			}
		}
		uint8_t mod_freq_int = (uint8_t)roundf(mod_freq/1000.0f);
		uint16_t sn_cnt = snprintf(air_buffer, sizeof(air_buffer), "$%u:", mod_freq_int);
		for(uint16_t i=0; i<pairs-1; i++){
			sn_cnt+=snprintf(&air_buffer[sn_cnt], sizeof(air_buffer)-sn_cnt, "%u,%u,", (uint16_t)(buffer[i]>>16), (uint16_t)(buffer[i]&0xffff));
		}
		sn_cnt += snprintf(&air_buffer[sn_cnt], sizeof(air_buffer)-sn_cnt, "%u,;\r\n", buffer[pairs-1]>>16);
		if(sn_cnt>sizeof(air_buffer)){
			printf("AIR packet too long, max 512 characters\r\n");
			res->error = true;
			//goto exit_irrx_handler;
		}
		printf("\r\n%s%s%s\r\n", ui_term_color_info(), air_buffer, ui_term_color_reset());

		//for debugging
		/*
		printf("\r\n$%u:", mod_freq_int);
		for(uint16_t i=0; i<pairs; i++){
			printf("%u,%u,", buffer[i]>>16, buffer[i]&0xffff);
		}
		printf(";\r\n\r\n");
		*/

		//display mod_freq, mark/space pairs	
		printf("Modulation frequency %dkHz, %d MARK/SPACE pairs\r\n\r\n", mod_freq_int, pairs);
		//s to save (if -f flag), space for next, x to exit
menu_irrx_handler:
		printf("%s", ui_term_color_prompt());
		if(save_file){
			printf("\'s\' to save, ");
		}
		printf("\'r\' or \'t\' to re-transmit, space for next, \'x\' to exit > ");
		printf("%s", ui_term_color_reset());
		//use a prompt function to get the user input?
		//if 's', save to file
		//if 'x', exit
		//if space, continue
		char c;
		while(!rx_fifo_try_get(&c)){
			//wait for input
		}
		printf("%c\r\n", c);
		switch(c){
			case 's':
				if(!save_file){
					printf("No file specified with the -f flag, cannot save\r\n");
					goto menu_irrx_handler;
				}
				//write to file
				printf("Saving to file %s\r\n", file);
				//write the data to the file
				UINT bytes_written; // somewhere to store the number of bytes written
				result = f_write(&file_handle, air_buffer, strlen(air_buffer), &bytes_written); // write the data to the file
				if (result != FR_OK) {
					printf("Error writing to file %s\r\n", file);
					res->error = true; // set the error flag
					goto exit_irrx_handler;
				}
				goto menu_irrx_handler;
				break;
			case 'r':
			case 't': //retransmit this packet	
				printf("\r\nTransmitting...");
				irio_pio_tx_frame_write((float)(mod_freq), pairs, buffer);
				printf("done\r\n\r\n");		
				goto menu_irrx_handler;
				break;
			case ' ':
				break;				
			case 'x':
exit_irrx_handler:
				//resume IR PIO programs
				irio_pio_rx_deinit(bio2bufiopin[ir_rx_pins[rx_sensor].bio]);
				irio_pio_tx_deinit(bio2bufiopin[BIO4]);
				infrared_setup_resume();
				//close file
				if(save_file){
					result = f_close(&file_handle); // close the file
					if (result != FR_OK) {
						printf("Error closing file %s\r\n", file);
						res->error = true; // set the error flag
					}
				}
				return;
			default:
				printf("Invalid input\r\n");
				goto menu_irrx_handler;
				break;
		}

	}

}
