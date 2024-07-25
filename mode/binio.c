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
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "queue.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "hardware/pwm.h"
#include "pirate/bio.h"
#include "system_config.h"
#include "bytecode.h" //needed because modes.h has some functions that use it TODO: move all the opt args and bytecode stuff to a single helper file
#include "pirate.h"
#include "opt_args.h" //needed for same reason as bytecode and needs same fix
#include "commands.h"
#include "modes.h"
#include "mode/binio.h"
//#include "pirate/pullup.h"
#include "pirate/psu.h"
#include "pirate/amux.h"
#include "sump.h"
#include "binio_helpers.h"
#include "tusb.h"
#include "commands/global/l_bitorder.h"
#include "commands/global/p_pullups.h"
#include "commands/global/cmd_mcu.h"
#include "commands/global/pwm.h"
#include "timestamp.h"
#include "ui/ui_const.h"

void script_print(const char *str) {
    for(uint8_t i=0; i<strlen(str); i++){
        bin_tx_fifo_put(str[i]);
    }
}

void binBBversion(void){
    //const char version_string[]="BBIO1";
    script_print("BBIO2");
}

void script_enabled(void){
    printf("\r\nScripting mode enabled. Terminal locked.\r\n");
}

void script_disabled(void){
    printf("\r\nTerminal unlocked.\r\n");     //fall through to prompt 
}

enum binmode_statemachine {
    BINMODE_COMMAND=0,
    BINMODE_GLOBAL_ARG,
    BINMODE_LOCAL_ARG,
    BINMODE_PRINT_STRING
};
#define BINMODE_MAX_ARGS 7

//0x00 reset
//w/W Power supply (off/ON)
//p/P Pull-up resistors (off/ON)
//a/A/@ x Set IO x state (low/HI/READ)
//v x/V x Show volts on IOx (once/CONT)
// maybe splitting global commands from mode commands 0-127, 127-255 might speed processing?


static uint8_t binmode_args[BINMODE_MAX_ARGS];

struct _binmode_struct{
    uint32_t(*func)(void);
    uint32_t arg_count;
};

// [/{ Start/Start II (mode dependent) 	]/} Stop/Stop II (mode dependent)
// 123 Write value (decimal) 	r Read
// / Clock high 	\ Clock low
// ^ Clock tick 	- Data high
// _ Data low 	. Read data pin state
// d/D Delay 1 us/MS (d:4 to repeat) 	a/A/@.x Set IO.x state (low/HI/READ)
// v.x Measure volts on IO.x 	> Run bus syntax (really a global command…)
enum{
    BM_CONFIG=0,
    BM_WRITE,
    BM_START,
    BM_START_ALT,
    BM_STOP,
    BM_STOP_ALT,
    BM_READ,
    BM_CLKH,
    BM_CLKL,
    BM_TICK,
    BM_DATH,
    BM_DATL,
    BM_BITR,
};

uint32_t binmode_config(void){
    struct _bytecode result; 
    struct _bytecode next;
    if(modes[system_config.mode].binmode_setup()) return 1;
    modes[system_config.mode].protocol_setup_exc();
    return 0;
}

uint32_t binmode_write(void){
    struct _bytecode result; 
    struct _bytecode next;
    char c;

    //arg 0 and 1 are number of bytes to write
    uint16_t bytes_to_write=binmode_args[0]<<8 | binmode_args[1];
    //loop through the bytes
    for(uint32_t i=0;i<bytes_to_write;i++){
        bin_rx_fifo_get_blocking(&c);
        result.out_data=c; 
        modes[system_config.mode].protocol_write(&result, &next);
        if(result.read_with_write){
            bin_tx_fifo_put(result.in_data);
        }
    }
    return 0;
}

uint32_t binmode_start(void){
    struct _bytecode result; 
    struct _bytecode next;
    modes[system_config.mode].protocol_start(&result, &next);
    return 0;
}

uint32_t binmode_start_alt(void){
    struct _bytecode result; 
    struct _bytecode next;
    modes[system_config.mode].protocol_start_alt(&result, &next);
    return 0;
}

uint32_t binmode_stop(void){
    struct _bytecode result; 
    struct _bytecode next;
    modes[system_config.mode].protocol_stop(&result, &next);
    return 0;
}

