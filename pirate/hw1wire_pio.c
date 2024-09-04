/*
 * Copyright (c) 2021 Stefan Althöfer
 * SPDX-License-Identifier: BSD-3-Clause (as indicated in the PIO file)
 * https://github.com/stefanalt/RP2040-PIO-1-Wire-Master
 * 
 * 1-Wire is a tradmark of Maxim Integrated
 * 2023 Modified by Ian Lesnet for Bus Pirate 5 buffered IO
 *
 * OWxxxx functions might fall under Maxxim copyriht.
 *
 * Demo code for PIO 1-Wire interface
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
//#include "hardware/gpio.h"
//#include "hardware/pio.h"
//#include "pico/binary_info.h"
#include "hardware/clocks.h"
#include "hw1wire.pio.h"
#include "pirate/hw1wire_pio.h"

struct owobj owobj;

void onewire_init(uint pin, uint dir){
    //owobj.pio = pio;
    //owobj.sm = sm;
    owobj.pin = pin;
    owobj.dir = dir;
    //bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&onewire_program, &owobj.pio, &owobj.sm, &owobj.offset, dir, 9, true);
    //hard_assert(success);
    owobj.pio = PIO_MODE_PIO;
    owobj.sm = 0;
    owobj.offset = pio_add_program(owobj.pio, &onewire_program);
    #ifdef BP_PIO_SHOW_ASSIGNMENT
    printf("pio %d, sm %d, offset %d\n", PIO_NUM(owobj.pio), owobj.sm, owobj.offset);
    #endif
    //gpio_set_function(owobj.dir, GPIO_FUNC_PIO1); 
    onewire_program_init(owobj.pio, owobj.sm, owobj.offset, owobj.pin, owobj.dir);
    onewire_set_fifo_thresh(8);
    pio_sm_set_enabled(owobj.pio, owobj.sm, true);
}

void onewire_cleanup(void){
    //pio_remove_program_and_unclaim_sm(&onewire_program, owobj.pio, owobj.sm, owobj.offset);
    pio_remove_program(owobj.pio, &onewire_program, owobj.offset);
}
 
void onewire_set_fifo_thresh(uint thresh) {
    PIO pio = owobj.pio;
    uint sm = owobj.sm;
    uint offset = owobj.offset;
    uint pin =  owobj.pin;
    uint old, new;
    uint8_t waiting_addr;
    uint need_restart;
    int timeout;
    
    if( thresh >= 32 ){
        thresh = 0;
    }

    old  = pio->sm[sm].shiftctrl;
    old &= PIO_SM0_SHIFTCTRL_PUSH_THRESH_BITS |
        PIO_SM0_SHIFTCTRL_PULL_THRESH_BITS;

    new  = ((thresh & 0x1fu) << PIO_SM0_SHIFTCTRL_PUSH_THRESH_LSB) |
        ((thresh & 0x1fu) << PIO_SM0_SHIFTCTRL_PULL_THRESH_LSB);

    if( old != new ){
        need_restart = 0;
        if( pio->ctrl & (1u<<sm) ){
            /* If state machine is enabled, it must be disabled
               and restarted when we change fifo thresholds,
               or evil things happen */

            /* When we attempt fifo threshold switching, we assume
               that all fifo operations have been done and hence
               all bits have been almost processed, but the
               state machine might not have reached the wating state
               as it still does some delays to ensure timing for
               the very last bit (Similar for reset).
               Just wait for the 'wating' state to be reached */
            waiting_addr = offset+onewire_offset_waiting;
            timeout = 2000;
            while( pio_sm_get_pc(pio, sm) != waiting_addr ){
                sleep_us(1);
                if( timeout-- < 0 ){
                    /* FIXME: do something clever in case of
                       timeout */
                }
            }

            pio_sm_set_enabled(pio, sm, false);
            need_restart = 1;
        }

        hw_write_masked(&pio->sm[sm].shiftctrl, new,
                        PIO_SM0_SHIFTCTRL_PUSH_THRESH_BITS |
                        PIO_SM0_SHIFTCTRL_PULL_THRESH_BITS);

        if( need_restart ){
            pio_sm_restart(pio, sm);
            pio_sm_set_enabled(pio, sm, true);
        }
    }
}

