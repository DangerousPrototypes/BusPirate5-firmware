#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"

//align a given start address to 16 bytes, and calculate the end address also aligned to 16 bytes
void ui_hex_align(uint32_t start_address, uint32_t read_bytes, uint32_t max_bytes, uint32_t *aligned_start, uint32_t *aligned_end, uint32_t *total_read_bytes) {
    // user specified start address: align to 16 bytes, then adjust the number of bytes to read
    (*aligned_start) = start_address & 0xFFFFFFF0; // align to 16 bytes
    (*aligned_end) = (((start_address) + (read_bytes-1))|0xF); // align to 16 bytes, remember -1 to account for the start byte (0)
    (*total_read_bytes) = ((*aligned_end)-(*aligned_start))+1;
    //printf("Total read before %04X\r\n", total_read_bytes);
    if(max_bytes > 0 && (*aligned_end) > max_bytes) {
        (*aligned_end) = (max_bytes-1); // limit to device size, adjust for 0 based index
        (*total_read_bytes) = ((*aligned_end) - (*aligned_start))+1; // recalculate the total read bytes
    }
    //printf("Total read after %04X\r\n", total_read_bytes);
}

void ui_hex_header(uint32_t aligned_start, uint32_t aligned_end, uint32_t total_read_bytes) {
    printf("Start address: 0x%08X, end address: 0x%08X, total bytes: %d\r\n\r\n", aligned_start, aligned_end, total_read_bytes);
    printf("          00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\r\n");
    printf("---------------------------------------------------------\r\n");
}

void ui_hex_row(uint32_t address, uint8_t *buf, uint32_t buf_size) {
    if(buf_size > 16) {
        printf("Error: Buffer size must be <16 bytes\r\n");
        return; // error
    }
    
    // print the address
    printf("%08X: ", address);
    
    // print the data in hex
    for(uint32_t j = 0; j < 16; j++) {
        if(j < buf_size) {
            printf("%02X ", (uint8_t)buf[j]);
        } else {
            printf("   "); // print spaces for empty bytes
        }
    }
    printf(" ");
    
    // print the ASCII representation of the data
    printf("|"); 
    for(uint32_t j = 0; j < 16; j++) {
        if(j < buf_size) {
            char c = (char)buf[j];
            if(c >= 32 && c <= 126) { // printable ASCII range
                printf("%c", c);
            } else {
                printf("."); // non-printable characters as dot
            }
        } else {
            printf(" "); // print space for empty bytes
        }
    }
    printf("|\r\n");    
}
