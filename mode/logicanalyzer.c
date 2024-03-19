#include <stdio.h>
#include <math.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "pirate.h"
#include "system_config.h"
#include "pirate/bio.h"
#include "opt_args.h"
#include "logicanalyzer.h"
#include "hardware/pio.h"
#include "build/logicanalyzer.pio.h"
#include "mem.h"
#include "hardware/structs/bus_ctrl.h"
#include "ui/ui_term.h"
#include "usb_rx.h"
#include "storage.h"
#include "rgb.h"
#include "pico/multicore.h"
#include "amux.h"
#include "ui/ui_cmdln.h"

enum logicanalyzer_status
{
    LA_IDLE=0,
    LA_ARMED_INIT,
    LA_ARMED,
    LA_CAPTURE
};

#define DMA_BYTES_PER_CHUNK 32768
#define LA_DMA_COUNT 4

int la_dma[LA_DMA_COUNT];
uint8_t *la_buf;
uint32_t la_ptr=0;
uint8_t la_status=LA_IDLE;

PIO pio = pio0;
uint sm = 0; 
static uint offset=0;
static const struct pio_program *pio_program_active;

void la_redraw(uint32_t start_pos, uint32_t la_samples)
{

    if(start_pos+76>la_samples)//no more samples to show on screen
    {
        start_pos=la_samples-76;
    }

    //find the start point
    uint32_t sample_ptr; //number of samples 
    sample_ptr = (la_ptr-la_samples) & 0x1ffff;
    sample_ptr = (sample_ptr+start_pos) & 0x1ffff;
    system_config.terminal_ansi_statusbar_pause=true;
    busy_wait_ms(1);
    system_config.terminal_hide_cursor=true; //prevent the status bar from showing the cursor again
    printf("%s",ui_term_cursor_hide());    
    //save cursor
    printf("\e7");
    //draw timing marks
    //printf("%s\e[3A\r\t\e[8X%d\t\t\e[8X%d\t\t\e[8X%d\t\t\e[8X%d\t\t\e[8X%d", ui_term_color_reset(), start_pos+6, start_pos+6+(16*1), start_pos+6+(16*2), start_pos+6+(16*3), start_pos+6+(16*4));

    printf("%s\e[%d;0H\e[K   \t%d\t\t%d\t\t%d\t\t%d\t\t%d", ui_term_color_reset(), system_config.terminal_ansi_rows-(12), start_pos+6, start_pos+6+(16*1), start_pos+6+(16*2), start_pos+6+(16*3), start_pos+6+(16*4));

    for(int i=0; i<76; i++)
    {
        uint8_t sample, previous_sample;
        
        sample=la_buf[sample_ptr];
            
        sample_ptr++;
        sample_ptr &= 0x1ffff;

        printf("\e[%d;%dH",system_config.terminal_ansi_rows-(11), i+3); //line graph top, current position

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
                    //printf("%s\u2B1B", ui_term_color_prompt());
                    ui_term_color_text_background(hw_pin_label_ordered_color[pins+1][0],hw_pin_label_ordered_color[pins+1][1]);
                    printf(" %s",ui_term_color_reset());
                    //ui_term_color_text(hw_pin_label_ordered_color[pins+1][1]);
                    //printf("\u2500");
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
        
    }
    printf("\e8"); //restore cursor
    system_config.terminal_ansi_statusbar_pause=false;
    system_config.terminal_hide_cursor=false;
    printf("%s",ui_term_cursor_show());
}

void la_draw_frame(void)
{  
    //printf("\r\nCommands: (r)un, (s)ave, e(x)it, arrow keys to navigate\r\n");
    //80 characters wide box outline
    //box top and corners
    system_config.terminal_ansi_statusbar_pause=true;
    busy_wait_ms(1);
    system_config.terminal_hide_cursor=true; //prevent the status bar from showing the cursor again
    printf("%s",ui_term_cursor_hide());
    
    for(uint8_t i=0; i<10; i++)
    {
        printf("\r\n"); //make space!
    }

    //set scroll region, disable line wrap
    printf("\e[%d;%dr\e[7l", 1, system_config.terminal_ansi_rows-14);
    
    //a little header thing?
    printf("\e[%d;0H\e[K\u253C", system_config.terminal_ansi_rows-(13)); //row 10 of LA
    for(int i=0; i<78; i++) printf("\u2500");
    printf("\u253c");

    //sample numbers, row 9 of LA
    printf("\e[%d;0H\e[K   \t0000\t\t1000\t\t2000\t\t4000\t\t5000", system_config.terminal_ansi_rows-(12));

    //box left and right
    //8 bars start at monitor area (+3)
    //todo: lower if monitor bar disabled?
    for(int i=0; i<8; i++) //row 8 to 1 of LA         
    {   
        printf("\e[%d;0H\e[K",system_config.terminal_ansi_rows-(11-i));
        ui_term_color_text_background(hw_pin_label_ordered_color[i+1][0],hw_pin_label_ordered_color[i+1][1]);
        printf(" %d%s\e[76C", i, ui_term_color_reset());
        ui_term_color_text_background(hw_pin_label_ordered_color[i+1][0],hw_pin_label_ordered_color[i+1][1]);
        printf("%d %s",i,ui_term_color_reset());
    }

    printf("\e[%d;0H\e[K", system_config.terminal_ansi_rows-(14)); //return to non-scroll area
    system_config.terminal_hide_cursor=false;
    printf("%s",ui_term_cursor_show());
}

