#include <pico/stdlib.h>
#include <string.h>
#include <math.h>
#include "hardware/clocks.h"
#include "pirate.h"

#include "usb_rx.h"
#include "usb_tx.h"
#include "system_config.h"
// #include "bytecode.h" //needed because modes.h has some functions that use it TODO: move all the opt args and
// bytecode stuff to a single helper file #include "command_struct.h" //needed for same reason as bytecode and needs same fix
// #include "modes.h"
#include "binmode/binmodes.h"
#include "binmode/irtoy-air.h"
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

static const char pin_labels[][5]={
    "LERN",
    "IRTX",
    "38k",
};

// binmode setup on mode start
void irtoy_air_setup(void) {
    system_bio_update_purpose_and_label(true, BIO1, BP_PIN_IO, pin_labels[0]);
    system_bio_update_purpose_and_label(true, BIO4, BP_PIN_IO, pin_labels[1]);
    system_bio_update_purpose_and_label(true, BIO5, BP_PIN_IO, pin_labels[2]);
    system_config.binmode_usb_rx_queue_enable = true;
    system_config.binmode_usb_tx_queue_enable = true;
    //setup buffers, pass correct pin numbers
    bio_buf_output(BIO0);
    bio_buf_output(BIO4);
    //psu_enable(5, 0, true);
    irio_pio_tx_init(bio2bufiopin[BIO4], 38000);
    irio_pio_rx_init(bio2bufiopin[BIO5]);
}

// binmode cleanup on exit
void irtoy_air_cleanup(void) {
    system_bio_update_purpose_and_label(false, BIO1, BP_PIN_IO, pin_labels[0]);
    system_bio_update_purpose_and_label(false, BIO4, BP_PIN_IO, pin_labels[1]);
    system_bio_update_purpose_and_label(false, BIO5, BP_PIN_IO, pin_labels[2]);
    irio_pio_rx_deinit(bio2bufiopin[BIO5]);
    irio_pio_tx_deinit(bio2bufiopin[BIO4]);
    //psu_disable();
    bio_init();
    system_config.binmode_usb_rx_queue_enable = true;
    system_config.binmode_usb_tx_queue_enable = true;
}

#define HARDWARE_VERSION '3'
#define FIRMWARE_VERSION_H '1'
#define FIRMWARE_VERSION_L '0'

// $38:900,1800,900,65535,;
// returns AIR_OK when a whole packet is ready, AIR_PENDING until then, or the error that dropped it
int air_rx_char(air_tx_t *tx, char c){
    if(c=='$'){ //beginning of frame
        int result = (tx->state==AIR_TX_IDLE) ? AIR_PENDING : AIR_ERROR_RESTART; //a packet in progress is dropped
        tx->state = AIR_TX_MOD_FREQ;
        tx->mod_freq = 0;
        tx->digits = false;
        return result;
    }

    switch(tx->state){
        case AIR_TX_MOD_FREQ:
            if(c==':' && tx->digits){
                tx->state = AIR_TX_DATA;
                tx->value = 0;
                tx->digits = false;
                tx->mark = true;
                tx->pairs = 0;
                return AIR_PENDING;
            }
            if(c<'0' || c>'9' || tx->mod_freq>25){
                tx->state = AIR_TX_IDLE;
                return AIR_ERROR_MOD_FREQ;
            }
            tx->mod_freq = (tx->mod_freq*10) + (c-'0');
            tx->digits = true;
            if(tx->mod_freq>0xff){
                tx->state = AIR_TX_IDLE;
                return AIR_ERROR_MOD_FREQ;
            }
            return AIR_PENDING;
        case AIR_TX_DATA:
            if(c>='0' && c<='9' && tx->value<=6553){
                tx->value = (tx->value*10) + (c-'0');
                tx->digits = true;
                if(tx->value<=0xffff){
                    return AIR_PENDING;
                }
            }else if(c==',' && tx->digits){
                if(tx->mark){
                    if(tx->pairs>=AIR_TX_MAX_PAIRS){
                        tx->state = AIR_TX_IDLE;
                        return AIR_ERROR_LENGTH;
                    }
                    tx->data[tx->pairs] = (tx->value<<16); //upper 16 bits are the mark
                }else{
                    tx->data[tx->pairs] |= tx->value;
                    tx->pairs++;
                }
                tx->mark = !tx->mark;
                tx->value = 0;
                tx->digits = false;
                return AIR_PENDING;
            }else if(c==';' && !tx->digits && (tx->pairs || !tx->mark)){
                //found end of AIR packet
                if(!tx->mark){ //end with a SPACE if not included in AIR packet
                    tx->data[tx->pairs] |= 0xffff;
                    tx->pairs++;
                }
                tx->state = AIR_TX_IDLE;
                return AIR_OK;
            }
            tx->state = AIR_TX_IDLE;
            return AIR_ERROR_DATA;
        default: //waiting for $
            return AIR_PENDING;
    }
}

