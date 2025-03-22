#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"
#include "modes.h"
#include "ui/ui_prompt.h"
#include "ui/ui_parse.h"
#include "ui/ui_const.h"
#include "ui/ui_term.h"
#include "ui/ui_cmdln.h"
#include "syntax.h"
#include "pirate/bio.h"
#include "pirate/amux.h"

// print the in and out arrays, other debug info
//#define SYNTAX_DEBUG

//TODO: big cleanup...
// divide into three or four files

//A. syntax begins with bus start [ or /
//B. some kind of final byte before stop flag? look ahead?
//1. Compile loop: process all commands to bytecode
//2. Run loop: run the bytecode all at once
//3. Post-process loop: process output into UI
//mode commands:
//start
//stop
//write
//read
//global commands:
//delay
//aux pins
//adc
//pwm?
//freq?
#define SYN_MAX_LENGTH 1024


struct _syntax_io {
    struct _bytecode out[SYN_MAX_LENGTH];
    struct _bytecode in[SYN_MAX_LENGTH];
    uint32_t out_cnt;
    uint32_t in_cnt;
} syntax_io = { .out_cnt = 0, .in_cnt = 0 };

// empty bytecode for quick zero init
const struct _bytecode bytecode_empty; 

struct _output_info {
    uint8_t previous_command;
    uint8_t previous_number_format;
    uint8_t row_length;
    uint8_t row_counter;
};

void postprocess_mode_write(struct _bytecode* in, struct _output_info* info);
void postprocess_format_print_number(struct _bytecode* in, uint32_t* value, bool read);

struct _syntax_compile_commands_t{
    char symbol;
    uint8_t code;
};

const struct _syntax_compile_commands_t syntax_compile_commands[] = {
    {'r', SYN_READ},
    {'[', SYN_START},
    {'{', SYN_START_ALT},
    {']', SYN_STOP},
    {'}', SYN_STOP_ALT},
    {'d', SYN_DELAY_US},
    {'D', SYN_DELAY_MS},
    {'^', SYN_TICK_CLOCK},
    {'/', SYN_SET_CLK_HIGH},
    {'\\', SYN_SET_CLK_LOW},
    {'_', SYN_SET_DAT_LOW},
    {'-', SYN_SET_DAT_HIGH},
    {'.', SYN_READ_DAT},
    {'a', SYN_AUX_OUTPUT_LOW},
    {'A', SYN_AUX_OUTPUT_HIGH},
    {'@', SYN_AUX_INPUT},
    {'v', SYN_ADC}
};

