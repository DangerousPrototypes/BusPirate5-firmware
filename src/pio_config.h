struct _pio_config {
    PIO pio;
    uint sm;
    uint offset;
    const struct pio_program* program;
};

static inline bool pio_sm_check_idle (PIO pio, int sm) {
    // Correct way to detect the state machine is idle because of an empty FIFO:
    // 1. Clear the (latching) state machine stall bit
    // 2. Check if the state machine TX fifo is empty
    // 3. THEN check if the state machine stall bit is set
    uint32_t SM_STALL_MASK = 1u << (PIO_FDEBUG_TXSTALL_LSB + sm);
    pio->fdebug = SM_STALL_MASK; // writing a 1 clears the bit

    // NOTE: the order of these operations *DOES* matter!
    bool result =
        pio_sm_is_tx_fifo_empty(pio, sm) &&
        (pio->fdebug & SM_STALL_MASK);
    return result;
}

static inline bool pio_sm_wait_idle (PIO pio, int sm, uint32_t timeout) {
    // Wait for the state machine to become idle
    while (!pio_sm_check_idle(pio, sm)) {
        timeout--;
        if (timeout == 0) {
            return false;
        }
    }
    return true;
}