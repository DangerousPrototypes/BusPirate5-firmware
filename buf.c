#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "queue.h"
#include "hardware/dma.h"
#include "buf.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "tusb.h"

void rx_fifo_handler(void);

// TODO: rework all the TX stuff into a nice struct with clearer naming 
queue_t tx_fifo;
#define TX_FIFO_LENGTH_IN_BITS 10 // 2^n buffer size. 2^3=8, 2^9=512
#define TX_FIFO_LENGTH_IN_BYTES (0x0001<<TX_FIFO_LENGTH_IN_BITS)
char tx_buf[TX_FIFO_LENGTH_IN_BYTES] __attribute__((aligned(2048)));

char tx_sb_buf[1024];
uint16_t tx_sb_buf_cnt=0;
bool tx_sb_buf_ready=false;

queue_t rx_fifo;
#define RX_FIFO_LENGTH_IN_BITS 7 // 2^n buffer size. 2^3=8, 2^9=512
#define RX_FIFO_LENGTH_IN_BYTES (0x0001<<RX_FIFO_LENGTH_IN_BITS)
char rx_buf[RX_FIFO_LENGTH_IN_BYTES]; 

uint8_t cnt=0;
int chan;
int chan_sb;
bool chan_busy=false;
uint16_t bytes_available;

bool chan_caller_sb=false;

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

void tx_fifo_init(void)
{   
    queue2_init(&tx_fifo, tx_buf, TX_FIFO_LENGTH_IN_BYTES); //buffer size must be 2^n for queue AND DMA rollover
    
    #ifdef BP_DEBUG_ENABLED
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
    #endif   

}

void tx_fifo_put(char *c)
{
    queue2_add_blocking(&tx_fifo, c);
}

void tx_sb_start(uint32_t len)
{
    #ifdef BP_DEBUG_ENABLED
        if( (chan_busy && chan_caller_sb) )
        {
            printf("hum, I was unable to start the status bar..\r\n");
            return;
        }
        tx_sb_buf_ready=true;
        tx_sb_buf_cnt=len;
    #endif
}

void tx_fifo_service(void)
{
    #ifdef BP_DEBUG_ENABLED
        //if busy, return here to avoid the spin lock in the byte check
        if(chan_busy || dma_channel_is_busy(chan) || dma_channel_is_busy(chan_sb)) return;
    #else
        char data[64];
    #endif
 
    // RX FIFO has priority here
    // This prevents the status bar from being wiped out by the VT100 setup commands 
    // that might be pending in the RX FIFO
     queue_available_bytes(&tx_fifo, &bytes_available);
    
    if(bytes_available)
    {
        #ifdef BP_DEBUG_ENABLED
            chan_busy=true;
            chan_caller_sb=false;
            dma_channel_set_trans_count(chan, bytes_available, true);
            // Wait blocking and update pointer for non-interrupt debugging
            //dma_channel_wait_for_finish_blocking(chan);
            //queue_update_read_pointer(&tx_fifo, &bytes_available);
        #else
            uint16_t i=0;
            while(i<1) 
            {
                if(!queue2_try_remove(&tx_fifo,&data[i])) break;
                i++;
            }

            tud_cdc_write(&data, i);
            tud_cdc_write_flush();
        #endif
        
        return;
    }


    if(tx_sb_buf_ready)
    {
        #ifdef BP_DEBUG_ENABLED
            chan_busy=true;
            chan_caller_sb=true;
            dma_channel_set_read_addr(chan_sb, tx_sb_buf, false );
            dma_channel_set_trans_count(chan_sb, tx_sb_buf_cnt, true);       
        return;
        #else

        #endif
    }  
    

  
}
void dma_test_old() 
{
    for(uint8_t i=0; i<7; i++){
        char t='0'+cnt;
        queue2_add_blocking(&tx_fifo,&t);
        cnt++;
        if(cnt>9) cnt=0;
    }

    uint16_t bytes_available;
    queue_available_bytes(&tx_fifo, &bytes_available);
    dma_channel_set_trans_count(chan, bytes_available, true);

    // We could choose to go and do something else whilst the DMA is doing its
    // thing. In this case the processor has nothing else to do, so we just
    // wait for the DMA to finish.
    dma_channel_wait_for_finish_blocking(chan);

    // DMA copy complete, free up the bytes consumed
    queue_update_read_pointer(&tx_fifo, &bytes_available);

    // The DMA has now copied our text from the transmit buffer (src) to the
    // receive buffer (dst), so we can print it out from there.
    //printf("%s",dst);
    //dma_channel_unclaim(chan);

}

/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// Use two DMA channels to make a programmed sequence of data transfers to the
// UART (a data gather operation). One channel is responsible for transferring
// the actual data, the other repeatedly reprograms that channel.
#include "hardware/structs/uart.h"

// These buffers will be DMA'd to the UART, one after the other.

char word0[] = "Transferring ";
char word1[] = "one ";
char word2[] = "word ";
char word3[] = "at ";
char word4[] = "a ";
char word5[] = "time.\n";

// Note the order of the fields here: it's important that the length is before
// the read address, because the control channel is going to write to the last
// two registers in alias 3 on the data channel:
//           +0x0        +0x4          +0x8          +0xC (Trigger)
// Alias 0:  READ_ADDR   WRITE_ADDR    TRANS_COUNT   CTRL
// Alias 1:  CTRL        READ_ADDR     WRITE_ADDR    TRANS_COUNT
// Alias 2:  CTRL        TRANS_COUNT   READ_ADDR     WRITE_ADDR
// Alias 3:  CTRL        WRITE_ADDR    TRANS_COUNT   READ_ADDR
//
// This will program the transfer count and read address of the data channel,
// and trigger it. Once the data channel completes, it will restart the
// control channel (via CHAIN_TO) to load the next two words into its control
// registers.