uint32_t binmode_stop_alt(void){
    struct _bytecode result; 
    struct _bytecode next;
    modes[system_config.mode].protocol_stop_alt(&result, &next);
    return 0;
}

// same as binmode_write, but with a read
uint32_t binmode_read(void){
    struct _bytecode result; 
    struct _bytecode next;

    //arg 0 and 1 are number of bytes to read
    uint16_t bytes_to_read=binmode_args[0]<<8 | binmode_args[1];
    //loop through the bytes
    for(uint32_t i=0;i<bytes_to_read;i++){
        modes[system_config.mode].protocol_read(&result, &next);
        bin_tx_fifo_put(result.in_data);
    }
    modes[system_config.mode].protocol_read(&result, &next);
    return 0;
}

// might have to compare the function to the 
// address of the dummy functions to see if 
// these apply to the mode
uint32_t binmode_clkh(void){
    struct _bytecode result; 
    struct _bytecode next;
    modes[system_config.mode].protocol_clkh(&result, &next);
}

uint32_t binmode_clkl(void){
    struct _bytecode result; 
    struct _bytecode next;
    modes[system_config.mode].protocol_clkl(&result, &next);
}

uint32_t binmode_tick(void){
    struct _bytecode result; 
    struct _bytecode next;
    modes[system_config.mode].protocol_tick_clock(&result, &next);
}
 
uint32_t binmode_dath(void){
    struct _bytecode result; 
    struct _bytecode next;
    modes[system_config.mode].protocol_dath(&result, &next);
}

uint32_t binmode_datl(void){
    struct _bytecode result; 
    struct _bytecode next;
    modes[system_config.mode].protocol_datl(&result, &next);
}

uint32_t binmode_bitr(void){
    struct _bytecode result; 
    struct _bytecode next;
    modes[system_config.mode].protocol_bitr(&result, &next);
}

static const struct _binmode_struct local_commands[]={ 
    [BM_CONFIG]={&binmode_config,0},
    [BM_WRITE]={&binmode_write,1},
    [BM_START]={&binmode_start,0},
    [BM_START_ALT]={&binmode_start_alt,0},
    [BM_STOP]={&binmode_stop,0},
    [BM_STOP_ALT]={&binmode_stop_alt,0},
    [BM_READ]={&binmode_read,0},
    [BM_CLKH]={&binmode_clkh,0}, 
    [BM_CLKL]={&binmode_clkl,0},
    [BM_TICK]={&binmode_tick,0},
    [BM_DATH]={&binmode_dath,0},
    [BM_DATL]={&binmode_datl,0},
    [BM_BITR]={&binmode_bitr,0},
};


enum {
    BM_RESET = 0,
    BM_DEBUG_LEVEL, //1
    BM_POWER_EN, //2
    BM_POWER_DIS, //3
    BM_PULLUP_EN, //4
    BM_PULLUP_DIS, //5
    BM_LIST_MODES, //6
    BM_CHANGE_MODE, //7
    BM_INFO, //8
    BM_VERSION_HW, //9
    BM_VERSION_FW, //10
    BM_BITORDER_MSB, //11
    BM_BITORDER_LSB, //12
    BM_AUX_DIRECTION_MASK, //13
    BM_AUX_READ, //14
    BM_AUX_WRITE_MASK, //15
    BM_ADC_SELECT, //16
    BM_ADC_READ, //17
    BM_ADC_RAW, //18
    BM_PWM_EN, //19
    BM_PWM_DIS, //20
    BM_PWM_RAW, //21
    //BM_PWM_ACTUAL_FREQ, //22
    BM_FREQ, //23
    BM_FREQ_RAW, //24
    BM_DELAY_US, //25
    BM_DELAY_MS, //26
    BM_BOOTLOADER, //27
    BM_RESET_BUSPIRATE, //28
    BM_PRINT_STRING, //29
    //self test
    //disable all interrupt
    //enable all interrupt
    //LEDs?

};
uint8_t binmode_debug=0;

