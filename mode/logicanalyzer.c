#include <stdio.h>
#include <math.h>
#include <string.h>
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
#include "ui/ui_term.h"
#include "usb_rx.h"

#define DMA_BYTES_PER_CHUNK 32768
#define LA_DMA_COUNT 4

int la_dma[LA_DMA_COUNT];
uint8_t *la_buf;
uint32_t la_ptr=0;
bool la_done;

PIO pio = pio0;
uint sm = 0; 
static uint offset=0;

void la_print_row(char c, uint8_t count)
{
    for(int i=0; i<count; i++) printf("%c",c);
}

void la_redraw(uint32_t start_pos)
{
    //draw timing marks
    printf("%s\e[3A\r\u2551 \u2551\t\e[8X%d\t\t\e[8X%d\t\t\e[8X%d\t\t\e[8X%d\t\t\e[8X%d", ui_term_color_reset(), la_ptr, la_ptr+10, la_ptr+20, la_ptr+30, la_ptr+40);
    
    //back to line graph
    printf("\e[3B\r\e[3C"); //move to top, right three
    for(int i=0; i<76; i++)
    {
        uint8_t sample, previous_sample;

        logicanalyzer_dump(&sample);
        
        for(int pins=0; pins<8; pins++)
        {
            if(sample & (0b1<<pins))
            {
                //if(!(previous_sample & (0b1<<pins))) //rising edge 
                //{
                    //printf("%s\u250C", ui_term_color_prompt());
                //}
                //else
                //{
                    printf("%s\u2550", ui_term_color_prompt());
                //}
            }
            else
            {
                //if((previous_sample & (0b1<<pins))) //falling edge 
                //{
                //    printf("%s\u2510", ui_term_color_error());
                //}
                //else
                //{
                    printf("%s_", ui_term_color_error());
                //}                

            }
                printf("\e[1B\e[1D"); //move one line down, one position left
        }
        previous_sample=sample;
        printf("\e[8A\e[1C"); //move to top, right one
    }
}

void la_test_args(opt_args (*args), struct command_result *res)
{

    printf("Commands: (r)un, e(x)it, arrow keys to navigate\r\n");
    printf("Sampling...\n\n\n\n\n\n\n\n\n\n\n\n\n\r"); //free screen space for graph
    logicanalyzer_setup();
    logic_analyzer_arm(1000000, 1000, 0x00, 0x00);
    while(!logic_analyzer_is_done());
    
    //80 characters wide box outline
    //box top and corners
    system_config.terminal_hide_cursor=true; //prevent the status bar from showing the cursor again
    printf("\e[?25l\e[13A\r\u2554\u2550\u252c"); //move to top, left
    for(int i=0; i<76; i++) printf("\u2550");
    printf("\u2557");

    //time display ticks
    printf("\e[1B\r\u2551 \u2551\t0000\t\t1000\t\t2000\t\t4000\t\t5000");
    printf("\e[1B\r\u2551 \u2551\t\u2502\t\t\u2502\t\t\u2502\t\t\u2502\t\t\u2502");
    printf("\e[1B\r\u251c\u2550\u253c"); 
    for(int i=0; i<76; i++) printf("\u2550");
    printf("\u2557");

    //box left and right
    for(int i=0; i<8; i++)
    {   printf("\e[1B\r\u2551");//box left and right
        ui_term_color_text_background(hw_pin_label_ordered_color[i+1][0],hw_pin_label_ordered_color[i+1][1]);
        printf("%d%s\u2502\e[79C\u2551", i, ui_term_color_reset());
    }
    
    //box bottom and corners
    printf("\e[1B\r\u255a\u2550\u2569");
    for(int i=0; i<76; i++) printf("\u2550");
    printf("\u255d");
    printf("\e[8A\r\e[3C"); //move to top, right three
    la_redraw(32);

    while(true)
    {
        char c;

        if(rx_fifo_try_get(&c))
        {
           switch(c)
           {
                case 'r':
                    logic_analyzer_arm(1000000, 1000, 0x00, 0x00);
                    while(!logic_analyzer_is_done());
                    la_redraw(32);
                    break;
                case 'x':
                    system_config.terminal_hide_cursor=false;
                    printf("\e[?25h\e[9B%s%s", ui_term_color_reset(), ui_term_cursor_show()); //back to bottom
                    logic_analyzer_cleanup();
                    return;
                    break;
                case '\x1B': // escape commands	
                    rx_fifo_get_blocking(&c);
                    switch(c)
                    {
                        case '[': // arrow keys
                            rx_fifo_get_blocking(&c);
                            switch(c)
                            {
                                case 'D': //left
                                la_redraw(32);
                                break;
                                case 'C': //right
                                la_redraw(32);
                                break;
                            }
                            break;
                    }
                    break;
           }
        }
    }




}

