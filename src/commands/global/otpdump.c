// TODO: BIO use, pullups, psu
/*
    Welcome to dummy.c, a growing demonstration of how to add commands to the Bus Pirate firmware.
    You can also use this file as the basis for your own commands.
    Type "dummy" at the Bus Pirate prompt to see the output of this command
    Temporary info available at https://forum.buspirate.com/t/command-line-parser-for-developers/235
*/
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "command_struct.h"       // File system related
//#include "fatfs/ff.h"       // File system related
//#include "pirate/storage.h" // File system related
#include "lib/bp_args/bp_cmd.h"    // This file is needed for the command line parsing functions
// #include "ui/ui_prompt.h" // User prompts and menu system
// #include "ui/ui_const.h"  // Constants and strings
#include "ui/ui_help.h"    // Functions to display help in a standardized way
#include "system_config.h" // Stores current Bus Pirate system configuration
//#include "pirate/amux.h"   // Analog voltage measurement functions
//#include "pirate/button.h" // Button press functions
//#include "msc_disk.h"
#include "pico/bootrom.h"
#include "hardware/structs/otp.h"
#include "ui/ui_term.h"

// This array of strings is used to display help USAGE examples for the dummy command
static const char* const usage[] = {
    "otpdump -r <start row> -c <maximum row count> -a",
    "   -r <start row>         : First OTP Row Address to dump",
    "   -c <maximum row count> : Maximum number of OTP rows to dump",
    "   -a                     : Show even blank (all-zero) rows",
    "",
    "By default, this command will show only non-blank rows.",
};

static const bp_command_opt_t otpdump_opts[] = {
    { "row",   'r', BP_ARG_REQUIRED, "start", T_HELP_FLAG },
    { "count", 'c', BP_ARG_REQUIRED, "count", T_HELP_FLAG },
    { "all",   'a', BP_ARG_NONE,     NULL,      T_HELP_FLAG },
    { 0 }
};

const bp_command_def_t otpdump_def = {
    .name         = "otpdump",
    .description  = T_HELP_DUMMY_COMMANDS,
    .actions      = NULL,
    .action_count = 0,
    .opts         = otpdump_opts,
    .usage        = usage,
    .usage_count  = count_of(usage),
};

typedef struct _PARSED_OTP_COMMAND_OPTIONS {
    uint16_t StartRow;
    uint16_t MaximumRows;
    bool ShowAllRows;
} PARSED_OTP_COMMAND_OPTIONS;

typedef struct _OTP_DUAL_ROW_READ_RESULT {
    union {
        uint32_t as_uint32;
        struct { // is this order backwards?
            uint16_t row0;
            uint16_t row1;
        };
    };
} OTP_DUAL_ROW_READ_RESULT;
static_assert(sizeof(OTP_DUAL_ROW_READ_RESULT) == sizeof(uint32_t), "");

typedef struct _OTP_RAW_READ_RESULT {
    // anonymous structs are supported in C11
    union {
        uint32_t as_uint32;
        struct {
            uint8_t lsb;
            uint8_t msb;
            uint8_t correction;
            uint8_t is_error; // 0: ok, 0xFF: Permission failure, 0x77: ECC error likely
        };
    };
} OTP_RAW_READ_RESULT;
static_assert(sizeof(OTP_RAW_READ_RESULT) == sizeof(uint32_t), "");

typedef struct _OTP_READ_RESULT {
    OTP_RAW_READ_RESULT as_raw;
    uint16_t read_with_ecc;
    uint16_t read_via_bootrom; // recommended to use this as "truth" if no errors
    bool data_ok;
    bool err_permissions;
    bool err_possible_ecc_mismatch;
    bool err_from_bootrom;
} OTP_READ_RESULT;

static const uint16_t LAST_OTP_ROW  = 4095u; // 8k of corrected ECC data == 4k rows
static const uint16_t OTP_ROW_COUNT = 4096u; // 8k of corrected ECC data == 4k rows