uint32_t la_freq=1000, la_samples=1000;
uint32_t la_trigger_pin=0, la_trigger_level=0;

bool la_active=false;

void la_periodic(void)
{
    if(la_active && la_status==LA_IDLE)
    {
        la_redraw(0, la_samples);
        logic_analyzer_arm((float)(la_freq*1000), la_samples, la_trigger_pin, la_trigger_level);
    }

}

void la_test_args(struct command_result *res)
{

    uint32_t sample_position=0;

    if(!la_active)
    {
        if(!logicanalyzer_setup())
        {
            printf("Failed to allocate buffer. Is the scope running?\r\n");
        }
    }
    command_var_t arg;
    uint32_t temp;
    cmdln_args_find_flag_uint32('f', &arg, &temp);
    if(arg.has_value) //freq in khz
    {
        la_freq=temp;
    }
    printf("Freq: %dkHz ", la_freq);
    cmdln_args_find_flag_uint32('s', &arg, &temp);
    if(arg.has_value) //samples
    {
        la_samples=temp;
    }
    printf("Samples: %d ",la_samples);

    cmdln_args_find_flag_uint32('t', &arg, &temp);   
    if(arg.has_value) //trigger pin (or none)
    {
        if(temp >=0 && temp<=7)
        {
            la_trigger_pin=1u<<temp;
            printf("Trigger pin: IO%d ",temp);
            cmdln_args_find_flag_uint32('l', &arg, &temp); 
            if(arg.has_value) //trigger level
            {
                la_trigger_level=temp?1u<<temp:0;
                printf("Trigger level: %d \r\n",temp);
            }            
        }
        else
        {
            printf("Trigger pin: range error! Using none.");
        }
    }

    if(!la_active)
    {
        la_draw_frame();
        la_active=true;
        amux_sweep();
        if(hw_adc_voltage[HW_ADC_MUX_VREG_OUT]<100)
        {
            printf("%s", t[T_WARN_VOUT_VREF_LOW]);
        }
        return;
    }

    //goto la_sample;

    while(true)
    {
        char c;

        if(rx_fifo_try_get(&c))
        {
           switch(c)
           {
                case 's'://TODO: need to handle wrap...
                    //storage_save_binary_blob_rollover();
                    storage_save_binary_blob_rollover(la_buf, (la_ptr-la_samples) & 0x1ffff, la_samples, 0x1ffff);
                    break;
                case 'r':
la_sample:                
                    logic_analyzer_arm((float)(la_freq*1000), la_samples, la_trigger_pin, la_trigger_level);
                    sample_position=0;
                    while(!logic_analyzer_is_done())
                    {
                        char c;
                        if(rx_fifo_try_get(&c))
                        {
                            if(c=='x')
                            {
                                printf("Canceled!\r\n");
                                goto la_x;
                            }
                        }
                    }
                    la_redraw(sample_position, la_samples);
                    logicanalyzer_reset_led();
                    break;
                case 'q':
                case 'x':
la_x:
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
                                if(sample_position<64)
                                {
                                    sample_position=0;
                                }
                                else
                                {
                                    sample_position-=64;
                                }
                                la_redraw(sample_position, la_samples);
                                break;
                                case 'C': //right
                                if(sample_position>la_samples-76) //samples - columns
                                {
                                    sample_position=la_samples-76;
                                }
                                else
                                {
                                    sample_position+=64;
                                }                                
                                la_redraw(sample_position, la_samples);
                                break;
                            }
                            break;
                    }
                    break;
           }
        }
    }




}

void logicanalyzer_reset_led(void)
{
    multicore_fifo_push_blocking(0xf4);
    multicore_fifo_pop_blocking();
}


uint8_t logicanalyzer_dump(uint8_t *txbuf)
{
    *txbuf=la_buf[la_ptr];

    la_ptr--;
    la_ptr&=0x1ffff;
       
    return 1;
}

