#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "ui/ui_term.h"
#include "ui/ui_cmdln.h"
#include "fatfs/ff.h"       // File system related

//file anme aparsing
bool file_get_args(char *file, size_t size_of_file) {
    command_var_t arg;
    bool file_flag = cmdln_args_find_flag_string('f'|0x20, &arg, size_of_file, file);
    //detect flag with no argument, report error
    if(!file_flag) {
        printf("Missing file name: -f <file name>\r\n");
        return true; // return true for error
    }
    return false; // return false for success
}


bool file_close(FIL *file_handle) {
    FRESULT result = f_close(file_handle); // close the file
    if (result != FR_OK) {
        //storage_file_error(fr);
        printf("\r\nError closing file\r\n");
        return true; // return true if there was an error
    }
    return false; // return false if the file was closed successfully
}

// true for error, false for success
bool file_open(FIL *file_handle, const char *file, uint8_t file_status) {
    FRESULT result;
    result = f_open(file_handle, file, file_status); 
    if (result != FR_OK){
        //storage_file_error(fr);
        printf("\r\nError opening file %s\r\n", file);
        file_close(file_handle); // close the file if there was an error
        return true;
    }
    return false; // return false if the file was opened successfully
}

uint32_t file_size(FIL *file_handle) {
    return f_size(file_handle); // get the file size
}

bool file_size_check(FIL *file_handle, uint32_t expected_size) {
    if(f_size(file_handle) != expected_size) { // get the file size
        //storage_file_error(fr);
        printf("\r\nError: File must be exactly %d bytes long\r\n", expected_size);
        file_close(file_handle); // close the file
        return true; // return true if the file size is not as expected
    }
    return false; // return false if the file size is as expected
}

bool file_read(FIL *file_handle, uint8_t *buffer, uint32_t size, uint32_t *bytes_read) {
    //UINT bytes_read;
    FRESULT result = f_read(file_handle, buffer, size, (UINT*)bytes_read); // read the file
    if (result != FR_OK) { // check if the read was successful
        //storage_file_error(fr);
        printf("\r\nError reading file\r\n");
        file_close(file_handle); // close the file if there was an error
        return true; // return true if there was an error
    }
    return false; // return false if the read was successful
}

bool file_write(FIL *file_handle, uint8_t *buffer, uint32_t size) {
    UINT bytes_written;
    FRESULT result = f_write(file_handle, buffer, size, &bytes_written); // write the file
    if (result != FR_OK || bytes_written != size) { // check if the write was successful
        //storage_file_error(fr);
        printf("\r\nError writing to file\r\n");
        file_close(file_handle); // close the file if there was an error
        return true; // return true if there was an error
    }
    return false; // return false if the write was successful
}