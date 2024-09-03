struct _pio_config{
    PIO pio;
    int sm;
    uint offset;
    const struct pio_program* program;
};