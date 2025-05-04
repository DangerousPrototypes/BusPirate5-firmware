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
#include "infrared-struct.h"
#include "lib/pico_ir_nec/nec_transmit.h"
#include "lib/pico_ir_nec/nec_receive.h"
#include "mode/infrared.h"
#include "ui/ui_prompt.h"
#include "pirate/storage.h"
#include "ui/ui_term.h"
#include "pirate/rc5_pio.h"
#include "commands/infrared/tvbgone.h"
#include "pirate/irio_pio.h"
#include "commands/infrared/irtxrx.h"



static struct _infrared_mode_config mode_config;
static uint8_t device_cleanup;

// an array of all the IR protocol functions
typedef struct _ir_protocols {
    int (*irtx_init)(uint pin_num, uint32_t mod_freq); // init    
    void (*irtx_deinit)(uint pin_num);   
    void (*irtx_write)(uint32_t *data);     // write
    bool (*irtx_wait_idle)(void);
    int (*irrx_init)(uint pin_num);
    void (*irrx_deinit)(uint pin_num); 
    ir_rx_status_t (*irrx_read)(uint32_t *rx_frame);      // read
    void (*irrx_drain_fifo)(void);
    uint32_t mod_freq;
    char display_name[4];
    uint8_t num_bits;
} ir_protocols;
static const ir_protocols ir_protocol[] = {
    {   .irtx_init = irio_pio_tx_init, 
        .irtx_deinit = irio_pio_tx_deinit,
        .irtx_write = irio_pio_tx_write,
        .irtx_wait_idle = irio_pio_tx_wait_idle,
        .irrx_init = irio_pio_rx_init,
        .irrx_deinit = irio_pio_rx_deinit,
        .irrx_read = irio_pio_rx_frame_printf, 
        .irrx_drain_fifo = irio_pio_rxtx_drain_fifo, 
        .mod_freq = 0,
        .display_name = "RAW",
        .num_bits = 32
    },
    {   .irtx_init = nec_tx_init, 
        .irtx_deinit = nec_tx_deinit,
        .irtx_write = nec_write,
        .irtx_wait_idle = nec_tx_wait_idle,
        .irrx_init = nec_rx_init,
        .irrx_deinit = nec_rx_deinit,
        .irrx_read = nec_get_frame,
        .irrx_drain_fifo = nec_rx_drain_fifo ,
        .mod_freq = 38000,
        .display_name = "NEC",
        .num_bits = 16
    },
    {   .irtx_init = rc5_tx_init,
        .irtx_deinit = rc5_tx_deinit,
        .irtx_write = rc5_send,
        .irtx_wait_idle = rc5_tx_wait_idle,
        .irrx_init = rc5_rx_init,
        .irrx_deinit = rc5_rx_deinit,
        .irrx_read = rc5_receive, 
        .irrx_drain_fifo = rc5_drain_fifo,
        .mod_freq = 36000,
        .display_name = "RC5",
        .num_bits = 16
    }
};  

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
    // each protocol has its own write function
    uint32_t tx_frame = 0xffff;
    ir_protocol[mode_config.protocol].irtx_write(&tx_frame);
    //wait for idle
    ir_protocol[mode_config.protocol].irtx_wait_idle();
    uint32_t timeout = 0;
    bool fail = 0;
    while (true) {
        //wait for valid data
        if (ir_protocol[mode_config.protocol].irrx_read(&tx_frame) == IR_RX_FRAME_OK) {
            printf("\r\nOK\r\n");
            break;
        }
        timeout++;
        if (timeout > 10000) {
            printf("FAIL\r\n");
            fail = 1;
            break;
        }
    }
    return fail;
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

    printf("\r\n");
}

// command configuration
const struct _mode_command_struct infrared_commands[] = {
    {   .command="test", 
        .func=&irtoy_test, 
        .description_text=T_INFRARED_CMD_TEST, 
        .supress_fala_capture=false
    },
    {   .command="tvbgone",
        .func=&tvbgone_player,
        .description_text=T_IR_CMD_TV_BGONE,  
        .supress_fala_capture=true
    },
    {
        .command="irtx",
        .func=&irtx_handler,
        .description_text=T_IR_CMD_IRTX,  
        .supress_fala_capture=false
    },
    {
        .command="irrx",
        .func=&irrx_handler,
        .description_text=T_IR_CMD_IRRX,  
        .supress_fala_capture=false
    }    
};
const uint32_t infrared_commands_count = count_of(infrared_commands);

// Pin labels shown on the display and in the terminal status bar
// No more than 4 characters long
static const char pin_labels[][5]={
    "LERN",
    "BARR",
    "IRTX",
    "38k",
    "56k"
};

static const uint8_t ir_rx_pins[] = {BIO3, BIO5, BIO7};

