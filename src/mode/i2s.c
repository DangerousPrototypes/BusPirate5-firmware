// This is an example mode for Bus Pirate 5
// How modes work:
// Bus Pirate 5 uses a three step process to get tight timings between operations.
// This means it is no longer possible to just spit out data from printf directly from the mode.
// All messages and data must be handed back in the result struct to be shown later.
// Eventually I will make syntax processing optional for modes so you can opt for a more relaxed environment (ask me, I
// wont make it a priority until someone actually wants it)
// 1. The syntax system pre-processes the user input into a simple bytecode
// 2. A loop hands each user command to a mode function below for actual IO or other actions
// 3. A final loop post-processes the result and outputs to the user terminal
// To enable dummy mode open pirate.h and uncomment "#define BP_USE_i2s"
// The dummy functions are implemented in mode.c. To create a new mode make a copy of the dummy portion of the mode
// struck and link to your new functions
#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"   // Bytecode structure for data IO
#include "pirate/bio.h" // Buffered pin IO functions
#include "ui/ui_help.h"
#include "i2s.h"
#include "i2s_out.pio.h"
#include "i2s_in.pio.h"
#include "pio_config.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pirate/storage.h"
#include "ui/ui_term.h"
#include "commands/i2s/sine.h"
#include "commands/i2s/wav.h"
#include "pirate/storage.h"
#include "fatfs/ff.h"       // File system related
#include "lib/bp_args/bp_cmd.h"

static uint32_t returnval;
struct _pio_config i2s_pio_config_out,  i2s_pio_config_in;
struct _i2s_mode_config i2s_mode_config;

// command configuration
const struct _mode_command_struct i2s_commands[] = { 
    {   .func=&sine_handler,
        .def=&sine_def,
        .supress_fala_capture=false
    },
    {   .func=&wav_handler,
        .def=&wav_def,
        .supress_fala_capture=false
    },
 };
const uint32_t i2s_commands_count = count_of(i2s_commands);

// Pin labels shown on the display and in the terminal status bar
// No more than 4 characters long
static const char pin_labels[][5] = { "DATO", "CLKO", "WSO", "DATI", "CLKI", "WSI" };

// Sample rate — flag -f / --freq (Hz)
static const bp_val_constraint_t i2s_freq_range = {
    .type = BP_VAL_UINT32,
    .u = { .min = 4000, .max = 96000, .def = 44100 },
    .prompt = T_I2S_SPEED_MENU,
    .hint = T_I2S_SPEED_MENU_1,
};

// Data bits — flag -d / --databits (currently fixed at 16)
static const bp_val_constraint_t i2s_databits_range = {
    .type = BP_VAL_UINT32,
    .u = { .min = 16, .max = 16, .def = 16 },
    .prompt = T_I2S_DATA_BITS_MENU,
    .hint = T_I2S_DATA_BITS_MENU_1,
};

static const bp_command_opt_t i2s_setup_opts[] = {
    { "freq",     'f', BP_ARG_REQUIRED, "4000-96000", 0, &i2s_freq_range },
    { "databits", 'd', BP_ARG_REQUIRED, "16",         0, &i2s_databits_range },
    { 0 },
};

const bp_command_def_t i2s_setup_def = {
    .name = "i2s",
    .description = 0,
    .opts = i2s_setup_opts,
};

void i2s_settings(void) {
    ui_help_setting_int(GET_T(T_I2S_SPEED_MENU), i2s_mode_config.freq, GET_T(T_HZ));
    ui_help_setting_int(GET_T(T_I2S_DATA_BITS_MENU), i2s_mode_config.bits, GET_T(T_UART_STOP_BITS_PROMPT));
}

static bool update_pio_frequency(uint32_t sample_freq, bool enable) {
    uint32_t system_clock_frequency = clock_get_hz(clk_sys);
    if(!(system_clock_frequency < 0x40000000)) return false; // sanity check;
    uint32_t divider = system_clock_frequency * 4 / sample_freq; // avoid arithmetic overflow
    if(!(divider < 0x1000000)) return false; // sanity check, divider must be less than 2^24
    if(enable) pio_sm_set_clkdiv_int_frac(i2s_pio_config_out.pio,  i2s_pio_config_out.sm, divider >> 8u, divider & 0xffu);
    return true; // return true if the frequency was set successfully
}