void air_tx_error(int error){
    switch(error){
        case AIR_ERROR_START:
            printf("Unable to find start of data ($) in AIR packet\r\n");
            break;
        case AIR_ERROR_MOD_FREQ:
            printf("Unable to find modulation frequency (:) in AIR packet\r\n");
            break;
        case AIR_ERROR_LENGTH:
            printf("Too many MARK/SPACE pairs in AIR packet, max %d\r\n", AIR_TX_MAX_PAIRS);
            break;
        case AIR_ERROR_RESTART:
            printf("AIR packet dropped, a new one started ($) before its end (;)\r\n");
            break;
        default:
            printf("Unable to find end of MARK/SPACE data (,) in AIR packet\r\n");
            break;
    }
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
    air_tx_t tx = { .state = AIR_TX_IDLE };
    bool send_id = true; //the host may already have the port open

    IR_PULSE pulse;
    pulse.always_LF = '\n';

    IR_MODULATION modulation;
    modulation.identifier_modulation = 'M';
    modulation.measured_sample_count = 1;
    modulation.always_LF = '\n';

    //let's talk about this loop
    //ideally we'd be co-op multitasking, but we're not
    //to multitask we'd need to make the buffer and air_buffer static
    //which would eat a lot of memory
    //instead we just do a never ending loop here
    while(true){
        //need to be careful about PIO fifo overflow here
        if (!tud_cdc_n_connected(CDC_INTF)) {
            irio_pio_rxtx_drain_fifo();
            send_id=true;
            tx.state = AIR_TX_IDLE; //a packet the host didn't finish is dropped
            continue;
        }else if(send_id){
            send_id=false;
            const char version[] = {'\n','!','B','P',' ','V', HARDWARE_VERSION, FIRMWARE_VERSION_H, FIRMWARE_VERSION_L,'!','\n', 0x00};
            for (uint32_t i = 0; i < strlen(version); i++) {
                bin_tx_fifo_put(version[i]);
            }
        }

        if(irio_pio_rx_frame_buf(&mod_freq, &us, &pairs, buffer, count_of(buffer)) ){
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
            //no M record if the carrier wasn't measured, AnalysIR then records the signal without one
            if(us){
                ir_rx_modulation_packet(&modulation, (us *1000) / 5); //convert to 1ns ticks
            }
        }
        
        char c;
        while(bin_rx_fifo_try_get(&c)){
            int result = air_rx_char(&tx, c);
            if(result==AIR_OK){ //send the AIR packet
                irio_pio_rx_deinit(bio2bufiopin[BIO5]);
                irio_pio_tx_frame_write((float)(tx.mod_freq*1000), tx.pairs, tx.data);
                irio_pio_rx_init(bio2bufiopin[BIO5]);
            }else if(result!=AIR_PENDING){
                air_tx_error(result);
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