static const struct prompt_item infrared_rx_sensor_menu[] = { //{ T_IR_RX_SENSOR_MENU_LEARNER },
                                                        { T_IR_RX_SENSOR_MENU_BARRIER },
                                                        { T_IR_RX_SENSOR_MENU_38K_DEMOD },
                                                        {T_IR_RX_SENSOR_MENU_56K_DEMOD} };
static const struct prompt_item infrared_protocol_menu[] = { {T_IR_PROTOCOL_MENU_RAW}, 
                                                            { T_IR_PROTOCOL_MENU_NEC },
                                                            { T_IR_PROTOCOL_MENU_RC5 } };   

static const struct prompt_item infrared_tx_speed_menu[] = { { T_IR_TX_SPEED_MENU_1 } };

static const struct ui_prompt infrared_menu[] = {
    {
        .description = T_IR_PROTOCOL_MENU,
        .menu_items = infrared_protocol_menu,
        .menu_items_count = count_of(infrared_protocol_menu),
        .prompt_text = T_IR_PROTOCOL_MENU,
        .minval = 0,
        .maxval = 0,
        .defval = 1,
        .menu_action = 0,
        .config = &prompt_list_cfg
    },       
    {
        .description = T_IR_RX_SENSOR_MENU,
        .menu_items = infrared_rx_sensor_menu,
        .menu_items_count = count_of(infrared_rx_sensor_menu),
        .prompt_text = T_IR_RX_SENSOR_MENU,
        .minval = 0,
        .maxval = 0,
        .defval = 2,
        .menu_action = 0,
        .config = &prompt_list_cfg
    },
    {   .description = T_IR_TX_SPEED_MENU,
        .menu_items = infrared_tx_speed_menu,
        .menu_items_count = count_of(infrared_tx_speed_menu),
        .prompt_text = T_IR_TX_SPEED_PROMPT,
        .minval = 20,
        .maxval = 60,
        .defval = 38,
        .menu_action = 0,
        .config = &prompt_int_cfg 
    },    
     
};

static struct _infrared_mode_config mode_config;
// Pre-setup step. Show user menus for any configuration options.
// The Bus Pirate hardware is not "clean" and reset at this point.
// Any previous mode may still be running. This is only a configuration step,
// the user may cancel out of the menu and return to the previous mode.
// Don't touch hardware yet, save the settings in variables for later.
uint32_t infrared_setup(void) {
    prompt_result result;

    const char config_file[] = "bpirrxtx.bp";
    const mode_config_t config_t[] = {
        // clang-format off
        { "$.protocol", &mode_config.protocol, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.sensor", &mode_config.rx_sensor, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.freq", &mode_config.tx_freq, MODE_CONFIG_FORMAT_DECIMAL },
        // clang-format on
    };

    if (storage_load_mode(config_file, config_t, count_of(config_t))) {

        printf("\r\n\r\n%s%s%s\r\n", ui_term_color_info(), GET_T(T_USE_PREVIOUS_SETTINGS), ui_term_color_reset());
        infrared_settings();
        bool user_value;
        if (!ui_prompt_bool(&result, true, true, true, &user_value)) {
            return 0;
        }
        if (user_value) {
            return 1; // user said yes, use the saved settings
        }
    }

    ui_prompt_uint32(&result, &infrared_menu[0], &mode_config.protocol);
    if (result.exit) {
        return 0;
    }
    mode_config.protocol--;  

    if(ir_protocol[mode_config.protocol].mod_freq){
        mode_config.tx_freq = ir_protocol[mode_config.protocol].mod_freq;
        printf("\r\n\r\n%s%s TX modulation%s: %ukHz\r\n", ui_term_color_info(), ir_protocol[mode_config.protocol].display_name, ui_term_color_reset(), mode_config.tx_freq/1000);
    }else{
        ui_prompt_uint32(&result, &infrared_menu[2], &mode_config.tx_freq);
        if (result.exit) {
            return 0;
        }
        mode_config.tx_freq*=1000;     
    }    
    
    ui_prompt_uint32(&result, &infrared_menu[1], &mode_config.rx_sensor);
    if (result.exit) {
        return 0;
    }    
    mode_config.rx_sensor--;



    storage_save_mode(config_file, config_t, count_of(config_t));

    return 1;
}

