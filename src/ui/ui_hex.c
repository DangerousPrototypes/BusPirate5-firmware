#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "ui/ui_term.h"
#include "ui/ui_cmdln.h"
#include "ui/ui_hex.h"
#include "system_config.h"

//a function to initialize the hex config structure
// this is primarily useful for initializing config 
// if you DO NOT use command line arguments ui_hex_get_args_config();
void ui_hex_init_config(struct hex_config_t *config) {
    config->start_address = 0; // default start address
    config->requested_bytes = 0; // default to 0, will be set by user
    config->max_size_bytes = 0; // default to 0, will be set by user
    config->_aligned_start = 0; // aligned start address
    config->_aligned_end = 0; // aligned end address
    config->_total_read_bytes = 0; // total number of bytes read
    config->quiet = false; // quiet mode disabled by default
    config->rows_terminal= system_config.terminal_ansi_rows; // number of rows in the terminal
    config->rows_printed = 0; // number of rows printed
}

bool ui_hex_get_args_config(struct hex_config_t *config){

    config->header_verbose = false; // do not show the header by default

    command_var_t arg;
    // start address
    if (cmdln_args_find_flag_uint32('s' | 0x20, &arg, &config->start_address)) {
        if (config->start_address >= config->max_size_bytes) {
            printf("Start address out of range: %d\r\n", config->start_address);
            return true; // error
        }
        //config->header_verbose = true; // show the header with start and end address
    } else {
        config->start_address = 0; // default to 0
    }

    // end address: user provides number of bytes to read/write, we calculate the end address
    if (cmdln_args_find_flag_uint32('b' | 0x20, &arg, &config->requested_bytes)) {
        if(config->requested_bytes == 0) {
           config->requested_bytes = 1;
        }
        //config->header_verbose = true; // show the header with start and end address
    }else{
       config->requested_bytes=config->max_size_bytes;
    }
    // disable address column and ascii dump
    config->quiet = cmdln_args_find_flag('q' | 0x20);
    config->pager_off = cmdln_args_find_flag('c' | 0x20); // disable paging
    config->rows_terminal = system_config.terminal_ansi_rows; // number of rows in the terminal
    config->rows_printed = 0; // reset the number of rows printed
    return false; // no error
}

void ui_hex_align_config(struct hex_config_t *config){
        // user specified start address: align to 16 bytes, then adjust the number of bytes to read
    (config->_aligned_start) = config->start_address & 0xFFFFFFF0; // align to 16 bytes
    (config->_aligned_end) = (((config->start_address) + (config->requested_bytes-1))|0xF); // align to 16 bytes, remember -1 to account for the start byte (0)
    (config->_total_read_bytes) = ((config->_aligned_end)-(config->_aligned_start))+1;
    //printf("Total read before %04X\r\n", total_read_bytes);
    if(config->max_size_bytes > 0 && (config->_aligned_end) > config->max_size_bytes) {
        (config->_aligned_end) = (config->max_size_bytes-1); // limit to device size, adjust for 0 based index
        (config->_total_read_bytes) = ((config->_aligned_end) - (config->_aligned_start))+1; // recalculate the total read bytes
    }
    //printf("Total read after %04X\r\n", total_read_bytes);
}

void ui_hex_header_config(struct hex_config_t *config){
    if(config->header_verbose){
        printf("Start address: 0x%08X, end address: 0x%08X, total bytes: %d\r\n", config->_aligned_start, config->_aligned_end, config->_total_read_bytes);
    }

    printf("\r\n");

    if(!config->quiet) {
        printf("          ");       
    }
    printf("00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F");
    if(!config->quiet) {
        printf("  0123456789ABCDEF");
    }
    printf("\r\n");

    if(!config->quiet) {
        printf("----------");
    }
    printf("-----------------------------------------------");
    if(!config->quiet) {
        printf("-|----------------|"); // reset color
    }
    printf("\r\n");
}

