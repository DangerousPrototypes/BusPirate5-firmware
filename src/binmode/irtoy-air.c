#include <pico/stdlib.h>
#include <string.h>
#include <math.h>
#include "hardware/clocks.h"
#include "pirate.h"
#include "queue.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "system_config.h"
// #include "bytecode.h" //needed because modes.h has some functions that use it TODO: move all the opt args and
// bytecode stuff to a single helper file #include "command_struct.h" //needed for same reason as bytecode and needs same fix
// #include "modes.h"
#include "binmode/binmodes.h"
#include "tusb.h"
#include "ui/ui_term.h"
#include "binmode/binio_helpers.h"
#include "mode/infrared-struct.h"
#include "pirate/irio_pio.h"
#include "pirate/bio.h"
#include "pirate/psu.h"


#define MAX_UART_PKT 64
#define CDC_INTF 1

// binmode name to display
const char irtoy_air_name[] = "AIR capture (AnalysIR, etc)";
//TODO: binmode reset to Hiz option, power supply, etc?
// binmode setup on mode start
void irtoy_air_setup(void) {
    // time counter
    // learner counter
    system_config.binmode_usb_rx_queue_enable = true;
    system_config.binmode_usb_tx_queue_enable = true;
    //setup buffers, pass correct pin numbers
    bio_buf_output(BIO0);
    bio_buf_output(BIO4);
    psu_enable(5, 0, true);
    irio_pio_tx_init(bio2bufiopin[BIO4], 38000);
    irio_pio_rx_init(bio2bufiopin[BIO5]);
}

// binmode cleanup on exit
void irtoy_air_cleanup(void) {
    irio_pio_rx_deinit(bio2bufiopin[BIO5]);
    irio_pio_tx_deinit(bio2bufiopin[BIO4]);
    psu_disable();
    bio_init();
    system_config.binmode_usb_rx_queue_enable = true;
    system_config.binmode_usb_tx_queue_enable = true;
}

#define HARDWARE_VERSION '3'
#define FIRMWARE_VERSION_H '1'
#define FIRMWARE_VERSION_L '0'
enum {
    AIR_OK = 0,
    AIR_ERROR_START = 1,
    AIR_ERROR_LENGTH = 2,
    AIR_ERROR_MOD_FREQ,
    AIR_ERROR_DATA,
};
/*
printf("Unable to find start of data ($) in AIR packet\r\n");
printf("Unable to find modulation frequency (:) in AIR packet\r\n");
				printf("Unable to find end of MARK data (,) in AIR packet\r\n");
*/
//buffer should be a NULL terminaled string
static bool air_decode_transmit(char* air_buffer, uint32_t air_len, uint32_t *data, uint32_t data_len){
	//parse the csv formatted values into 16 bit value pairs
	uint16_t data_cnt=0;
	uint8_t mod_freq;
	uint16_t air_cnt=0;

    //search start of frame
	while(air_buffer[air_cnt]!='$'){
		if(air_buffer[air_cnt]==0x00) return AIR_ERROR_START; //null terminated
        air_cnt++;
		if(air_cnt>=air_len) return AIR_ERROR_START;  //buffer length out of bounds      
	}

    //skip past the $ character
	air_cnt++;
    if(air_cnt>=air_len){
        return AIR_ERROR_START;
    }

	//parse the ascii into an 8 bit value
	while(air_buffer[air_cnt]!=':'){
        if(air_buffer[air_cnt]==0x00||air_buffer[air_cnt]<'0'||air_buffer[air_cnt]>'9'){
            return AIR_ERROR_MOD_FREQ;
        }
		mod_freq*=10;
		mod_freq+=air_buffer[air_cnt]-0x30;
		air_cnt++;
        if(air_cnt>=air_len) return AIR_ERROR_MOD_FREQ;
	} 

    //skip past the : character
	air_cnt++;
    if(air_cnt>=air_len){
        return AIR_ERROR_LENGTH;
    }
    
    bool mark=true;
    while(true){
        uint16_t value=0;
        while(air_buffer[air_cnt]!=','){
			if(air_buffer[air_cnt]==0x00||air_buffer[air_cnt]<'0'||air_buffer[air_cnt]>'9'){
				return AIR_ERROR_DATA;
			}
			value*=10;
			value+=air_buffer[air_cnt]-0x30;
			air_cnt++;
            if(air_cnt>=air_len) return AIR_ERROR_DATA;
		}
        if(mark){
            data[data_cnt] = (value<<16);//upper 16 bits are the mark
            mark = false;
        }else{
            data[data_cnt] |= value;
            data_cnt++;
            if(data_cnt>=data_len){
                return AIR_ERROR_DATA;
            }
            mark = true;
        }
        air_cnt++;
        if(air_cnt>=air_len) return AIR_ERROR_DATA;
        if(air_buffer[air_cnt]==';'){
            //found end of AIR packet
            if(!mark){
                data[data_cnt] |= 0xffff;
                data_cnt++;
                if(data_cnt>=data_len){
                    return AIR_ERROR_DATA;
                }                
            }
            break;
        }
    }

	//debug: show loaded data packet
	//printf("Read: %s", buffer);
	printf("\r\n$%u:", mod_freq);
	for(uint16_t i=0; i<data_cnt; i++){
		printf("%u,%u,", data[i]>>16, data[i]&0xffff);
	}
	printf(";\r\n\r\n");		
	printf("Parsed AIR packet: modulation frequency %dkHz, %d MARK/SPACE pairs\r\nTransmitting...", mod_freq, data_cnt);
	irio_pio_tx_frame_write((float)(mod_freq*1000), data_cnt, data);
	printf("done\r\n");
	return AIR_OK;
}