SYNTAX_STATUS syntax_compile(void) {
    uint32_t current_position = 0;
    uint32_t generated_in_cnt = 0;
    uint32_t i;
    char c;

    syntax_io.out_cnt = 0;
    syntax_io.in_cnt = 0;
    for (i = 0; i < SYN_MAX_LENGTH; i++) {
        syntax_io.out[i] = bytecode_empty;
        syntax_io.in[i] = bytecode_empty;
    }

    // we need to track pin functions to avoid blowing out any existing pins
    // if a conflict is found, the compiler can throw an error
    enum bp_pin_func pin_func[HW_PINS - 2];
    for (i = 1; i < HW_PINS - 1; i++) {
        pin_func[i - 1] = system_config.pin_func[i]; //=BP_PIN_IO;
    }

    while (cmdln_try_peek(0, &c)) {
        current_position++;

        if (c <= ' ' || c > '~' || c =='>') {
            // out of ascii range, or > syntax indication character
            cmdln_try_discard(1);
            continue;
        }

        // if number parse it
        if (c >= '0' && c <= '9') {
            struct prompt_result result;
            ui_parse_get_int(&result, &syntax_io.out[syntax_io.out_cnt].out_data);
            if (result.error) {
                printf("Error parsing integer at position %d\r\n", current_position);
                return SSTATUS_ERROR;
            }
            syntax_io.out[syntax_io.out_cnt].command = SYN_WRITE;
            syntax_io.out[syntax_io.out_cnt].number_format = result.number_format;
            goto compiler_get_attributes;
        }
        
        //if string, parse it
        if (c == '"') {
            cmdln_try_remove(&c); // remove "
            // sanity check! is there a terminating "?
            i = 0;
            while (cmdln_try_peek(i, &c)) {
                if (c == '"') {
                    if((syntax_io.out_cnt+i)>=SYN_MAX_LENGTH){
                        printf("Syntax exceeds available space (%d slots)\r\n", SYN_MAX_LENGTH);
                        return SSTATUS_ERROR;
                    }      
                    goto compile_get_string;
                }
                i++;
            }
            printf("Error: string missing terminating '\"'");
            return SSTATUS_ERROR;

compile_get_string:
            while (i--) {
                cmdln_try_remove(&c);
                syntax_io.out[syntax_io.out_cnt].command = SYN_WRITE;
                syntax_io.out[syntax_io.out_cnt].out_data = c;
                syntax_io.out[syntax_io.out_cnt].has_repeat = false;
                syntax_io.out[syntax_io.out_cnt].repeat = 1;
                syntax_io.out[syntax_io.out_cnt].number_format = df_ascii;
                syntax_io.out[syntax_io.out_cnt].bits = system_config.num_bits;
                syntax_io.out_cnt++;
                generated_in_cnt++;
            }
            cmdln_try_remove(&c); // consume the final "
            if (generated_in_cnt >= SYN_MAX_LENGTH) {
                printf("Syntax exceeds available space (%d slots)\r\n", SYN_MAX_LENGTH);
                return SSTATUS_ERROR;
            }
            continue;
        } 
        
        uint8_t cmd=0xff;
        for (i = 0; i < count_of(syntax_compile_commands); i++) {
            if (c == syntax_compile_commands[i].symbol) {
                syntax_io.out[syntax_io.out_cnt].command  = syntax_compile_commands[i].code;
                // parsing an int value from the command line sets the pointer to the next value
                // if it's another command, we need to do that manually now to keep the pointer
                // where the next parsing function expects it
                cmdln_try_discard(1);
                goto compiler_get_attributes;
            }
        }
        printf("Unknown syntax '%c' at position %d\r\n", c, current_position);
        return SSTATUS_ERROR;     

compiler_get_attributes:

        if (ui_parse_get_dot(&syntax_io.out[syntax_io.out_cnt].bits)) {
            syntax_io.out[syntax_io.out_cnt].has_bits = true;
        } else {
            syntax_io.out[syntax_io.out_cnt].has_bits = false;
            syntax_io.out[syntax_io.out_cnt].bits = system_config.num_bits;
        }

        if (ui_parse_get_colon(&syntax_io.out[syntax_io.out_cnt].repeat)) {
            syntax_io.out[syntax_io.out_cnt].has_repeat = true;
        } else {
            syntax_io.out[syntax_io.out_cnt].has_repeat = false;
            syntax_io.out[syntax_io.out_cnt].repeat = 1;
        }

        //these syntax commands need to specify a pin
        if (syntax_io.out[syntax_io.out_cnt].command >= SYN_AUX_OUTPUT_HIGH) {
            if (syntax_io.out[syntax_io.out_cnt].has_bits == false) {
                printf("Error: missing IO number for command %c at position %d. Try %c.0\r\n", c, current_position, c);
                return SSTATUS_ERROR;
            }

            if (syntax_io.out[syntax_io.out_cnt].bits >= count_of(bio2bufiopin)) {
                printf("%sError:%s pin IO%d is invalid\r\n",
                       ui_term_color_error(),
                       ui_term_color_reset(),
                       syntax_io.out[syntax_io.out_cnt].bits);
                return SSTATUS_ERROR;
            }

            if (syntax_io.out[syntax_io.out_cnt].command != SYN_ADC && pin_func[syntax_io.out[syntax_io.out_cnt].bits] != BP_PIN_IO) {
                printf("%sError:%s at position %d IO%d is already in use\r\n",
                       ui_term_color_error(),
                       ui_term_color_reset(),
                       current_position,
                       syntax_io.out[syntax_io.out_cnt].bits);
                return SSTATUS_ERROR;
            }
            // AUX high and low need to set function until changed to read again...
        }

        //track the number of "in" slots that will be used to avoid overflow
        //TODO: this does not account for strings....
        if (syntax_io.out[syntax_io.out_cnt].command == SYN_DELAY_US || syntax_io.out[syntax_io.out_cnt].command == SYN_DELAY_MS ||
            syntax_io.out[syntax_io.out_cnt].command == SYN_TICK_CLOCK) // delays repeat but don't take up slots
        {
            generated_in_cnt += 1;
        } else {
            generated_in_cnt += syntax_io.out[syntax_io.out_cnt].repeat;
        }

        if (generated_in_cnt >= SYN_MAX_LENGTH) {
            printf("Syntax exceeds available space (%d slots)\r\n", SYN_MAX_LENGTH);
            return SSTATUS_ERROR;
        }

        syntax_io.out_cnt++;
    }

    #ifdef SYNTAX_DEBUG
    for(i=0; i<syntax_io.out_cnt; i++){
        printf("%d:%d\r\n", syntax_io.out[i].command, syntax_io.out[i].repeat);
    }
    #endif

    syntax_io.in_cnt = 0;
    return SSTATUS_OK;
}
/*
*
*   Run/execute the syntax_io bytecode
*
*/
static const char labels[][5] = { "AUXL", "AUXH" };

