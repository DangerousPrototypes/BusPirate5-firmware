#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pirate.h"
#include "commands.h"
#include "fatfs/ff.h"
#include "storage.h"
#include "mode/hwspi.h"


void dump(opt_args (*args), struct command_result *res)
{
    FIL fil;			/* File object needed for each open file */
    FRESULT fr;     /* FatFs return code */
    uint8_t buffer[16]; //TODO: lookup page size...
    UINT count;
    uint8_t page_size=16;
    uint8_t page=0;
    UINT bw;

    //open file
    fr = f_open(&fil, args[0].c, FA_WRITE | FA_CREATE_ALWAYS);	
    if (fr != FR_OK) {
        printf("File error %d", fr);
        res->error=true;
        return;
    }

    printf("Reading...");
     //read from eeprom
    spi_start();
    spi_send(0b00000011);
    spi_send(0);
    page=0;
    count=16;
    for(uint8_t j=0;j<16;j++)
    {
        for(UINT i=0; i<16; i++)
        {
            buffer[i]=spi_read(0xff);
        }

        f_write(&fil, buffer, count, &bw);	

        if(fr != FR_OK || bw!=count)
        {
            printf("Disk access error");
            res->error=true;
            return;
        }
        page++;
    }
    spi_stop();
    printf("%d bytes OK", page*page_size);

    /* Close open files */
    f_close(&fil);
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
        spi_start();
        spi_send(0b00000110);
        spi_stop();
        //read file
        fr = f_read(&fil, buffer, sizeof buffer, &count); /* Read a chunk of data from the source file */
        if (count == 0) break; /* error or eof */
        
        //write buffer to EEPROM page
        spi_start();
        spi_send(0b00000010);
        spi_send(page_size*page);
        for(UINT i=0; i<count; i++)
        {
            //printf("%c",buffer[i]);
            spi_send(buffer[i]);
        }
        spi_stop();
        page++;

        //poll status register for write bit
        //0b00000001
        while(true)
        {
            spi_start();
            spi_send(0b00000101);
            uint8_t temp=spi_read(0xff);
            spi_stop();
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
    spi_start();
    spi_send(0b00000011);
    spi_send(0);
    page=0;
    for(;;)
    {
        fr = f_read(&fil, buffer, sizeof buffer, &count); /* Read a chunk of data from the source file */
        if (count == 0) break; /* error or eof */

        for(UINT i=0; i<count; i++)
        {
            if(spi_read(0xff)!=buffer[i])
            {
                printf("mismatch at %d",count*page);
                res->error=true;
                return;
            };
        }
        page++;
    }
    spi_stop();
    printf("%d bytes OK", page*page_size);

    /* Close open files */
    f_close(&fil);


}