typedef struct _IR_PULSE {
    uint8_t identifier_mark_or_space;
    uint8_t entry_count;
    uint8_t pulse_length_lsb; // lsb bit 0 also stores mark(1)/space(0)
    uint8_t pulse_length_msb;
    uint8_t always_LF; // always '\n'
} IR_PULSE;

static void ir_rx_pulse_packet(IR_PULSE *pulse_buffer, bool is_mark, uint16_t pulse_length){
    if(is_mark){
        pulse_buffer->identifier_mark_or_space = '+'; //mark
        //pulse_length = pulse_length |= 0x0001; //always set low bit for mark
    }else{
        pulse_buffer->identifier_mark_or_space = '-'; //space
        //pulse_length = pulse_length &= 0xfffe; //always clear low bit for space
    }
    pulse_buffer->pulse_length_msb = (uint8_t)(pulse_length >> 8);
    pulse_buffer->pulse_length_lsb = (uint8_t)(pulse_length);
    for(uint8_t j=0; j<sizeof(IR_PULSE); j++){
        bin_tx_fifo_put(((uint8_t*)pulse_buffer)[j]);
    }
    pulse_buffer->entry_count++;
}

typedef struct _IR_MODULATION {
    uint8_t identifier_modulation; // always 'M'
    uint8_t measured_sample_count;
    uint8_t period_sum_useconds_lsb;
    uint8_t period_sum_useconds_msb;
    uint8_t always_LF; // always '\n'
} IR_MODULATION;

static void ir_rx_modulation_packet(IR_MODULATION *modulation_buffer, uint16_t period_sum_useconds){
    modulation_buffer->period_sum_useconds_msb = (uint8_t)(period_sum_useconds >> 8);
    modulation_buffer->period_sum_useconds_lsb = (uint8_t)(period_sum_useconds);
    for(uint8_t j=0; j<sizeof(IR_MODULATION); j++){
        bin_tx_fifo_put(((uint8_t*)modulation_buffer)[j]);
    }
}