int onewire_reset(void) {
    int ret;
    PIO pio = owobj.pio;
    uint sm = owobj.sm;
    uint offset = owobj.offset;
    uint pin =  owobj.pin;
    uint8_t waiting_addr;
    int timeout;
    uint div;
    
    /* Switch to slow timing for reset */
    div = clock_get_hz(clk_sys)/1000000 * 70;
    pio_sm_set_clkdiv_int_frac(pio, sm, div, 0);
    pio_sm_clkdiv_restart(pio, sm);
    onewire_set_fifo_thresh(1);

    //onewire_do_reset(pio, sm, offset);
    pio_sm_exec(pio, sm, pio_encode_jmp(offset + onewire_offset_reset));
    while( pio_sm_get_rx_fifo_level(pio, sm) == 0 )
        /* wait */;
    ret = ((pio_sm_get(pio, sm) & 0x80000000) == 0 );
    
    /* when rx fifo has filled we still need to wait for
       the remaineder of the reset to execute before we
       can manipulate the clkdiv.
       Just wait until we reach the waiting state */
    waiting_addr = offset+onewire_offset_waiting;
    timeout = 2000;
    while( pio_sm_get_pc(pio, sm) != waiting_addr ){
        sleep_us(1);
        if( timeout-- < 0 ){
            /* FIXME: do something clever in case of
               timeout */
        }
    }

    /* Restore normal timing */
    div = clock_get_hz(clk_sys)/1000000 * 3;
    pio_sm_set_clkdiv_int_frac(pio, sm, div, 0);
    pio_sm_clkdiv_restart(pio, sm);

    return ret; // 1=detected, 0=not
}

/* Wait for idle state to be reached. This is only
   useful when you know that all but the last bit
   have been processe (after having checked fifos) */
void onewire_wait_for_idle(void) {
    int ret;
    PIO pio = owobj.pio;
    uint sm = owobj.sm;
    uint offset = owobj.offset;
    uint8_t waiting_addr;
    int timeout;
    
    /* when rx fifo has filled the bit timing has not
       fully completed.
       Just wait until we reach the waiting state */
    waiting_addr = offset+onewire_offset_waiting;
    timeout = 2000;
    while( pio_sm_get_pc(pio, sm) != waiting_addr ){
        sleep_us(1);
        if( timeout-- < 0 ){
            /* FIXME: do something clever in case of
               timeout */
        }
    }

    return;
}

/* Transmit a byte */
void onewire_tx_byte(uint byte){
    PIO pio = owobj.pio;
    uint sm = owobj.sm;
    uint offset = owobj.offset;
    uint pin =  owobj.pin;

    onewire_set_fifo_thresh(8);
    pio->txf[sm] = byte;
    while( pio_sm_get_rx_fifo_level(pio, sm) == 0 )
        /* wait */;
    pio_sm_get(pio, sm); /* read to drain RX fifo */
    return;
}

/* Receive a byte */
uint onewire_rx_byte(void){
    PIO pio = owobj.pio;
    uint sm = owobj.sm;
    uint offset = owobj.offset;
    uint pin =  owobj.pin;

    onewire_set_fifo_thresh(8);
    pio->txf[sm] = 0xff;
    while( pio_sm_get_rx_fifo_level(pio, sm) == 0 )
        /* wait */;
    /* Returned byte is in 31..24 of RX fifo! */
    return (pio_sm_get(pio, sm) >> 24) & 0xff;
}

/* Do a ROM search triplet.
   Receive two bits and store the read values to
   id_bit and cmp_id_bit respectively.
   Then transmit a bit with this logic:
     id_bit | cmp_id_bit | tx-bit
          0 |          1 |      0
          1 |          0 |      1
          0 |          0 | search_direction     
          1 |          1 |      1
    The actually transmitted bit is returned via search_direction.
    
    Refer to MAXIM APPLICATION NOTE 187 "1-Wire Search Algorithm"
 */
