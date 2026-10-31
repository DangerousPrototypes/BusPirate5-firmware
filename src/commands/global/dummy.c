// ============================================================================
// DUMMY — Teaching Example Command for Bus Pirate 5/6/7
// ============================================================================
//
// This file is a reference implementation showing how to create a new global
// command for the Bus Pirate firmware. It demonstrates every major pattern a
// command developer needs:
//
//   1. Command definition (bp_command_def_t) — the single source of truth
//   2. Action verbs / subcommands (bp_command_action_t) — "dummy init", "dummy test"
//   3. Flags with constraints (bp_command_opt_t + bp_val_constraint_t)
//   4. Simple flag queries — boolean, string, integer
//   5. Constraint-aware flag parsing (bp_cmd_flag with validation)
//   6. Interactive prompting (bp_cmd_prompt — fallback when args missing)
//   7. Help display (bp_cmd_help_check)
//   8. Usage examples
//   9. File I/O (create, write, read)
//  10. Error handling and system_config.error
//
// Type "dummy" at the Bus Pirate prompt to see this command in action.
// Type "dummy -h" for help.
//
// Registration:
//   The command is registered in commands.c in the commands[] array:
//     { .command="dummy", .allow_hiz=true, .func=&dummy_handler,
//       .def=&dummy_def, .description_text=0x00, .category=CMD_CAT_HIDDEN }
//
//   Key fields:
//     .command          — the string users type at the prompt
//     .func             — pointer to your handler function
//     .def              — pointer to your bp_command_def_t (enables help, hints, completion)
//     .allow_hiz        — true if the command can run in HiZ mode
//     .description_text — T_ key for the `h` help listing (0x00 = hidden)
//     .category         — CMD_CAT_HIDDEN, CMD_CAT_TOOLS, CMD_CAT_IO, etc.
//
// ============================================================================

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "command_struct.h"
#include "fatfs/ff.h"              // File system (FatFS)
#include "pirate/storage.h"        // File system helpers
#include "lib/bp_args/bp_cmd.h"    // Unified command parsing, validation, prompting, help, hints
#include "ui/ui_help.h"            // Help display utilities
#include "system_config.h"         // Current Bus Pirate system configuration
#include "pirate/amux.h"           // Analog voltage measurement functions
#include "pirate/button.h"         // Button press functions
#include "msc_disk.h"

// ============================================================================
// USAGE EXAMPLES
// ============================================================================
// Displayed when the user enters "dummy -h".
// The first entry is the synopsis (with \r\n\t for line wrapping).
// Remaining entries are "Label:%s command example" — %s is replaced with the
// prompt name by the help renderer.
static const char* const usage[] = {
    "dummy [init|test]\r\n\t[-b(utton)] [-i(nteger) <value>] [-f <file>]",
    "Initialize:%s dummy init",
    "Test:%s dummy test",
    "Test, require button press:%s dummy test -b",
    "Integer, value required:%s dummy -i 123",
    "Interactive integer prompt:%s dummy init -b",
    "Create/write/read file:%s dummy -f dummy.txt",
    "Kitchen sink:%s dummy test -b -i 123 -f dummy.txt",
};

// ============================================================================
// ACTIONS / SUBCOMMANDS
// ============================================================================
// Actions are the first non-flag token after the command name.
// They are matched by bp_cmd_get_action() and returned as an enum value.
// This replaces manual strcmp() parsing of positional arguments.
enum dummy_actions {
    DUMMY_INIT = 1, // enum values should start at 1 (0 = no action)
    DUMMY_TEST = 2,
};

static const bp_command_action_t dummy_action_defs[] = {
    { DUMMY_INIT, "init", T_HELP_DUMMY_INIT },  // "dummy init" → action=DUMMY_INIT
    { DUMMY_TEST, "test", T_HELP_DUMMY_TEST },   // "dummy test" → action=DUMMY_TEST
};

// ============================================================================
// VALUE CONSTRAINTS
// ============================================================================
// Each validated parameter gets a bp_val_constraint_t that defines its type,
// valid range, default value, and prompt text.
//
// This constraint is used by:
//   bp_cmd_flag()   — validates the CLI value against the range
//   bp_cmd_prompt() — drives an interactive prompt with range display
static const bp_val_constraint_t integer_range = {
    .type = BP_VAL_UINT32,
    .u = { .min = 0, .max = 65535, .def = 0 },
    .prompt = 0, // T_ key for interactive prompt title (0 = placeholder)
    .hint = 0,   // T_ key for hint subtitle (0 = placeholder)
};

