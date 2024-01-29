/**
 * @file		main.c
 * @author		Andrew Loebs
 * @brief		Main application
 *
 */

#include <stdint.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "mem.h"
#include "nand/spi.h"
#include "nand/sys_time.h"
#include "fatfs/ff.h"
#include "fatfs/ffconf.h"

// private function prototypes
//static void clock_config(void);

// private variables
FATFS nand_fs; // file system object

// application main function
int nand_init(void)
{
    // setup clock
    //clock_config();
    sys_time_init();
}  

bool nand_mount(void)
{
    
    // mount file system
    FRESULT res = f_mount(&nand_fs, "", 1);
    if (FR_OK == res) {
        //printf("f_mount succeeded!");
    }
    else {
        //printf("f_mount failed, result: %d.", res);
    }

    #if 0
    // if filesystem mount failed due to no filesystem, attempt to make it
    if (FR_NO_FILESYSTEM == res) {
        printf("No filesystem present. Attempting to make file system..");
        uint8_t *work_buffer = mem_alloc(FF_MAX_SS);
        if (!work_buffer) {
            printf("Unable to allocate f_mkfs work buffer. File system not created.");
        }
        else {
            // make the file system
            MKFS_PARM opt;
            opt.fmt = FM_FAT32;
            //opt.n_fat = 1;
            //opt.align = 0;
            //opt.au_size = 0;
            //opt.n_root = 0;

            res = f_mkfs("", &opt, work_buffer, FF_MAX_SS);
            if (FR_OK != res) {
                printf("f_mkfs failed, result: %d.", res); // fs make failure
            }
            else {
                printf("f_mkfs succeeded!"); // fs make success
                // retry mount
                res = f_mount(&nand_fs, "", 1);
                if (FR_OK == res) {
                    printf("f_mount succeeded!");
                }
                else {
                    printf("f_mount failed, result: %d.", res);
                }
            }

            mem_free(work_buffer);
        }
    }
    #endif
}

