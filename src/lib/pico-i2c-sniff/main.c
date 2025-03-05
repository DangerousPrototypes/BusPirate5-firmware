/**
 * i2c bus sniffer pico.
 * Sniffer for an i2c bus using the PIO of Raspberry Pi Pico (RP2040)
 * (C) Juan Schiavoni 2021
 *
 * It is composed of 4 state machines that communicate through an IRQ and 
 * two auxiliary pins to encode the event. Three of them decode the 
 * START/STOP/DATA condition, and the last one serializes the events into 
 * a single FIFO; and reads the value of the 8-bit data plus the ACK.
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "i2c_sniffer.pio.h"
#include "pico/multicore.h"

#include "ram_fifo.h"

#undef PRINT_VAL

#undef PRINT_TIME_T
#undef PRINT_HEX_INDEX

const uint led_pin = 25;
bool ram_fifo_overflow = false;

static char ascii_buff[64];
static uint32_t ascii_index = 0;

/*! \brief Converts a nibble to hexadecimal ascii..
 *  \ingroup main
 *
 * Quick routine to convert integers to hexadecimal characters.
 *
 * \return char (0,1,2,3,4,5,6,7,8,9,A,B,C,D,E,F).
 */
static inline char nibble_to_hex(uint8_t nibble) {
    nibble &= 0x0F;
  
    if (nibble > 9) {
        nibble += 'A' - '0' - 10;
    }

    return( nibble + '0' );
}

/*! \brief Print the buffered string.
 *  \ingroup main
 *
 * If the string buffer contains data, it sends it through the serial port.
 *
 * \param count quantity of items for FIFO
 * @return true if there is enough dynamic memory, false otherwise
 */
static inline void buff_print(void) {
    if (ascii_index > 0) {
        ascii_buff[ascii_index] = '\0';
        printf(ascii_buff); 
        ascii_index = 0;
    }
}

/*! \brief Insert a char in the buffered string.
 *  \ingroup main
 *
 * When the output is via USB CDC, the data is sent in packets of maximum 64 bytes every 1mS. 
 * As the decoding of the i2c frame is composed of more than one event (Start / Stop / Data) 
 * that are separated by a few uS, to optimize they are stored in buffer waiting for STOP, 
 * that the buffer is full, or that elapsed more than 100 uS since the last event.
 *
 * \param c char
 */
static inline void buff_putchar(char c) {
    // Reserve one byte for the NULL character.
    if (ascii_index >= sizeof(ascii_buff)-1) {
        buff_print();
    } 

    ascii_buff[ascii_index++] = c;
}

/*! \brief Decode and print the captured events.
 *  \ingroup main
 *
 * CPU1 blocks waiting for CPU0 to send the events captured by the sniffer.
 *
 * \return void.
 */
void core1_print() {
    uint32_t val;
#ifdef PRINT_HEX_INDEX
    uint32_t capture_index = 0;
#endif
    
    printf("i2c sniffer pico initialiced!\r\n");

    // Blocks the CPU waiting for FIFO captures from the core 0.
    while (true) {
        // It waits for the arrival of a new event for 100 uS, if it comes out due to timeout 
        // and there is a string stored in the buffer, it sends it.
        if (!multicore_fifo_pop_timeout_us(100, &val)) {
            buff_print();
        } else {
            gpio_put(led_pin, false);
            
            // The format of the uint32_t returned by the sniffer is composed of two event
            // code bits (EV1 = Bit12, EV0 = Bit11), and when it comes to data, the nine least
            // significant bits correspond to (ACK = Bit0), and the value 8 bits
            // where (B0 = Bit1 and B7 = Bit8).
            uint32_t ev_code = (val >> 10) & 0x03;
            uint8_t  data = ((val >> 1) & 0xFF);
            bool ack = (val & 1) ? false : true;
#ifdef PRINT_VAL
            printf("val: %x, ev_code: %x, data:%x, ack: %d \r\n", val, ev_code, data, ack);
#else
            if (ev_code == EV_START) {
#if defined(PRINT_TIME_T)
                printf("%010lu ", time_us_32());
#elif defined(PRINT_HEX_INDEX)
                printf("%08x ", capture_index++);
#endif
                buff_putchar('s');
            } else if (ev_code == EV_STOP) {
                buff_putchar('p');
                buff_putchar('\r');
                buff_putchar('\n');
                buff_print();
            } else if (ev_code == EV_DATA) {
                buff_putchar(nibble_to_hex(data>>4));
                buff_putchar(nibble_to_hex(data));
                buff_putchar(ack ? 'a' : 'n');
            } else {
                buff_putchar('u');
            }
#endif
            if (!ram_fifo_overflow) {
                gpio_put(led_pin, true);
            }
        }
    }
}

int main()
{
    // Full speed for the PIO clock divider
    float div = 1;
    PIO pio = pio0;
    uint32_t capture_val = 0;

    // The LED is used to indicate that the board has initialized successfully (ON), 
    // flashes when there is activity on the i2c bus, and turns off when it detects 
    // a RAM overflow.
    gpio_init(led_pin);
    gpio_set_dir(led_pin, GPIO_OUT);
    
    // Initialize chosen serial port
    stdio_init_all();

    // Initialize the four state machines that decode the i2c bus states.
    uint sm_main = pio_claim_unused_sm(pio, true);
    uint offset_main = pio_add_program(pio, &i2c_main_program);
    i2c_main_program_init(pio, sm_main, offset_main, div);

    uint sm_data = pio_claim_unused_sm(pio, true);
    uint offset_data = pio_add_program(pio, &i2c_data_program);
    i2c_data_program_init(pio, sm_data, offset_data, div);

    uint sm_start = pio_claim_unused_sm(pio, true);
    uint offset_start = pio_add_program(pio, &i2c_start_program);
    i2c_start_program_init(pio, sm_start, offset_start, div);

    uint sm_stop = pio_claim_unused_sm(pio, true);
    uint offset_stop = pio_add_program(pio, &i2c_stop_program);
    i2c_stop_program_init(pio, sm_stop, offset_stop, div);

    // Start running our PIO program in the state machine
    pio_sm_set_enabled(pio, sm_main, true);
    pio_sm_set_enabled(pio, sm_start, true);
    pio_sm_set_enabled(pio, sm_stop, true);
    pio_sm_set_enabled(pio, sm_data, true);

    multicore_launch_core1(core1_print);

    // Initialize the ram FIFO with a depth of 40K captures.
    if (!ram_fifo_init(40000)) {
        while(true);
    }
    
    gpio_put(led_pin, true);

    // CPU 0 captures the data generated by the i2c sniffer and uses the multicore 
    // FIFO (hardware) to send it to CPU 1, which is in charge of processing and printing 
    // it on the console. If the multicore Fifo is full, it stores them in the ram Fifo. 
    // All this without blocking to be able to attend the FIFO of the sniffer.
    while (true) {
        bool new_val = (pio_sm_get_rx_fifo_level(pio, sm_main) > 0);
        if (new_val) {
            capture_val = pio_sm_get(pio, sm_main);
        }
        // NOTE: activates a flag when it detects that RAM FIFO overflows
        if (multicore_fifo_wready()) {
            if (!ram_fifo_is_empty()) {
                if (new_val) {
                    if (!ram_fifo_set(capture_val) ){
                        ram_fifo_overflow = true;
                    }
                }
                capture_val = ram_fifo_get();
                new_val = true;
            } 
            if (new_val) {
                multicore_fifo_push_blocking(capture_val);
            }
        } else if (new_val) {
            if (!ram_fifo_set(capture_val)) {
                ram_fifo_overflow = true;
            }
        }
    }
}