uint32_t binmode_reset(void){
    if(binmode_debug) printf("[RESET] Resetting mode\r\n");
    //bin_tx_fifo_put(0);
    //busy_wait_ms(100);
    modes[system_config.mode].protocol_cleanup();
    system_config.mode=0;
    modes[system_config.mode].protocol_setup();
    return 0;
}

uint32_t binmode_debug_level(void){
    if(binmode_args[0]>1) return 1;
    binmode_debug=binmode_args[0];
    if(binmode_debug) printf("[DEBUG] Enabled\r\n");
    return 0;
}

uint32_t binmode_psu_enable(void){
    if(binmode_args[0]>5 || binmode_args[1]>99){
        if(binmode_debug) printf("[PSU] Invalid voltage: %d.%d\r\n", binmode_args[0], binmode_args[1]); 
        return 10;
    }
    float voltage=(float)(binmode_args[0])+(float)((float)binmode_args[1]/100.00);
    bool current_limit_override=false;
    float current=0;
    if((binmode_args[2]==0xff) && (binmode_args[3]==0xff)){
        current_limit_override=true;
    }else{
        current=(binmode_args[2]<<8)+binmode_args[3]; 
    }    
    if(binmode_debug) printf("[PSU] Voltage: %f, Current: %f, Override: %d\r\n",voltage,current,current_limit_override);
    return psu_enable(voltage,current,current_limit_override);
}

uint32_t binmode_psu_disable(void){
    psu_disable();
    if(binmode_debug) printf("[PSU] Disabled\r\n");
    return 0;
}

uint32_t binmode_pullup_enable(void){
    struct command_result res;
    if(binmode_debug){
        pullups_enable_handler(&res);
        printf("\r\n");
    }else{
        pullups_enable();
    }
    return 0;
}

uint32_t binmode_pullup_disable(void){
    struct command_result res;
    if(binmode_debug){
        pullups_disable_handler(&res);
        printf("\r\n");
    }else{
        pullups_disable();
    }
    return 0;
}

uint32_t mode_list(void){
    for(uint8_t i=0;i<count_of(modes);i++){
        script_print(modes[i].protocol_name);
        if(i< (count_of(modes)-1) ) bin_tx_fifo_put(',');
    }
    //bin_tx_fifo_put(';');
    return 0;
}

uint32_t mode_change(void){
    char mode_name[10];
    for(uint8_t i=0;i<10;i++){
        //capture mode name
        bin_rx_fifo_get_blocking(&mode_name[i]);
        if(mode_name[i]==0x00) break;    
    }
    //compare mode name to modes.protocol_name
    for(uint8_t i=0;i<count_of(modes);i++){
        if(strcmp(mode_name, modes[i].protocol_name)==0){
            modes[system_config.mode].protocol_cleanup();
            system_config.mode=i;
            if(binmode_debug) printf("[MODE] Changed to %s\r\n", modes[system_config.mode].protocol_name);
            return 0;
        }
    }
    return 1;
}

uint32_t binmode_info(void){
    return 1;
}

uint32_t binmode_hwversion(void){
    script_print(BP_HARDWARE_VERSION);
    return 0;
}

uint32_t binmode_fwversion(void){
    script_print(BP_FIRMWARE_HASH);
    return 0;
}

uint32_t binmode_bitorder_msb(void){
    struct command_result res;
    if(binmode_debug){
        bitorder_msb_handler(&res);
        printf("\r\n");
    }else{
        bitorder_msb();
    }
    return 0;
}

uint32_t binmode_bitorder_lsb(void){
    struct command_result res;
    if(binmode_debug){
        bitorder_lsb_handler(&res);
        printf("\r\n");
    }else{
        bitorder_lsb();
    }
    return 0;
}

uint32_t binmode_aux_direction_mask(void){
    for(uint8_t i=0;i<8;i++){
        if(binmode_args[0] & (1<<i)){
            if(binmode_args[1] & 1<<i){
                bio_output(i);
            }else{
                bio_input(i);
            }
            if(binmode_debug) printf("[AUX] set pin %d direction to %d\r\n", i, (bool)(binmode_args[1] & (1<<i)));  
        }
    }
    return 0;
}

