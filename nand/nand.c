/**
 * @file		main.c
 * @author		Andrew Loebs
 * @brief		Main application
 *
 */

#include <stdint.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "opt_args.h"
#include "psu.h"

//#include "st/ll/stm32l4xx_ll_bus.h"
//#include "st/ll/stm32l4xx_ll_rcc.h"
//#include "st/ll/stm32l4xx_ll_system.h"
//#include "st/ll/stm32l4xx_ll_utils.h"
//#include "st/stm32_assert.h"

//#include "modules/led.h"
#include "nand/mem.h"
//#include "modules/shell.h"
#include "nand/spi.h"
#include "nand/sys_time.h"
//#include "modules/uart.h"

#include "fatfs/ff.h"
#include "fatfs/ffconf.h"

// defines
//#define STARTUP_LED_DURATION_MS 200

// private function prototypes
//static void clock_config(void);

// private variables
FATFS nand_fs; // file system object

// application main function
int nand_init(void)
{
    // setup clock
    //clock_config();

    // init base modules
    //led_init();
    //sys_time_init();
    //uart_init();
    //shell_init();
    nand_spi_init();
    uint32_t result=psu_set(3.3,100, false);
    // blink LED to let user know we're on
    //led_set_output(true);
    //sys_time_delay(STARTUP_LED_DURATION_MS);
    //led_set_output(false);
}  

bool nand_mount(void)
{
    // mount file system
    FRESULT res = f_mount(&nand_fs, "", 1);
    if (FR_OK == res) {
        printf("f_mount succeeded!");
    }
    else {
        printf("f_mount failed, result: %d.", res);
    }

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

}