void onewire_triplet(
    int *id_bit,
    int *cmp_id_bit,
    unsigned char *search_direction
){
    PIO pio = owobj.pio;
    uint sm = owobj.sm;
    uint offset = owobj.offset;
    uint pin =  owobj.pin;
    uint fiforx;

    onewire_set_fifo_thresh(2);
    pio->txf[sm] = 0x3;
    while( pio_sm_get_rx_fifo_level(pio, sm) == 0 )
        /* wait */;

    fiforx = pio_sm_get(pio, sm);
    *id_bit = (fiforx>>30) & 1;
    *cmp_id_bit = (fiforx>>31) & 1;
    if( (*id_bit == 0) && (*cmp_id_bit == 1) ){
        *search_direction = 0;
    } else if( (*id_bit == 1) && (*cmp_id_bit == 0) ){
        *search_direction = 1;
    } else if( (*id_bit == 0) && (*cmp_id_bit == 0) ){
        /* do not change search direction */
    } else {
        *search_direction = 1;
    }

    onewire_set_fifo_thresh( 1);
    pio->txf[sm] = *search_direction;
    while( pio_sm_get_rx_fifo_level(pio, sm) == 0 )
        /* wait */;
    fiforx = pio_sm_get(pio, sm);
    return;
}

unsigned char calc_crc8_buf(unsigned char *data, int len){
    int            i,j;
    unsigned char  crc8;

    // See Application Note 27
    crc8 = 0;
    for(j=0; j<len; j++){
        crc8 = crc8 ^ data[j];
        for (i = 0; i < 8; ++i){
            if (crc8 & 1)
                crc8 = (crc8 >> 1) ^ 0x8c;
            else
                crc8 = (crc8 >> 1);
        }
    }

    return crc8;
}

/* Select a device by ROM ID */
int onewire_select(
    unsigned char *romid
){
    int    i;
    
    if( ! onewire_reset() ){
        return 0;
    }
    onewire_tx_byte(0x55); // Match ROM command
    for(i=0; i<8; i++){
        onewire_tx_byte(romid[i]);
    }
    return 1;
}

/* This is code stolen from MAXIM AN3684, slightly modified
   to interface to the PIO onewire and to eliminate global
   variables. */

#define TRUE    1
#define FALSE   0

typedef unsigned char byte;

//--------------------------------------------------------------------------
// Calculate the CRC8 of the byte value provided with the current 
// global 'crc8' value. 
// Returns current global crc8 value
//
unsigned char calc_crc8(unsigned char data){
    int i; 

    // See Application Note 27
    owobj.crc8 = owobj.crc8 ^ data;
    for (i = 0; i < 8; ++i){
        if ( owobj.crc8 & 1)
            owobj.crc8 = ( owobj.crc8 >> 1) ^ 0x8c;
        else
            owobj.crc8 = ( owobj.crc8 >> 1);
    }

    return owobj.crc8;
}