uint32_t binmode_aux_read(void){
    uint32_t temp=gpio_get_all();
    temp=temp>>8;
    temp=temp&0xFF;
    if(binmode_debug) printf("[AUX] read: %d\r\n", temp);
    return temp;
}

//two byte argument: pins, output mask
uint32_t binmode_aux_write_mask(void){
    for(uint8_t i=0;i<8;i++){
        if(binmode_args[0] & (1<<i)){
            bio_output(i); //for safety
            bio_put(i, (binmode_args[1] & 1<<i));
            if(binmode_debug) printf("[AUX] write pin %d to %d\r\n", i, (bool)(binmode_args[1] & (1<<i)));  
        }
    }
    return 0;
}

// 1 argument - ADC#
/*
    HW_ADC_MUX_BPIO7, //0
    HW_ADC_MUX_BPIO6, //1
    HW_ADC_MUX_BPIO5, //2
    HW_ADC_MUX_BPIO4, //3
    HW_ADC_MUX_BPIO3, //4
    HW_ADC_MUX_BPIO2, //5
    HW_ADC_MUX_BPIO1, //6
    HW_ADC_MUX_BPIO0, //7
    HW_ADC_MUX_VUSB, //8
    HW_ADC_MUX_CURRENT_DETECT,    //9
    HW_ADC_MUX_VREG_OUT, //10
    HW_ADC_MUX_VREF_VOUT, //11
    CURRENT_SENSE //12 */
uint8_t binmode_adc_channel=0;
uint32_t binmode_adc_select(void){
    
    if(binmode_args[0]>12){
        if(binmode_debug) printf("[ADC] Invalid channel %d\r\n", binmode_args[0]);
        return 1;
    }

    if(binmode_args[0]>11){
        amux_select_input(binmode_args[0]);
    }else if(binmode_args[0]==12){
        amux_read_current();//not really needed, but can be improved in the future
    }
    binmode_adc_channel=binmode_args[0];
    if(binmode_debug) printf("[ADC] selected channel %d\r\n", binmode_args[0]);
    return 0;
}

uint32_t binmode_adc_read(void){
    uint32_t temp;
    if(binmode_adc_channel<12){ // divide by two channels
        temp=amux_read(binmode_adc_channel);
        temp=((6600*temp)/4096);
    }else{
        temp=amux_read_current();
        temp=((temp*3300)/4096);
    }
    if(binmode_debug) printf("[ADC] channel %d read %d.%d\r\n", binmode_adc_channel ,temp/1000, (temp%1000)/100);
    bin_tx_fifo_put(temp/1000);
    return (temp%1000/100);
}

uint32_t binmode_adc_raw(void){
    uint32_t temp;
    if(binmode_adc_channel<12){ // divide by two channels
        temp=amux_read(binmode_adc_channel);
    }else{
        temp=amux_read_current();
    }
    if(binmode_debug) printf("[ADC] channel %d raw read %d\r\n", binmode_adc_channel, temp);
    bin_tx_fifo_put(temp>>8);
    return temp&0xFF;
}

