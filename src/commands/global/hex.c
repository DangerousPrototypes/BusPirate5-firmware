/**
 * @file hex.c
 * @author
 * @brief moved hex display to its own file
 * @version 0.1
 * @date 2024-05-11
 *
 * @copyright Copyright (c) 2024
 * Modified by Lior Shalmay Copyright (c) 2024
 * Modified by Ian Lesnet Copyright (c) 2025
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "fatfs/ff.h"
#include "pirate/file.h"
#include "ui/ui_prompt.h"
#include "ui/ui_parse.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "pirate/storage.h"
#include "pirate/mem.h"
#include "ui/ui_cmdln.h"
//#include "pirate/storage.h"
#include "ui/ui_hex.h"


//#define DEF_ROW_SIZE 16
//#define PRINTABLE(_c) (_c > 0x1f && _c < 0x7f ? _c : '.')

static const char* const hex_usage[] = { "hex <file> [-s <start address>] [-b <bytes>] [-p (disable pager)] [-a (disable address column)]",
                                         "Print file contents in HEX:%s hex example.bin",
                                         "Print 32 bytes starting at address 0x50:%s hex example.bin -s 0x50 -b 32",
                                         "Disable address and ASCII columns:%s hex example.bin -q",
                                         "press 'x' to quit pager" };
static const struct ui_help_options hex_options[] = { { 1, "", T_HELP_DISK_HEX }, // section heading
                                                      { 0, "<file>", T_HELP_DISK_HEX_FILE },
                                                        { 0, "-s", UI_HEX_HELP_START }, // start address for dump
                                                        { 0, "-b", UI_HEX_HELP_BYTES }, // bytes to dump
                                                        { 0, "-q", UI_HEX_HELP_QUIET}, // quiet mode, disable address and ASCII columns
                                                        { 0, "-c", T_HELP_DISK_HEX_PAGER_OFF }};

void hex_handler(struct command_result* res) {
    // check help
    if (ui_help_show(res->help_flag, hex_usage, count_of(hex_usage), &hex_options[0], count_of(hex_options))) {
        return;
    }

    FIL fil;    /* File object needed for each open file */
    FRESULT fr; /* FatFs return code */
    char location[32];

    //file name
    if(!cmdln_args_string_by_position(1, sizeof(location), location)){
        //printf("Missing <file name>\r\n\r\n");
        ui_help_show(true, hex_usage, count_of(hex_usage), &hex_options[0], count_of(hex_options));
        res->error = true;
        return;
    }
    
    if(file_open(&fil, location, FA_READ)) {
        // file_open will print the error message
        res->error = true;
        return;
    }
    struct hex_config_t hex_config;
    hex_config.max_size_bytes= file_size(&fil); // maximum size of the device in bytes
    if(ui_hex_get_args_config(&hex_config)) goto hex_cleanup; // parse the command line arguments
    ui_hex_align_config(&hex_config);
    
    //advance to the start address
    f_lseek(&fil, hex_config._aligned_start);

    ui_hex_header_config(&hex_config);
    uint32_t current_address = hex_config._aligned_start; //current address in the file
    UINT bytes_read = 0;
    uint8_t buf[16]; // buffer to read the file
    while (true) {
        fr=f_read(&fil, &buf, 16, &bytes_read);
        if(!bytes_read || fr != FR_OK) {
            goto hex_cleanup; // no more data to read
        }

        if(ui_hex_row_config(&hex_config, current_address, buf, bytes_read)){
            //pager exit or other error
            goto hex_cleanup; // exit the hex dump
        }

        current_address += 16; // advance the current address by 16 bytes
        if(current_address >= (hex_config._aligned_end+1)) {
            goto hex_cleanup; // we reached the end of the range
        }
    }

hex_cleanup:
    f_close(&fil);
    printf("\r\n");
}

#if 0

// Show flags
#define HEX_NONE 0x00
#define HEX_ADDR 0x01
#define HEX_ASCII 0x02

// shown_off:  starting shown offset, used for display only
// page_lines: number of lines per page. 0 means no paging
// row_size:   row size in bytes
// flags:      show flags (address and ascii only for now)
static uint32_t hex_dump(
    FIL* fil, uint32_t shown_off, const uint16_t page_lines, const uint16_t row_size, const uint8_t flags) {
    const bool flag_addr = flags & HEX_ADDR;
    const bool flag_ascii = flags & HEX_ASCII;
    const uint32_t page_size = page_lines ? page_lines * row_size : (uint32_t)-1;
    char buf[512];
    uint32_t buf_off = 0;
    uint32_t line_start_off = 0;
    uint32_t tot_read = 0;
    bool print_addr = false;

    if (flag_addr) {
        print_addr = true;
    }
    UINT bytes_read = 0;
    while (true) {
        f_read(fil, &buf, MIN(sizeof(buf), page_size), &bytes_read);
        tot_read += bytes_read;
        if (!bytes_read) {
            // Flush last line
            if (flag_ascii) {
                uint16_t rem = buf_off % row_size;
                if (rem) {
                    for (uint16_t j = 0; j < row_size - rem; j++) {
                        printf("   ");
                    }
                    printf(" |");
                    for (uint16_t j = 0; j < rem; j++) {
                        printf("%c", PRINTABLE(buf[line_start_off + j]));
                    }
                    printf("|");
                }
            }
            break;
        }
        for (UINT i = 0; i < bytes_read; i++) {
            if (print_addr) {
                print_addr = false;
                printf("%08X  ", shown_off);
            }
            printf("%02x ", buf[i]);
            buf_off++;
            shown_off++;
            if (!(buf_off % row_size)) {
                if (flag_ascii) {
                    printf(" |");
                    for (uint16_t j = 0; j < row_size; j++) {
                        printf("%c", PRINTABLE(buf[line_start_off + j]));
                    }
                    printf("|");
                }
                printf("\r\n");
                if (flag_addr) {
                    print_addr = true;
                }
                line_start_off = buf_off;
            }
        }
        if (tot_read >= page_size) {
            break;
        }
    }
    return bytes_read;
}
    #endif