//--------------------------------------------------------------------------
// The 'OWSearch' function does a general search.  This function
// continues from the previous search state. The search state
// can be reset by using the 'OWFirst' function.
//
// Returns:   TRUE (1) : when a 1-Wire device was found and its
//                       Serial Number placed in the global ROM 
//            FALSE (0): when no new device was found.  Either the
//                       last search was the last device or there
//                       are no devices on the 1-Wire Net.
//
int OWSearch(struct owobj *search_owobj){
    int id_bit_number;
    int last_zero, rom_byte_number, search_result;
    int id_bit, cmp_id_bit;
    unsigned char rom_byte_mask, search_direction, status;

    // initialize for search
    id_bit_number = 1;
    last_zero = 0;
    rom_byte_number = 0;
    rom_byte_mask = 1;
    search_result = FALSE;
    search_owobj->crc8 = 0;

    // if the last call was not the last one
    if (! search_owobj->LastDeviceFlag){       
        // 1-Wire reset
        if( ! onewire_reset() ){
            // reset the search
            search_owobj->LastDiscrepancy = 0;
            search_owobj->LastDeviceFlag = FALSE;
            search_owobj->LastFamilyDiscrepancy = 0;
            return FALSE;
        }

        // issue the search command
        onewire_tx_byte(0xf0);

        // loop to do the search
        do{
            // if this discrepancy if before the Last Discrepancy
            // on a previous next then pick the same as last time
            if (id_bit_number < search_owobj->LastDiscrepancy){
                if (( search_owobj->ROM_NO[rom_byte_number] & rom_byte_mask) > 0)
                    search_direction = 1;
                else
                    search_direction = 0;
            }else{
                // if equal to last pick 1, if not then pick 0
                if (id_bit_number == search_owobj->LastDiscrepancy)
                    search_direction = 1;
                else
                    search_direction = 0;
            }

            // Perform a triple operation on the DS2482 which will perform 2 read bits and 1 write bit
            onewire_triplet(&id_bit, &cmp_id_bit, &search_direction);

            // check for no devices on 1-Wire
            if ((id_bit) && (cmp_id_bit))
                break;
            else{
                if ((!id_bit) && (!cmp_id_bit) && (search_direction == 0)){
                    last_zero = id_bit_number;

                    // check for Last discrepancy in family
                    if (last_zero < 9)
                        search_owobj->LastFamilyDiscrepancy = last_zero;
                }

                // set or clear the bit in the ROM byte rom_byte_number
                // with mask rom_byte_mask
                if (search_direction == 1)
                    search_owobj->ROM_NO[rom_byte_number] |= rom_byte_mask;
                else
                    search_owobj->ROM_NO[rom_byte_number] &= (byte)~rom_byte_mask;

                // increment the byte counter id_bit_number
                // and shift the mask rom_byte_mask
                id_bit_number++;
                rom_byte_mask <<= 1;

                // if the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask
                if (rom_byte_mask == 0)
                {
                    calc_crc8( search_owobj->ROM_NO[rom_byte_number]);  // accumulate the CRC
                    rom_byte_number++;
                    rom_byte_mask = 1;
                }
            }
        }
        while(rom_byte_number < 8);  // loop until through all ROM bytes 0-7

        // if the search was successful then
        if (!((id_bit_number < 65) || ( search_owobj->crc8 != 0))){
            // search successful so set LastDiscrepancy,LastDeviceFlag,search_result
            search_owobj->LastDiscrepancy = last_zero;

            // check for last device
            if ( search_owobj->LastDiscrepancy == 0)
                search_owobj->LastDeviceFlag = TRUE;

            search_result = TRUE;
        }
    }

    // if no device found then reset counters so next 'search' will be like a first
    if (!search_result || ( search_owobj->ROM_NO[0] == 0)){
        search_owobj->LastDiscrepancy = 0;
        search_owobj->LastDeviceFlag = FALSE;
        search_owobj->LastFamilyDiscrepancy = 0;
        search_result = FALSE;
    }

    return search_result;
}

int OWSearchReset(struct owobj *search_owobj){
    // reset the search state
    search_owobj->LastDiscrepancy = 0;
    search_owobj->LastDeviceFlag = FALSE;
    search_owobj->LastFamilyDiscrepancy = 0;
}

//--------------------------------------------------------------------------
// Find the 'first' devices on the 1-Wire network
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : no device present
//
int OWFirst(struct owobj *search_owobj){
    // reset the search state
    search_owobj->LastDiscrepancy = 0;
    search_owobj->LastDeviceFlag = FALSE;
    search_owobj->LastFamilyDiscrepancy = 0;
    return OWSearch(search_owobj);
}

//--------------------------------------------------------------------------
// Find the 'next' devices on the 1-Wire network
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : device not found, end of search
//
int OWNext(struct owobj *search_owobj){
    // leave the search state alone
    return OWSearch(search_owobj);
}

/* End of MAXIM AN3684 code */

/* ROM Search test */
// This is now in the commands/1wire/scan.c
#if 0 
void onewire_test_romsearch(void){
    int i;
    int ret;
    int id_bit, cmp_id_bit;
    unsigned char search_direction;
    PIO pio = owobj.pio;
    uint sm = owobj.sm;
    uint offset = owobj.offset;
    uint pin =  owobj.pin;
    uint dir =  owobj.dir;
    int devcount;
    
    //onewire_program_init(pio, sm, offset, pin, dir);
    //pio_sm_set_enabled(pio, sm, true);

#if 0
    /* Simple test for onewire_triplet */
    if( ! onewire_reset() ){
        printf("No Device\n");
        return;
    }

    onewire_tx_byte( 0xf0); // ROM Search
    sleep_us(120);

    search_direction = 0;
    for(i=0; i<64; i++){
        onewire_triplet( &id_bit, &cmp_id_bit, &search_direction);
        printf("%d: %d\n", i, id_bit);
    }
#endif

#if 1
    /* Full flegged romsearch */
    printf("1-Wire ROM search:\r\n");
    ret = OWFirst();
    devcount = 0;
    while( ret == TRUE ){
        devcount++;
        printf("%d:", devcount);
        for(i=0; i<8; i++){
            printf(" %.2x", owobj.ROM_NO[i]);
        }
        printf(" (");
        DS1wireID( owobj.ROM_NO[0]);
        printf(")\r\n");
        ret = OWNext();
    }
    if( devcount == 0 ){
        printf("No devices found\r\n");
    }
#endif
}
#endif