//arguments 6 bytes: IO pin, frequency Hz, duty cycle 0-100
// 0 62500000 50
// 0x00 0x3 B9 AC A0 0x32
uint32_t binmode_pwm_enable(void){

    //label should be 0, not in use
    //FREQ on the B channel should not be in use!
    //PWM should not already be in use on A or B channel of this slice
    
    //bounds check
    if((binmode_args[0])>=count_of(bio2bufiopin)) return 1;
    
    //temp fix for power supply PWM sharing
    #if BP5_REV <= 8
    if((binmode_args[0])==0 || (binmode_args[0])==1) return 1;
    #endif 
    
    //not active or used for frequency
    if(!(system_config.pin_labels[(binmode_args[0])+1]==0 && 
            !(system_config.freq_active & (0b11<<( (uint8_t)( (binmode_args[0])%2 ? (binmode_args[0])-1 : (binmode_args[0]) ) ) ) ) &&
            !(system_config.pwm_active & (0b11<<( (uint8_t)( (binmode_args[0])%2 ? (binmode_args[0])-1 : (binmode_args[0]) ) ) ) )
    )){
        return 2;
    }

    uint slice_num = pwm_gpio_to_slice_num(bio2bufiopin[(uint8_t)binmode_args[0]]);
    uint chan_num= pwm_gpio_to_channel((uint8_t)binmode_args[0]);

    float freq_user_value, pwm_hz_actual_temp, pwm_ns_actual;
    uint32_t freq_int=binmode_args[1]<<24 | binmode_args[2]<<16 | binmode_args[3]<<8 | binmode_args[4];
    freq_user_value=(float)freq_int;

    if(binmode_debug) printf("[PWM] %fHz\r\n", freq_user_value);

    uint32_t pwm_divider, pwm_top, pwm_duty;
    if(pwm_freq_find(&freq_user_value, &pwm_hz_actual_temp, &pwm_ns_actual, &pwm_divider, &pwm_top)){
        return 3;
    }
    pwm_duty=((float)(pwm_top)*(float)((float)binmode_args[5]/100.0))+1;

    system_config.freq_config[binmode_args[0]].period=pwm_hz_actual_temp;
    system_config.freq_config[binmode_args[0]].dutycycle=pwm_duty;

    pwm_set_clkdiv_int_frac(slice_num, pwm_divider>>4,pwm_divider&0b1111);
    pwm_set_wrap(slice_num, pwm_top);
    pwm_set_chan_level(slice_num, chan_num, pwm_duty);

    bio_buf_output((uint8_t)binmode_args[0]);
    gpio_set_function(bio2bufiopin[(uint8_t)binmode_args[0]], GPIO_FUNC_PWM);
    pwm_set_enabled(slice_num, true);
            
    //register the freq active, apply the pin label
    system_bio_claim(true, (uint8_t)binmode_args[0], BP_PIN_PWM, ui_const_pin_states[3]);
    system_set_active(true, (uint8_t)binmode_args[0], &system_config.pwm_active);  
    return 0;
}

uint32_t binmode_pwm_disable(void){
    //bounds check
    if((binmode_args[0])>=count_of(bio2bufiopin)) return 1;

    if( !(system_config.pwm_active&(0x01<<binmode_args[0])) ) {
        if(binmode_debug) printf("[PWM] Not active on IO %d\r\n", binmode_args[0]);
        return 2;
    }
    //disable    
    pwm_set_enabled(pwm_gpio_to_slice_num(bio2bufiopin[(uint8_t)binmode_args[0]]), false);
    gpio_set_function(bio2bufiopin[(uint8_t)binmode_args[0]], GPIO_FUNC_SIO);
    bio_input((uint8_t)binmode_args[0]);
    
    //unregister, remove pin label
    system_bio_claim(false, (uint8_t)binmode_args[0], 0, 0);
    system_set_active(false, (uint8_t)binmode_args[0], &system_config.pwm_active);     

    if(binmode_debug) printf("[PWM] Disabled\r\n");
    return 0;
}

uint32_t binmode_pwm_raw(void){
  //label should be 0, not in use
    //FREQ on the B channel should not be in use!
    //PWM should not already be in use on A or B channel of this slice
    
    //bounds check
    if((binmode_args[0])>=count_of(bio2bufiopin)) return 1;
    
    //temp fix for power supply PWM sharing
    #if BP5_REV <= 8
    if((binmode_args[0])==0 || (binmode_args[0])==1) return 1;
    #endif 
    
    //not active or used for frequency
    if(!(system_config.pin_labels[(binmode_args[0])+1]==0 && 
            !(system_config.freq_active & (0b11<<( (uint8_t)( (binmode_args[0])%2 ? (binmode_args[0])-1 : (binmode_args[0]) ) ) ) ) &&
            !(system_config.pwm_active & (0b11<<( (uint8_t)( (binmode_args[0])%2 ? (binmode_args[0])-1 : (binmode_args[0]) ) ) ) )
    )){
        return 2;
    }

    uint slice_num = pwm_gpio_to_slice_num(bio2bufiopin[(uint8_t)binmode_args[0]]);
    uint chan_num= pwm_gpio_to_channel((uint8_t)binmode_args[0]);

    uint16_t pwm_divider = binmode_args[1]<<8 | binmode_args[2];
    uint16_t pwm_top = binmode_args[3]<<8 | binmode_args[4];
    uint16_t pwm_duty = binmode_args[5]<<8 | binmode_args[6];
    if(binmode_debug) printf("[PWM] top %d, divider %d, duty %d\r\n", pwm_top, pwm_divider, pwm_duty);

    pwm_set_clkdiv_int_frac(slice_num, pwm_divider>>4,pwm_divider&0b1111);
    pwm_set_wrap(slice_num, pwm_top);
    pwm_set_chan_level(slice_num, chan_num, pwm_duty);

    bio_buf_output((uint8_t)binmode_args[0]);
    gpio_set_function(bio2bufiopin[(uint8_t)binmode_args[0]], GPIO_FUNC_PWM);
    pwm_set_enabled(slice_num, true);
            
    //register the freq active, apply the pin label
    system_bio_claim(true, (uint8_t)binmode_args[0], BP_PIN_PWM, ui_const_pin_states[3]);
    system_set_active(true, (uint8_t)binmode_args[0], &system_config.pwm_active);  
    return 0;
}


