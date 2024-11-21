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

//TODO: big cleanup...
// Create a nice struct to pass around everything
// divide into three or four files
// use fancy pointers for everything 
// some rational return codes instead of true/false

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

const struct command_attributes attributes_empty;
const struct command_response response_empty;
struct command_attributes attributes;
struct prompt_result result;
struct _bytecode out[SYN_MAX_LENGTH];
struct _bytecode in[SYN_MAX_LENGTH];
const struct _bytecode bytecode_empty;
uint32_t out_cnt = 0;
uint32_t in_cnt = 0;

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
    {'a', SYN_AUX_OUTPUT},
    {'A', SYN_AUX_OUTPUT},
    {'@', SYN_AUX_INPUT},
    {'v', SYN_ADC}
};

bool syntax_compile(void) {
    uint32_t pos = 0;
    uint32_t i;
    char c;
    bool error = false;
    uint32_t slot_cnt = 0;
    out_cnt = 0;
    in_cnt = 0;
    for (i = 0; i < SYN_MAX_LENGTH; i++) {
        out[i] = bytecode_empty;
        in[i] = bytecode_empty;
    }

    // we need to track pin functions to avoid blowing out any existing pins
    // if a conflict is found, the compiler can throw an error
    enum bp_pin_func pin_func[HW_PINS - 2];
    for (i = 1; i < HW_PINS - 1; i++) {
        pin_func[i - 1] = system_config.pin_func[i]; //=BP_PIN_IO;
    }

    while (cmdln_try_peek(0, &c)) {
        pos++;

        if (c <= ' ' || c > '~' || c =='>') {
            // out of ascii range, or > syntax indication character
            cmdln_try_discard(1);
            continue;
        }

        // if number parse it
        if (c >= '0' && c <= '9') {
            ui_parse_get_int(&result, &out[out_cnt].out_data);
            if (result.error) {
                printf("Error parsing integer at position %d\r\n", pos);
                return true;
            }
            out[out_cnt].command = SYN_WRITE;
            out[out_cnt].number_format = result.number_format;
            goto compiler_get_attributes;
        }
        
        //if string, parse it
        if (c == '"') {
            cmdln_try_remove(&c); // remove "
            // sanity check! is there a terminating "?
            error = true;
            i = 0;
            while (cmdln_try_peek(i, &c)) {
                if (c == '"') {
                    goto compile_get_string;
                }
                i++;
            }
            printf("Error: string missing terminating '\"'");
            return true;

compile_get_string:
            if((out_cnt+i)>=SYN_MAX_LENGTH){
                printf("Syntax exceeds available space (%d slots)\r\n", SYN_MAX_LENGTH);
                return true;
            }            

            while (i--) {
                cmdln_try_remove(&c);
                out[out_cnt].command = SYN_WRITE;
                out[out_cnt].out_data = c;
                out[out_cnt].has_repeat = false;
                out[out_cnt].repeat = 1;
                out[out_cnt].number_format = df_ascii;
                out[out_cnt].bits = system_config.num_bits;
                out_cnt++;
            }
            cmdln_try_remove(&c); // consume the final "
            continue;
        } 
        
        uint8_t cmd=0xff;
        for (i = 0; i < count_of(syntax_compile_commands); i++) {
            if (c == syntax_compile_commands[i].symbol) {
                out[out_cnt].command  = syntax_compile_commands[i].code;
                // parsing an int value from the command line sets the pointer to the next value
                // if it's another command, we need to do that manually now to keep the pointer
                // where the next parsing function expects it
                cmdln_try_discard(1);
                goto compiler_get_attributes;
            }
        }
        printf("Unknown syntax '%c' at position %d\r\n", c, pos);
        return true;     

compiler_get_attributes:

        if (ui_parse_get_dot(&out[out_cnt].bits)) {
            out[out_cnt].has_bits = true;
        } else {
            out[out_cnt].has_bits = false;
            out[out_cnt].bits = system_config.num_bits;
        }

        if (ui_parse_get_colon(&out[out_cnt].repeat)) {
            out[out_cnt].has_repeat = true;
        } else {
            out[out_cnt].has_repeat = false;
            out[out_cnt].repeat = 1;
        }

        if (out[out_cnt].command >= SYN_AUX_OUTPUT) {
            if (out[out_cnt].has_bits == false) {
                printf("Error: missing IO number for command %c at position %d. Try %c.0\r\n", c, pos);
                return true;
            }

            if (out[out_cnt].bits >= count_of(bio2bufiopin)) {
                printf("%sError:%s pin IO%d is invalid\r\n",
                       ui_term_color_error(),
                       ui_term_color_reset(),
                       out[out_cnt].bits);
                return true;
            }

            if (out[out_cnt].command != SYN_ADC && pin_func[out[out_cnt].bits] != BP_PIN_IO) {
                printf("%sError:%s at position %d IO%d is already in use\r\n",
                       ui_term_color_error(),
                       ui_term_color_reset(),
                       pos,
                       out[out_cnt].bits);
                return true;
            }
            // AUX high and low need to set function until changed to read again...
        }

        if (out[out_cnt].command == SYN_DELAY_US || out[out_cnt].command == SYN_DELAY_MS ||
            out[out_cnt].command == SYN_TICK_CLOCK) // delays repeat but don't take up slots
        {
            slot_cnt += 1;
        } else {
            slot_cnt += out[out_cnt].repeat;
        }

        if (slot_cnt >= SYN_MAX_LENGTH) {
            printf("Syntax exceeds available space (%d slots)\r\n", SYN_MAX_LENGTH);
            return true;
        }

        out_cnt++;
    }

    in_cnt = 0;
    return false;
}