/* Simple test with single DS18B20
   Configure, start conversion and read temperature.
   Assume phantom power. */
#if 0 // This is now in the commands/1wire/demos.c
uint32_t onewire_test_ds18b20_conversion(void){
    int  i;
    unsigned char buf[9];
    int32_t temp;


    if(!onewire_reset() ){
        return 1;
    }

    onewire_tx_byte( 0xcc); // Skip ROM command
    onewire_tx_byte( 0x4e); // Write Scatchpad
    onewire_tx_byte( 0x00); // TH
    onewire_tx_byte( 0x00); // TL
    onewire_tx_byte( 0x7f); // CONF (12bit)
    onewire_wait_for_idle(); // added: need to wait or the transmission isn't done before the reset

    if(!onewire_reset()){
        return 1;
    }

    onewire_tx_byte( 0xcc); // Skip ROM command
    onewire_tx_byte( 0x44); // Convert T
    sleep_ms(800);  /* 12bit: 750 ms */

    printf("RX:");

    if(!onewire_reset()){
        return 1;
    }
    
    onewire_tx_byte( 0xcc); // Skip ROM command
    onewire_tx_byte( 0xbe); // Read Scatchpad
    for(i=0; i<9; i++){
        buf[i] = onewire_rx_byte();
        printf(" %.2x", buf[i]);
    }
    
    if(calc_crc8_buf(buf, 8) != buf[8]){
        printf("\r\nCRC Fail\r\n");
        return 0; //return 0 to avoid device not found message
    }

    temp = buf[0]|(buf[1]<<8);
    if( temp & 0x8000 ){
        temp |= 0xffff0000;
    }
    printf("\r\nTemperature: %.3f\r\n", (float)temp/16);

    return 0;
}
#endif
/* onewire_temp_app: Large scale example.
 *
 * Assumes: Only DS18B20 (or compatible) sensors on the 1-Wire and
 * they are using phantom power (external strong pullup via MOSFET).
 *
 * Scan all 1-Wire buses for all devices and convert all temperatures
 * periodically. While scanning temperatures, do a "background" scan
 * to check for removed are added sensors.
 *
 * The code assumes that the T_H register is programmed with a "unique"
 * 8-bit identification number for each sensor, so sensors can be
 * distinguished easily (without knowing their actual ROM-ID).
 * Remember that T_H is backed-up in EEPROM.
 * 
 * Output format:
 * || LOOP || N ||  T-ID ||  T-ID ||
 * |  1234 |  2 | -99.87 | 103.54 |
 *
 * N is the numer of sensors found and is added for convenience to easyly
 * check sensor setup.
 */

/* number for 1wire channels for this test */
#define N_CH       2 
/* max number of devices for one 1 wire channel */
#define MAX_TDEV  64  

#define MAX_LINE_LENGTH ((MAX_TDEV)*9+2)
char hd_buf[MAX_LINE_LENGTH];
char dt_buf[MAX_LINE_LENGTH];
int  hd_pos;
int  dt_pos;

struct tdev {
    unsigned char rom_id[8];
    unsigned char dev_id;
    int32_t       temp;
    int           fail;     /* 0: ok, 1: invalid temperature */
};

struct ch_obj {
    struct tdev tdevs[MAX_TDEV]; /* active devices */
    struct tdev sdevs[MAX_TDEV]; /* searching devices */
    int         spos;            /* search position, -1 if search is done */
};