// Pre-setup step. Show user menus for any configuration options.
// The Bus Pirate hardware is not "clean" and reset at this point.
// Any previous mode may still be running. This is only a configuration step,
// the user may cancel out of the menu and return to the previous mode.
// Don't touch hardware yet, save the settings in variables for later.
uint32_t i2s_setup(void) {
    uint32_t new_freq;
    uint32_t new_bits;

    const char config_file[] = "bpi2s.bp";
    const mode_config_t config_t[] = {
        // clang-format off
        { "$.freq", &i2s_mode_config.freq, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.bits", &i2s_mode_config.bits, MODE_CONFIG_FORMAT_DECIMAL },
        // clang-format off
    };    

    // Detect interactive vs CLI mode by checking the primary flag
    bp_cmd_status_t st = bp_cmd_flag(&i2s_setup_def, 'f', &new_freq);
    if (st == BP_CMD_INVALID) return 0;
    bool interactive = (st == BP_CMD_MISSING);

    if (interactive) {
        if (storage_load_mode(config_file, config_t, count_of(config_t))) {
            printf("\r\n\r\n%s%s%s\r\n", ui_term_color_info(), GET_T(T_USE_PREVIOUS_SETTINGS), ui_term_color_reset());
            i2s_settings();
            int r = bp_cmd_yes_no_exit("");
            if (r == BP_YN_EXIT) return 0; // exit
            if (r == BP_YN_YES)  return 1; // use saved settings
        }

        // Prompt for frequency, loop until PIO can support the selected rate
        do {
            if (bp_cmd_prompt(&i2s_freq_range, &new_freq) != BP_CMD_OK) return 0;
        } while (!update_pio_frequency(new_freq, false));

        if (bp_cmd_prompt(&i2s_databits_range, &new_bits) != BP_CMD_OK) return 0;
    } else {
        // CLI mode — validate frequency once
        if (!update_pio_frequency(new_freq, false)) {
            printf("Unsupported frequency: %u Hz\r\n", (unsigned)new_freq);
            return 0;
        }

        st = bp_cmd_flag(&i2s_setup_def, 'd', &new_bits);
        if (st == BP_CMD_INVALID) return 0;
    }

    i2s_mode_config.freq = new_freq;
    i2s_mode_config.bits = (uint8_t)new_bits;

    storage_save_mode(config_file, config_t, count_of(config_t));

    return 1;
}


// Setup execution. This is where we actually configure any hardware.
uint32_t i2s_setup_exc(void) {
    // I2S OUTPUT has DATA, CLOCK, WORD_SELECT (L/R)
    bio_output(BIO0);
    bio_output(BIO1);
    bio_output(BIO2);

    gpio_set_function(bio2bufiopin[BIO0], GPIO_FUNC_PIO1);
    gpio_set_function(bio2bufiopin[BIO1], GPIO_FUNC_PIO1);
    gpio_set_function(bio2bufiopin[BIO2], GPIO_FUNC_PIO1);
    
    i2s_pio_config_out.pio = PIO_MODE_PIO;
    i2s_pio_config_out.sm = 0;
    i2s_pio_config_out.program = &i2s_out_program;
    i2s_pio_config_out.offset = pio_add_program(i2s_pio_config_out.pio, i2s_pio_config_out.program);
#ifdef BP_PIO_SHOW_ASSIGNMENT
    printf("PIO: pio=%d, sm=%d, offset=%d\r\n", PIO_NUM(i2s_pio_config_out.pio), i2s_pio_config_out.sm, i2s_pio_config_out.offset);
#endif
    i2s_out_program_init(i2s_pio_config_out.pio,  i2s_pio_config_out.sm, i2s_pio_config_out.offset, bio2bufiopin[BIO0], bio2bufiopin[BIO1]);

    // I2S INPUT has DATA, CLOCK, WORD_SELECT (L/R)
    bio_input(BIO5);
    bio_output(BIO6);
    bio_output(BIO7);
    gpio_set_function(bio2bufiopin[BIO5], GPIO_FUNC_PIO1);
    gpio_set_function(bio2bufiopin[BIO6], GPIO_FUNC_PIO1);
    gpio_set_function(bio2bufiopin[BIO7], GPIO_FUNC_PIO1);
    // Initialize the I2S microphone PIO program
    i2s_pio_config_in.pio = PIO_MODE_PIO;
    i2s_pio_config_in.sm = 1; // Use a different state machine for the microphone
    i2s_pio_config_in.program = &i2s_in_program;
    i2s_pio_config_in.offset = pio_add_program(i2s_pio_config_in.pio, i2s_pio_config_in.program);
#ifdef BP_PIO_SHOW_ASSIGNMENT
    printf("PIO: pio=%d, sm=%d, offset=%d\r\n", PIO_NUM(i2s_pio_config_in.pio), i2s_pio_config_in.sm, i2s_pio_config_in.offset);        
#endif
    // Initialize the I2S microphone PIO program
    i2s_in_program_init(i2s_pio_config_in.pio, i2s_pio_config_in.sm, i2s_pio_config_in.offset, i2s_mode_config.freq, bio2bufiopin[BIO5], bio2bufiopin[BIO6]);

    // 2. Claim IO pins that are used by your hardware/protocol
    // The Bus Pirate won't let the user manipulate these pins
    //  or use PWM/FREQ/etc on these pins while claimed.
    system_bio_update_purpose_and_label(true, BIO0, BP_PIN_MODE, pin_labels[0]);
    system_bio_update_purpose_and_label(true, BIO1, BP_PIN_MODE, pin_labels[1]);
    system_bio_update_purpose_and_label(true, BIO2, BP_PIN_MODE, pin_labels[2]);
    system_bio_update_purpose_and_label(true, BIO5, BP_PIN_MODE, pin_labels[3]);
    system_bio_update_purpose_and_label(true, BIO6, BP_PIN_MODE, pin_labels[4]);
    system_bio_update_purpose_and_label(true, BIO7, BP_PIN_MODE, pin_labels[5]);

    update_pio_frequency(i2s_mode_config.freq, true); // default sample rate

    pio_sm_set_enabled(i2s_pio_config_out.pio,  i2s_pio_config_out.sm, true);


    //printf("-i2s- setup_exc()\r\n");
    return 1;
}

// Cleanup any configuration on exit.
void i2s_cleanup(void) {
    pio_sm_set_enabled(i2s_pio_config_out.pio, i2s_pio_config_out.sm, false);
    pio_sm_set_enabled(i2s_pio_config_in.pio, i2s_pio_config_in.sm, false);
    pio_remove_program(i2s_pio_config_out.pio, i2s_pio_config_out.program, i2s_pio_config_out.offset);
    pio_remove_program(i2s_pio_config_in.pio, i2s_pio_config_in.program, i2s_pio_config_in.offset);
    // 1. Disable any hardware you used
    bio_init();

    // 2. Release the IO pins and reset the labels
    system_bio_update_purpose_and_label(false, BIO0, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO1, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO2, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO5, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO6, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO7, BP_PIN_MODE, 0);
    //printf("-i2s- cleanup()\r\n");
}

// Handler for any numbers the user enters (1, 0x01, 0b1) or string data "string"
// This function generally writes data out to the IO pins or a peripheral
void i2s_write(struct _bytecode* result, struct _bytecode* next) {
    // The result struct has data about the command the user entered.
    // next is the same, but the next command in the sequence (if any).
    // next is used to predict when to ACK/NACK in I2C mode for example
    /*
    result->out_data; Data value the user entered, up to 32 bits long
    result->bits; The bit count configuration of the command (or system default) eg 0xff.4 = 4 bits. Can be useful for
    some protocols. result->number_format; The number format the user entered df_bin, df_hex, df_dec, df_ascii, mostly
    used for post processing the results result->data_message; A reference to null terminated char string to show the
    user. result->error; Set to true to halt execution, the results will be printed up to this step
    result->error_message; Reference to char string with error message to show the user.
    result->in_data; 32 bit value returned from the mode (eg read from SPI), will be shown to user
    result->repeat; THIS IS HANDLED IN THE LAYER ABOVE US, do not implement repeats in mode functions
    */
    static const char message[] = "--i2s- write()";

   // Can add a text decoration if you like (optional)
    // This is for passing ACK/NACK for I2C mode and similar
    result->data_message = message;
}

// This function is called when the user enters 'r' to read data
void i2s_read(struct _bytecode* result, struct _bytecode* next) {
    static const char message[] = "--i2s- read()";

    // your code
    uint32_t data = bio_get(BIO7);

    result->in_data = data;         // put the read value in in_data (up to 32 bits)
    result->data_message = message; // add a text decoration if you like
}

// Handler for mode START when user enters the '[' key
void i2s_start(struct _bytecode* result, struct _bytecode* next) {
    static const char message[] = "-i2s- start()"; // The message to show the user
    
    uint32_t cnt=0;
    while(!pio_sm_is_rx_fifo_empty(i2s_pio_config_in.pio, i2s_pio_config_in.sm)) {
        // Clear the RX FIFO before starting to read data
        pio_sm_get_blocking(i2s_pio_config_in.pio, i2s_pio_config_in.sm);
    }
    pio_sm_set_enabled(i2s_pio_config_in.pio,  i2s_pio_config_in.sm, true);
    do{
        if(!pio_sm_is_rx_fifo_empty(i2s_pio_config_in.pio, i2s_pio_config_in.sm)) {
            pio_sm_put_blocking(i2s_pio_config_out.pio, i2s_pio_config_out.sm, ((pio_sm_get(i2s_pio_config_in.pio, i2s_pio_config_in.sm)>>(7+8)) & 0xFFFF));
            cnt++;
        }
    }while(cnt<(44100 * 10)); // wait for 10 seconds of data 
    pio_sm_set_enabled(i2s_pio_config_in.pio,  i2s_pio_config_in.sm, false);


    result->data_message = message; // return a reference to the message to show the user
}


// Handler for mode STOP when user enters the ']' key
void i2s_stop(struct _bytecode* result, struct _bytecode* next) {
    static const char message[] = "-i2s- stop()"; // The message to show the user


    result->data_message = message; // return a reference to the message to show the user
}

// The Bus Pirate will make a periodic call to this function (if linked in modes.c)
// Useful for checking async stuff like bytes in a UART
void i2s_periodic(void) {
    // your periodic service functions

}

// Handler for mode START when user enters the '{' key
// This is for full duplex SPI read/write and is not yet implemented
void i2s_startr(struct _bytecode* result, struct _bytecode* next) {
    printf("-i2s- startr()");
}

// Handler for mode STOP when user enters the '}' key
// This is for full duplex SPI read/write and is not yet implemented
void i2s_stopr(struct _bytecode* result, struct _bytecode* next) {
    printf("-i2s- stopr()");
}

// These are old bitwise commands.
// They are not currently supported because we don't have any bitbanged code (yeah!)
void i2s_clkh(struct _bytecode* result, struct _bytecode* next) {
    printf("-i2s- clkh()");
}
void i2s_clkl(struct _bytecode* result, struct _bytecode* next) {
    printf("-i2s- clkl()");
}
void i2s_dath(struct _bytecode* result, struct _bytecode* next) {
    printf("-i2s- dath()");
}
void i2s_datl(struct _bytecode* result, struct _bytecode* next) {
    printf("-i2s- datl()");
}
void i2s_dats(struct _bytecode* result, struct _bytecode* next) {
    printf("-i2s- dats()=%08X", returnval);
}
void i2s_clk(struct _bytecode* result, struct _bytecode* next) {
    printf("-i2s- clk()");
}
void i2s_bitr(struct _bytecode* result, struct _bytecode* next) {
    printf("-i2s- bitr()=%08X", returnval);
}

void i2s_help(void) {
    ui_help_mode_commands(i2s_commands, i2s_commands_count);
}