typedef void (*syntax_run_func_ptr_t)(struct _bytecode* in, struct _bytecode* out, uint32_t* in_cnt, uint32_t current_command );

void syntax_run_write(struct _bytecode* in, struct _bytecode* out, uint32_t* in_cnt, uint32_t current_command) {
    if (*in_cnt + out->repeat >= SYN_MAX_LENGTH) {
        in[*in_cnt].error_message = GET_T(T_SYNTAX_EXCEEDS_MAX_SLOTS);
        in[*in_cnt].error = SRES_ERROR;
        return;
    }
    for (uint16_t j = 0; j < out->repeat; j++) {
        if (j > 0) {
            (*in_cnt)++;
            in[*in_cnt] = *out;
        }
        modes[system_config.mode].protocol_write(&in[*in_cnt], NULL);
    }
}

void syntax_run_read(struct _bytecode* in, struct _bytecode* out, uint32_t* in_cnt, uint32_t current_command) {
    if (*in_cnt + out->repeat >= SYN_MAX_LENGTH) {
        in[*in_cnt].error_message = GET_T(T_SYNTAX_EXCEEDS_MAX_SLOTS);
        in[*in_cnt].error = SRES_ERROR;
        return;
    }
    for (uint16_t j = 0; j < out->repeat; j++) {
        if (j > 0) {
            (*in_cnt)++;
            in[*in_cnt] = *out;
        }
        modes[system_config.mode].protocol_read(
            &in[*in_cnt], (current_command + 1 < out_cnt && j + 1 == out[current_command].repeat) ? &out[current_command + 1] : NULL);
    }
}

void syntax_run_start(struct _bytecode* in, struct _bytecode* out, uint32_t* in_cnt, uint32_t current_command) {
    modes[system_config.mode].protocol_start(&in[*in_cnt], NULL);
}

void syntax_run_start_alt(struct _bytecode* in, struct _bytecode* out, uint32_t* in_cnt, uint32_t current_command) {
    modes[system_config.mode].protocol_start_alt(&in[*in_cnt], NULL);
}

void syntax_run_stop(struct _bytecode* in, struct _bytecode* out, uint32_t* in_cnt, uint32_t current_command) {
    modes[system_config.mode].protocol_stop(&in[*in_cnt], NULL);
}

void syntax_run_stop_alt(struct _bytecode* in, struct _bytecode* out, uint32_t* in_cnt, uint32_t current_command) {
    modes[system_config.mode].protocol_stop_alt(&in[*in_cnt], NULL);
}

void syntax_run_delay_us(struct _bytecode* in, struct _bytecode* out, uint32_t* in_cnt, uint32_t current_command) {
    busy_wait_us_32(out[current_command].repeat);
}

void syntax_run_delay_ms(struct _bytecode* in, struct _bytecode* out, uint32_t* in_cnt, uint32_t current_command) {
    busy_wait_ms(out[current_command].repeat);
}