static char map_to_printable_char(uint8_t c) {
    if (c >= ' ' && c <= '~') {
        return (char)c;
    }
    return '.';
}
static void print_otp_read_result(const OTP_READ_RESULT * data, uint16_t row) {    
    uint16_t row_data =
        (!data->err_from_bootrom         ) ? data->read_via_bootrom :
        (!data->err_possible_ecc_mismatch) ? data->read_with_ecc    :
        data->read_via_bootrom;
    printf("%s", ui_term_color_info());
    printf("Row 0x%03" PRIX16 ":", row);
    printf(" %04" PRIX16, data->read_via_bootrom);

    if (data->read_via_bootrom != data->read_with_ecc) {
        printf(" %s!==%s ", ui_term_color_warning(), ui_term_color_info());
    } else {
        printf(" === ");
    }
    printf("%04" PRIX16, data->read_with_ecc);

    if (((data->read_via_bootrom >> 8)    != data->as_raw.msb) ||
        ((data->read_via_bootrom & 0xFFu) != data->as_raw.lsb)) {
        printf(" %s=?=%s ", ui_term_color_warning(), ui_term_color_info());
    } else {
        printf(" === ");
    }
    printf("%02" PRIX8 " %02" PRIX8 " [%02" PRIX8 "] (%c%c)",
        data->as_raw.lsb,
        data->as_raw.msb,
        data->as_raw.correction,
        map_to_printable_char(row_data & 0xFFu),
        map_to_printable_char(row_data >> 8)
        );
    if (data->err_permissions) {
        printf("%s", ui_term_color_error());
        printf(" PERM");
        printf("%s", ui_term_color_info());
    } else {
        printf("     ");
    }
    if (data->err_possible_ecc_mismatch) {
        printf("%s", ui_term_color_warning());
        printf("  ECC");
        printf("%s", ui_term_color_info());
    } else {
        printf("     ");
    }
    if (data->err_from_bootrom) {
        printf("%s", ui_term_color_error());
        printf("  ROM");
        printf("%s", ui_term_color_info());
    } else {
        printf("     ");
    }
    printf("%s", ui_term_color_reset());
    printf("\r\n");
}

// check res->error after calling this function to see if error occurred
static void parse_otp_command_line(PARSED_OTP_COMMAND_OPTIONS* options, struct command_result* res) {
    res->error = false;
    memset(options, 0, sizeof(PARSED_OTP_COMMAND_OPTIONS));
    options->StartRow = 0u;
    options->MaximumRows = OTP_ROW_COUNT;
    options->ShowAllRows = false; // normal dump is only non-zero data

    // NOTE: Optimizer will do its job.  Below formatting allows collapsing
    //       the code while allowing all main decision points to be at top level.

    // Parse the row count first, so if omitted, can auto-adjust row count later
    uint32_t row_count = 0u;

    bool row_count_flag = bp_cmd_get_uint32(&otpdump_def, 'c', &row_count);
    if (row_count_flag && (row_count > OTP_ROW_COUNT || row_count == 0)) {
        printf(
            "ERROR: Row count (-c) must be in range [1..%" PRId16 "] (0x0..0x%" PRIx16 ")\r\n",
            OTP_ROW_COUNT, OTP_ROW_COUNT
            );
        // ui_help_show(true, usage, count_of(usage), &options[0], count_of(options));
        res->error = true;
    } else if (row_count_flag) {
        options->MaximumRows = row_count; // bounds checked above
    }


    uint32_t start_row = 0;
    bool start_row_flag = bp_cmd_get_uint32(&otpdump_def, 'r', &start_row);
    if (start_row_flag && start_row > LAST_OTP_ROW) {
        printf(
            "ERROR: Start row (-r) must be in range [0..%" PRId16 "] (0x0..0x%" PRIx16 ")\r\n",
            LAST_OTP_ROW, LAST_OTP_ROW
            );
        res->error = true;
    } else if (res->error) {
        // no checks of row count vs. start row ...
        // already had an error earlier and thus not meaningful check
    } else if (start_row_flag && row_count_flag) {
        uint16_t maximum_start_row = OTP_ROW_COUNT - options->MaximumRows;
        // no automatic adjustment if both start and count arguments were provided.
        // instead, validate that the start + count are within bounds.
        // row_count is already individually bounds-checked 
        // start_row is already individually bounds-checked
        if (start_row > maximum_start_row) {
            printf(
                "ERROR: Start row (-r) + row count (-c) may not exceed %" PRId16 " (0x%" PRIx16 ")\r\n",
                OTP_ROW_COUNT, OTP_ROW_COUNT
                );
            res->error = true;
        }
    } else if (start_row_flag) {
        options->StartRow = start_row;
        // automatically adjust the row count
        // (only when row count not explicitly provided)
        // start_row bounds-checked above
        uint16_t maximum_row_count = OTP_ROW_COUNT - start_row;
        if (options->MaximumRows > maximum_row_count) {
            options->MaximumRows = maximum_row_count;
        }
    }

    options->ShowAllRows = bp_cmd_find_flag(&otpdump_def, 'a');
    return;
}