typedef void (*syntax_run_func_ptr_t)(struct _syntax_io* syntax_io, uint32_t current_position );

void syntax_run_write(struct _syntax_io* syntax_io, uint32_t current_position) {
    if (syntax_io->in_cnt + syntax_io->out[current_position].repeat >= SYN_MAX_LENGTH) {
        syntax_io->in[syntax_io->in_cnt].error_message = GET_T(T_SYNTAX_EXCEEDS_MAX_SLOTS);
        syntax_io->in[syntax_io->in_cnt].error = SERR_ERROR;
        return;
    }
    for (uint32_t j = 0; j < syntax_io->out[current_position].repeat; j++) {
        if (j > 0) {
            syntax_io->in_cnt++;
            syntax_io->in[syntax_io->in_cnt] = syntax_io->out[current_position];
        }
        modes[system_config.mode].protocol_write(&syntax_io->in[syntax_io->in_cnt], NULL);
    }
}

void syntax_run_read(struct _syntax_io* syntax_io, uint32_t current_position) {
    if (syntax_io->in_cnt + syntax_io->out->repeat >= SYN_MAX_LENGTH) {
        syntax_io->in[syntax_io->in_cnt].error_message = GET_T(T_SYNTAX_EXCEEDS_MAX_SLOTS);
        syntax_io->in[syntax_io->in_cnt].error = SERR_ERROR;
        return;
    }
    #ifdef SYNTAX_DEBUG
        printf("[DEBUG] repeat %d, pos %d, cmd: %d\r\n", syntax_io->out[current_position].repeat, current_position, syntax_io->out[current_position].command);
    #endif
    for (uint32_t j = 0; j < syntax_io->out[current_position].repeat; j++) {
        if (j > 0) {
            syntax_io->in_cnt++;
            syntax_io->in[syntax_io->in_cnt] = syntax_io->out[current_position];
        }
        modes[system_config.mode].protocol_read(
            &syntax_io->in[syntax_io->in_cnt], 
            ((current_position + 1 < syntax_io->out_cnt) && 
            (j + 1 == syntax_io->out[current_position].repeat)) ? 
            &syntax_io->out[current_position + 1] : NULL
        );
    }
}

void syntax_run_start(struct _syntax_io* syntax_io, uint32_t current_position) {
    modes[system_config.mode].protocol_start(&syntax_io->in[syntax_io->in_cnt], NULL);
}

void syntax_run_start_alt(struct _syntax_io* syntax_io, uint32_t current_position) {
    modes[system_config.mode].protocol_start_alt(&syntax_io->in[syntax_io->in_cnt], NULL);
}

void syntax_run_stop(struct _syntax_io* syntax_io, uint32_t current_position) {
    modes[system_config.mode].protocol_stop(&syntax_io->in[syntax_io->in_cnt], NULL);
}

void syntax_run_stop_alt(struct _syntax_io* syntax_io, uint32_t current_position) {
    modes[system_config.mode].protocol_stop_alt(&syntax_io->in[syntax_io->in_cnt], NULL);
}

void syntax_run_delay_us(struct _syntax_io* syntax_io, uint32_t current_position) {
    busy_wait_us_32(syntax_io->out[current_position].repeat);
}