void syntax_run_aux_output(struct _bytecode* in, struct _bytecode* out, uint32_t* in_cnt, uint32_t current_command) {
    bio_output(out[current_command].bits);
    bio_put((uint8_t)out[current_command].bits, (bool)out[current_command].out_data);
    system_bio_claim(
        true,
        out[current_command].bits,
        BP_PIN_IO,
        labels[out[current_command].out_data]); // this should be moved to a cleanup function to reduce overhead
    system_set_active(true, out[current_command].bits, &system_config.aux_active);
}

void syntax_run_aux_input(struct _bytecode* in, struct _bytecode* out, uint32_t* in_cnt, uint32_t current_command) {
    bio_input(out[current_command].bits);
    in[*in_cnt].in_data = bio_get(out[current_command].bits);
    system_bio_claim(false, out[current_command].bits, BP_PIN_IO, 0);
    system_set_active(false, out[current_command].bits, &system_config.aux_active);  
}

void syntax_run_adc(struct _bytecode* in, struct _bytecode* out, uint32_t* in_cnt, uint32_t current_command) {
    in[*in_cnt].in_data = amux_read_bio(out[current_command].bits);
}

void syntax_run_tick_clock(struct _bytecode* in, struct _bytecode* out, uint32_t* in_cnt, uint32_t current_command) {
    for (uint16_t j = 0; j < out[current_command].repeat; j++) {
        modes[system_config.mode].protocol_tick_clock(&in[*in_cnt], NULL);
    }
}

void syntax_run_set_clk_high(struct _bytecode* in, struct _bytecode* out, uint32_t* in_cnt, uint32_t current_command) {
    modes[system_config.mode].protocol_clkh(&in[*in_cnt], NULL);
}

void syntax_run_set_clk_low(struct _bytecode* in, struct _bytecode* out, uint32_t* in_cnt, uint32_t current_command) {
    modes[system_config.mode].protocol_clkl(&in[*in_cnt], NULL);
}

void syntax_run_set_dat_high(struct _bytecode* in, struct _bytecode* out, uint32_t* in_cnt, uint32_t current_command) {
    modes[system_config.mode].protocol_dath(&in[*in_cnt], NULL);
}

void syntax_run_set_dat_low(struct _bytecode* in, struct _bytecode* out, uint32_t* in_cnt, uint32_t current_command) {
    modes[system_config.mode].protocol_datl(&in[*in_cnt], NULL);
}

