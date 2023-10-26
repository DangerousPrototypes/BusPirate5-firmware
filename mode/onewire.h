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
void onewire_init(struct owobj *owobj);
void onewire_test_romsearch(struct owobj *owobj);
void pio_sm_trace(PIO pio, uint sm, uint usleep);
void onewire_set_fifo_thresh(struct owobj *owobj, uint thresh);
int onewire_reset(struct owobj *owobj);
void onewire_wait_for_idle(struct owobj *owobj);
void onewire_tx_byte(struct owobj *owobj, uint byte);
void onewire_tx_byte_spu(struct owobj *owobj, uint byte);
void onewire_end_spu(struct owobj *owobj);
uint onewire_rx_byte(struct owobj *owobj);
void onewire_triplet(
    struct owobj *owobj,
    int *id_bit,
    int *cmp_id_bit,
    unsigned char *search_direction
);
unsigned char calc_crc8_buf(unsigned char *data, int len);
int onewire_select(
    struct owobj  *owobj, 
    unsigned char *romid
);
unsigned char calc_crc8(struct owobj *owobj, unsigned char data);
int OWSearch(struct owobj *owobj);
int OWSearchReset(struct owobj *owobj);
int OWFirst(struct owobj *owobj);
int OWNext(struct owobj *owobj);
void onewire_test_ds18b20_scratchpad(struct owobj *owobj);
void onewire_test_ds18b20_conversion(struct owobj *owobj);
void onewire_test_spu(struct owobj *owobj);
void onewire_test_wordlength(struct owobj *owobj);
void onewire_test_romsearch(struct owobj *owobj);
void onewire_temp_app(
    struct owobj *owobj1,
    struct owobj *owobj2
);