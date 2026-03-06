/**
 * @file hardfault.c
 * @brief HardFault crash handler with RGB LED visual feedback.
 * @details Overrides the weak isr_hardfault from pico_crt0.
 *          On crash, turns all RGB LEDs solid red, then blinks them
 *          to signal a fault condition. Uses direct PIO register writes
 *          with no heap/stack dependencies beyond what the exception
 *          entry already provides.
 */

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/structs/pio.h"
#include "pirate.h"

/* WS2812 GRB pixel format, shifted left 8 bits for PIO */
#define FAULT_COLOR_RED   ((uint32_t)0x00FF0000u)  /* G=0x00, R=0xFF, B=0x00 */
#define FAULT_COLOR_BLACK ((uint32_t)0x00000000u)

/**
 * @brief Busy-wait delay loop (no systick/timer dependency).
 * @param cycles  Approximate number of loop iterations (~3 CPU cycles each at 125 MHz)
 */
static void __attribute__((noinline)) fault_delay(volatile uint32_t cycles) {
    while (cycles--) {
        __asm volatile("" ::: "memory");
    }
}

/**
 * @brief Push a color to all LEDs via PIO TX FIFO.
 * @param pio    PIO hardware instance
 * @param sm     State machine index
 * @param color  GRB pixel value (already in PIO format)
 * @param count  Number of LEDs
 */
static void fault_push_pixels(pio_hw_t *pio, uint sm, uint32_t color, uint count) {
    for (uint i = 0; i < count; i++) {
        /* Spin until TX FIFO has space */
        while (pio->fstat & (1u << (PIO_FSTAT_TXFULL_LSB + sm))) {
            /* tight loop */
        }
        pio->txf[sm] = color;
    }
}

/**
 * @brief HardFault handler core logic — blinks LEDs red forever.
 * @details Called from the naked isr_hardfault wrapper.
 *          This function never returns.
 */
static void __attribute__((noreturn, used)) hardfault_handler_c(void) {
    /* Abort any in-flight RGB DMA to avoid bus contention on the PIO TX FIFO.
     * We iterate all channels since we don't know which one rgb.c claimed. */
    for (uint ch = 0; ch < NUM_DMA_CHANNELS; ch++) {
        dma_hw->abort = (1u << ch);
    }
    /* Wait for aborts to complete */
    while (dma_hw->abort) {
        /* tight loop */
    }

    pio_hw_t *pio = PIO_RGB_LED_PIO;
    uint sm = PIO_RGB_LED_SM;
    uint led_count = RGB_LEN;

    /* ~200 ms on / ~200 ms off blink loop at ~125MHz */
    const uint32_t delay_on  = 4000000u;
    const uint32_t delay_off = 4000000u;

    for (;;) {
        /* Red ON */
        fault_push_pixels(pio, sm, FAULT_COLOR_RED, led_count);
        /* Wait for WS2812 reset (>280 µs) plus visible on-time */
        fault_delay(delay_on);

        /* OFF */
        fault_push_pixels(pio, sm, FAULT_COLOR_BLACK, led_count);
        fault_delay(delay_off);
    }
}

/**
 * @brief HardFault exception entry point — overrides the weak SDK default.
 * @details Naked function: no prologue/epilogue, just branches to the C handler.
 *          The ARM exception entry has already pushed the exception frame.
 *          We use the existing MSP since the exception entry guarantees at least
 *          the 32-byte exception frame is valid. The C handler uses minimal stack.
 */
void __attribute__((naked)) isr_hardfault(void) {
    __asm volatile(
        "ldr r0, =%c0 \n"
        "bx  r0       \n"
        :
        : "i" (hardfault_handler_c)
    );
}