void syntax_run_read_dat(struct _bytecode* in, struct _bytecode* out, uint32_t* in_cnt, uint32_t current_command) {
    //TODO: reality check out slots, actually repeat the read?
    for (uint16_t j = 0; j < out[current_command].repeat; j++) {
        modes[system_config.mode].protocol_bitr(&in[*in_cnt], NULL);
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
    [SYN_AUX_OUTPUT]=syntax_run_aux_output,
    [SYN_AUX_INPUT]=syntax_run_aux_input,
    [SYN_ADC]=syntax_run_adc,
    [SYN_TICK_CLOCK]=syntax_run_tick_clock,
    [SYN_SET_CLK_HIGH]=syntax_run_set_clk_high,
    [SYN_SET_CLK_LOW]=syntax_run_set_clk_low,
    [SYN_SET_DAT_HIGH]=syntax_run_set_dat_high,
    [SYN_SET_DAT_LOW]=syntax_run_set_dat_low,
    [SYN_READ_DAT]=syntax_run_read_dat
};
static const char labels[][5] = { "AUXL", "AUXH" };
bool syntax_run(void) {
    uint32_t current_command;

    if (!out_cnt) {
        return true;
    }

    in_cnt = 0;

    for (current_command = 0; current_command < out_cnt; current_command++) {
        in[in_cnt] = out[current_command];

        if (in[current_command].command >= count_of(syntax_run_func)) {
            printf("Unknown internal code %d\r\n", out[current_command].command);
            return true;
        }

        syntax_run_func[in[current_command].command](in, out, &in_cnt, current_command);

        if (in_cnt + 1 >= SYN_MAX_LENGTH) {
            in[in_cnt].error_message = GET_T(T_SYNTAX_EXCEEDS_MAX_SLOTS);
            in[in_cnt].error = SRES_ERROR;
            return false;
        }

        if (in[in_cnt].error >= SRES_ERROR) {
            in_cnt++;
            return false; // halt execution, but let the post process show the error.
        }

        in_cnt++;
    }

    return false;
}


void syntax_post_write(struct _bytecode* in, struct _output_info* info) {
    postprocess_mode_write(&in[i], &info);
}

void syntax_post_delay_us_ms(struct _bytecode* in, struct _output_info* info) {
    printf("\r\n%s%s:%s %s%d%s%s",
              ui_term_color_notice(),   
                GET_T(T_MODE_DELAY),
                ui_term_color_reset(),
                ui_term_color_num_float(),
                in[i].repeat,
                ui_term_color_reset(),
                (in[i].command == SYN_DELAY_US ? GET_T(T_MODE_US) : GET_T(T_MODE_MS)));
}

void syntax_post_read(struct _bytecode* in, struct _output_info* info) {
    postprocess_mode_write(&in[i], &info);
}

void syntax_post_start_stop(struct _bytecode* in, struct _output_info* info) {
    if (in[i].data_message) {
        printf("\r\n%s", in[i].data_message);
    }
}

void syntax_post_aux_output(struct _bytecode* in, struct _output_info* info) {
    printf("\r\nIO%s%d%s set to%s OUTPUT: %s%d%s",
              ui_term_color_num_float(),
                in[i].bits,
                ui_term_color_notice(),
                ui_term_color_reset(),
                ui_term_color_num_float(),
                (in[i].out_data),
                ui_term_color_reset());
}

void syntax_post_aux_input(struct _bytecode* in, struct _output_info* info) {
    printf("\r\nIO%s%d%s set to%s INPUT: %s%d%s",
              ui_term_color_num_float(),
                in[i].bits,
                ui_term_color_notice(),
                ui_term_color_reset(),
                ui_term_color_num_float(),
                in[i].in_data,
                ui_term_color_reset());
}

void syntax_post_adc(struct _bytecode* in, struct _output_info* info) {
    uint32_t received = (6600 * in[i].in_data) / 4096;
    printf("\r\n%s%s IO%d:%s %s%d.%d%sV",
              ui_term_color_info(),
                GET_T(T_MODE_ADC_VOLTAGE),
                in[i].bits,
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
                in[i].repeat,
                ui_term_color_reset());
}

void syntax_post_set_clk_high_low(struct _bytecode* in, struct _output_info* info) {
    printf("\r\n%s%s:%s %s%d%s",
                ui_term_color_notice(),
                GET_T(T_MODE_SET_CLK),
                ui_term_color_reset(),
                ui_term_color_num_float(),
                in[i].out_data,
                ui_term_color_reset());
}

void syntax_post_set_dat_high_low(struct _bytecode* in, struct _output_info* info) {
    printf("\r\n%s%s:%s %s%d%s",
                ui_term_color_notice(),
                GET_T(T_MODE_SET_DAT),
                ui_term_color_reset(),
                ui_term_color_num_float(),
                in[i].out_data,
                ui_term_color_reset());
} 

void syntax_post_read_dat(struct _bytecode* in, struct _output_info* info) {
    printf("\r\n%s%s:%s %s%d%s",
                ui_term_color_notice(),
                GET_T(T_MODE_READ_DAT),
                ui_term_color_reset(),
                ui_term_color_num_float(),
                in[i].in_data,
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
    [SYN_AUX_OUTPUT] = syntax_post_aux_output,
    [SYN_AUX_INPUT] = syntax_post_aux_input,
    [SYN_ADC] = syntax_post_adc,
    [SYN_TICK_CLOCK] = syntax_post_tick_clock,
    [SYN_SET_CLK_HIGH] = syntax_post_set_clk_high_low,
    [SYN_SET_CLK_LOW] = syntax_post_set_clk_high_low,
    [SYN_SET_DAT_HIGH] = syntax_post_set_dat_high_low,
    [SYN_SET_DAT_LOW] = syntax_post_set_dat_high_low,
    [SYN_READ_DAT] = syntax_post_read_dat
};



bool syntax_post(void) {
    uint32_t current_command, received;
    static struct _output_info info;

    if (!in_cnt) return true;
    
    info.previous_command = 0xff; // set invalid command so output display works
    for (current_command = 0; current_command < in_cnt; current_command++) {
        if (in[current_command].command >= count_of(syntax_post_func)) {
            printf("Unknown internal code %d\r\n", in[current_command].command);
            continue;
        }

        syntax_post_func[in[current_command].command](&in[current_command], &info);
        info.previous_command = in[current_command].command;

        if (in[current_command].error) {
            printf(" (%s)", in[current_command].error_message);
        }
    }
    printf("\r\n");
    in_cnt = 0;
    return false;
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

