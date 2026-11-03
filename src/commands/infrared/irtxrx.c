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
#include "ui/ui_cmdln.h"
#include "ui/ui_term_linenoise.h"
#include "lib/bp_linenoise/linenoise.h"
#include "binmode/irtoy-air.h"

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
	{ "aIR packet", "packet", T_HELP_IRTX_PACKET, false },
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

//the error for text that ended before a whole packet
static int irtx_end_error(air_tx_t *tx){
	return (tx->state==AIR_TX_IDLE) ? AIR_ERROR_START : (tx->state==AIR_TX_MOD_FREQ) ? AIR_ERROR_MOD_FREQ : AIR_ERROR_DATA;
}

//show a parsed packet and send it
static void irtx_send(air_tx_t *tx){
	//debug: show loaded data packet
	//printf("Read: %s", buffer);
	printf("\r\n%s$%u:", ui_term_color_info(), tx->mod_freq);
	for(uint16_t i=0; i<tx->pairs; i++){
		printf("%u,%u,", tx->data[i]>>16, tx->data[i]&0xffff);
	}
	printf(";%s\r\n\r\n", ui_term_color_reset());		
	printf("Parsed AIR packet: modulation frequency %ukHz, %d MARK/SPACE pairs\r\nTransmitting...", tx->mod_freq, tx->pairs);
	irio_pio_tx_frame_write((float)(tx->mod_freq*1000), tx->pairs, tx->data);
	printf("done\r\n");
}

//returns true (success) false (failed)
bool irtx_transmit(char* buffer){
	//same parser as AIR mode
	air_tx_t tx = { .state = AIR_TX_IDLE };
	int result = AIR_PENDING;
	for(uint16_t cnt=0; buffer[cnt]!=0x00 && result==AIR_PENDING; cnt++){
		result = air_rx_char(&tx, buffer[cnt]);
	}
	if(result==AIR_PENDING){ //the line ended before the packet did
		result = irtx_end_error(&tx);
	}
	if(result!=AIR_OK){
		air_tx_error(result);
		return false;
	}
	irtx_send(&tx);
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
		//one parser for the whole file: a packet can be longer than a read, a line can hold several
		static air_tx_t tx;
		tx.state = AIR_TX_IDLE;
		bool sent = false;
		while(!res->error && f_gets(buffer, sizeof(buffer), &file_handle)){
			for(uint16_t i=0; buffer[i]!=0x00; i++){
				int result = air_rx_char(&tx, buffer[i]);
				if(result==AIR_OK){
					irtx_send(&tx);
					sent = true;
				}else if(result!=AIR_PENDING){
					air_tx_error(result);
					res->error = true;
					break;
				}
			}
		}
		if(!res->error && (tx.state!=AIR_TX_IDLE || !sent)){ //the file ended inside a packet, or had none
			air_tx_error(irtx_end_error(&tx));
			res->error = true;
		}
		if(res->error){
			printf("Error parsing AIR packet\r\n");
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
		//bp_cmd splits arguments at commas, read the packet as one word
		if(cmdln_args_string_by_position(1, sizeof(buffer), buffer)){
			//the command line drops anything typed past its end, the packet may be cut
			if(ui_term_linenoise_get_len()>=BP_LINENOISE_MAX_LINE){
				printf("AIR packet too long for the command line, use irtx -f\r\n");
				res->error = true;
				return;
			}
			//a space or a line break in the packet cuts it too
			char word[2];
			if(cmdln_args_string_by_position(2, sizeof(word), word)){
				printf("No spaces allowed in the AIR packet\r\n");
				res->error = true;
				return;
			}
			char c;
			if(!cmdln_try_peek(command_info.endptr+1, &c) || c!=';'){
				printf("AIR packet must end with ;\r\n");
				res->error = true;
				return;
			}
			printf("\r\nTransmitting from command line\r\n");
			//here's the deal: the command line parser is removing the final ';'
			// as it is a seperation character for multiple commands
			// so we need to add it back in
			size_t len = strlen(buffer);
			if(len<sizeof(buffer)-1){
				buffer[len]=';';
				buffer[len+1]=0x00;
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
			if(irio_pio_rx_frame_buf(&mod_freq, &us, &pairs, buffer, count_of(buffer))) break;
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
		for(uint16_t i=0; i<pairs-1 && sn_cnt<sizeof(air_buffer); i++){
			sn_cnt+=snprintf(&air_buffer[sn_cnt], sizeof(air_buffer)-sn_cnt, "%u,%u,", (uint16_t)(buffer[i]>>16), (uint16_t)(buffer[i]&0xffff));
		}
		if(sn_cnt<sizeof(air_buffer)){
			sn_cnt += snprintf(&air_buffer[sn_cnt], sizeof(air_buffer)-sn_cnt, "%u,;\r\n", buffer[pairs-1]>>16);
		}
		//a cut packet is not shown or saved, it can still be re-transmitted
		bool too_long = (sn_cnt>=sizeof(air_buffer));
		if(too_long){
			printf("AIR packet too long, max 512 characters\r\n");
			res->error = true;
		}else{
			printf("\r\n%s%s%s\r\n", ui_term_color_info(), air_buffer, ui_term_color_reset());
		}

		//for debugging
		/*
		printf("\r\n$%u:", mod_freq_int);
		for(uint16_t i=0; i<pairs; i++){
			printf("%u,%u,", buffer[i]>>16, buffer[i]&0xffff);
		}
		printf(";\r\n\r\n");
		*/

		//display mod_freq, mark/space pairs	
		printf("Modulation frequency %dkHz, %d MARK/SPACE pairs%s\r\n\r\n", mod_freq_int, pairs, (pairs>=count_of(buffer))?", longer signals are cut":"");
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
				if(too_long){
					printf("AIR packet too long, max 512 characters\r\n");
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
