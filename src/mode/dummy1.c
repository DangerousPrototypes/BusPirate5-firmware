// ============================================================================
// DUMMY1 — Teaching Example Mode for Bus Pirate 5/6/7
// ============================================================================
//
// This file is a reference implementation showing how to create a new protocol
// mode for the Bus Pirate firmware. It demonstrates every major pattern a mode
// developer needs:
//
//   1. Constraint-based setup (bp_cmd) — flags, choices, ranges
//   2. Dual-path setup: interactive wizard vs command-line flags
//   3. Saved settings — load previous config, offer to reuse, save on exit
//   4. Settings display — show current configuration via 'i' command
//   5. Hardware setup/teardown — pin claiming, peripheral init, cleanup
//   6. Syntax engine handlers — write, read, start, stop, macros
//   7. Mode-specific commands — extending the command table
//
// How modes work (three-step syntax pipeline):
//   Step 1: The syntax system pre-processes user input into simple bytecodes.
//   Step 2: A loop hands each bytecode to a mode function below for actual IO.
//   Step 3: A post-processing loop outputs the results to the user terminal.
//
//   Because of this pipeline, you cannot printf directly during IO operations.
//   Instead, set fields on the result struct (data_message, error, in_data, etc).
//
// To enable this mode:
//   Open pirate.h and uncomment: #define BP_USE_DUMMY1
//
// Registration:
//   The mode is registered in modes.c in the modes[] array under [DUMMY1].
//   Key fields to set:
//     .protocol_setup     = dummy1_setup        — your setup UI function
//     .protocol_setup_exc = dummy1_setup_exc    — hardware init (called after setup)
//     .protocol_cleanup   = dummy1_cleanup      — teardown on mode exit
//     .protocol_settings  = dummy1_settings     — display config for 'i' command
//     .setup_def          = &dummy1_setup_def   — enables CLI flags & linenoise hints
//     .mode_commands      = dummy1_commands      — mode-specific command table
//     .mode_commands_count= &dummy1_commands_count
//
// ============================================================================

#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"           // Bytecode structure for data IO
#include "pirate/bio.h"         // Buffered pin IO functions
#include "pirate/storage.h"     // storage_load_mode / storage_save_mode
#include "ui/ui_help.h"         // ui_help_mode_commands
#include "ui/ui_prompt.h"       // ui_prompt_bool, ui_prompt_mode_settings_*
#include "ui/ui_term.h"         // ui_term_color_info, ui_term_color_reset
#include "dummy1.h"
#include "lib/bp_args/bp_cmd.h" // Constraint-based setup: flags, prompts, help, hints

// ---- Mode-specific configuration struct ----
// Each mode keeps its own file-static config struct.
// Fields are uint32_t so they work with the storage_load/save helpers.
static struct {
    uint32_t speed;     // Example numeric parameter (1..1000)
    uint32_t output;    // Example choice parameter (0=push-pull, 1=open-drain)
} mode_config;

static uint32_t returnval;

// ---- Mode-specific commands ----
// If your mode has special commands (like UART's bridge, monitor, etc),
// add them here with a bp_command_def_t for each.
// The table MUST be { 0 } terminated.
const struct _mode_command_struct dummy1_commands[] = { 0 };
const uint32_t dummy1_commands_count = count_of(dummy1_commands);

// ---- Pin labels ----
// Shown on the display and in the terminal status bar.
// No more than 4 characters long.
static const char pin_labels[][5] = { "OUT1", "OUT2", "OUT3", "IN1" };

// ============================================================================
// CONSTRAINT-BASED SETUP DEFINITIONS
// ============================================================================
//
// The bp_cmd system uses a single bp_command_def_t to drive:
//   - CLI flag parsing        (m dummy1 -s 500 -o od)
//   - Interactive prompting   (wizard menus with validation)
//   - Help display            (m dummy1 -h)
//   - Linenoise hints         (ghost text as you type)
//   - Tab completion          (complete flag names)
//
// Build it bottom-up: constraints → opts → def.

// ---- Step 1: Value constraints ----
// Each configurable parameter gets a bp_val_constraint_t that defines its
// type, valid range (or choices), default value, and prompt/hint text.
//
// BP_VAL_UINT32 — integer range constraint:
//   .u.min, .u.max = valid range (inclusive)
//   .u.def         = default value (used when flag absent on CLI)
//   .prompt        = T_ key for the interactive menu title (0 = placeholder)
//   .hint          = T_ key for help text below the prompt (0 = placeholder)
static const bp_val_constraint_t dummy1_speed_range = {
    .type = BP_VAL_UINT32,
    .u = { .min = 1, .max = 1000, .def = 100 },
    .prompt = 0, // Use a T_ key here for translation, e.g. T_DUMMY1_SPEED_MENU
    .hint = 0,   // Use a T_ key here for a hint subtitle
};