void syntax_run_delay_ms(struct _syntax_io* syntax_io, uint32_t current_position) {
    busy_wait_ms(syntax_io->out[current_position].repeat);
}

static inline void _syntax_run_aux_output(uint8_t bio, bool direction) {
    bio_output(bio);
    bio_put(bio, direction);
    system_bio_update_purpose_and_label(
        true,
        bio,
        BP_PIN_IO,
        labels[direction]); // this should be moved to a cleanup function to reduce overhead
    system_set_active(true, bio, &system_config.aux_active);
}

void syntax_run_aux_output_high(struct _syntax_io* syntax_io, uint32_t current_position) {
    _syntax_run_aux_output(syntax_io->out[current_position].bits, true);
}

void syntax_run_aux_output_low(struct _syntax_io* syntax_io, uint32_t current_position) {
    _syntax_run_aux_output(syntax_io->out[current_position].bits, false);
}

void syntax_run_aux_input(struct _syntax_io* syntax_io, uint32_t current_position) {
    bio_input(syntax_io->out[current_position].bits);
    syntax_io->in[syntax_io->in_cnt].in_data = bio_get(syntax_io->out[current_position].bits);
    system_bio_update_purpose_and_label(false, syntax_io->out[current_position].bits, BP_PIN_IO, 0);
    system_set_active(false, syntax_io->out[current_position].bits, &system_config.aux_active);  
}

void syntax_run_adc(struct _syntax_io* syntax_io, uint32_t current_position) {
    syntax_io->in[syntax_io->in_cnt].in_data = amux_read_bio(syntax_io->out[current_position].bits);
}

void syntax_run_tick_clock(struct _syntax_io* syntax_io, uint32_t current_position) {
    for (uint32_t j = 0; j < syntax_io->out[current_position].repeat; j++) {
        modes[system_config.mode].protocol_tick_clock(&syntax_io->in[syntax_io->in_cnt], NULL);
    }
}

void syntax_run_set_clk_high(struct _syntax_io* syntax_io, uint32_t current_position) {
    modes[system_config.mode].protocol_clkh(&syntax_io->in[syntax_io->in_cnt], NULL);
}

void syntax_run_set_clk_low(struct _syntax_io* syntax_io, uint32_t current_position) {
    modes[system_config.mode].protocol_clkl(&syntax_io->in[syntax_io->in_cnt], NULL);
}

void syntax_run_set_dat_high(struct _syntax_io* syntax_io, uint32_t current_position) {
    modes[system_config.mode].protocol_dath(&syntax_io->in[syntax_io->in_cnt], NULL);
}

void syntax_run_set_dat_low(struct _syntax_io* syntax_io, uint32_t current_position) {
    modes[system_config.mode].protocol_datl(&syntax_io->in[syntax_io->in_cnt], NULL);
}

void syntax_run_read_dat(struct _syntax_io* syntax_io, uint32_t current_position) {
    //TODO: reality check out slots, actually repeat the read?
    for (uint32_t j = 0; j < syntax_io->out[current_position].repeat; j++) {
        modes[system_config.mode].protocol_bitr(&syntax_io->in[syntax_io->in_cnt], NULL);
    }
}

//a struct of function pointers to run the commands
syntax_run_func_ptr_t syntax_run_func[]={
    [SYN_WRITE]=syntax_run_write,
    [SYN_READ]=syntax_run_read,
    [SYN_START]=syntax_run_start,
    [SYN_START_ALT]=syntax_run_start_alt,
    [SYN_STOP]=syntax_run_stop,
    [SYN_STOP_ALT]=syntax_run_stop_alt,
    [SYN_DELAY_US]=syntax_run_delay_us,
    [SYN_DELAY_MS]=syntax_run_delay_ms,
    [SYN_AUX_OUTPUT_HIGH]=syntax_run_aux_output_high,
    [SYN_AUX_OUTPUT_LOW]=syntax_run_aux_output_low,
    [SYN_AUX_INPUT]=syntax_run_aux_input,
    [SYN_ADC]=syntax_run_adc,
    [SYN_TICK_CLOCK]=syntax_run_tick_clock,
    [SYN_SET_CLK_HIGH]=syntax_run_set_clk_high,
    [SYN_SET_CLK_LOW]=syntax_run_set_clk_low,
    [SYN_SET_DAT_HIGH]=syntax_run_set_dat_high,
    [SYN_SET_DAT_LOW]=syntax_run_set_dat_low,
    [SYN_READ_DAT]=syntax_run_read_dat
};