static void internal_triple_read_otp(OTP_READ_RESULT* out_data, uint16_t row) {
    memset(out_data, 0, sizeof(OTP_READ_RESULT));

    // Use the read aliases ... not the ROM functions ...
    // as they are read-only and thus "safer" to use without error.
    // These MUST remain as 32-bit-size, 32-bit-aligned reads.
    // OTP_DATA_BASE     // reads two consecutive rows, with ECC correction applied -- returns 0xFFFFFFFFu on failure(!)
    // OTP_DATA_RAW_BASE // reads 24 bits of raw data (HW ECC correction disabled)

    // Read the memory-mapped raw OTP data.
    do {
        out_data->as_raw.as_uint32 = ((volatile uint32_t*)(OTP_DATA_RAW_BASE))[row];

        if (out_data->as_raw.is_error != 0) {
            out_data->err_permissions = true;
        }
    } while (0);

    // Read the memory-mapped ECC corrected data.
    do {
        // divide the row number by two to determine the correct read offset
        OTP_DUAL_ROW_READ_RESULT corrected_data = {0};
        corrected_data.as_uint32 = ((volatile uint32_t*)(OTP_DATA_BASE))[row/2u];
        out_data->read_with_ecc = (row & 1) ? corrected_data.row1 : corrected_data.row0;
        // Check if there was a potential ECC error
        if (corrected_data.as_uint32 == 0xFFFFFFFFu) {
            out_data->err_possible_ecc_mismatch = true;
        }
    } while (0);

    // Maybe the ROM function will detect ECC on a single row?
    do {
        uint16_t buffer[2] = {0xAAAAu, 0xAAAAu}; // detect if bootrom overflows reading single ECC row
        otp_cmd_t cmd = {0};
        cmd.flags = (row & 0xFFFFu) | 0x00020000; // IS_ECC + Row number
        int ret = rom_func_otp_access((uint8_t*)buffer, sizeof(uint16_t), cmd);
        if (ret != BOOTROM_OK) {
            out_data->err_from_bootrom = true;
        } else if (buffer[1] != 0xAAAAu) {
            printf("Bootrom asked to read single uint16_t from OTP, but read two values (buffer overflow)");
            out_data->read_via_bootrom = buffer[0];
            out_data->err_from_bootrom = true;
        } else {
            out_data->read_via_bootrom = buffer[0];
        }
    } while (0);

    if (out_data->err_permissions || out_data->err_possible_ecc_mismatch || out_data->err_from_bootrom) {
        out_data->data_ok = false;
    } else {
        out_data->data_ok = true;
    }

    return;
}

void dump_otp(uint16_t start_row, uint16_t row_count, bool show_all_rows) {
    uint16_t remaining_rows = row_count;
    for (uint16_t current_row = start_row; remaining_rows; --remaining_rows, ++current_row) {
        OTP_READ_RESULT result = {0};
        internal_triple_read_otp(&result, current_row);
        if (!show_all_rows && result.data_ok && result.read_with_ecc == 0) {
            continue; // to next row ... skipping blank rows
        }
        print_otp_read_result(&result, current_row);
    }
    return;
}

void otpdump_handler(struct command_result* res) {
    // Show help if requested
    if (bp_cmd_help_check(&otpdump_def, res->help_flag)) {
        return;
    }

    PARSED_OTP_COMMAND_OPTIONS options;
    parse_otp_command_line(&options, res);
    if (res->error) {
        bp_cmd_help_show(&otpdump_def);
        return;
    }

    dump_otp(options.StartRow, options.MaximumRows, options.ShowAllRows);
    return;
}