/*
$36:420,280,168,280,420,;
$ start character
: carrier frequency / 1000 (this comes from the learner sensor)
ASCII decimals representing the lengths of pulse and no-pulse in uS CSV formatted, including the final value
; Terminated with ;

    $ - Start AIR packet
    36 - Modulation frequency in kHz
    : - Start of IR pulse timing data (in us), beginning with MARK (IR pulse)
    900 - Length of first IR MARK/pulse (in us)
    , - Each value separated by a comma, including after the final timeout
    850 - Length of first IR SPACE/no pulse (in us)
    , - Each value separated by a comma, including after the final timeout
    … More MARK/SPACE timing pairs
    65535 - Final timeout, when no IR MARK has been sensed in 65535us
    , - Remember the comma after the final timing
    ; - Terminated with a semicolon

*/
// Use PIO to count 1uS ticks for each pulse and no-pulse, with timeout?
void irtoy_air_service(void){
    float mod_freq;
    uint16_t us;
    uint16_t pairs;
    uint32_t buffer[128];
    uint8_t air_buffer[512];
    bool frame_found=false;
    uint16_t air_cnt;
    bool send_id;

    IR_PULSE pulse;
    pulse.always_LF = '\n';

    IR_MODULATION modulation;
    modulation.identifier_modulation = 'M';
    modulation.measured_sample_count = 1;
    modulation.always_LF = '\n';

    while(true){
        //need to be careful about PIO fifo overflow here
        if (!tud_cdc_n_connected(CDC_INTF)) {
            irio_pio_rxtx_drain_fifo();
            send_id=true;
            continue;
        }else if(send_id){
            send_id=false;
            const char version[] = {'\n','!','B','P',' ','V', HARDWARE_VERSION, FIRMWARE_VERSION_H, FIRMWARE_VERSION_L,'!','\n', 0x00};
            for (uint32_t i = 0; i < strlen(version); i++) {
                bin_tx_fifo_put(version[i]);
            }
        }

        if(irio_pio_rx_frame_buf(&mod_freq, &us, &pairs, buffer) ){
            #if 0
            //create the AIR packet
            uint8_t mod_freq_int = (uint8_t)roundf(mod_freq / 1000.0f);
            uint16_t sn_cnt = snprintf(air_buffer, sizeof(air_buffer), "$%u:", mod_freq_int);
            for (uint16_t i = 0; i < (pairs-1); i++) {
                sn_cnt += snprintf(&air_buffer[sn_cnt], sizeof(air_buffer) - sn_cnt, "%u,%u,", (uint16_t)(buffer[i] >> 16), (uint16_t)(buffer[i] & 0xffff));
            }
            sn_cnt += snprintf(&air_buffer[sn_cnt], sizeof(air_buffer) - sn_cnt, "%u,;", (uint16_t)(buffer[pairs-1] >> 16));
            //sn_cnt += snprintf(&air_buffer[sn_cnt], sizeof(air_buffer) - sn_cnt, ";");
            if (sn_cnt >= count_of(air_buffer)) {
                continue;
            }
            
            //send the AIR packet
            for (uint32_t i = 0; i < sn_cnt; i++) {
                bin_tx_fifo_put(air_buffer[i]);
            }
            #endif

            //uint8_t mod_freq_int = (uint8_t)roundf(mod_freq / 1000.0f);
            pulse.entry_count=0;
            for (uint16_t i = 0; i < (pairs); i++) {
                ir_rx_pulse_packet(&pulse, true, (uint16_t)(buffer[i] >> 16));
                ir_rx_pulse_packet(&pulse, false, (uint16_t)(buffer[i] & 0xffff));
            }
            //ir_rx_pulse_packet(&pulse, true, (uint16_t)(buffer[pairs-1] >> 16)); 
            //have us, want nano seconds
            us = (us *1000) / 5; //convert to 1ns ticks
            ir_rx_modulation_packet(&modulation, us);         
        }
        
        char c;
        while(bin_rx_fifo_try_get(&c)){
            if(c=='$'){ //beginning of frame
                frame_found=true;
                air_cnt=0;
            }
            if(frame_found){ 
                air_buffer[air_cnt]=c;
                air_cnt++;
                if(air_cnt>=count_of(air_buffer)){ //overflow buffer
                    frame_found=false;
                    air_cnt=0;
                }
                if(c==';'){ //end of frame
                    //send the AIR packet
                    air_cnt++;
                    if(air_cnt>=count_of(air_buffer)){ //overflow buffer
                        frame_found=false;
                        air_cnt=0;
                    }      
                    air_buffer[air_cnt]=0; //null terminate              
                    air_decode_transmit(air_buffer, count_of(air_buffer), buffer, count_of(buffer));
                    frame_found=false;
                    air_cnt=0;
                }
            }
        }

        /*char c;
        if (!bin_rx_fifo_try_get(&c)){
            return;
        }
        const char version[4] = {'V', (HARDWARE_VERSION + 0x30), FIRMWARE_VERSION_H, FIRMWARE_VERSION_L};

        switch (c) {
            case 'V':
            case 'v':// Acquire Version
                script_send(version, 4);
                break;           
            default:
                break;

        }*/
    }

}
