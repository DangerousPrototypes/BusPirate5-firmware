#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "ui/ui_cmdln.h"
#include "fatfs/ff.h"       // File system related
#include "pirate/storage.h" // File system related
#include "ui/ui_lcd.h"
#include "hardware/spi.h"

typedef struct __attribute__((packed)) {
    uint16_t bfType;      // Specifies the file type, must be 'BM'
    uint32_t bfSize;      // Specifies the size of the file in bytes
    uint16_t bfReserved1; // Reserved, must be 0
    uint16_t bfReserved2; // Reserved, must be 0
    uint32_t bfOffBits;   // Specifies the offset from the beginning of the file to the bitmap data
} BITMAPFILEHEADER;

typedef struct __attribute__((packed)) {
    uint32_t biSize;         // Specifies the number of bytes required by the structure
    int32_t biWidth;         // Specifies the width of the image, in pixels
    int32_t biHeight;        // Specifies the height of the image, in pixels
    uint16_t biPlanes;       // Specifies the number of color planes, must be 1
    uint16_t biBitCount;     // Specifies the number of bits per pixel
    uint32_t biCompression;  // Specifies the type of compression
    uint32_t biSizeImage;    // Specifies the size of the image data, in bytes
    int32_t biXPelsPerMeter; // Specifies the horizontal resolution, in pixels per meter
    int32_t biYPelsPerMeter; // Specifies the vertical resolution, in pixels per meter
    uint32_t biClrUsed;      // Specifies the number of colors used in the bitmap
    uint32_t biClrImportant; // Specifies the number of important colors
} BITMAPINFOHEADER;

typedef struct __attribute__((packed)) {
    uint32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
    uint32_t biRedMask;   // Mask identifying bits of red component
    uint32_t biGreenMask; // Mask identifying bits of green component
    uint32_t biBlueMask;  // Mask identifying bits of blue component
} BITMAPV2INFOHEADER;

typedef struct __attribute__((packed)) {
    uint32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
    uint32_t biRedMask;
    uint32_t biGreenMask;
    uint32_t biBlueMask;
    uint32_t biAlphaMask;
} BITMAPV3INFOHEADER;

typedef union {
    BITMAPINFOHEADER infoHeader;
    BITMAPV2INFOHEADER v2InfoHeader;
    BITMAPV3INFOHEADER v3InfoHeader;
} BITMAPINFOHEADER_UNION;