SYNTAX_STATUS syntax_run(void) {
    uint32_t current_position;

    if (!syntax_io.out_cnt) return SSTATUS_ERROR;

    syntax_io.in_cnt = 0;

    for (current_position = 0; current_position < syntax_io.out_cnt; current_position++) {
        syntax_io.in[syntax_io.in_cnt] = syntax_io.out[current_position];

        if (syntax_io.out[current_position].command >= count_of(syntax_run_func)) {
            printf("Unknown internal code %d\r\n", syntax_io.out[current_position].command);
            return SSTATUS_ERROR;
        }

        syntax_run_func[syntax_io.out[current_position].command](&syntax_io, current_position);

        if (syntax_io.in_cnt + 1 >= SYN_MAX_LENGTH) {
            syntax_io.in[syntax_io.in_cnt].error_message = GET_T(T_SYNTAX_EXCEEDS_MAX_SLOTS);
            syntax_io.in[syntax_io.in_cnt].error = SERR_ERROR;
            return SSTATUS_OK; // halt execution, but let the post process show the error.
        }

        // this will pick up any errors from the void functions
        if (syntax_io.in[syntax_io.in_cnt].error >= SERR_ERROR) {
            syntax_io.in_cnt++; //is this needed?
            return SSTATUS_OK; // halt execution, but let the post process show the error.
        }

        syntax_io.in_cnt++;
    }

    #ifdef SYNTAX_DEBUG
    printf("Out:\r\n");
    for(uint32_t i=0; i<syntax_io.out_cnt; i++){
        printf("%d:%d\r\n", syntax_io.out[i].command, syntax_io.out[i].repeat);
    }
    printf("In:\r\n");
    for (uint32_t i = 0; i < syntax_io.in_cnt; i++) {
        printf("%d:%d\r\n", syntax_io.in[i].command, syntax_io.in[i].repeat);
    }
    #endif

    return SSTATUS_OK;
}


void syntax_post_write(struct _bytecode* in, struct _output_info* info) {
    postprocess_mode_write(in, info);
}

void syntax_post_delay_us_ms(struct _bytecode* in, struct _output_info* info) {
    printf("\r\n%s%s:%s %s%d%s%s",
              ui_term_color_notice(),   
                GET_T(T_MODE_DELAY),
                ui_term_color_reset(),
                ui_term_color_num_float(),
                in->repeat,
                ui_term_color_reset(),
                (in->command == SYN_DELAY_US ? GET_T(T_MODE_US) : GET_T(T_MODE_MS)));
}

void syntax_post_read(struct _bytecode* in, struct _output_info* info) {
    postprocess_mode_write(in, info);
}

void syntax_post_start_stop(struct _bytecode* in, struct _output_info* info) {
    if (in->data_message) {
        printf("\r\n%s", in->data_message);
    }
}

static inline void _syntax_post_aux_output(uint8_t bio, bool direction) {
    printf("\r\nIO%s%d%s set to%s OUTPUT: %s%d%s",
                ui_term_color_num_float(),
                bio,
                ui_term_color_notice(),
                ui_term_color_reset(),
                ui_term_color_num_float(),
                direction,
                ui_term_color_reset());
}

void syntax_post_aux_output_high(struct _bytecode* in, struct _output_info* info) {
    _syntax_post_aux_output(in->bits, 1);
}

void syntax_post_aux_output_low(struct _bytecode* in, struct _output_info* info) {
    _syntax_post_aux_output(in->bits, 0);
}

void syntax_post_aux_input(struct _bytecode* in, struct _output_info* info) {
    printf("\r\nIO%s%d%s set to%s INPUT: %s%d%s",
              ui_term_color_num_float(),
                in->bits,
                ui_term_color_notice(),
                ui_term_color_reset(),
                ui_term_color_num_float(),
                in->in_data,
                ui_term_color_reset());
}

void syntax_post_adc(struct _bytecode* in, struct _output_info* info) {
    uint32_t received = (6600 * in->in_data) / 4096;
    printf("\r\n%s%s IO%d:%s %s%d.%d%sV",
              ui_term_color_info(),
                GET_T(T_MODE_ADC_VOLTAGE),
                in->bits,
                ui_term_color_reset(),
                ui_term_color_num_float(),
                ((received) / 1000),
                (((received) % 1000) / 100),
                ui_term_color_reset());
}