// ============================================================================
// FLAG / OPTION TABLE
// ============================================================================
// Maps CLI flags to constraints. Each entry:
//   long_name, short_name, arg_type, arg_hint, description T_ key, constraint ptr
//
// The array MUST end with a { 0 } sentinel.
static const bp_command_opt_t dummy_opts[] = {
    { "button",  'b', BP_ARG_NONE,     NULL,    T_HELP_DUMMY_B_FLAG },
    { "integer", 'i', BP_ARG_REQUIRED, "value", T_HELP_DUMMY_I_FLAG, &integer_range },
    { "file",    'f', BP_ARG_REQUIRED, "file",  T_HELP_DUMMY_FILE_FLAG },
    { 0 }, // ← sentinel — always required
};

// ============================================================================
// COMMAND DEFINITION
// ============================================================================
// The master struct that ties everything together. Non-static so it can be
// exported via the header and wired into commands[] in commands.c.
//
// This single struct drives: help display, CLI parsing, tab-completion,
// linenoise hints, and action resolution.
const bp_command_def_t dummy_def = {
    .name = "dummy",
    .description = 0x00,        // T_ key for `h` listing (0x00 = no description)
    .actions = dummy_action_defs,
    .action_count = count_of(dummy_action_defs),
    .opts = dummy_opts,
    .usage = usage,
    .usage_count = count_of(usage),
};

