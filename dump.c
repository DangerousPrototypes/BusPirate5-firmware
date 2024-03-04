#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pirate.h"
#include "opt_args.h"
#include "fatfs/ff.h"
#include "storage.h"
#include "bytecode.h"
#include "mode/hwspi.h"
#include "../lib/sfud/inc/sfud.h"
#include "../lib/sfud/inc/sfud_def.h"
#include "mode/spiflash.h"
#include "ui/ui_cmdln.h"
#include "ui/ui_prompt.h"
#include "ui/ui_const.h"
#include "ui/ui_args.h"

void flash(opt_args (*args), struct command_result *res)
{
    arg_var_t arg;
    char file_name[13];
    uint32_t value;

    enum arg_types {
        ARG_NONE=0,
        ARG_STRING,
        ARG_UINT32T
    };

    typedef struct arg_item_struct
    {
        char flag;
        uint8_t type;
        bool arg_required;
        bool val_required; //if the arg is present, is the value mandatory? TODO: think through
        uint32_t def_val;
    }arg_item_t;

    const arg_item_t options[]={
        {'e', ARG_NONE, false, false, 0}, //erase
        {'v', ARG_NONE, false, false, 0}, //verify
    };

    //erase chip?
    bool erase = ui_args_find_flag_novalue('e'|0x20, &arg);
    //verify chip?
    bool verify = ui_args_find_flag_novalue('v'|0x20, &arg);
    //read?
    bool read = ui_args_find_flag_string('r'|0x20, &arg, sizeof(file_name), file_name);
    //to file?
    if(read && !arg.has_value)
    {
        printf("Missing dump file name\r\n");
        return;
    }
    //write
    //from file?
    bool write = ui_args_find_flag_string('w'|0x20, &arg, sizeof(file_name), file_name);
    if(write && !arg.has_value)
    {
        printf("Missing load file name\r\n");
        return;
    }    

    //test read/write/verify?
    //start and end rage? bytes to write/dump???

    //only 1 (read) or (write + erase) allowed?
    if((read && write) || (read && erase))
    {
        printf("Read cannot be combined with erase or write\r\n");
        return;
    }

    sfud_flash flash_info= {.name = "SPI_FLASH", .spi.name = "SPI1"};
    uint8_t data[256];
    uint32_t start_address=0;

    if(!spiflash_init(&flash_info)) return;
    uint32_t end_address=flash_info.sfdp.capacity; //TODO: handle capacity in the flashtable when we pass from command line

    if(erase)
    {
        if(!spiflash_erase(&flash_info)) return;
        if(verify)
        {
            if(!spiflash_erase_verify(start_address, end_address, sizeof(data), data, &flash_info)) return;
        }
    }

    if(read)
    {
        if(!spiflash_dump(start_address, end_address, sizeof(data), data, &flash_info, file_name)) return;
    }

    if(write)
    {
        if(!spiflash_write_test(start_address, end_address, sizeof(data), data, &flash_info)) return;
        if(!spiflash_write_verify(start_address, end_address, sizeof(data), data, &flash_info)) return;
    }
    return;
}


void load(opt_args (*args), struct command_result *res)
{
    FIL fil;			/* File object needed for each open file */
    FRESULT fr;     /* FatFs return code */
    uint8_t buffer[16]; //TODO: lookup page size...
    UINT count;
    uint8_t page_size=16;
    uint8_t page=0;

    //open file
    fr = f_open(&fil, args[0].c, FA_READ);	
    if (fr != FR_OK) {
        printf("File error %d", fr);
        res->error=true;
        return;
    }
    printf("Writing...");
    //write bytes to eeprom
    for (;;) {
        //setup eeprom write
        //WREN 0b0000110
        spi_set_cs(0);
        spi_write_simple(0b00000110);
        spi_set_cs(1);
        //read file
        fr = f_read(&fil, buffer, sizeof buffer, &count); /* Read a chunk of data from the source file */
        if (count == 0) break; /* error or eof */
        
        //write buffer to EEPROM page
        spi_set_cs(0);
        spi_write_simple(0b00000010);
        spi_write_simple(page_size*page);
        for(UINT i=0; i<count; i++)
        {
            //printf("%c",buffer[i]);
            spi_write_simple(buffer[i]);
        }
        spi_set_cs(1);
        page++;

        //poll status register for write bit
        //0b00000001
        while(true)
        {
            spi_set_cs(0);
            spi_write_simple(0b00000101);
            uint8_t temp=spi_read_simple();
            spi_set_cs(1);
            if(temp && 0b1)
            {
                //printf("WIP\r\n");
                continue;
            }
            break;        
        }
        
    }    

    printf(" %d bytes. OK\r\nVerifying...", page*page_size);

    //verify write
    f_lseek(&fil, 0);

    //read from eeprom
    spi_set_cs(0);
    spi_write_simple(0b00000011);
    spi_write_simple(0);
    page=0;
    for(;;)
    {
        fr = f_read(&fil, buffer, sizeof buffer, &count); /* Read a chunk of data from the source file */
        if (count == 0) break; /* error or eof */

        for(UINT i=0; i<count; i++)
        {
            if(spi_read_simple()!=buffer[i])
            {
                printf("mismatch at %d",count*page);
                res->error=true;
                return;
            };
        }
        page++;
    }
    spi_set_cs(1);
    printf("%d bytes OK", page*page_size);

    /* Close open files */
    f_close(&fil);


}