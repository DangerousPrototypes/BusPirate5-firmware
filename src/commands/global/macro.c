#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "command_struct.h"       // File system related
#include "fatfs/ff.h"       // File system related
#include "pirate/storage.h" // File system related
#include "ui/ui_cmdln.h"    // This file is needed for the command line parsing functions
// #include "ui/ui_prompt.h" // User prompts and menu system
// #include "ui/ui_const.h"  // Constants and strings
#include "ui/ui_help.h"    // Functions to display help in a standardized way
#include "system_config.h" // Stores current Bus Pirate system configuration
#include "pirate/amux.h"   // Analog voltage measurement functions
#include "pirate/button.h" // Button press functions
#include "ui/ui_term.h"    // Terminal functions
#include "ui/ui_process.h"

static bool exec_macro_id(const char* id);
void disk_show_macro_file(const char* location);
void disk_get_line_id(const char* location, const char* id, char* line, int max_len);

static const char* const usage[] = {
    "macro <#>\r\n\t[-f <file>] [-a] [-l] [-h(elp)]",
    "Load macros:%s macro -f <file>",
    "List macros:%s macro -l",
    "Run macro 1:%s macro 1",
    "Macro system help:%s macro -h",
    "",
    "Macro files:",
    " Macros are stored in text files",
    " Lines starting with '#' are comments",
    " Lines starting with '#!' are macro usage instructions",
    " Every macro line includes an id (>0), a separator ':', and commands",
    "Example:",
    " # This is my example macro file",
    " #! Read 5 bytes from an I2C EEPROM",
    " 1:[0xa0 0][0xa1 r:5]",
};

static const struct ui_help_options options[] = { 0 };

#define MACRO_FNAME_LEN 13
#define ID_MAX_LEN 32
static char macro_file[MACRO_FNAME_LEN];

void macro_handler(struct command_result* res) {
    char value[ID_MAX_LEN];

    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
        return;
    }

    // list of mcr files
    #if 0
    if (cmdln_args_find_flag('a' | 0x20)) {
        printf("Available macro files:\r\n");
        storage_ls("", "mcr", LS_FILES /*| LS_SIZE*/); // disk ls should be integrated with existing list function???
        return;
    }
    #endif

    // file to load?
    command_var_t arg;
    bool file_flag = cmdln_args_find_flag_string('f' | 0x20, &arg, sizeof(macro_file), macro_file);
    if (file_flag) {
        // does file exist?
        FIL fil;    /* File object needed for each open file */
        FRESULT fr; /* FatFs return code */
        fr = f_open(&fil, macro_file, FA_READ);
        if (fr != FR_OK) {
            printf("'%s' not found\r\n", macro_file);
            res->error = true;
            return;
        }
        f_close(&fil);
        printf("Set macro file: '%s'\r\n", macro_file);
        return;
    }

    // list macros
    bool list_flag = cmdln_args_find_flag('l' | 0x20);
    if (list_flag) {
        // is a macro file loaded?
        // list macros
        printf("'%s' available macros:\r\n\r\n", macro_file);
        disk_show_macro_file(macro_file); // TODO: manage errors
        return;
    }

    // run macro

    if (cmdln_args_string_by_position(1, ID_MAX_LEN, value)) {
        // is a macro file loaded?
        // does this macro exist?
        exec_macro_id(value); // has return value to flag error
        return;
    }

    ui_help_show(true, usage, count_of(usage), &options[0], count_of(options));
}

static bool exec_macro_id(const char* id) {
    char line[512];
    printf("Exec macro id: %s\r\n", id);

    disk_get_line_id(macro_file, id, line, sizeof(line));
    if (!line[0]) {
        printf("Macro not found\r\n");
        return true;
    }

    char* m = line;
    // Skip id and separator
    // 123:MACRO
    while (*m && *m != ':') {
        m++;
    }
    if (*m != ':') {
        printf("Wrong macro line format\r\n");
        return true;
    }
    m++;
    
    cmdln_next_buf_pos();
    while (*m && ui_term_cmdln_char_insert(m)) {
        m++;
    }
    cmdln_try_add(0x00);
    return ui_process_commands(); // I think we're going to run into issues with the ui_process loop if &&||; are used....
}

void disk_show_macro_file(const char* location) {
    FIL fil;
    FRESULT fr;
    char line[512] = "\0";

    fr = f_open(&fil, location, FA_READ);
    if (fr != FR_OK) {
        return;
    }
    while (f_gets(line, sizeof(line), &fil)) {
        uint8_t line_len = strlen(line);
        // Remove trailing '\n' if present
        if (line_len > 1 && line[line_len - 1] == '\n') {
            line[line_len - 1] = '\0';
        }
        // Show only usage and macros
        //printf("%s\r\n", line);
        if (line[0] == '#' && line[1] == '!' && line[2]) {
            printf("%s%s%s\r\n", ui_term_color_info(), line, ui_term_color_reset());
        } else if (strchr(line, ':')) {
            printf("%s%s%s\r\n", ui_term_color_prompt(), line, ui_term_color_reset());
        }
    }
    f_close(&fil);
}

void disk_get_line_id(const char* location, const char* id, char* line, int max_len) {
    FIL fil;
    FRESULT fr;

    *line = '\0';

    fr = f_open(&fil, location, FA_READ);
    if (fr != FR_OK) {
        return;
    }
    while (f_gets(line, max_len, &fil)) {
        uint8_t line_len = strlen(line);
        uint8_t cmp_len = MIN(strlen(id), line_len);
        if (!strncmp(line, id, cmp_len)) {
            if (line_len > 1 && line[line_len - 1] == '\n') {
                line[line_len - 1] = '\0';
            }
            break;
        }
    }
    f_close(&fil);
}