struct {uint32_t len; const char *data;} control_blocks[] = {
    {count_of(word0) - 1, word0}, // Skip null terminator
    {count_of(word1) - 1, word1},
    {count_of(word2) - 1, word2},
    {count_of(word3) - 1, word3},
    {count_of(word4) - 1, word4},
    {count_of(word5) - 1, word5},
    {0, word5}                     // Null trigger to end chain.
};

void dma_test() {

    puts("DMA control block example:");

    // ctrl_chan loads control blocks into data_chan, which executes them.
    int ctrl_chan = dma_claim_unused_channel(true);
    int data_chan = dma_claim_unused_channel(true);

    // The control channel transfers two words into the data channel's control
    // registers, then halts. The write address wraps on a two-word
    // (eight-byte) boundary, so that the control channel writes the same two
    // registers when it is next triggered.

    dma_channel_config c = dma_channel_get_default_config(ctrl_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, true);
    channel_config_set_ring(&c, true, 3); // 1 << 3 byte boundary on write ptr
    //can we set this 
    dma_channel_configure(
        ctrl_chan,
        &c,
        &dma_hw->ch[data_chan].al3_transfer_count, // Initial write address
        &control_blocks[0],                        // Initial read address
        2,                                         // Halt after each control block
        false                                      // Don't start yet
    );

    // The data channel is set up to write to the UART FIFO (paced by the
    // UART's TX data request signal) and then chain to the control channel
    // once it completes. The control channel programs a new read address and
    // data length, and retriggers the data channel.

    c = dma_channel_get_default_config(data_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_dreq(&c, DREQ_UART1_TX);
    // Trigger ctrl_chan when data_chan completes
    channel_config_set_chain_to(&c, ctrl_chan);
    // Raise the IRQ flag when 0 is written to a trigger register (end of chain):
    channel_config_set_irq_quiet(&c, true);

    dma_channel_configure(
        data_chan,
        &c,
        &uart_get_hw(BP_DEBUG_UART)->dr,
        NULL,           // Initial read address and transfer count are unimportant;
        0,              // the control channel will reprogram them each time.
        false           // Don't start yet.
    );

    // Everything is ready to go. Tell the control channel to load the first
    // control block. Everything is automatic from here.
    dma_start_channel_mask(1u << ctrl_chan);

    // The data channel will assert its IRQ flag when it gets a null trigger,
    // indicating the end of the control block list. We're just going to wait
    // for the IRQ flag instead of setting up an interrupt handler.
    while (!(dma_hw->intr & 1u << data_chan))
        tight_loop_contents();
    dma_hw->ints0 = 1u << data_chan;

    puts("DMA finished.");

}



void rx_fifo_init(void)
{
    queue2_init(&rx_fifo, rx_buf, RX_FIFO_LENGTH_IN_BYTES); //buffer size must be 2^n for queue AND DMA rollover
    
    #ifdef BP_DEBUG_ENABLED
    // setup interrupt on uart
    uart_set_fifo_enabled(BP_DEBUG_UART, true); //might set to false

    int UART_IRQ = BP_DEBUG_UART == uart0 ? UART0_IRQ : UART1_IRQ;

    irq_set_exclusive_handler(UART_IRQ, rx_fifo_handler);
    irq_set_enabled(UART_IRQ, true);

    uart_set_irq_enables(BP_DEBUG_UART, true, false);
    #else

    #endif

}


void rx_fifo_handler(void)
{
    #ifdef BP_DEBUG_ENABLED
    // while bytes available shove them in the buffer
    while(uart_is_readable(BP_DEBUG_UART)) {
        uint8_t c=uart_getc(BP_DEBUG_UART);
        queue2_add_blocking(&rx_fifo, &c);
    }    
    #else
    if ( tud_cdc_connected() )
    {
        // connected and there are data available
        if ( tud_cdc_available() )
        {
            uint8_t buf[64];

            uint32_t count = tud_cdc_read(buf, sizeof(buf));

            // while bytes available shove them in the buffer
            if(count>0)
            {
                for(uint8_t i=0; i<count; i++) {
                    uint8_t c=buf[i];

                    queue2_add_blocking(&rx_fifo, &c);
                } 
            }
        }
    }
    #endif
}
#ifndef BP_DEBUG_ENABLED
// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf)
{
    (void) itf;
    char buf[64];

    if(tud_cdc_available())
    {
        uint32_t count = tud_cdc_read(buf, 64);
        //(void) count;

        // while bytes available shove them in the buffer
        if(count>0)
        {
            for(uint8_t i=0; i<count; i++) {
                uint8_t c=buf[i];
                queue2_add_blocking(&rx_fifo, &c);
            } 
        }
    }
}
#endif

void rx_fifo_get_blocking(char *c)
{
    queue2_remove_blocking(&rx_fifo, c);
}

bool rx_fifo_try_get(char *c)
{
    return queue2_try_remove(&rx_fifo, c);
}

void rx_fifo_peek_blocking(char *c)
{
    queue2_peek_blocking(&rx_fifo, c);
}

bool rx_fifo_try_peek(char *c)
{
    return queue2_try_peek(&rx_fifo, c);
}