// BP_VAL_CHOICE — named-choice constraint:
//   Each bp_val_choice_t entry:
//     .name  = CLI string the user types (e.g. "push-pull")
//     .alias = short alias (e.g. "pp")
//     .label = T_ key for the interactive menu label (0 = placeholder)
//     .value = integer stored in config when selected
//   The constraint's .choice.def = index of the default choice.
static const bp_val_choice_t dummy1_output_choices[] = {
    { "push-pull",  "pp", 0, 0 }, // value=0 → push-pull
    { "open-drain", "od", 0, 1 }, // value=1 → open-drain
};
static const bp_val_constraint_t dummy1_output_choice = {
    .type = BP_VAL_CHOICE,
    .choice = { .choices = dummy1_output_choices, .count = 2, .def = 0 },
    .prompt = 0, // e.g. T_DUMMY1_OUTPUT_MENU
};

// ---- Step 2: Flag/option table ----
// Maps CLI flags to constraints. Each entry:
//   long_name   — "--speed"
//   short_name  — '-s'
//   arg_type    — BP_ARG_REQUIRED (flag takes a value), BP_ARG_NONE (boolean flag)
//   arg_hint    — shown in help: "-s <1-1000>"
//   description — T_ key for help text (0 = placeholder)
//   constraint  — pointer to the bp_val_constraint_t (NULL = no auto-validation)
//
// The array MUST end with a sentinel { 0 } entry.
static const bp_command_opt_t dummy1_setup_opts[] = {
    { "speed",  's', BP_ARG_REQUIRED, "1-1000",               0, &dummy1_speed_range },
    { "output", 'o', BP_ARG_REQUIRED, "push-pull/open-drain", 0, &dummy1_output_choice },
    { 0 }, // ← sentinel — always required
};

// ---- Step 3: Command definition ----
// The master struct that ties everything together. Non-static so it can be
// exported via the header and wired into modes[] as .setup_def.
//
//   .name        — matches the protocol_name (lowercase) used with `m` command
//   .description — T_ key (0 = placeholder)
//   .opts        — pointer to the flag table above
//   .positionals — NULL (modes use flags, not positional args)
//   .actions     — NULL (modes don't have sub-commands)
//   .usage       — NULL (auto-generated from opts)
const bp_command_def_t dummy1_setup_def = {
    .name = "dummy1",
    .description = 0,
    .opts = dummy1_setup_opts,
};

