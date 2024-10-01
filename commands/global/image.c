#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "ui/ui_cmdln.h"
#include "fatfs/ff.h" // File system related
#include "pirate/storage.h" // File system related

typedef struct __attribute__((packed)) {
    uint16_t bfType;      // Specifies the file type, must be 'BM'
    uint32_t bfSize;      // Specifies the size of the file in bytes
    uint16_t bfReserved1; // Reserved, must be 0
    uint16_t bfReserved2; // Reserved, must be 0
    uint32_t bfOffBits;   // Specifies the offset from the beginning of the file to the bitmap data
} BITMAPFILEHEADER;

typedef struct __attribute__((packed)) {
    uint32_t biSize;          // Specifies the number of bytes required by the structure
    int32_t  biWidth;         // Specifies the width of the image, in pixels
    int32_t  biHeight;        // Specifies the height of the image, in pixels
    uint16_t biPlanes;        // Specifies the number of color planes, must be 1
    uint16_t biBitCount;      // Specifies the number of bits per pixel
    uint32_t biCompression;   // Specifies the type of compression
    uint32_t biSizeImage;     // Specifies the size of the image data, in bytes
    int32_t  biXPelsPerMeter; // Specifies the horizontal resolution, in pixels per meter
    int32_t  biYPelsPerMeter; // Specifies the vertical resolution, in pixels per meter
    uint32_t biClrUsed;       // Specifies the number of colors used in the bitmap
    uint32_t biClrImportant;  // Specifies the number of important colors
} BITMAPINFOHEADER;

static const char* const usage[] = {
    "something to do with bitmap images",
    "Test: image example.bmp"
};

static const struct ui_help_options options[] = {
    { 0, "-h", T_HELP_FLAG },
};

void image_handler(struct command_result* res) {
    if (ui_help_show(res->help_flag, usage, count_of(usage), options, count_of(options))) {
        return;
    }

    BITMAPFILEHEADER fileHeader;
    BITMAPINFOHEADER infoHeader;

    //use cmdln_args_string_by_position to get the image file name
    char file[13];
    if (cmdln_args_string_by_position(1, sizeof(file), file)) {
        printf("Opening file %s\r\n", file);
    }else{
        ui_help_show(res->help_flag, usage, count_of(usage), options, count_of(options));
        return;
    }

    FIL file_handle; //file handle
    FRESULT result; //file system result
    /* Open file and read */
    //open the file
    result = f_open(&file_handle, file, FA_READ); //open the file for reading
    if(result!=FR_OK){ 
        printf("Error opening file %s for reading\r\n", file);
        system_config.error=true; //set the error flag
        return;   
    }
    //if the file was opened
    printf("File %s opened for reading\r\n", file);

    //read the file
    UINT bytes_read; //somewhere to store the number of bytes read
    result = f_read(&file_handle, &fileHeader, sizeof(BITMAPFILEHEADER), &bytes_read); //read the data from the file
    if(result==FR_OK){ //if the read was successful
        printf("Read %d bytes from file %s\r\n", bytes_read, file);
    }else{ //error reading file
        printf("Error reading file %s\r\n", file);
        system_config.error=true; //set the error flag
    }

    result = f_read(&file_handle, &infoHeader, sizeof(BITMAPINFOHEADER), &bytes_read); //read the data from the file
    if(result==FR_OK){ //if the read was successful
        printf("Read %d bytes from file %s\r\n", bytes_read, file);
    }else{ //error reading file
        printf("Error reading file %s\r\n", file);
        system_config.error=true; //set the error flag
    }    

    //close the file
    result = f_close(&file_handle); //close the file
    if(result!=FR_OK){ 
        printf("Error closing file %s\r\n", file);
        system_config.error=true; //set the error flag
        return;
    }
    //if the file was closed
    printf("File %s closed\r\n", file);  

    if(fileHeader.bfType != 0x4D42) {
        printf("Not a BMP file, header should shart with 'BM'!\r\n");
        return;
    }
    printf("File size: %u bytes\r\n", fileHeader.bfSize);
    printf("Image size: %d bytes\r\n", infoHeader.biSizeImage); 
    printf("Offset to image data: %d\r\n", fileHeader.bfOffBits);   
    printf("Image width: %d pixels\r\n", infoHeader.biWidth);
    printf("Image height: %d pixels\r\n", infoHeader.biHeight);
    printf("Bits per pixel: %d\r\n", infoHeader.biBitCount);
    printf("Compression: %d\r\n", infoHeader.biCompression);
}