uint32_t binmode_freq(void){
    if(binmode_debug) printf("[FREQ] %d.%d\r\n", binmode_args[0], binmode_args[1]);
    return 0;
}

uint32_t binmode_freq_raw(void){
    if(binmode_debug) printf("[FREQ] %d.%d\r\n", binmode_args[0], binmode_args[1]);
    return 0;
}

uint32_t binmode_delay_us(void){
    if(binmode_debug) printf("[DELAY] %dus\r\n", binmode_args[0]);
    busy_wait_us(binmode_args[0]);
    return 0;
}

uint32_t binmode_delay_ms(void){
    if(binmode_debug) printf("[DELAY] %dms\r\n", binmode_args[0]);
    busy_wait_ms(binmode_args[0]);
    return 0;
}

uint32_t binmode_bootloader(void){
    if(binmode_debug){
        printf("[BOOTLOADER] Jumping to bootloader\r\n");
        busy_wait_ms(100);
    }
    cmd_mcu_jump_to_bootloader();
    return 0;
}

uint32_t binmode_reset_buspirate(void){
    if(binmode_debug){
        printf("[RESET] Resetting Bus Pirate\r\n");
        busy_wait_ms(100);
    }
    cmd_mcu_reset();
    return 0;
}

static const struct _binmode_struct global_commands[]={ 
    [BM_RESET]={&binmode_reset,0},
    [BM_DEBUG_LEVEL]={&binmode_debug_level,1},
    [BM_POWER_EN]={&binmode_psu_enable,4},
    [BM_POWER_DIS]={&binmode_psu_disable,0},
    [BM_PULLUP_EN]={&binmode_pullup_enable,0},
    [BM_PULLUP_DIS]={&binmode_pullup_disable,0},
    [BM_LIST_MODES]={&mode_list,0},
    [BM_CHANGE_MODE]={&mode_change,1},
    [BM_INFO]={&binmode_info, 0},
    [BM_VERSION_HW]={&binmode_hwversion,0}, 
    [BM_VERSION_FW]={&binmode_fwversion,0}, 
    [BM_BITORDER_MSB]={&binmode_bitorder_msb,0}, 
    [BM_BITORDER_LSB]={&binmode_bitorder_lsb,0}, 
    [BM_AUX_DIRECTION_MASK]={&binmode_aux_direction_mask,2}, 
    [BM_AUX_READ]={&binmode_aux_read,0}, 
    [BM_AUX_WRITE_MASK]={&binmode_aux_write_mask,2}, 
    [BM_ADC_SELECT]={&binmode_adc_select,1}, 
    [BM_ADC_READ]={&binmode_adc_read,0}, 
    [BM_ADC_RAW]={&binmode_adc_raw,0}, 
    [BM_PWM_EN]={&binmode_pwm_enable,6},
    [BM_PWM_DIS]={&binmode_pwm_disable,1},
    [BM_PWM_RAW]={&binmode_pwm_raw,7},
    [BM_FREQ]={&binmode_freq,2},
    [BM_FREQ_RAW]={&binmode_freq_raw,2},
    [BM_DELAY_US]={&binmode_delay_us,1},
    [BM_DELAY_MS]={&binmode_delay_ms,1},
    [BM_BOOTLOADER]={&binmode_bootloader,0},
    [BM_RESET_BUSPIRATE]={&binmode_reset_buspirate,0},
    [BM_PRINT_STRING]={0,0},
};