// ============================================================================
// SETUP FUNCTION — dual-path interactive/CLI pattern
// ============================================================================
//
// This is the most complex function in a mode. It handles two paths:
//
//   Interactive (no flags given, e.g. `m dummy1`):
//     1. Try to load saved settings from flash
//     2. If found, display them and ask "use previous settings?"
//     3. If user says no (or no saved settings), run the prompt wizard
//     4. Save the new settings to flash
//
//   CLI (flags given, e.g. `m dummy1 -s 500 -o od`):
//     1. Primary flag already parsed — parse remaining flags
//     2. Missing flags get their constraint default value automatically
//     3. Save settings to flash
//
// Returns: 1 = success (proceed to setup_exc), 0 = user cancelled or error.
//
uint32_t dummy1_setup(void) {

    // ---- Saved settings file descriptor ----
    // Map each config field to a JSON tag for file storage.
    // The file is stored on the Bus Pirate's flash filesystem.
    // Fields: tag (JSON path), pointer to uint32_t, format.
    const char config_file[] = "bpdummy1.bp";
    const mode_config_t config_t[] = {
        // clang-format off
        { "$.speed",  &mode_config.speed,  MODE_CONFIG_FORMAT_DECIMAL },
        { "$.output", &mode_config.output, MODE_CONFIG_FORMAT_DECIMAL },
        // clang-format on
    };

    // ---- Detect interactive vs CLI mode ----
    // Check the "primary" flag. bp_cmd_flag() returns:
    //   BP_CMD_OK      — flag found and valid (value written to mode_config.speed)
    //   BP_CMD_MISSING — flag not on command line (default written to mode_config.speed)
    //   BP_CMD_INVALID — flag present but failed validation (error already printed)
    bp_cmd_status_t st = bp_cmd_flag(&dummy1_setup_def, 's', &mode_config.speed);
    if (st == BP_CMD_INVALID) return 0;
    bool interactive = (st == BP_CMD_MISSING);

    if (interactive) {
        // ---- Interactive path ----

        // Try to load previously saved settings from flash.
        // storage_load_mode() returns true if a config file was found and loaded.
        // The loaded values are written directly into mode_config.* fields.
        if (storage_load_mode(config_file, config_t, count_of(config_t))) {
            // Settings found — show them and ask if the user wants to reuse them.
            printf("\r\n\r\n%s%s%s\r\n", ui_term_color_info(),
                   GET_T(T_USE_PREVIOUS_SETTINGS), ui_term_color_reset());
            dummy1_settings(); // Display the loaded values

            prompt_result result;
            bool user_value;
            if (!ui_prompt_bool(&result, true, true, true, &user_value)) {
                return 0; // User pressed 'x' to exit
            }
            if (user_value) {
                return 1; // User accepted saved settings — skip wizard
            }
            // User said no — fall through to the full wizard below.
        }

        // ---- Full interactive wizard ----
        // bp_cmd_prompt() displays a menu driven by the constraint:
        //   - BP_VAL_UINT32: shows "min-max (default)" and validates input
        //   - BP_VAL_CHOICE: shows a numbered menu of named options
        // Returns BP_CMD_OK on success, BP_CMD_EXIT if user cancels.
        printf("\r\n-DUMMY1- setup()\r\n");

        if (bp_cmd_prompt(&dummy1_speed_range, &mode_config.speed) != BP_CMD_OK) return 0;
        if (bp_cmd_prompt(&dummy1_output_choice, &mode_config.output) != BP_CMD_OK) return 0;

    } else {
        // ---- CLI path ----
        // The primary flag (-s) was already parsed above.
        // Parse remaining flags. BP_CMD_MISSING means the flag was absent —
        // the constraint's default value is automatically written to the output.
        st = bp_cmd_flag(&dummy1_setup_def, 'o', &mode_config.output);
        if (st == BP_CMD_INVALID) return 0;
    }

    // ---- Save settings to flash ----
    // Always save after a successful setup so the next `m dummy1` (interactive)
    // can offer to reuse these settings.
    storage_save_mode(config_file, config_t, count_of(config_t));

    // Display the final configuration.
    dummy1_settings();

    return 1;
}

// ============================================================================
// HARDWARE SETUP — called after setup() succeeds
// ============================================================================
// This is where you actually configure peripherals, set pin directions, and
// claim IO pins. Separated from setup() so the UI/config step can be
// cancelled without touching hardware.
uint32_t dummy1_setup_exc(void) {
    // 1. Configure hardware pins / peripherals.
    //    In a real mode this is where you init UART/SPI/I2C/PIO, set baud, etc.
    //    Here we just set some pins to output/input as an example.
    bio_output(BIO4);
    bio_output(BIO5);
    bio_output(BIO6);
    bio_input(BIO7);

    // 2. Claim IO pins so the Bus Pirate won't let the user manipulate them
    //    (no PWM/FREQ/etc on claimed pins while mode is active).
    system_bio_update_purpose_and_label(true, BIO4, BP_PIN_MODE, pin_labels[0]);
    system_bio_update_purpose_and_label(true, BIO5, BP_PIN_MODE, pin_labels[1]);
    system_bio_update_purpose_and_label(true, BIO6, BP_PIN_MODE, pin_labels[2]);
    system_bio_update_purpose_and_label(true, BIO7, BP_PIN_MODE, pin_labels[3]);
    printf("-DUMMY1- setup_exc()\r\n");
    return 1;
}

// ============================================================================
// CLEANUP — called when user exits the mode (back to HiZ)
// ============================================================================
void dummy1_cleanup(void) {
    // 1. Disable/deinit any hardware you configured.
    bio_init();

    // 2. Release IO pins and clear labels.
    system_bio_update_purpose_and_label(false, BIO4, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO5, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO6, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO7, BP_PIN_MODE, 0);
    printf("-DUMMY1- cleanup()\r\n");
}

// ============================================================================
// SETTINGS DISPLAY — shown by the 'i' (info) command
// ============================================================================
// Use ui_prompt_mode_settings_int() for numeric values and
// ui_prompt_mode_settings_string() for choice/enum values.
// These print in a standardised format that matches all other modes.
void dummy1_settings(void) {
    // Numeric setting: label, value, units string (0 for no units)
    ui_prompt_mode_settings_int(
        "Speed",                    // label — use GET_T(T_xxx) for translation
        mode_config.speed,          // current value
        0                           // units string (e.g. GET_T(T_KHZ)) or 0
    );
    // Choice setting: label, selected choice name, units
    // Look up the name from the choice array using the stored value index.
    const char* output_name = "push-pull";
    for (uint32_t i = 0; i < count_of(dummy1_output_choices); i++) {
        if (dummy1_output_choices[i].value == mode_config.output) {
            output_name = dummy1_output_choices[i].name;
            break;
        }
    }
    ui_prompt_mode_settings_string(
        "Output type",              // label
        output_name,                // current choice name
        0                           // units
    );
}

