#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "pirate.h"
#include "system_config.h"
#include "bio.h"
#include "opt_args.h"
#include "logicanalyzer.h"
#include "hardware/pio.h"
#include "build/logicanalyzer.pio.h"
#include "mem.h"
#include "hardware/structs/bus_ctrl.h"

#define DMA_COUNT 32768
#define LA_DMA_COUNT 4

uint8_t *la_buf;

void la_test_args(opt_args (*args), struct command_result *res)
{
}

int logicanalyzer_status(void)
{
    return 1; //idle, armed, sampling, done   .

}

uint8_t logicanalyzer_dump(uint8_t *txbuf)
{
    static uint32_t ptr=0;
    *txbuf=la_buf[ptr];
    ptr++;
    if(ptr>=DMA_COUNT*LA_DMA_COUNT) ptr=0;
    return 1;
}

bool logicanalyzer_setup(void)
{
    PIO pio = pio0;
	uint sm = 0; 
    static uint offset=0;
    int la_dma[LA_DMA_COUNT];
    dma_channel_config la_dma_config[LA_DMA_COUNT];

    la_buf=mem_alloc(DMA_COUNT*LA_DMA_COUNT, 0);
    if(!la_buf)
    {
        //printf("Failed to allocate buffer. Is the scope running?\r\n");
        return false;
    }

    offset=pio_add_program(pio, &logicanalyzer_program);

    // high bus priority to the DMA
    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;
    
    for(uint8_t i=0; i<count_of(la_dma); i++)
    {
        la_dma[i]= dma_claim_unused_channel(true);
    }

    for(uint8_t i=0; i<count_of(la_dma); i++)
    {
        la_dma_config[i] = dma_channel_get_default_config(la_dma[i]);
        channel_config_set_read_increment(&la_dma_config[i], false); // read fixed PIO address
        channel_config_set_write_increment(&la_dma_config[i], true); // write to circular buffer
        channel_config_set_transfer_data_size(&la_dma_config[i], DMA_SIZE_8); // we have 8 IO pins
        channel_config_set_dreq(&la_dma_config[i], pio_get_dreq(pio, sm, false)); // &pio0_hw->rxf[sm] paces the rate of transfer
        channel_config_set_ring(&la_dma_config[i], true, 15); // loop at 2 * 8 bytes

        int la_dma_next = (i+1 < count_of(la_dma))? la_dma[i+1] : la_dma[0];
        channel_config_set_chain_to(&la_dma_config[i], la_dma_next); // chain to next DMA

        dma_channel_configure(la_dma[i], &la_dma_config[i], (volatile uint8_t *)&la_buf[DMA_COUNT * i], &pio->rxf[sm], DMA_COUNT, false); 

    }

    logicanalyzer_program_init(pio, sm, offset, bio2bufiopin[BIO0], 125000000);
    //start the first channel, will pause for data from PIO
    dma_channel_start(la_dma[0]);
/*
    uint8_t val = 31;
    while(true)
    {
        
        for(uint32_t i=0; i<4; i++)
        {
            printf("Block %i:\r\n", i);
            bool error=false;
            for(uint32_t b=0; b<256; b++)
            {
                if(val!=buf[b+ (i*256)])
                {
                    printf("%02x != %02x @ %i ", val, buf[b+ (i*256)], b);
                    error=true;
                }
                if(val==0) 
                    val=31;
                else
                    val--;        

            }
            if(error) printf("\r\nError!\r\n"); else printf("OK\r\n");
        }

        for(uint8_t i=0; i<count_of(la_dma); i++)
        {
            if(dma_channel_is_busy(la_dma[i]))
            {
                printf("DMA %i Busy!\r\n", i);
            }

        }
        
        busy_wait_ms(1000);
    }    
    */

   return true;
}