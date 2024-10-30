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
#include "opt_args.h"
#include "bytecode.h"   // Bytecode structure for data IO
#include "pirate/bio.h" // Buffered pin IO functions
#include "ui/ui_help.h"
#include "lib/pico_ir_nec/nec_transmit.h"
#include "mode/infrared.h"

static struct _infrared_mode_config mode_config;
static uint32_t returnval;
static int tx_sm;

bool irtoy_test_pullup(int bio) {
    if (bio_get(bio)) {
        printf("OK\r\n");
        return 0;
    } else {
        printf("FAIL\r\n");
        return 1;
    }
}

bool irtoy_test_rx(int bio) {
    uint32_t tx_frame = nec_encode_frame(0xff, 0xff);
    pio_sm_put(pio0, tx_sm, tx_frame);
    uint32_t timeout = 0;
    while (true) {
        if (!bio_get(bio)) {
            printf("OK\r\n");
            return 0;
        }
        timeout++;
        if (timeout > 10000) {
            printf("FAIL\r\n");
            return 1;
        }
    }
}
// irtoy_test function
void irtoy_test(struct command_result* res) {
    uint8_t fails = 0;
    printf("Test pull-ups\r\n");
    printf("BIO1 / 20-60kHz learner: ");
    fails += irtoy_test_pullup(BIO1);
    printf("BIO3 / 38kHz barrier: ");
    fails += irtoy_test_pullup(BIO3);
    printf("BIO5 / 38kHz demodulator: ");
    fails += irtoy_test_pullup(BIO5);
    printf("BIO7 / 56kHz modulator: ");
    fails += irtoy_test_pullup(BIO7);

    printf("Test RX\r\n");
    printf("BIO1 / 20-60kHz learner: ");
    fails += irtoy_test_rx(BIO1);
    printf("BIO3 / 38kHz barrier: ");
    fails += irtoy_test_rx(BIO3);
    printf("BIO5 / 38kHz demodulator: ");
    fails += irtoy_test_rx(BIO5);
    printf("BIO7 / 56kHz modulator: ");
    fails += irtoy_test_rx(BIO7);

    if (fails) {
        printf("\r\n%d FAILS :(\r\n", fails);
    } else {
        printf("\r\nOK :)\r\n");
    }
}

// command configuration
const struct _command_struct infrared_commands[] = {
    // HiZ? Function Help
    // note: for now the allow_hiz flag controls if the mode provides it's own help
    { "test", false, &irtoy_test, 0x00 }, // the help is shown in the -h *and* the list of mode apps
};
const uint32_t infrared_commands_count = count_of(infrared_commands);

// Pin labels shown on the display and in the terminal status bar
// No more than 4 characters long
/*static const char pin_labels[][5]={
    "OUT1",
    "OUT2",
    "OUT3",
    "IN1"
};*/

// Pre-setup step. Show user menus for any configuration options.
// The Bus Pirate hardware is not "clean" and reset at this point.
// Any previous mode may still be running. This is only a configuration step,
// the user may cancel out of the menu and return to the previous mode.
// Don't touch hardware yet, save the settings in variables for later.
uint32_t infrared_setup(void) {
    // printf("\r\n-DUMMY1- setup()\r\n");
    return 1;
}

// Setup execution. This is where we actually configure any hardware.
uint32_t infrared_setup_exc(void) {
    bio_buf_output(BIO4); // set gpio buffer to output
    // configure and enable the state machines
    tx_sm = nec_tx_init(bio2bufiopin[BIO4]); // uses two state machines, 16 instructions and one IRQ
    mode_config.baudrate = 38000;
    if (tx_sm < 0) {
        printf("Failed to initialize PIO\r\n");
    }

    return 1;
}

// Cleanup any configuration on exit.
void infrared_cleanup(void) {
    nec_tx_deinit();
    // 1. Disable any hardware you used
    bio_init();
}

// Handler for any numbers the user enters (1, 0x01, 0b1) or string data "string"
// This function generally writes data out to the IO pins or a peripheral
void infrared_write(struct _bytecode* result, struct _bytecode* next) {
    static const char labels[][5] = { "AUXL", "AUXH" };
    // your code
    /*for(uint8_t i=0; i<8; i++){
        // user data is in result->out_data
        bio_output(i);
        if(result->out_data & (0b1<<i)){
            system_bio_claim(true, i, BP_PIN_IO, labels[1]);
            bio_put(i, 1);
        }else{
            system_bio_claim(true, i, BP_PIN_IO, labels[0]);
            bio_put(i, 0);
        }
        system_set_active(true, i, &system_config.aux_active);
    }*/

    // transmit and receive frames
    uint8_t tx_address = result->out_data >> 8;
    uint8_t tx_data = result->out_data;
    // create a 32-bit frame and add it to the transmit FIFO
    uint32_t tx_frame = nec_encode_frame(tx_address, tx_data);
    nec_send_frame(tx_frame);
    printf("TX: %02x, %02x\r\n", tx_address, tx_data);
}

// This function is called when the user enters 'r' to read data
void infrared_read(struct _bytecode* result, struct _bytecode* next) {
    // your code
    /*uint8_t data=0;
    for(uint8_t i=0; i<8; i++){
        data |= bio_get(i) << i;
    }
    result->in_data=data; //put the read value in in_data (up to 32 bits)*/
}

// Handler for mode START when user enters the '[' key
#if 0
void infrared_start(struct _bytecode *result, struct _bytecode *next)
{
	static const char message[]="-DUMMY1- start()"; //The message to show the user
	
	bio_put(BIO4, 1); //your code
	
	result->data_message=message; //return a reference to the message to show the user
}

// Handler for mode STOP when user enters the ']' key
void infrared_stop(struct _bytecode *result, struct _bytecode *next)
{
	static const char message[]="-DUMMY1- stop()"; //The message to show the user
	
	bio_put(BIO4, 0); //your code
	
	result->data_message=message; //return a reference to the message to show the user

}
#endif
// modes can have useful macros activated by (1) (eg macro 1)
// macros are passed from the command line directly, not through the syntax system
void infrared_macro(uint32_t macro) {
    printf("-DUMMY1- macro(%d)\r\n", macro);
    // your code
    switch (macro) {
        // macro (0) is always a menu of macros
        case 0:
            printf(" 0. This menu\r\n");
            break;
    }
}

#if 0
// The Bus Pirate will make a periodic call to this function (if linked in modes.c)
// Useful for checking async stuff like bytes in a UART
uint32_t infrared_periodic(void)
{

}
#endif

void infrared_settings(void) {
}

void infrared_help(void) {
    ui_help_mode_commands(infrared_commands, infrared_commands_count);
}

uint32_t infrared_get_speed(void) {
    return mode_config.baudrate;
}