// ============================================================================
// WRITE HANDLER — user enters a number (1, 0x01, 0b1) or string "hello"
// ============================================================================
// This function writes data out to IO pins or a peripheral.
// DO NOT use printf() here — set result fields instead.
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

// ============================================================================
// READ HANDLER — user enters 'r' to read data
// ============================================================================
void dummy1_read(struct _bytecode* result, struct _bytecode* next) {
    static const char message[] = "--DUMMY1- read()";

    // your code
    uint32_t data = bio_get(BIO7);

    result->in_data = data;         // put the read value in in_data (up to 32 bits)
    result->data_message = message; // add a text decoration if you like
}

// ============================================================================
// START HANDLER — user enters '['
// ============================================================================
void dummy1_start(struct _bytecode* result, struct _bytecode* next) {
    static const char message[] = "-DUMMY1- start()"; // The message to show the user

    bio_put(BIO4, 1); // your code

    result->data_message = message; // return a reference to the message to show the user
}

// ============================================================================
// STOP HANDLER — user enters ']'
// ============================================================================
void dummy1_stop(struct _bytecode* result, struct _bytecode* next) {
    static const char message[] = "-DUMMY1- stop()"; // The message to show the user

    bio_put(BIO4, 0); // your code

    result->data_message = message; // return a reference to the message to show the user
}

// ============================================================================
// MACRO HANDLER — user enters (0), (1), etc.
// ============================================================================
// Macros are passed from the command line directly, not through the syntax system.
// (0) is always a menu listing available macros.
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

// ============================================================================
// PERIODIC SERVICE — called regularly by the main loop
// ============================================================================
// Useful for checking async events like bytes arriving in a UART buffer.
// Link via .protocol_periodic in modes.c (use noperiodic if not needed).
void dummy1_periodic(void) {
    // your periodic service functions
    static uint32_t cnt;
    if (cnt > 0xffffff) {
        printf("\r\n-DUMMY1- periodic\r\n");
        cnt = 0;
    }
    cnt++;
}

// ============================================================================
// FULL DUPLEX START/STOP — '{' and '}' keys
// ============================================================================
// Used by SPI for simultaneous read/write. Most modes leave these as stubs
// or point .protocol_start_alt / .protocol_stop_alt at the regular start/stop.
void dummy1_startr(struct _bytecode* result, struct _bytecode* next) {
    (void)result; (void)next;
    printf("-DUMMY1- startr()");
}

void dummy1_stopr(struct _bytecode* result, struct _bytecode* next) {
    (void)result; (void)next;
    printf("-DUMMY1- stopr()");
}

// ============================================================================
// BITWISE HANDLERS — clock/data line manipulation
// ============================================================================
// Legacy bitbang support. Most hardware modes don't need these —
// use nullfunc1_temp in your modes.c registration instead.
// If your protocol requires bit-level control, implement them here.
// Signature must match: void(struct _bytecode*, struct _bytecode*)
void dummy1_clkh(struct _bytecode* result, struct _bytecode* next) {
    (void)result; (void)next;
    printf("-DUMMY1- clkh()");
}
void dummy1_clkl(struct _bytecode* result, struct _bytecode* next) {
    (void)result; (void)next;
    printf("-DUMMY1- clkl()");
}
void dummy1_dath(struct _bytecode* result, struct _bytecode* next) {
    (void)result; (void)next;
    printf("-DUMMY1- dath()");
}
void dummy1_datl(struct _bytecode* result, struct _bytecode* next) {
    (void)result; (void)next;
    printf("-DUMMY1- datl()");
}
void dummy1_dats(struct _bytecode* result, struct _bytecode* next) {
    (void)result; (void)next;
    printf("-DUMMY1- dats()=%08X", returnval);
}
void dummy1_clk(struct _bytecode* result, struct _bytecode* next) {
    (void)result; (void)next;
    printf("-DUMMY1- clk()");
}
void dummy1_bitr(struct _bytecode* result, struct _bytecode* next) {
    (void)result; (void)next;
    printf("-DUMMY1- bitr()=%08X", returnval);
}

// ============================================================================
// HELP — show mode-specific commands (called by '?' in mode)
// ============================================================================
void dummy1_help(void) {
    ui_help_mode_commands(dummy1_commands, dummy1_commands_count);
}