void process_bitmap(FIL* file_handle, bool draw) {
    BITMAPFILEHEADER fileHeader;
    UINT bytes_read;
    FRESULT result;

    result = f_read(file_handle, &fileHeader, sizeof(BITMAPFILEHEADER), &bytes_read);
    if (result != FR_OK || bytes_read != sizeof(BITMAPFILEHEADER)) {
        printf("Failed to read file header!\r\n");
        return;
    }

    if (fileHeader.bfType != 0x4D42) { // 'BM' in little-endian
        printf("Not a valid BMP file, header should start with 'BM'!\r\n");
        return;
    }

    uint32_t infoHeaderSize;
    result = f_read(file_handle, &infoHeaderSize, sizeof(uint32_t), &bytes_read);
    if (result != FR_OK || bytes_read != sizeof(uint32_t)) {
        printf("Failed to read info header size!\r\n");
        return;
    }
    f_lseek(file_handle, f_tell(file_handle) - sizeof(uint32_t)); // Move back to the start of the info header

    BITMAPINFOHEADER_UNION infoHeaderUnion;

    if (infoHeaderSize == 40) {
        result = f_read(file_handle, &infoHeaderUnion.infoHeader, sizeof(BITMAPINFOHEADER), &bytes_read);
        if (result != FR_OK || bytes_read != sizeof(BITMAPINFOHEADER)) {
            printf("Failed to read BITMAPINFOHEADER!\r\n");
            return;
        }
        printf("Found BITMAPINFOHEADER (V1)\r\n");
        // Process BITMAPINFOHEADER
    } else if (infoHeaderSize == 52) {
        result = f_read(file_handle, &infoHeaderUnion.v2InfoHeader, sizeof(BITMAPV2INFOHEADER), &bytes_read);
        if (result != FR_OK || bytes_read != sizeof(BITMAPV2INFOHEADER)) {
            printf("Failed to read BITMAPV2INFOHEADER!\r\n");
            return;
        }
        printf("Found BITMAPV2INFOHEADER (V2)\r\n");
        // Process BITMAPV2INFOHEADER
    } else if (infoHeaderSize == 56) {
        result = f_read(file_handle, &infoHeaderUnion.v3InfoHeader, sizeof(BITMAPV3INFOHEADER), &bytes_read);
        if (result != FR_OK || bytes_read != sizeof(BITMAPV3INFOHEADER)) {
            printf("Failed to read BITMAPV3INFOHEADER!\r\n");
            return;
        }
        printf("Found BITMAPV3INFOHEADER (V3)\r\n");
        // Process BITMAPV3INFOHEADER
    } else {
        printf("Unsupported info header size: %u bytes!\r\n", infoHeaderSize);
        return;
    }

    printf("File size: %u bytes\r\n", fileHeader.bfSize);
    printf("Offset to image data: %d\r\n", fileHeader.bfOffBits);
    printf("Image width: %d pixels\r\n", infoHeaderUnion.infoHeader.biWidth);
    printf("Image height: %d pixels\r\n", infoHeaderUnion.infoHeader.biHeight);
    printf("Bits per pixel: %d\r\n", infoHeaderUnion.infoHeader.biBitCount);
    printf("Compression: %d\r\n", infoHeaderUnion.infoHeader.biCompression);

    if (!draw) {
        return;
    }

    printf("Drawing image on display...\r\n");

    // check bits per pixel
    if (infoHeaderUnion.infoHeader.biBitCount != 24 && infoHeaderUnion.infoHeader.biBitCount != 16) {
        printf("Only 16-bit (565) and 24-bit bitmaps are supported!\r\n");
        return;
    }

    // check width and height
    //  LCD size
    if (infoHeaderUnion.infoHeader.biWidth != BP_LCD_HEIGHT || infoHeaderUnion.infoHeader.biHeight != BP_LCD_WIDTH) {
        printf("Image (%dx%d) does not fit the display (%dx%d)\r\n",
               infoHeaderUnion.infoHeader.biWidth,
               infoHeaderUnion.infoHeader.biHeight,
               BP_LCD_WIDTH,
               BP_LCD_HEIGHT);
        return;
    }

    // now read image data and write to display
    f_lseek(file_handle, fileHeader.bfOffBits); // Move to the start of the image data
    // calculate length of image data by width, height, and bits per pixel
    uint32_t image_data_length = (infoHeaderUnion.infoHeader.biWidth * infoHeaderUnion.infoHeader.biHeight *
                                  infoHeaderUnion.infoHeader.biBitCount) /
                                 8;
    uint8_t image_data[256];        // buffer for image data
    uint8_t image_data_sorted[256]; // buffer for sorted image data
    uint32_t remaining_data = image_data_length;
    lcd_set_bounding_box(0, 240, 0, 320);
    while (remaining_data > 0) {
        // align to pixel boundary
        uint32_t chunk_size = (remaining_data > sizeof(image_data))
                                  ? (infoHeaderUnion.infoHeader.biBitCount == 24 ? ((sizeof(image_data) / 3) * 3)
                                                                                 : ((sizeof(image_data) / 2) * 2))
                                  : remaining_data;
        result = f_read(file_handle, image_data, chunk_size, &bytes_read);
        if (result != FR_OK || bytes_read != chunk_size) {
            printf("Failed to read image data!\r\n");
            return;
        }
        // turn 24-bit image data into 16-bit
        uint32_t sort_cnt = 0;
        if (infoHeaderUnion.infoHeader.biBitCount == 24) {
            for (int i = 0; i < chunk_size; i += 3) {
                uint16_t pixel =
                    (image_data[i] >> 3) | ((image_data[i + 1] >> 2) << 5) | ((image_data[i + 2] >> 3) << 11);
                image_data_sorted[sort_cnt] = pixel >> 8;
                image_data_sorted[sort_cnt + 1] = pixel & 0xFF;
                sort_cnt += 2;
            }
        } else {
            // 16-bit image data
            for (int i = 0; i < chunk_size; i += 2) {
                image_data_sorted[i] = image_data[i + 1];
                image_data_sorted[i + 1] = image_data[i];
            }
            sort_cnt = chunk_size;
        }

        // send pixel data to display
        remaining_data -= chunk_size;
        spi_busy_wait(true);
        gpio_put(DISPLAY_DP, 1);
        gpio_put(DISPLAY_CS, 0);
        spi_write_blocking(BP_SPI_PORT, image_data_sorted, sort_cnt);
        gpio_put(DISPLAY_CS, 1);
        spi_busy_wait(false);
    }
}

static const char* const usage[] = {
    "Read BMP info and display image file on LCD",
    "Usage: image <file> [-d] [-h]",
    "Read info: image example.bmp",
    "Draw on display: image example.bmp -d",
    "Read formats: BITMAPINFOHEADER V1 (40Bytes), V2 (52B), V3 (54B)",
    "Draw formats: 16-bit (565) and 24-bit bitmaps, 240x320 pixels",
};

static const struct ui_help_options options[] = {
    { 0, "-h", T_HELP_FLAG },
};

void image_handler(struct command_result* res) {
    if (ui_help_show(res->help_flag, usage, count_of(usage), options, count_of(options))) {
        return;
    }

    BITMAPFILEHEADER fileHeader;

    // use cmdln_args_string_by_position to get the image file name
    char file[13];
    if (cmdln_args_string_by_position(1, sizeof(file), file)) {
        printf("Opening file %s\r\n", file);
    } else {
        ui_help_show(res->help_flag, usage, count_of(usage), options, count_of(options));
        return;
    }

    // check for the -d flag
    bool draw = cmdln_args_find_flag('d');

    FIL file_handle; // file handle
    FRESULT result;  // file system result
    /* Open file and read */
    // open the file
    result = f_open(&file_handle, file, FA_READ); // open the file for reading
    if (result != FR_OK) {
        printf("Error opening file %s for reading\r\n", file);
        system_config.error = true; // set the error flag
        return;
    }
    // if the file was opened
    printf("File %s opened for reading\r\n", file);

    process_bitmap(&file_handle, draw);

    // close the file
    result = f_close(&file_handle); // close the file
    if (result != FR_OK) {
        printf("Error closing file %s\r\n", file);
        system_config.error = true; // set the error flag
        return;
    }
    // if the file was closed
    printf("File %s closed\r\n", file);
}