struct ch_obj ch_objs[N_CH];
#if 0
void onewire_temp_app(
    struct owobj *owobj1,
    struct owobj *owobj2
){
    int i;
    int n;
    int c;
    int t;
    int s_cnt;
    int s_id;
    int ret;
    unsigned char buf[9];
    int32_t temp;
    int fail;
    int num_sensors;
    int searching;
    int loop;

    struct owobj * owobjs[N_CH];
    owobjs[0] = owobj1;
    owobjs[1] = owobj2;

    /* Init all 1-Wire channels */
    for(c=0; c<N_CH; c++){
        onewire_program_init(owobjs[c]->pio, owobjs[c]->sm,
                             owobjs[c]->offset, owobjs[c]->pin,
                             owobjs[c]->dir);
        pio_sm_set_enabled(owobjs[c]->pio, owobjs[c]->sm, true);
        for(t=0; t<MAX_TDEV; t++){
            memset(&ch_objs[c].tdevs[t].rom_id[0], 0, 8);
        }
    }

    /* Initially scan for devices */
    for(c=0; c<N_CH; c++){
        ret = OWFirst(owobjs[c]);
        t = 0;
        while( ret == TRUE ){
            memcpy(ch_objs[c].tdevs[t].rom_id,
                   owobjs[c]->ROM_NO, 8);

            /* Read sensor ID (stored in T_H byte of all sensors) */
            onewire_tx_byte(owobjs[c], 0xbe); // Read Scatchpad
            for(i=0; i<9; i++){
                buf[i] = onewire_rx_byte(owobjs[c]);
            }
            /* FIXME: Should check CRC */
            ch_objs[c].tdevs[t].dev_id = buf[2];

            t++;
            ret = OWNext(owobjs[c]);
        }
    }

    /* Prepare for background searching of devices */
    for(c=0; c<N_CH; c++){
        OWSearchReset(owobjs[c]);
        ch_objs[c].spos = 0;
        for(t=0; t<MAX_TDEV; t++){
            memset(&ch_objs[c].sdevs[t].rom_id[0], 0, 8);
        }
    }

#if 0
    /* Print list of sensors */
    for(c=0; c<N_CH; c++){
        for(t=0; t<MAX_TDEV; t++){
            if( ch_objs[c].tdevs[t].rom_id[0] == 0x28 ){
                printf("%d,%d:", c, t);
                for(i=0; i<8; i++){
                    printf(" %.2x", ch_objs[c].tdevs[t].rom_id[i]);
                }
                printf(" (%2d)", ch_objs[c].tdevs[t].dev_id);
                printf("\n");
            } else {
                continue;
            }
        }
    }
#endif

    /* Scan temperatures forever */
    loop = 0;
    while( 1 ){
        loop++;

        /* Convert temperatures */
        for(c=0; c<N_CH; c++){
            if( ! onewire_reset(owobjs[c]) ){
                break;
            }
            onewire_tx_byte(owobjs[c], 0xcc); // Skip ROM command
            onewire_tx_byte_spu(owobjs[c], 0x44); // Convert T
        }
        sleep_ms(760);  /* 12bit: max. 750 ms */
        for(c=0; c<N_CH; c++){
            onewire_end_spu(owobjs[c]);
        }

        /* Read values */
        num_sensors = 0;
        for(c=0; c<N_CH; c++){
            for(t=0; t<MAX_TDEV; t++){
                if( ch_objs[c].tdevs[t].rom_id[0] != 0x28 ){
                    continue;
                }

                num_sensors++;
                ch_objs[c].tdevs[t].fail = 0;
                if( onewire_select(owobjs[c],
                                   ch_objs[c].tdevs[t].rom_id) == 0 )
                {
                    ch_objs[c].tdevs[t].fail++;
                }

                if(ch_objs[c].tdevs[t].fail == 0 ){
                    onewire_tx_byte(owobjs[c], 0xbe); // Read Scatchpad
                    for(i=0; i<9; i++){
                        buf[i] = onewire_rx_byte(owobjs[c]);
                    }
                    if( calc_crc8_buf(buf, 8) != buf[8] ){
                        /* CRC error */
                        ch_objs[c].tdevs[t].fail++;
                    } else {
                        temp = buf[0]|(buf[1]<<8);
                        if( temp & 0x8000 ){
                            temp |= 0xffff0000;
                        }
                        ch_objs[c].tdevs[t].temp = temp;
                    }
                }
            }
        }

        /* Print them all with a very brute force sorting */
        hd_buf[0] = '\0';
        dt_buf[0] = '\0';
        hd_pos = 0;
        dt_pos = 0;
        s_id = 0;
        s_cnt = 0;
        /* Search until we either have found all sensors
           or have passed the maximum possible sensor id */
        while( s_cnt<num_sensors && s_id < 256 ){
            for(c=0; c<N_CH; c++){
                for(t=0; t<MAX_TDEV; t++){
                    if( ch_objs[c].tdevs[t].rom_id[0] != 0x28 ){
                        continue;
                    }
                    if( ch_objs[c].tdevs[t].dev_id == s_id ){
                        /* 9 chars per sensors            123456789 */
                        hd_pos += sprintf(hd_buf+hd_pos, "||   %3d ", 
                                          ch_objs[c].tdevs[t].dev_id);
                        if( ch_objs[c].tdevs[t].fail ){
                            dt_pos += sprintf(dt_buf+dt_pos, "|    NaN ");
                        } else {
                            temp = ch_objs[c].tdevs[t].temp;
                            dt_pos += sprintf(dt_buf+dt_pos, "| %6.2f ", 
                                              (float)temp/16);
                        }
                        s_cnt++; /* one found */
                        goto next_search;
                    }
                }
            }
        next_search:
            s_id++;
        }

        /* Print what we have */
        printf("|| LOOP || N %s ||\n", hd_buf);
        printf("| %5d |%3d %s |\n", loop & 0xffff, num_sensors, dt_buf);

        /* Now scan 1-Wire to check for change in devices.
           Note: One ROM search takes about 15 ms.
           To avoid sacrifying scan time for large sensor counts, we
           only scan for three devices in one scan loop. */
        searching = 0; /* set if any device is still in search */
        for(c=0; c<N_CH; c++){
            for(n=0; n<3; n++){
                if( ch_objs[c].spos >= 0 ){
                    searching++;
                    ret = OWNext(owobjs[c]);
                    if( ret == FALSE ){
                        ch_objs[c].spos = -1;
                    } else {
                        memcpy(ch_objs[c].sdevs[ch_objs[c].spos].rom_id,
                               owobjs[c]->ROM_NO, 8);

                        /* Read sensor ID (stored in T_H byte of all sensors) */
                        onewire_tx_byte(owobjs[c], 0xbe); // Read Scatchpad
                        for(i=0; i<9; i++){
                            buf[i] = onewire_rx_byte(owobjs[c]);
                        }
                        /* FIXME: Should check CRC */
                        ch_objs[c].sdevs[ch_objs[c].spos].dev_id = buf[2];

                        ch_objs[c].spos++;
                    }
                }
            }
        }

        if( searching == 0 ){
#if 0
            /* Print list of found sensors */
            for(c=0; c<N_CH; c++){
                for(t=0; t<MAX_TDEV; t++){
                    if( ch_objs[c].sdevs[t].rom_id[0] == 0x28 ){
                        printf("%d,%d:", c, t);
                        for(i=0; i<8; i++){
                            printf(" %.2x", ch_objs[c].sdevs[t].rom_id[i]);
                        }
                        printf(" (%2d)", ch_objs[c].sdevs[t].dev_id);
                        printf("\n");
                    } else {
                        continue;
                    }
                }
            }
#endif

            /* Copy over from sdevs to tdevs. This is possible as
               tdevs does not store data which must persist over
               on scan loop. */
            for(c=0; c<N_CH; c++){
                memcpy(ch_objs[c].tdevs,
                       ch_objs[c].sdevs,
                       sizeof(ch_objs[c].tdevs));
            }
            
            /* Reset for next background search */
            for(c=0; c<N_CH; c++){
                OWSearchReset(owobjs[c]);
                ch_objs[c].spos = 0;
                for(t=0; t<MAX_TDEV; t++){
                    memset(&ch_objs[c].sdevs[t].rom_id[0], 0, 8);
                }
            }
        }

    }
}
#endif