void syntax_post_tick_clock(struct _bytecode* in, struct _output_info* info) {
    printf("\r\n%s%s:%s %s%d%s",
                ui_term_color_notice(),
                GET_T(T_MODE_TICK_CLOCK),
                ui_term_color_reset(),
                ui_term_color_num_float(),
                in->repeat,
                ui_term_color_reset());
}

void syntax_post_set_clk_high_low(struct _bytecode* in, struct _output_info* info) {
    printf("\r\n%s%s:%s %s%d%s",
                ui_term_color_notice(),
                GET_T(T_MODE_SET_CLK),
                ui_term_color_reset(),
                ui_term_color_num_float(),
                in->out_data,
                ui_term_color_reset());
}

void syntax_post_set_dat_high_low(struct _bytecode* in, struct _output_info* info) {
    printf("\r\n%s%s:%s %s%d%s",
                ui_term_color_notice(),
                GET_T(T_MODE_SET_DAT),
                ui_term_color_reset(),
                ui_term_color_num_float(),
                in->out_data,
                ui_term_color_reset());
} 

void syntax_post_read_dat(struct _bytecode* in, struct _output_info* info) {
    printf("\r\n%s%s:%s %s%d%s",
                ui_term_color_notice(),
                GET_T(T_MODE_READ_DAT),
                ui_term_color_reset(),
                ui_term_color_num_float(),
                in->in_data,
                ui_term_color_reset());
}

typedef void (*syntax_post_func_ptr_t)(struct _bytecode* in, struct _output_info* info);

syntax_post_func_ptr_t syntax_post_func[] = {
    [SYN_WRITE] = syntax_post_write,
    [SYN_READ] = syntax_post_read,
    [SYN_START] = syntax_post_start_stop,
    [SYN_START_ALT] = syntax_post_start_stop,
    [SYN_STOP] = syntax_post_start_stop,
    [SYN_STOP_ALT] = syntax_post_start_stop,
    [SYN_DELAY_US] = syntax_post_delay_us_ms,
    [SYN_DELAY_MS] = syntax_post_delay_us_ms,
    [SYN_AUX_OUTPUT_HIGH] = syntax_post_aux_output_high,
    [SYN_AUX_OUTPUT_LOW] = syntax_post_aux_output_low,
    [SYN_AUX_INPUT] = syntax_post_aux_input,
    [SYN_ADC] = syntax_post_adc,
    [SYN_TICK_CLOCK] = syntax_post_tick_clock,
    [SYN_SET_CLK_HIGH] = syntax_post_set_clk_high_low,
    [SYN_SET_CLK_LOW] = syntax_post_set_clk_high_low,
    [SYN_SET_DAT_HIGH] = syntax_post_set_dat_high_low,
    [SYN_SET_DAT_LOW] = syntax_post_set_dat_high_low,
    [SYN_READ_DAT] = syntax_post_read_dat
};

SYNTAX_STATUS syntax_post(void) {
    uint32_t current_position, received;
    static struct _output_info info;

    if (!syntax_io.in_cnt) return SSTATUS_ERROR;
    
    info.previous_command = 0xff; // set invalid command so output display works
    for (current_position = 0; current_position < syntax_io.in_cnt; current_position++) {
        if (syntax_io.in[current_position].command >= count_of(syntax_post_func)) {
            printf("Unknown internal code %d\r\n", syntax_io.in[current_position].command);
            continue;
        }

        syntax_post_func[syntax_io.in[current_position].command](&syntax_io.in[current_position], &info);
        info.previous_command = syntax_io.in[current_position].command;

        if (syntax_io.in[current_position].error) {
            printf("(%s) ", syntax_io.in[current_position].error_message);
        }
    }
    printf("\r\n");
    syntax_io.in_cnt = 0;
    return SSTATUS_OK;
}