void ui_hex_row_color(bool *is_grey, bool *is_nonzero, uint32_t address, uint8_t byte, struct hex_config_t *config, char *color_func) {
    if((address) < config->start_address || (address) > config->start_address + config->requested_bytes -1) { //leading bytes before start address, make grey
        if(!(*is_grey)) {
            (*is_grey) = true; // start printing grey color for addresses before start address
            printf("%s", ui_term_color_grey()); // print grey color
        }
    }else{
        if((byte == 0x00 || byte == 0xFF)) {
            if((*is_grey) || (*is_nonzero)) {
                (*is_nonzero) = false; // stop highlighting the address if we have a zero byte
                printf("%s", ui_term_color_reset()); // reset color
            }                              
        }else{
            if((*is_grey) || !(*is_nonzero)) {
                (*is_nonzero) = true; // highlight the address if we have a non-zero byte
                printf("%s", color_func); // start highlighting
            }
        }
        (*is_grey)=false;
    }
}

void ui_hex_ascii_color(bool *is_grey, bool *is_nonzero, uint32_t address, uint8_t byte, struct hex_config_t *config, char *color_func) {
    if((address) < config->start_address || (address) > config->start_address + config->requested_bytes -1) { //leading bytes before start address, make grey
        if(!(*is_grey)) {
            (*is_grey) = true; // start printing grey color for addresses before start address
            printf("%s", ui_term_color_grey()); // print grey color
        }
    }else{
        if(byte >= 32 && byte <= 126) { // printable ASCII range
            if((*is_grey) || !(*is_nonzero)) {
                (*is_nonzero) = true; // highlight the character if it is printable
                printf("%s", color_func); // start highlighting
            }
        } else {
            if((*is_grey) || (*is_nonzero)) {
                (*is_nonzero) = false; // stop highlighting the address if we have a non-printable character
                printf("%s", ui_term_color_reset()); // reset color
            }
        }
        (*is_grey)=false;
    }
}

bool ui_hex_row_config(struct hex_config_t *config, uint32_t address, uint8_t *buf, uint32_t buf_size){
        if(buf_size > 16) {
        printf("Error: Buffer size must be <16 bytes\r\n");
        return true; // error
    }
    // print the address
    if(!config->quiet){
        printf("%08X: ", address);
    }
    
    bool is_nonzero = false; // flag to highlight the address
    bool is_grey = false; // flag to print grey color for addresses before start address
    // print the data in hex
    for(uint32_t j = 0; j < 16; j++) {
        if(j < buf_size) {
            ui_hex_row_color(&is_grey, &is_nonzero, address+j, buf[j], config, ui_term_color_num_float());
            printf("%02X ", (uint8_t)buf[j]);  
            
        } else {
            printf("   "); // print spaces for empty bytes
        }
    }
    
    if(!config->quiet){
        // print the ASCII representation of the data
        printf("%s|", ui_term_color_reset());
        is_nonzero = false; // reset highlight flag for ASCII representation 
        is_grey = false; // reset grey flag for ASCII representation
        for(uint32_t j = 0; j < 16; j++) {
            if(j < buf_size) {
                ui_hex_ascii_color(&is_grey, &is_nonzero, address+j, buf[j], config, ui_term_color_info());
                if(buf[j] >= 32 && buf[j] <= 126) { // printable ASCII range
                    printf("%c", buf[j]); // print the character in color
                } else {
                    printf("."); // non-printable characters as dot
                }
            } else {
                printf(" "); // print space for empty bytes
            }
        }
        printf("%s|\r\n", ui_term_color_reset()); // reset color after ASCII representation
    }else{
        printf("%s\r\n", ui_term_color_reset()); // reset color after hex data
    }

    if(!config->pager_off){
        config->rows_printed++; // increment the row counter
        if(config->rows_printed >= config->rows_terminal-3) {
            // if we reached the end of the page, wait for user input
            // pager is on, wait for user input
            char recv_char = ui_term_cmdln_wait_char('\0');
            switch (recv_char) {
                // give the user the ability to bail out
                case 'x':
                    return true; // exit the function
                    break;
                // anything else just keep going
                default:
                    ui_hex_header_config(config);
                    break;
            }
            config->rows_printed = 0; // reset the row counter
        }
    }

    return false; // no error
}