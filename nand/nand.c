/**
 * @file		main.c
 * @author		Andrew Loebs
 * @brief		Main application
 *
 */

#include <stdint.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "pirate/mem.h"
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

