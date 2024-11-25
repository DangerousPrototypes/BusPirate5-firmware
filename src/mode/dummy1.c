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
// To enable dummy mode open pirate.h and uncomment "#define BP_USE_DUMMY1"
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
#include "dummy1.h"

static uint32_t returnval;

// command configuration
const struct _mode_command_struct dummy1_commands[] = {
    /*{ .command="", 
        .func=&function, 
        .description_text=T_MODE_COMMAND_DESCRIPTION, 
        .supress_fala_capture=false
    },*/
};
const uint32_t dummy1_commands_count = count_of(dummy1_commands);

// Pin labels shown on the display and in the terminal status bar
// No more than 4 characters long
static const char pin_labels[][5] = { "OUT1", "OUT2", "OUT3", "IN1" };

// Pre-setup step. Show user menus for any configuration options.
// The Bus Pirate hardware is not "clean" and reset at this point.
// Any previous mode may still be running. This is only a configuration step,
// the user may cancel out of the menu and return to the previous mode.
// Don't touch hardware yet, save the settings in variables for later.
uint32_t dummy1_setup(void) {
    printf("\r\n-DUMMY1- setup()\r\n");
    return 1;
}

// Setup execution. This is where we actually configure any hardware.
uint32_t dummy1_setup_exc(void) {
    // 1. Do any hardware configuration.
    // This is just an example that sets some pins to outputs and inputs.
    // Generally this is where you configure the UART/I2c/PIO peripheral
    bio_output(BIO4);
    bio_output(BIO5);
    bio_output(BIO6);
    bio_input(BIO7);

    // 2. Claim IO pins that are used by your hardware/protocol
    // The Bus Pirate won't let the user manipulate these pins
    //  or use PWM/FREQ/etc on these pins while claimed.
    system_bio_claim(true, BIO4, BP_PIN_MODE, pin_labels[0]);
    system_bio_claim(true, BIO5, BP_PIN_MODE, pin_labels[1]);
    system_bio_claim(true, BIO6, BP_PIN_MODE, pin_labels[2]);
    system_bio_claim(true, BIO7, BP_PIN_MODE, pin_labels[3]);
    printf("-DUMMY1- setup_exc()\r\n");
    return 1;
}

// Cleanup any configuration on exit.
void dummy1_cleanup(void) {
    // 1. Disable any hardware you used
    bio_init();

    // 2. Release the IO pins and reset the labels
    system_bio_claim(false, BIO4, BP_PIN_MODE, 0);
    system_bio_claim(false, BIO5, BP_PIN_MODE, 0);
    system_bio_claim(false, BIO6, BP_PIN_MODE, 0);
    system_bio_claim(false, BIO7, BP_PIN_MODE, 0);
    printf("-DUMMY1- cleanup()\r\n");
}

// Handler for any numbers the user enters (1, 0x01, 0b1) or string data "string"
// This function generally writes data out to the IO pins or a peripheral
void dummy1_write(struct _bytecode* result, struct _bytecode* next) {
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
    static const char message[] = "--DUMMY1- write()";

    // your code
    for (uint8_t i = 0; i < 8; i++) {
        // user data is in result->out_data
        bio_put(BIO5, result->out_data & (0b1 << i));
    }

    // example error
    static const char err[] = "Halting: 0xff entered";
    if (result->out_data == 0xff) {
        /*
        Error result codes.
        SERR_NONE
        SERR_DEBUG Displays error_message, does not halt execution
        SERR_INFO Displays error_message, does not halt execution
        SERR_WARN Displays error_message, does not halt execution
        SERR_ERROR Displays error_message, halts execution
        */
        result->error = SERR_ERROR; // mode error halts execution
        result->error_message = err;
        return;
    }

    // Can add a text decoration if you like (optional)
    // This is for passing ACK/NACK for I2C mode and similar
    result->data_message = message;
}

// This function is called when the user enters 'r' to read data
void dummy1_read(struct _bytecode* result, struct _bytecode* next) {
    static const char message[] = "--DUMMY1- read()";

    // your code
    uint32_t data = bio_get(BIO7);

    result->in_data = data;         // put the read value in in_data (up to 32 bits)
    result->data_message = message; // add a text decoration if you like
}

// Handler for mode START when user enters the '[' key
void dummy1_start(struct _bytecode* result, struct _bytecode* next) {
    static const char message[] = "-DUMMY1- start()"; // The message to show the user

    bio_put(BIO4, 1); // your code

    result->data_message = message; // return a reference to the message to show the user
}

// Handler for mode STOP when user enters the ']' key
void dummy1_stop(struct _bytecode* result, struct _bytecode* next) {
    static const char message[] = "-DUMMY1- stop()"; // The message to show the user

    bio_put(BIO4, 0); // your code

    result->data_message = message; // return a reference to the message to show the user
}

// modes can have useful macros activated by (1) (eg macro 1)
// macros are passed from the command line directly, not through the syntax system
void dummy1_macro(uint32_t macro) {
    printf("-DUMMY1- macro(%d)\r\n", macro);
    // your code
    switch (macro) {
        // macro (0) is always a menu of macros
        case 0:
            printf(" 0. This menu\r\n 1. Print \"Hello World!\"\r\n");
            break;
        // rick rolled!
        case 1:
            printf("Never gonna give you up\r\nNever gonna let you down\r\nNever gonna run around and desert you\r\n");
            break;
    }
}

// The Bus Pirate will make a periodic call to this function (if linked in modes.c)
// Useful for checking async stuff like bytes in a UART
uint32_t dummy1_periodic(void) {
    // your periodic service functions
    static uint32_t cnt;
    if (cnt > 0xffffff) {
        printf("\r\n-DUMMY1- periodic\r\n");
        cnt = 0;
    }
    cnt++;
}

// Handler for mode START when user enters the '{' key
// This is for full duplex SPI read/write and is not yet implemented
void dummy1_startr(void) {
    printf("-DUMMY1- startr()");
}

// Handler for mode STOP when user enters the '}' key
// This is for full duplex SPI read/write and is not yet implemented
void dummy1_stopr(void) {
    printf("-DUMMY1- stopr()");
}

// These are old bitwise commands.
// They are not currently supported because we don't have any bitbanged code (yeah!)
void dummy1_clkh(void) {
    printf("-DUMMY1- clkh()");
}
void dummy1_clkl(void) {
    printf("-DUMMY1- clkl()");
}
void dummy1_dath(void) {
    printf("-DUMMY1- dath()");
}
void dummy1_datl(void) {
    printf("-DUMMY1- datl()");
}
uint32_t dummy1_dats(void) {
    printf("-DUMMY1- dats()=%08X", returnval);
    return returnval;
}
void dummy1_clk(void) {
    printf("-DUMMY1- clk()");
}
uint32_t dummy1_bitr(void) {
    printf("-DUMMY1- bitr()=%08X", returnval);
    return returnval;
}

/*const char *dummy1_pins(void)
{
    return "pin1\tpin2\tpin3\tpin4";
}*/
void dummy1_settings(void) {
    printf("DUMMY (arg1 arg2)=(%d, %d)", 1, 2);
}

void dummy1_help(void) {
    ui_help_mode_commands(dummy1_commands, dummy1_commands_count);
}
