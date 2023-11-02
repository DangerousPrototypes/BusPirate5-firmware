#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "queue.h"
#include "hardware/dma.h"
//#include "buf.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "tusb.h"
#include "bio.h"
#include "system_config.h"
#include "debug.h"

// USB TX:
// This file contains the code for the user interface ring buffer
// and IO method. USB is the normal IO, but running a chip under debug 
// on a USB attached device is a nightmare. BP_DEBUG_ENABLED In the /platform/ folder
// configuration file enables a UART debugging option. All user interface IO
// will be routed to one of the two UARTs (selectable) on the Bus Pirate buffered IO pins
// UART debug mode is way over engineered using DMA et al, and has some predelection for bugs
// in status bar updates


// TODO: rework all the TX stuff into a nice struct with clearer naming 
queue_t tx_fifo;
queue_t bin_tx_fifo;
#define TX_FIFO_LENGTH_IN_BITS 10 // 2^n buffer size. 2^3=8, 2^9=512
#define TX_FIFO_LENGTH_IN_BYTES (0x0001<<TX_FIFO_LENGTH_IN_BITS)
char tx_buf[TX_FIFO_LENGTH_IN_BYTES] __attribute__((aligned(2048)));
char bin_tx_buf[TX_FIFO_LENGTH_IN_BYTES] __attribute__((aligned(2048)));

char tx_sb_buf[1024];
uint16_t tx_sb_buf_cnt=0;
uint16_t tx_sb_buf_index=0;
bool tx_sb_buf_ready=false;

void tx_fifo_init(void)
{   
    queue2_init(&tx_fifo, tx_buf, TX_FIFO_LENGTH_IN_BYTES); //buffer size must be 2^n for queue AND DMA rollover
    queue2_init(&bin_tx_fifo, bin_tx_buf, TX_FIFO_LENGTH_IN_BYTES); //buffer size must be 2^n for queue AND DMA rollover
}

void tx_sb_start(uint32_t len)
{
    tx_sb_buf_cnt=len;
    tx_sb_buf_ready=true;
}

void tx_fifo_service(void)
{
    //state machine:
    #define IDLE 0
    #define STATUSBAR_DELAY 1
    #define STATUSBAR_TX 2
    static uint8_t tx_state=IDLE;

    uint16_t bytes_available;
    char data[64];
    uint8_t i=0;

    if(system_config.terminal_usb_enable)
    {   // is tinyUSB CDC ready?
        if(tud_cdc_n_write_available(0)<64)
        {        
            return;
        }
    }

    switch(tx_state)
    {
        case IDLE:
            queue_available_bytes(&tx_fifo, &bytes_available);
            if(bytes_available)
            {             
                i=0;
                while(queue2_try_remove(&tx_fifo, &data[i])) 
                {
                    i++;
                    if(i>=64) break;
                } 
                break; //break out of switch and continue below       
            }

            //status bar update is ready
            if(tx_sb_buf_ready)
            {
                tx_state=STATUSBAR_DELAY;
                tx_sb_buf_index=0;
                return; //return for next state
            }
            
            return; //nothing, just return

            break;
        case STATUSBAR_DELAY:
            // test: check that no bytes in tx_fifo minimum 2 cycles in a row
            // prevent the status bar from being wiped out by the VT100 setup commands 
            // that might be pending in the TX FIFO
            queue_available_bytes(&tx_fifo, &bytes_available);
            tx_state=(bytes_available ? IDLE : STATUSBAR_TX);   
            return; //return for next cycle

            break;
        case STATUSBAR_TX:
            //read out 64 bytes into data at a time until complete
            //TODO: pass a pointer to the array cause this is inefficient
            i=0;
            while(tx_sb_buf_index<tx_sb_buf_cnt) 
            {
                data[i]=tx_sb_buf[tx_sb_buf_index]; 
                tx_sb_buf_index++;
                i++;
                if(tx_sb_buf_index>=tx_sb_buf_cnt)
                {
                    tx_sb_buf_ready=false;
                    tx_state=IDLE; //done, next cycle go to idle
                    system_config.terminal_ansi_statusbar_update=true; //after first draw of status bar, then allow updates by core1 service loop
                    break;
                }
                if(i>=64)
                {
                    break;
                }
            } 
            break;
        default:
            tx_state=IDLE;
            break;    

    }
 
    //if(i==0) return; //safety check

    //write to terminal usb
    if(system_config.terminal_usb_enable)
    {           
        tud_cdc_n_write(0, &data, i);
        tud_cdc_n_write_flush(0);
        if(system_config.terminal_uart_enable) tud_task(); //makes it nicer if we service when the UART is enabled
    }
    
    //write to terminal debug uart
    if(system_config.terminal_uart_enable)
    {
        for(uint8_t j=0; j<i; j++)
        {
            uart_putc(debug_uart[system_config.terminal_uart_number].uart, data[j]);
        }
    }
    
    return;
}

void tx_fifo_put(char *c)
{
    queue2_add_blocking(&tx_fifo, c);
}

void bin_tx_fifo_put(const char c)
{
    queue2_add_blocking(&bin_tx_fifo, &c);
}

void bin_tx_fifo_service(void)
{
    uint16_t bytes_available;
    char data[64];
    uint8_t i=0;

    // is tinyUSB CDC ready?
    if(tud_cdc_n_write_available(1)<64)
    {        
        return;
    }

    queue_available_bytes(&bin_tx_fifo, &bytes_available);
    if(bytes_available)
    {             
        i=0;
        while(queue2_try_remove(&bin_tx_fifo, &data[i])) 
        {
            i++;
            if(i>=64) break;
        } 
    }
   
    tud_cdc_n_write(1, &data, i);
    tud_cdc_n_write_flush(1);
}

