/*
 * Copyright (c) 2021 Stefan Althöfer
 *
 * OWxxxx functions might fall under Maxxim copyriht.
 *
 * Demo code for PIO 1-Wire interface
 */

struct owobj 
{
    PIO    pio;       /* pio object (pio0/pio1) */
    uint   sm;        /* state machine number */
    uint   offset;    /* onewire code offset in PIO instr. memory */
    uint   pin;       /* Pin number for 1wire data signal */
    uint   dir;   /* Pin number for buffer manipulation */

    // Search state global variables
    unsigned char ROM_NO[8];
    int LastDiscrepancy;
    int LastFamilyDiscrepancy;
    int LastDeviceFlag;
    unsigned char crc8;
};
void onewire_init(PIO pio, uint sm, uint pin, uint dir);
void onewire_cleanup(void);
void pio_sm_trace(PIO pio, uint sm, uint usleep);
void onewire_set_fifo_thresh(uint thresh);
int onewire_reset(void);
void onewire_wait_for_idle(void);
void onewire_tx_byte(uint byte);
void onewire_tx_byte_spu(uint byte);
void onewire_end_spu(void);
uint onewire_rx_byte(void);
void onewire_triplet(
    int *id_bit,
    int *cmp_id_bit,
    unsigned char *search_direction
);
unsigned char calc_crc8_buf(unsigned char *data, int len);
int onewire_select( 
    unsigned char *romid
);
unsigned char calc_crc8(unsigned char data);
int OWSearch(struct owobj *search_owobj);
int OWSearchReset(struct owobj *search_owobj);
int OWFirst(struct owobj *search_owobj);
int OWNext(struct owobj *search_owobj);
void onewire_test_ds18b20_scratchpad(void);
uint32_t onewire_test_ds18b20_conversion(void);
void onewire_test_spu(void);
void onewire_test_wordlength(void);