void postprocess_mode_write(struct _bytecode* in, struct _output_info* info) {
    uint32_t repeat;
    uint32_t value;
    uint8_t row_length;
    bool new_line = false;

    // how many numbers per row
    row_length = 8;
    if (in->number_format == df_bin || system_config.display_format == df_ascii) {
        row_length = 4;
    }

    // if number format changed, make a new row
    if (in->number_format != info->previous_number_format || in->command != info->previous_command) {
        new_line = true;
        info->row_counter = info->row_length = row_length;
        info->previous_number_format = in->number_format;
    }

    if (in->command == SYN_WRITE) //(!system_config.write_with_read)
    {
        value = in->out_data;
        repeat = 1;
        if (new_line) {
            printf("\r\n%sTX:%s ", ui_term_color_info(), ui_term_color_reset());
        }
    }

    if (in->command == SYN_READ) //(!system_config.write_with_read)
    {
        repeat = 1;
        value = in->in_data;
        if (new_line) {
            printf("\r\n%sRX:%s ", ui_term_color_info(), ui_term_color_reset());
        }
    }

    while (repeat--) {
        postprocess_format_print_number(in, &value, (in->command == SYN_READ));
        if (in->read_with_write) {
            printf("(");
            postprocess_format_print_number(in, &in->in_data, false);
            printf(")");
        }

        info->row_counter--;
        if (in->data_message) {
            printf(" %s ", in->data_message);
        } else {
            printf(" ");
        }

        if (!info->row_counter) {
            printf("\r\n    ");
            info->row_counter = row_length;
        }
    }
}

// represent d in the current display mode. If numbits=8 also display the ascii representation
void postprocess_format_print_number(struct _bytecode* in, uint32_t* value, bool read) {
    uint32_t mask, i, d, j;
    uint8_t num_bits, num_nibbles, display_format;
    bool color_flip;

    d = (*value);

    num_bits = in->bits;

    // maybe just tell it if we're reading or writing, instead of this convoluted logic pretzel
    if (!read && (system_config.display_format == df_auto || system_config.display_format == df_ascii ||
                  in->number_format == df_ascii)) {
        display_format = in->number_format;
    } else {
        display_format = system_config.display_format;
    }

    if (num_bits < 32) {
        mask = ((1 << num_bits) - 1);
    } else {
        mask = 0xFFFFFFFF;
    }
    d &= mask;

    if (display_format == df_ascii) {
        if ((char)d >= ' ' && (char)d <= '~') {
            printf("'%c' ", (char)d);
        } else {
            printf("''  ");
        }
    }

    // TODO: move this part to second function/third functions so we can reuse it from other number print places with
    // custom specs that aren't in attributes
    switch (display_format) {
        case df_ascii: // drop through and show hex
        case df_auto:
        case df_hex:
            num_nibbles = num_bits / 4;
            if (num_bits % 4) {
                num_nibbles++;
            }
            if (num_nibbles & 0b1) {
                num_nibbles++;
            }
            color_flip = true;
            printf("%s0x%s", "", "");
            for (i = num_nibbles * 4; i > 0; i -= 8) {
                printf("%s", (color_flip ? ui_term_color_num_float() : ui_term_color_reset()));
                color_flip = !color_flip;
                printf("%c", ascii_hex[((d >> (i - 4)) & 0x0F)]);
                printf("%c", ascii_hex[((d >> (i - 8)) & 0x0F)]);
            }
            printf("%s", ui_term_color_reset());
            break;
        case df_dec:
            printf("%d", d);
            break;
        case df_bin:
            j = num_bits % 4;
            if (j == 0) {
                j = 4;
            }
            color_flip = false;
            printf("%s0b%s", "", ui_term_color_num_float());
            for (i = 0; i < num_bits; i++) {
                if (!j) {
                    if (color_flip) {
                        color_flip = !color_flip;
                        printf("%s", ui_term_color_num_float());
                    } else {
                        color_flip = !color_flip;
                        printf("%s", ui_term_color_reset());
                    }
                    j = 4;
                }
                j--;

                mask = 1 << (num_bits - i - 1);
                if (d & mask) {
                    printf("1");
                } else {
                    printf("0");
                }
            }
            printf("%s", ui_term_color_reset());
            break;
    }

    if (num_bits != 8) {
        printf(".%d", num_bits);
    }

    // if( attributes->has_string)
    //{
    // printf(" %s\'%c\'", ui_term_color_reset(), d);
    //}

    // printf("\r\n");
}