bool bin_tx_not_empty(void)
{
    uint16_t cnt;
    queue_available_bytes(&bin_tx_fifo, &cnt);
    return TX_FIFO_LENGTH_IN_BYTES - cnt;
}




#if 0
//This is old TX stuff uing DMA + UART
// leaving it here for reference

char tx_sb_buf[1024];
uint16_t tx_sb_buf_cnt=0;
bool tx_sb_buf_ready=false;

uint8_t cnt=0;
int chan;
int chan_sb;
bool chan_busy=false;
uint16_t bytes_available;

bool chan_caller_sb=false;

void tx_fifo_init(void)
{   
    queue2_init(&tx_fifo, tx_buf, TX_FIFO_LENGTH_IN_BYTES); //buffer size must be 2^n for queue AND DMA rollover
    
        // Get a free channel, panic() if there are none
        chan=dma_claim_unused_channel(true);
        chan_sb=dma_claim_unused_channel(true);

        dma_channel_config c=dma_channel_get_default_config(chan);
        channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
        channel_config_set_dreq(&c, DREQ_UART1_TX);
        channel_config_set_read_increment(&c, true);
        channel_config_set_ring(&c, false, TX_FIFO_LENGTH_IN_BITS); // ring size =2^n 2^3=8, 2^9=512
        channel_config_set_write_increment(&c, false);
        //channel_config_set_irq_quiet(&c,true); 	
        // chain to the fifo DMA channel
        //channel_config_set_chain_to(&c, chan_sb);        
        dma_channel_configure(
            chan,          // Channel to be configured
            &c,            // The configuration we just created
            &uart_get_hw(BP_DEBUG_UART)->dr,           // The initial write address
            tx_fifo.data,           // The initial read address
            0, // Number of transfers; in this case each is 1 byte.
            false           // Start immediately.
        );


        c=dma_channel_get_default_config(chan_sb);
        channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
        channel_config_set_dreq(&c, DREQ_UART1_TX);
        channel_config_set_read_increment(&c, true);
        channel_config_set_write_increment(&c, false);
        dma_channel_configure(
            chan_sb,          // Channel to be configured
            &c,            // The configuration we just created
            &uart_get_hw(BP_DEBUG_UART)->dr,           // The initial write address
            tx_sb_buf,           // The initial read address
            0, // Number of transfers; in this case each is 1 byte.
            false           // Start immediately.
        );    
        
        // Tell the DMA to raise IRQ line 0 when the channel finishes a block
        dma_channel_set_irq0_enabled(chan, true);
        dma_channel_set_irq0_enabled(chan_sb, true);

        // Configure the processor to run dma_handler() when DMA IRQ 0 is asserted
        irq_set_exclusive_handler(DMA_IRQ_0, tx_fifo_handler);
        irq_set_enabled(DMA_IRQ_0, true); 

}

void tx_fifo_service(void)
{
        //if busy, return here to avoid the spin lock in the byte check
        if(chan_busy || dma_channel_is_busy(chan) || dma_channel_is_busy(chan_sb)) return;
 
    // TX FIFO has priority here
    // This prevents the status bar from being wiped out by the VT100 setup commands 
    // that might be pending in the TX FIFO
    queue_available_bytes(&tx_fifo, &bytes_available);
    
    if(bytes_available)
    {
            chan_busy=true;
            chan_caller_sb=false;
            dma_channel_set_trans_count(chan, bytes_available, true);
            // Wait blocking and update pointer for non-interrupt debugging
            //dma_channel_wait_for_finish_blocking(chan);
            //queue_update_read_pointer(&tx_fifo, &bytes_available);
               
        return;
    }


    if(tx_sb_buf_ready)
    {
            chan_busy=true;
            chan_caller_sb=true;
            dma_channel_set_read_addr(chan_sb, tx_sb_buf, false );
            dma_channel_set_trans_count(chan_sb, tx_sb_buf_cnt, true);       
        return;
    }  
     
}


// DMA interrupt handler for UART debug mode
void tx_fifo_handler(void)
{
    //dma_channel_get_irq0_status()
    if(dma_channel_get_irq0_status(chan_sb)) //called from status bar transfer
    {
        tx_sb_buf_cnt=0;
        tx_sb_buf_ready=false;
    }
    else //called from fifo transfer
    {
        // DMA copy complete, free up the bytes consumed in the ring buffer
        queue_update_read_pointer(&tx_fifo, &bytes_available);
    }

    //see if there are any other transfers pending
    //if more data just start right now!
    queue_available_bytes_unsafe(&tx_fifo, &bytes_available);
    if(bytes_available)
    {   
        dma_hw->ints0 = 1u << chan_sb; 
        dma_hw->ints0 = 1u << chan; // Disable interrupt, terrible documentation on this... 
        chan_caller_sb=false;
        dma_channel_set_trans_count(chan, bytes_available, true);
    }
    else if(tx_sb_buf_ready)
    {
        dma_hw->ints0 = 1u << chan; // Disable interrupt, terrible documentation on this... 
        dma_hw->ints0 = 1u << chan_sb; 
        dma_channel_set_read_addr(chan_sb, tx_sb_buf, false );
        dma_channel_set_trans_count(chan_sb, tx_sb_buf_cnt, true);
        return;          
    }    
    else
    {
        dma_hw->ints0 = 1u << chan; // Disable interrupt, terrible documentation on this... 
        dma_hw->ints0 = 1u << chan_sb; 
        //dma_irqn_acknowledge_channel(0,chan);
        //dma_irqn_acknowledge_channel(0,chan_sb);        
        chan_busy=false;
    }
}
#endif