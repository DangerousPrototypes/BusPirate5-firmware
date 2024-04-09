#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "opt_args.h" // File system related
#include "fatfs/ff.h" // File system related
#include "pirate/storage.h" // File system related
#include "ui/ui_cmdln.h" // This file is needed for the command line parsing functions
//#include "ui/ui_prompt.h" // User prompts and menu system
//#include "ui/ui_const.h"  // Constants and strings
#include "ui/ui_help.h" // Functions to display help in a standardized way
#include "system_config.h" // Stores current Bus Pirate system configuration
#include "pirate/amux.h"   // Analog voltage measurement functions
#include "pirate/button.h" // Button press functions



#include "commands/global/disk.h"
#define MACRO_FNAME_LEN  32
static char macro_file[MACRO_FNAME_LEN];
static bool exec_macro_id(uint8_t id)
{
    char line[512];
    printf("Exec macro id: %u\r\n", id);

    disk_get_line_id(macro_file, id, line, sizeof(line));
    if (!line[0]) {
        printf("Macro not fund\r\n");
        return true;
    }

    char *m = line;
    // Skip id and separator
    // 123:MACRO
    while (*m && *m!=':')
        m++;
    if (*m != ':') {
        printf("Wrong macro line format\r\n");
        return true;
    }
    m++;

    // FIXME: injecting the bus syntax into cmd line (i.e. simulating
    // user input) is probably not the best solution... :-/
    //printf("Inject cmd\r\n");
    char c;
    while (cmdln_try_remove(&c)) ;
    while (*m && cmdln_try_add(m))
        m++;
    cmdln_try_add('\0');
    //printf("Process syntax\r\n");
    return ui_process_syntax();
}

// Must return true in case of error
#include <stdlib.h>
bool ui_process_macro_file(void)
{
    char arg[MACRO_FNAME_LEN];
    char c;
    prompt_result result = { 0 };

    cmdln_try_remove(&c); // remove '('
    cmdln_try_remove(&c); // remove ':'
    ui_parse_get_macro_file(&result, arg, sizeof(arg));

    if (result.success) {
        if (arg[0] == '\0') {
            printf("Macro files list:\r\n");
            disk_ls("", "mcr");
        }
        else {
            // Command stars with a number, exec macro id
            if (arg[0]>='0' && arg[0]<='9') {
                uint8_t id = (uint8_t)strtol(arg, NULL, 10);
                if (!id) {
                    printf("'%s' available macros:\r\n\r\n", macro_file);
                    disk_show_macro_file(macro_file); // TODO: manage errors
                }
                else {
                    return exec_macro_id(id);
                }
            }
            // Command is a string, use as macro file name
            else {
                strncpy(macro_file, arg, sizeof(macro_file));
                printf("Set current file macro to: '%s'\r\n", macro_file);
            }
        }
    }
    else {
        printf("%s\r\n", "Error parsing file name");
        return true;
    }
    return false;
}


void disk_show_macro_file(const char *location)
{
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
        if (line_len>1 && line[line_len-1]=='\n')
            line[line_len-1] = '\0';
        // Show only usage and macros
        if (line[0]=='#' && line[1]=='!' && line[2])
            printf(".- %s\r\n", line+3); // TODO: pretty print comment in VT100
        else if ((uint8_t)strtol(line, NULL, 10) > 0)
            printf("%s\r\n\r\n", line); // TODO: pretty print macro in VT100
    }
    f_close(&fil);
}

void disk_get_line_id(const char *location, uint8_t id, char *line, int max_len)
{
    FIL fil;
    FRESULT fr;

    *line = '\0';

    fr = f_open(&fil, location, FA_READ);
    if (fr != FR_OK) {
        return;
    }
    while (f_gets(line, max_len, &fil)) {
        if ((uint8_t)strtol(line, NULL, 10) == id) {
            uint8_t line_len = strlen(line);
            if (line_len>1 && line[line_len-1]=='\n')
                line[line_len-1] = '\0';
            break;
        }
    }
    f_close(&fil);
}

bool disk_ls(const char *location, const char *ext)
{
    FRESULT fr;
    DIR dir;
    FILINFO fno;
    int nfile, ndir;

    fr = f_opendir(&dir, location);                       /* Open the directory */
    if (fr != FR_OK) {
        return false;
    }

    nfile = ndir = 0;
    for(;;) {
        fr = f_readdir(&dir, &fno);                   /* Read a directory item */
        if (fr != FR_OK || fno.fname[0] == 0)
            break;  /* Error or end of dir */
        strlwr(fno.fname); //FAT16 is only UPPERCASE, make it lower to be easy on the eyes...
        if (ext) {
            int fname_len = strlen(fno.fname);
            int ext_len = strlen(ext);
            if (fname_len > ext_len+1) {
                if (memcmp(fno.fname+fname_len-ext_len, ext, ext_len)) {
                    continue;
                }
            }

        }
        if (fno.fattrib & AM_DIR) {   /* Directory */
            printf("%s   <DIR>   %s%s%s\r\n",ui_term_color_prompt(), ui_term_color_info(), fno.fname, ui_term_color_reset());
            ndir++;
        }
        else {   /* File */
            printf("%s%10u %s%s%s\r\n",
            ui_term_color_prompt(),fno.fsize, ui_term_color_info(), fno.fname, ui_term_color_reset());
            nfile++;
        }
    }
    f_closedir(&dir);
    printf("%s%d dirs, %d files%s\r\n", ui_term_color_info(), ndir, nfile, ui_term_color_reset());
    return true;
}