// NOTE: THIS DOES NOT WORK ATM BECAUSE I TRIED TO GET CUTE AND BROKE LOTS OF STUFF
// NOTE2: I'M PRETTY UNHAPPY WITH REPEATING THE TESTS FOR GLOBAL/LOCAL, BUT I WANT THAT SEPARATION.
// handler needs to be cooperative multitasking until mode is enabled
bool script_mode(void){
    static uint8_t binmode_state=BINMODE_COMMAND;
    static uint8_t binmode_command;

    static uint8_t binmode_arg_count=0;
    //could activate binmode just by opening the port?
    //if(!tud_cdc_n_connected(1)) return false;  
    //if(!tud_cdc_n_available(1)) return false;
    //script_enabled();
    //while(true){
        //do an echo test so we can interface via a mode ;)
        //if(tud_cdc_n_available(1)){
            char c;
            uint32_t temp;
            if(bin_rx_fifo_try_get(&c)){
                //bin_tx_fifo_put(c); //echo for debug   
        
                switch(binmode_state){
                    case BINMODE_COMMAND:
                        if(c<0x7f){

                            if(c>=count_of(global_commands)){
                                if(binmode_debug) printf("[MAIN] Invalid global command %d\r\n",c);
                                bin_tx_fifo_put(1);
                                break;
                            } 

                            if(binmode_debug) printf("[MAIN] Global command %d, args: %d\r\n",c, global_commands[c].arg_count);
                            binmode_command=c;

                            if(binmode_command==BM_PRINT_STRING){
                                binmode_state=BINMODE_PRINT_STRING;
                                binmode_arg_count=0;
                                break;
                            }
                            
                            if(global_commands[c].arg_count){ 
                                binmode_arg_count=global_commands[c].arg_count;
                                binmode_state=BINMODE_GLOBAL_ARG;
                            }else{
                                goto do_global_command;
                            }
                            break;
                        }

                        if(c>0x7f){
                            c=c&0b01111111; //clear upper
                            if(c>=count_of(local_commands)){                                
                                if(binmode_debug) printf("[MAIN] Invalid local command %d\r\n",c);
                                bin_tx_fifo_put(1);
                                break;
                            } 

                            if(binmode_debug) printf("[MAIN] Local command %d, args: %d\r\n",c, local_commands[c].arg_count);
                            binmode_command=c;

                            if(local_commands[c].arg_count){ 
                                binmode_arg_count=local_commands[c].arg_count;
                                binmode_state=BINMODE_LOCAL_ARG;
                            }else{
                                goto do_local_command;
                            }
                            break;
                        }
                        break; //invalid command
                    case BINMODE_GLOBAL_ARG:
                        binmode_args[(global_commands[binmode_command].arg_count - binmode_arg_count)]=c;
                        binmode_arg_count--;
                        if(!binmode_arg_count){
                    do_global_command:
                            temp = global_commands[binmode_command].func();
                            if(binmode_debug) printf("[MAIN] Global command %d returned %d\r\n", binmode_command, temp);
                            bin_tx_fifo_put(temp);
                            binmode_state=BINMODE_COMMAND;
                        }
                        break;
                    case BINMODE_LOCAL_ARG:
                        binmode_args[(local_commands[binmode_command].arg_count - binmode_arg_count)]=c;
                        binmode_arg_count--;
                        if(!binmode_arg_count){
                    do_local_command:
                            temp = local_commands[binmode_command].func();
                            if(binmode_debug) printf("[MAIN] Local command %d returned %d\r\n", binmode_command, temp);
                            bin_tx_fifo_put(temp);
                            binmode_state=BINMODE_COMMAND;
                        }
                        break;                        
                    case BINMODE_PRINT_STRING:
                        if(c==0x00){
                            printf("\r\n");
                            binmode_state=BINMODE_COMMAND;
                            break;
                        }
                        printf("%c",c);
                        break;

                }
            
            }
        //}

    //}
    return false;
}

 
 