// Setup execution. This is where we actually configure any hardware.
uint32_t infrared_setup_exc(void) {
    bio_input(BIO1); // 20-60kHz learner
    bio_input(BIO3); // 38kHz barrier
    bio_input(BIO5); // 36-40kHz demodulator
    bio_input(BIO7); // 56kHz modulator
    bio_buf_output(BIO4); // IR TX

    //claim and label pins
    system_bio_update_purpose_and_label(true, BIO1, BP_PIN_IO, pin_labels[0]);
    system_bio_update_purpose_and_label(true, BIO3, BP_PIN_IO, pin_labels[1]);
    system_bio_update_purpose_and_label(true, BIO4, BP_PIN_IO, pin_labels[2]);
    system_bio_update_purpose_and_label(true, BIO5, BP_PIN_IO, pin_labels[3]);
    system_bio_update_purpose_and_label(true, BIO7, BP_PIN_IO, pin_labels[4]);


    // configure and enable the state machines
    int status = ir_protocol[mode_config.protocol].irtx_init(bio2bufiopin[BIO4], mode_config.tx_freq); // uses two state machines, 16 instructions and one IRQ
    if (status < 0) {
        printf("Failed to initialize TX PIO\r\n");
    }
    status = ir_protocol[mode_config.protocol].irrx_init(bio2bufiopin[ir_rx_pins[mode_config.rx_sensor]]);
    if (status < 0) {
        printf("Failed to initialize RX PIO\r\n");
    }
    device_cleanup = mode_config.protocol;
    //system_config.subprotocol_name = ir_protocol_type[mode_config.protocol];
    system_config.subprotocol_name = ir_protocol[mode_config.protocol].display_name;
    system_config.num_bits=ir_protocol[mode_config.protocol].num_bits;
    return 1;
}

bool infrared_preflight_sanity_check(void){
    return ui_help_sanity_check(true, 0);
}

void infrared_setup_resume(void){
    // configure and enable the state machines
    int status = ir_protocol[mode_config.protocol].irtx_init(bio2bufiopin[BIO4], mode_config.tx_freq); // uses two state machines, 16 instructions and one IRQ
    if (status < 0) {
        printf("Failed to initialize TX PIO\r\n");
    }
    status = ir_protocol[mode_config.protocol].irrx_init(bio2bufiopin[ir_rx_pins[mode_config.rx_sensor]]);
    if (status < 0) {
        printf("Failed to initialize RX PIO\r\n");
    }
}

void infrared_cleanup_temp(void) {
    ir_protocol[device_cleanup].irtx_deinit(bio2bufiopin[BIO4]);
    ir_protocol[device_cleanup].irrx_deinit(bio2bufiopin[ir_rx_pins[mode_config.rx_sensor]]);
}

// Cleanup any configuration on exit.
void infrared_cleanup(void) {
    ir_protocol[device_cleanup].irtx_deinit(bio2bufiopin[BIO4]);
    ir_protocol[device_cleanup].irrx_deinit(bio2bufiopin[ir_rx_pins[mode_config.rx_sensor]]);
    // unclaim pins
    system_bio_update_purpose_and_label(false, BIO1, BP_PIN_IO, pin_labels[0]);
    system_bio_update_purpose_and_label(false, BIO3, BP_PIN_IO, pin_labels[1]);
    system_bio_update_purpose_and_label(false, BIO4, BP_PIN_IO, pin_labels[2]);
    system_bio_update_purpose_and_label(false, BIO5, BP_PIN_IO, pin_labels[3]);
    system_bio_update_purpose_and_label(false, BIO7, BP_PIN_IO, pin_labels[4]);
    // 1. Disable any hardware you used
    bio_init();
    system_config.subprotocol_name = 0x00;
    system_config.num_bits=8;
}



// Handler for any numbers the user enters (1, 0x01, 0b1) or string data "string"
// This function generally writes data out to the IO pins or a peripheral
void infrared_write(struct _bytecode* result, struct _bytecode* next) {
    // each protocol has its own write function
    ir_protocol[mode_config.protocol].irtx_write(&result->out_data);
    //TODO: frame delay if next command is a write or something?
    //ir_protocol[mode_config.protocol].irtx_wait_idle();
}

// This function is called when the user enters 'r' to read data
void infrared_read(struct _bytecode* result, struct _bytecode* next) {
    // your code
    /*uint8_t data=0;
    for(uint8_t i=0; i<8; i++){
        data |= bio_get(i) << i;
    }
    result->in_data=data; //put the read value in in_data (up to 32 bits)*/
    //rc5_test();
}

void infrared_wait_idle(void) {
    ir_protocol[mode_config.protocol].irtx_wait_idle();
}

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


// The Bus Pirate will make a periodic call to this function (if linked in modes.c)
// Useful for checking async stuff like bytes in a UART
void infrared_periodic(void){
    uint32_t rx_frame;
    ir_protocol[mode_config.protocol].irrx_read(&rx_frame);
    return;
}

void infrared_help(void) {
    ui_help_mode_commands(infrared_commands, infrared_commands_count);
}

uint32_t infrared_get_speed(void) {
    return mode_config.tx_freq * 1000;
}

void infrared_settings(void) {
    ui_prompt_mode_settings_string(GET_T(T_IR_PROTOCOL_MENU), GET_T(infrared_protocol_menu[mode_config.protocol].description), 0x00);
    ui_prompt_mode_settings_string(GET_T(T_IR_RX_SENSOR_MENU), GET_T(infrared_rx_sensor_menu[mode_config.rx_sensor].description), 0x00);
    ui_prompt_mode_settings_int(GET_T(T_IR_TX_SPEED_MENU), mode_config.tx_freq/1000, GET_T(T_KHZ));
}