// ============================================================================
// COMMAND HANDLER
// ============================================================================
// This is the entry point called when the user types "dummy" at the prompt.
// The command_result struct is passed by the dispatcher; res->help_flag is
// set automatically if the user entered -h.
void dummy_handler(struct command_result* res) {

    // ================================================================
    // STEP 1: Help check
    // ================================================================
    // bp_cmd_help_check() displays auto-generated help from the def
    // (usage examples, flags table, actions list) and returns true.
    // If -h was given, display help and return immediately.
    if (bp_cmd_help_check(&dummy_def, res->help_flag)) {
        return;
    }

    // ================================================================
    // STEP 2: Safety / precondition checks (optional)
    // ================================================================
    // If your command requires a valid voltage reference, call
    // ui_help_check_vout_vref(). It returns false and prints an error
    // if the voltage is too low.
    //
    // To restrict a command to a specific mode, check system_config.mode.
    // However, mode commands (registered per-mode) are the preferred way
    // to scope commands to a mode.
    printf("Current mode: %d\r\n", system_config.mode);

    if (!ui_help_check_vout_vref()) {
        printf("Warning: Vout pin is not connected to a valid voltage source\r\n");
    } else {
        printf("Vout pin is connected to a valid voltage source\r\n");
    }

    // ================================================================
    // STEP 3: Action / subcommand resolution
    // ================================================================
    // bp_cmd_get_action() matches the first non-flag token against the
    // actions[] array in the def. Returns true + writes the enum value,
    // or false if no action token is present.
    //
    // This replaces manual strcmp() on positional arguments.
    //
    // Command:  dummy init -b
    //           ^^^^^ ^^^^
    //           cmd   action
    uint32_t action = 0;
    if (bp_cmd_get_action(&dummy_def, &action)) {
        printf("Action: %s (enum=%d)\r\n",
               (action == DUMMY_INIT ? "init" : "test"),
               action);
    } else {
        printf("No action given (try: dummy init, dummy test)\r\n");
    }

    // ================================================================
    // STEP 4: Boolean flag — bp_cmd_find_flag()
    // ================================================================
    // Returns true if the flag is present, false if not.
    // No value is consumed — this is a simple on/off switch.
    bool b_flag = bp_cmd_find_flag(&dummy_def, 'b');
    printf("Flag -b is %s\r\n", (b_flag ? "set" : "not set"));
    if (b_flag) {
        printf("Press Bus Pirate button to continue\r\n");
        while (!button_get(0)) {
            tight_loop_contents();
        }
        printf("Button pressed\r\n");
    }

    // ================================================================
    // STEP 5: Constraint-aware integer flag — bp_cmd_flag()
    // ================================================================
    // bp_cmd_flag() uses the constraint on the opt to:
    //   - Parse the CLI value as the correct type
    //   - Validate against the constraint range
    //   - Write the constraint default if the flag is absent
    //
    // Returns bp_cmd_status_t:
    //   BP_CMD_OK      — present and valid (value written)
    //   BP_CMD_MISSING — not present (constraint default written)
    //   BP_CMD_INVALID — present but out of range (error already printed)
    uint32_t value;
    bp_cmd_status_t i_status = bp_cmd_flag(&dummy_def, 'i', &value);

    if (i_status == BP_CMD_OK) {
        printf("Flag -i is set with value %d\r\n", value);
    } else if (i_status == BP_CMD_INVALID) {
        // Constraint violation — the API already printed the range error.
        printf("Flag -i has an invalid value. Try -i 0\r\n");
        system_config.error = true;
        return;
    } else { // BP_CMD_MISSING — flag not entered, default written to value
        printf("Flag -i is not set (default: %d)\r\n", value);
    }

    // ================================================================
    // STEP 6: Interactive prompt fallback — bp_cmd_prompt()
    // ================================================================
    // If a flag is missing, you can fall back to an interactive prompt
    // driven by the same constraint. This is the "dual-path" pattern:
    //   CLI path:         user provides -i 123 → parsed above
    //   Interactive path: user omits -i → prompt wizard
    //
    // For a full dual-path example see dummy1.c (mode) or w_psu.c (command).
    // Here we demonstrate a simpler version: prompt only if action is "init"
    // and -i was not given.
    if (action == DUMMY_INIT && i_status == BP_CMD_MISSING) {
        printf("No -i flag given — entering interactive prompt:\r\n");
        // bp_cmd_prompt() displays a menu driven by the constraint,
        // validates input, and loops on error. Returns BP_CMD_OK or BP_CMD_EXIT.
        bp_cmd_status_t prompt_st = bp_cmd_prompt(&integer_range, &value);
        if (prompt_st != BP_CMD_OK) {
            printf("Prompt cancelled\r\n");
            return;
        }
        printf("User entered: %d\r\n", value);
    }

    // ================================================================
    // STEP 7: String flag — bp_cmd_get_string()
    // ================================================================
    // Copies the flag's value as a string into a buffer.
    // Returns true if the flag is present, false if not.
    // Use for filenames, search strings, etc.
    char file[13]; // 8.3 filename + null = 13 characters max
    bool f_flag = bp_cmd_get_string(&dummy_def, 'f', file, sizeof(file));

    if (!f_flag) {
        printf("Flag -f is not set\r\n");
    } else {
        printf("Flag -f is set with file name %s\r\n", file);

        // ============================================================
        // File I/O example: create, write, read
        // ============================================================

        // Create and write
        printf("Creating file %s\r\n", file);
        FIL file_handle;
        FRESULT result;
        result = f_open(&file_handle, file, FA_CREATE_ALWAYS | FA_WRITE);
        if (result != FR_OK) {
            printf("Error creating file %s\r\n", file);
            system_config.error = true;
            return;
        }
        printf("File %s created\r\n", file);

        char buffer[256] = "This is a test file created by the dummy command";
        printf("Writing to file %s: %s\r\n", file, buffer);
        UINT bytes_written;
        result = f_write(&file_handle, buffer, strlen(buffer), &bytes_written);
        if (result != FR_OK) {
            printf("Error writing to file %s\r\n", file);
            f_close(&file_handle);
            system_config.error = true;
            return;
        }
        printf("Wrote %d bytes to file %s\r\n", bytes_written, file);

        result = f_close(&file_handle);
        if (result != FR_OK) {
            printf("Error closing file %s\r\n", file);
            system_config.error = true;
            return;
        }
        printf("File %s closed\r\n", file);

        // Read back
        result = f_open(&file_handle, file, FA_READ);
        if (result != FR_OK) {
            printf("Error opening file %s for reading\r\n", file);
            system_config.error = true;
            return;
        }
        printf("File %s opened for reading\r\n", file);

        UINT bytes_read;
        result = f_read(&file_handle, buffer, sizeof(buffer), &bytes_read);
        if (result == FR_OK) {
            printf("Read %d bytes from file %s\r\n", bytes_read, file);
            printf("File contents: %s\r\n", buffer);
        } else {
            printf("Error reading file %s\r\n", file);
            system_config.error = true;
        }

        result = f_close(&file_handle);
        if (result != FR_OK) {
            printf("Error closing file %s\r\n", file);
            system_config.error = true;
            return;
        }
        printf("File %s closed\r\n", file);

        printf("Hint: use the ls and check that %s is in the list of files\r\n", file);
        printf("Hint: use the cat %s to print the contents\r\n", file);
        printf("Hint: use the rm %s to delete\r\n", file);
    }

    // ================================================================
    // STEP 8: Error reporting
    // ================================================================
    // To signal an error back to the command dispatcher (for chaining
    // with ; || &&), set: system_config.error = true;
    // The command_result struct res is also available for future use.

    printf("dummy command complete\r\n");
}