void logic_analyser_done(void)
{
    //turn off stuff!
    pio_interrupt_clear(pio, 0);
    irq_set_enabled(PIO0_IRQ_0, false);
    irq_set_enabled(pio_get_dreq(pio, sm, false), false);
    irq_remove_handler(PIO0_IRQ_0, logic_analyser_done);
    pio_sm_set_enabled(pio, sm, false);
    //pio_clear_instruction_memory(pio);
    if(pio_program_active){
        pio_remove_program(pio, pio_program_active, offset);
        pio_program_active=0;
    }

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

    rgb_set_all(0x00,0xff,0);//,0x00FF00 green for dump

    la_status=LA_IDLE;
}

uint32_t logic_analyzer_get_dma_tail(void)
{
    uint8_t tail_dma=0xff;
    for(uint8_t i=0; i<count_of(la_dma); i++)
    {
        if(dma_channel_is_busy(la_dma[i]))
        {
            tail_dma=i;
            break;
        }
    }

    if(tail_dma>count_of(la_dma))
    {
        //hum
        return 0;
    }
    
    return dma_channel_hw_addr(la_dma[tail_dma])->transfer_count;
}

bool logic_analyzer_is_done(void)
{
    static int32_t tail;
    uint8_t tail_dma=0xff;

    if(la_status==LA_ARMED_INIT)
    {
        tail=logic_analyzer_get_dma_tail();
        la_status=LA_ARMED;
    }

    if(la_status==LA_ARMED && tail!=logic_analyzer_get_dma_tail())
    {
        la_status=LA_CAPTURE;
        rgb_set_all(0xab,0x7f,0);//0xAB7F00 yellow for capture in progress.

    }

    return (la_status==LA_IDLE);
}

bool logic_analyzer_arm(float freq, uint32_t samples, uint32_t trigger_mask, uint32_t trigger_direction)
{
    memset(la_buf, 0, sizeof(la_buf));

    /*for(uint8_t i=0; i<BIO_MAX_PINS; i++)
    {
        bio_input(BIO0+i);
    }*/

    //pio_clear_instruction_memory(pio);
    if(pio_program_active){
        pio_remove_program(pio, pio_program_active, offset);
        pio_program_active=0;
    }
    
    uint8_t trigger_pin = 0;
    bool trigger_ok=false;
    if(trigger_mask)
    {
        for(uint8_t i=0; i<8; i++)
        {
            if(trigger_mask & 1u<<i)
            {
                trigger_pin=i;
                trigger_ok=true;
                break; //use first masked pin
            }
        }
    }

    if(trigger_ok)
    {
        if(trigger_direction & 1u<<trigger_pin) //high level trigger program
        {
            offset=pio_add_program(pio, &logicanalyzer_high_trigger_program);
            pio_program_active=&logicanalyzer_high_trigger_program;
            logicanalyzer_high_trigger_program_init(pio, sm, offset, bio2bufiopin[BIO0], bio2bufiopin[trigger_pin], freq);
        }
        else //low level trigger program
        {
            offset=pio_add_program(pio, &logicanalyzer_low_trigger_program);
            pio_program_active=&logicanalyzer_low_trigger_program;
            logicanalyzer_low_trigger_program_init(pio, sm, offset, bio2bufiopin[BIO0], bio2bufiopin[trigger_pin], freq);           
        }
    }
    else    //else no trigger program
    {
       offset=pio_add_program(pio, &logicanalyzer_no_trigger_program); 
       pio_program_active=&logicanalyzer_no_trigger_program;
       logicanalyzer_no_trigger_program_init(pio, sm, offset, bio2bufiopin[BIO0], freq);
    }
    

    // interrupt on done notification
    pio_interrupt_clear(pio, 0);
    pio_set_irq0_source_enabled(pio, pis_interrupt0, true);
    irq_set_exclusive_handler(PIO0_IRQ_0, logic_analyser_done);
    irq_set_enabled(PIO0_IRQ_0, true);
    irq_set_enabled(pio_get_dreq(pio, sm, false), true);
    irq_clear(pio_get_dreq(pio, sm, false));
    la_status=LA_ARMED_INIT;
    multicore_fifo_push_blocking(0xf3);
    multicore_fifo_pop_blocking();
    //rgb_irq_enable(false);
    busy_wait_ms(5);
    rgb_set_all(0xff,0,0); //RED LEDs for armed
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

    //pio_clear_instruction_memory(pio);
    if(pio_program_active){
        pio_remove_program(pio, pio_program_active, offset);
        pio_program_active=0;
    }

    mem_free(la_buf);

    logicanalyzer_reset_led();

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