int logicanalyzer_status(void)
{
    return 1; //idle, armed, sampling, done   .

}

uint8_t logicanalyzer_dump(uint8_t *txbuf)
{
    *txbuf=la_buf[la_ptr];

    if(la_ptr==0)
        la_ptr=(DMA_BYTES_PER_CHUNK*LA_DMA_COUNT)-1;
    else
        la_ptr--;
        
    return 1;
}

void logic_analyser_done(void)
{
    la_done=true;

    //turn off stuff!
    pio_interrupt_clear(pio, 0);
    irq_set_enabled(PIO0_IRQ_0, false);
    irq_set_enabled(pio_get_dreq(pio, sm, false), false);
    irq_remove_handler(PIO0_IRQ_0, logic_analyser_done);
    pio_sm_set_enabled(pio, sm, false);
    pio_clear_instruction_memory(pio);

    busy_wait_ms(1);

    int tail_dma = -1;
    uint32_t tail_offset;

    //find the final sample
    for(uint8_t i=0; i<count_of(la_dma); i++)
    {
        if(dma_channel_is_busy(la_dma[i]))
        {
            tail_dma=i;
            break;
        }
    }
    //error, return
    if(tail_dma==-1) 
    {
        return;
    }
    
    //transfer count is the words remaining in the stalled transfer, dma deincrements on start (-1)
    int32_t tail = DMA_BYTES_PER_CHUNK - dma_channel_hw_addr(la_dma[tail_dma])->transfer_count - 1;

    //add the preceding chunks of DMA to find the location in the array
    //ready to dump
    la_ptr = ( (DMA_BYTES_PER_CHUNK * tail_dma) + tail);

}

bool logic_analyzer_is_done(void)
{
    return la_done;
}

bool logic_analyzer_arm(float freq, uint32_t samples, uint32_t trigger_mask, uint32_t trigger_direction)
{
    memset(la_buf, 0, sizeof(la_buf));

    for(uint8_t i=0; i<BIO_MAX_PINS; i++)
    {
        bio_input(BIO0+i);
    }

    pio_clear_instruction_memory(pio);
    if(trigger_mask)
    {
        uint8_t trigger_pin = 0; //TODO: ensure single pin and determine high or low
        offset=pio_add_program(pio, &logicanalyzer_program);
        logicanalyzer_program_init(pio, sm, offset, bio2bufiopin[BIO0], trigger_pin, freq);
    }
    else
    {

       offset=pio_add_program(pio, &logicanalyzer_no_trigger_program); 
       logicanalyzer_no_trigger_program_init(pio, sm, offset, bio2bufiopin[BIO0], freq);
    }
    

    // interrupt on done notification
    pio_interrupt_clear(pio, 0);
    pio_set_irq0_source_enabled(pio, pis_interrupt0, true);
    irq_set_exclusive_handler(PIO0_IRQ_0, logic_analyser_done);
    irq_set_enabled(PIO0_IRQ_0, true);
    irq_set_enabled(pio_get_dreq(pio, sm, false), true);
    irq_clear(pio_get_dreq(pio, sm, false));
    la_done=false;
    //write sample count and enable sampling
    pio_sm_put_blocking(pio, sm, samples - 1);
    pio_sm_set_enabled(pio, sm, true);
    
}

bool logic_analyzer_cleanup(void)
{
    
    for(uint8_t i=0; i<count_of(la_dma); i++)
    {
        dma_channel_cleanup(la_dma[i]);
        dma_channel_unclaim(la_dma[i]);
    }
    pio_clear_instruction_memory(pio);

    mem_free(la_buf);

}

bool logicanalyzer_setup(void)
{
    dma_channel_config la_dma_config[LA_DMA_COUNT];

    la_buf=mem_alloc(DMA_BYTES_PER_CHUNK*LA_DMA_COUNT, 0);
    if(!la_buf)
    {
        //printf("Failed to allocate buffer. Is the scope running?\r\n");
        return false;
    }

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

        dma_channel_configure(la_dma[i], &la_dma_config[i], (volatile uint8_t *)&la_buf[DMA_BYTES_PER_CHUNK * i], &pio->rxf[sm], DMA_BYTES_PER_CHUNK, false); 

    }

    //start the first channel, will pause for data from PIO
    dma_channel_start(la_dma[0]);
    return true;
}