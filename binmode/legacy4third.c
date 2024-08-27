/*
 * This file is part of the Bus Pirate project (http://code.google.com/p/the-bus-pirate/).
 *
 * Written and maintained by the Bus Pirate project.
 *
 * To the extent possible under law, the project has
 * waived all copyright and related or neighboring rights to Bus Pirate. This
 * work is published from United States.
 *
 * For details see: http://creativecommons.org/publicdomain/zero/1.0/.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

/* Legacy Binary Mode for third parties */


/* ************************
WARNING: It's very easy to break this code if you don't know what you're doing. 
There are things that might seem unnecessary, but they're not! Be very careful!
************************ */

// VERY EXPERIMENTAL & BETA


//#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "queue.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "pirate.h"
#include "opt_args.h" //needed for same reason as bytecode and needs same fix
#include "bytecode.h"
#include "modes.h"
#include "binio_helpers.h"
#include "tusb.h"
#include "binmode/binio.h"
#include "system_config.h"
#include "ui/ui_prompt.h"
#include "ui/ui_term.h"
#include "pirate/hwspi.h"
#include "pirate/mem.h"

const char legacy4third_mode_name[]="Legacy Binary Mode for Flashrom and AVRdude (EXPERIMENTAL)";

#define TMPBUFF_SIZE 0x4000
#define CDCBUFF_SIZE 0x4000
#define DEFAULT_MAX_TRIES 100000
#define CDC_SEND_STR(cdc_n, str) tud_cdc_n_write(cdc_n, (uint8_t*)str, sizeof(str) - 1); tud_cdc_n_write_flush(1);

static uint8_t volts_integer;
static uint8_t volts_decimal;
static uint16_t current_integer;
static uint8_t current_decimal;
static uint8_t* tmpbuf;
static uint8_t* cdc_buff;
static uint32_t remain_bytes;


void disable_psu_legacy(void)
{
    uint8_t binmode_args = 0x00;
    
    uint32_t result = binmode_psu_disable(&binmode_args);
}

void setup_spi_legacy(uint32_t spi_speed, uint8_t data_bits, uint8_t cpol, uint8_t cpah, uint8_t cs)
{
    /*
        Example:
            data_bits = 0x08;
            uint8_t cpol = 0x00;
            uint8_t cpah = 0x00;
            uint8_t cs = 0x01;
            uint32_t spi_speed = 10000000; // ~10Mhz   
    */
    uint8_t spi_binmode_args[] = { 
        (spi_speed >> 24) & 0xFF, 
        (spi_speed >> 16) & 0xFF,
        (spi_speed >> 8) & 0xFF,
        spi_speed & 0xFF, 
        data_bits, 
        cpol, 
        cpah, 
        cs 
    };

    mode_change((uint8_t*)"SPI");
    binmode_config(spi_binmode_args);
    system_config.binmode_usb_rx_queue_enable=false; 
    system_config.binmode_usb_tx_queue_enable=false; 
}

void enable_debug_legacy(void)
{
    uint8_t binmode_args = 1;

    binmode_debug_level(&binmode_args);
}

uint32_t read_buff(uint8_t *buf, uint32_t len, uint32_t max_tries)
{
    uint32_t pending_data = 0;
    uint32_t bytes_readed = 0;
    uint32_t total_bytes_readed = 0;

    if (remain_bytes > 0)
    {
        bytes_readed = remain_bytes >= len ? len : remain_bytes;
        memcpy(buf, cdc_buff, bytes_readed);
        remain_bytes -= bytes_readed;

        if (remain_bytes > 0)
        {
            memmove(cdc_buff, cdc_buff + bytes_readed, remain_bytes);
        }

        total_bytes_readed = bytes_readed;
    }

    while (total_bytes_readed < len && max_tries--)
    {
        pending_data = tud_cdc_n_available(1);
        if (pending_data > 0)
        {
            bytes_readed = tud_cdc_n_read(1, cdc_buff + remain_bytes, pending_data);
            tud_task();
            remain_bytes += bytes_readed;
            uint32_t bytes_to_copy = len - total_bytes_readed;
            if (remain_bytes < bytes_to_copy)
            {
                bytes_to_copy = remain_bytes;
            }

            memcpy(buf + total_bytes_readed, cdc_buff, bytes_to_copy);
            total_bytes_readed += bytes_to_copy;
            remain_bytes -= bytes_to_copy;

            if (remain_bytes > 0)
            {
                memmove(cdc_buff, cdc_buff + bytes_to_copy, remain_bytes);
            }
        }
    }

    return total_bytes_readed;
}

void cdc_full_flush(uint32_t cdc_id)
{
    tud_cdc_n_read_flush(cdc_id);
    tud_cdc_n_write_flush(cdc_id);
    remain_bytes = 0;
}

void reset_legacy(void)
{
    uint8_t binmode_args = 0;

    hwspi_deinit();
    disable_psu_legacy();
    binmode_pullup_disable(&binmode_args);
    binmode_reset(&binmode_args);
    mode_change((uint8_t*)"HiZ");
}


void legacy_protocol(void)
{
    uint8_t op_byte;
    uint8_t extended_info;
    uint8_t count_zero = 0;
    uint32_t spi_speed = 0;
    uint8_t cs_init = 0x01;

    cdc_full_flush(1);

    while (1)
    {
        op_byte = 0;
        extended_info = 0;
        tud_task();
        while (!read_buff(&op_byte, 1, DEFAULT_MAX_TRIES));
        if (binmode_debug)
        {
            printf("\r\n-\r\nop_byte=0x%02X", op_byte);
            printf(", extended_info=0x%02X", extended_info);
        }

        if (op_byte)
        {
            count_zero = 0; // ugly, but simple
            if (op_byte >= 0x10 && op_byte <= 0x1F)
            {
                extended_info = op_byte;
                op_byte = 0x10;
            }
            else if (op_byte >= 0x60 && op_byte <= 0x67) // this must be the first
            {
                extended_info = op_byte;
                op_byte = 0x60;
            }
            else if (op_byte & 0x80)
            {
                extended_info = op_byte;
                op_byte = 0x80;
            }
            else if (op_byte & 0x40)
            {
                extended_info = op_byte;
                op_byte = 0x40;
            }
        }
        
        if (binmode_debug)
        {
            printf("\r\nop_byte=0x%02X", op_byte);
            printf(", extended_info=0x%02X", extended_info);
        }
        switch (op_byte)
        {
            case 0x00:
                if (!count_zero)
                {
                    if (binmode_debug)
                    {   
                        printf("\r\nBBIO1->");
                    }
                    CDC_SEND_STR(1, "BBIO1");
                    spi_speed = 0;
                    reset_legacy();
                    system_config.binmode_usb_rx_queue_enable=false; 
                    system_config.binmode_usb_tx_queue_enable=false; 
                }
                else if (count_zero > 15)
                {
                    count_zero = 0;
                }
                count_zero++;
            break;

            case 0x0F:
                if (binmode_debug)
                {
                    printf("\r\nBus Pirate CLI prompt");
                }   
                // ugly hack for fixed baudarate (look flashrom src!):
                CDC_SEND_STR(1, "Bus Pirate v2.5\r\nCommunity Firmware v7.1\r\nHiZ>"); 
            break;

            case 0x01:
                if (binmode_debug)
                {
                    printf("\r\nSPI1->");
                }
                CDC_SEND_STR(1, "SPI1");
            break;

            case 0x40:
                if (binmode_debug)
                {   
                    printf("\r\npsu...");
                }

                // PSU
                if ((extended_info & 0b00001000) == 0)
                {
                    disable_psu_legacy();
                }
                else
                {
                    //uint8_t args[] = { 0x03, 0x21, 0x00, 0x80 }; // 3.3v 
                    uint8_t args[] = { 
                        volts_integer, 
                        volts_decimal, 
                        (uint8_t)(current_integer >> 8), 
                        (uint8_t)(current_integer & 0xFF) 
                    }; 
                    uint32_t result = binmode_psu_enable(args);

                    if(result)
                    {
                        if (binmode_debug)
                        {   
                            printf("\r\nPSU ERROR CODE %d", result);
                        }
                    }
                    else
                    {
                        if (binmode_debug)
                        {
                            printf("\r\nPSU Enabled");
                        }
                    }
                }

                // Pull-ups
                if ((extended_info & 0b00000100) == 0)
                {
                    if (binmode_debug)
                    {
                        printf("\r\npullup_disable");
                    }
                    uint8_t binmode_args = 0;
                    binmode_pullup_disable(&binmode_args);
                }
                else
                {
                    if (binmode_debug)
                    {
                        printf("\r\npullup_enable");
                    }
                    uint8_t binmode_args = 0;
                    binmode_pullup_enable(&binmode_args);
                }

                // AUX
                if ((extended_info & 0b00000010) == 0)
                {
                    if (binmode_debug)
                    {
                        printf("\r\naux_disable");
                    }
                }
                else
                {
                    if (binmode_debug)
                    {
                        printf("\r\naux_enable");
                    }
                }

                // CS
                if ((extended_info & 0b00000001) == 0)
                {
                    if (binmode_debug)
                    {
                        printf("\r\ncs 0");
                    }
                    cs_init = 0x00;
                }
                else
                {
                    if (binmode_debug)
                    {
                        printf("\r\ncs 1");
                    }
                    cs_init = 0x01;
                }

                CDC_SEND_STR(1, "\x01");
            break;

            case 0x60:
                if (binmode_debug)
                {
                    printf("\r\nspi_speed");
                }
                switch (extended_info & 0x9F)
                {
                    case 0b00: // 30kHz
                        spi_speed = 30000;
                    break;

                    case 0b01: // 125kHz
                        spi_speed = 125000;
                    break;

                    case 0b10: // 250kHz
                        spi_speed = 250000;
                    break;

                    case 0b11: // 1MHz
                        spi_speed = 1000000;
                    break;

                    case 0b100: // 2MHz
                        spi_speed = 2000000;
                    break;

                    case 0b101: // 2.6MHz
                        spi_speed = 2600000;
                    break;

                    case 0b110: // 4MHz
                        spi_speed = 4000000;
                    break;

                    case 0b111: // 8MHz
                        spi_speed = 8000000;
                    break;

                    default:
                        spi_speed = 0;
                    break;
                }
                if (binmode_debug)
                {
                    printf("\r\nspi_speed: %d", spi_speed);
                }
                CDC_SEND_STR(1, "\x01");
            break;

            case 0x80:
                if (binmode_debug)
                {
                    printf("\r\nhwspi_init");
                }

                 // SMP sample time (middle=0)
                if ((extended_info & 0x1) == 0)
                {
                    if (binmode_debug)
                    {
                        printf("\r\nSMP 0");
                    }
                }
                else
                {
                    if (binmode_debug)
                    {
                        printf("\r\nSMP 1");
                    }
                }

                // CKE clock edge (active to idle=1)
                if ((extended_info & 0x2) == 0) 
                {
                    if (binmode_debug)
                    {
                        printf("\r\nCKE 0");
                    }
                }
                else
                {
                    if (binmode_debug)
                    {
                        printf("\r\nCKE 1");
                    }
                }

                // CKP clock idle phase (low=0)
                if ((extended_info & 0x4) == 0) 
                {
                    if (binmode_debug)
                    {
                        printf("\r\nCKP 0");
                    }
                }
                else
                {
                    if (binmode_debug)
                    {
                        printf("\r\nCKP 1");
                    }
                }

                // Output HiZ(0)/3.3v(1)
                if ((extended_info & 0x8) == 0) 
                {
                    if (binmode_debug)
                    {
                        printf("\r\nHiZ");
                    }
                }
                else
                {
                    if (binmode_debug)
                    {
                        printf("\r\n3.3v");
                    }   
                }

                setup_spi_legacy(spi_speed, 8, 0, 0, cs_init);
                hwspi_select();
                CDC_SEND_STR(1, "\x01");

                /*
                uint8_t data_bits = 8;
                uint8_t cpol = 0;
                uint8_t cpha = 0;
                static const char mpin_labels[][5]={
                    "CLK",
                    "MOSI",
                    "MISO",
                    "CS"
                };

                spi_init(SPI1_BASE, 100000); // ~0.1MHz
                hwspi_init(data_bits, cpol, cpha);
                system_bio_claim(true, 6, 1, mpin_labels[0]);
                system_bio_claim(true, 7, 1, mpin_labels[1]);
                system_bio_claim(true, 4, 1, mpin_labels[2]);
                system_bio_claim(true, 5, 1, mpin_labels[3]);
                */
            break;

            case 0x03:
                if (binmode_debug)
                {
                    printf("\r\nhwspi_deselect");
                }
                hwspi_deselect();
                CDC_SEND_STR(1, "\x01");
            break;

            case 0x02:
                if (binmode_debug)
                {
                    printf("\r\nhwspi_select");
                }
                hwspi_select();
                CDC_SEND_STR(1, "\x01");
            break;

            case 0x10:
                if (binmode_debug)
                {
                    printf("\r\nBulk SPI transfer");
                }
                memset(tmpbuf, 0, TMPBUFF_SIZE);
                uint32_t bytes2read = (extended_info & 0x0F) + 1;
                if (binmode_debug)
                {
                    printf("\r\nbytes_to_read: %d", bytes2read);
                }
                CDC_SEND_STR(1, "\x01");
                while (!read_buff(tmpbuf, bytes2read, DEFAULT_MAX_TRIES));
                if (binmode_debug)
                {
                    printf("\r\n>> ");
                }
                for (int i = 0; i < bytes2read; i++)
                {
                    if (binmode_debug)
                    {
                        printf("\r\n0x%02X | ", tmpbuf[i]);
                    }
                    tmpbuf[i] = hwspi_write_read(tmpbuf[i]);
                    if (binmode_debug)
                    {
                        printf("0x%02X", tmpbuf[i]);
                    }
                }
                if (binmode_debug)
                {
                    printf("\r\n");
                }
                tud_cdc_n_write(1, tmpbuf, bytes2read);
                tud_cdc_n_write_flush(1);
            break;

            // SPI b00000100 (0x04) - Write then read & b00000101 (0x05) - Write then read, no CS
            case 0x04: 
            case 0x05:
                uint16_t bytes_to_read = 0;
                uint16_t bytes_to_write = 0;

                memset(tmpbuf, 0, TMPBUFF_SIZE);

                while (!read_buff(tmpbuf, 4, DEFAULT_MAX_TRIES));
                if (binmode_debug)
                {
                    printf("\r\nbytes_to_write H: 0x%02X", tmpbuf[0]);
                    printf("\r\nbytes_to_write L: 0x%02x", tmpbuf[1]);
                    printf("\r\nbytes_to_read H: 0x%02X", tmpbuf[2]);
                    printf("\r\nbytes_to_read L: 0x%02x", tmpbuf[3]);
                }
                bytes_to_write = (tmpbuf[0] << 8) | tmpbuf[1];
                bytes_to_read = (tmpbuf[2] << 8) | tmpbuf[3];
                if (binmode_debug)
                {
                    printf("\r\nbytes_to_write: %d", bytes_to_write);
                    printf("\r\nbytes_to_read: %d", bytes_to_read);
                }

                if (0x00 == bytes_to_read && 0x00 == bytes_to_write)
                {
                    // for AVRDUDE
                    CDC_SEND_STR(1, "\x01");
                    break;
                }

                if (bytes_to_write)
                {
                    while (!read_buff(tmpbuf, bytes_to_write, DEFAULT_MAX_TRIES));
                }

                if (0x04 == op_byte)
                {
                    hwspi_select();
                }
                if (binmode_debug)
                {
                    printf("\r\n>> ");
                }
                int j = 0;
                uint32_t total_bytes_spi = bytes_to_write + bytes_to_read;
                while (j < total_bytes_spi)
                {
                    if (binmode_debug)
                    {
                        printf("\r\n[%d] 0x%02X -> | ", j, tmpbuf[j]);
                    }
                    tmpbuf[j] = hwspi_write_read(j >= bytes_to_write ? 0x00 : tmpbuf[j]);
                    if (binmode_debug)
                    {
                        printf("<- 0x%02X", tmpbuf[j]);  
                    }
                    j++;
                }
                if (0x04 == op_byte)
                {
                    hwspi_deselect();
                }

                int bytes_sent = 0;
                int chunk_size = 32;
                int total_bytes = bytes_to_read + 1;
                int total_cdc_bytes_sended = 0;
                uint32_t delta = bytes_to_write  ? bytes_to_write - 1 : 0;
                if (bytes_to_write)
                {
                    tmpbuf[delta] = '\x01';
                }
                else
                {
                    CDC_SEND_STR(1, "\x01");
                    total_bytes--;
                }
                tud_cdc_n_read_flush(1);
                remain_bytes = 0;
                while (bytes_sent < total_bytes) 
                {
                    int bytes_left = total_bytes - bytes_sent;
                    int current_chunk_size = (bytes_left < chunk_size) ? bytes_left : chunk_size;
                    while (tud_cdc_n_write_available(1) < current_chunk_size) 
                    {
                        tud_task();
                        tud_cdc_n_write_flush(1);
                    }
                    total_cdc_bytes_sended += tud_cdc_n_write(1, tmpbuf + delta + bytes_sent, current_chunk_size);
                    tud_cdc_n_write_flush(1);
                    bytes_sent += current_chunk_size;
                }
                if (binmode_debug)
                {
                    printf("\r\ntotal_cdc_bytes_sended: %d", total_cdc_bytes_sended);
                }
                tud_task();
            break;

            case 0x06: // AVR EXTENDED COMMAND
                CDC_SEND_STR(1, "\x01");
                while (!read_buff(&op_byte, 1, DEFAULT_MAX_TRIES));
                if (binmode_debug)
                {
                    printf("\r\n-\r\nAVR op_byte=0x%02X", op_byte);
                }
                switch (op_byte)
                {
                    case 0x00:
                        if (binmode_debug)
                        {
                            printf("\r\nAVR NOOP");
                        }
                        CDC_SEND_STR(1, "\x01");
                    break;

                    case 0x01:
                        if (binmode_debug)
                        {
                            printf("\r\nAVR VERSION");
                        }
                        CDC_SEND_STR(1, "\x01\x00\x01");
                    break;

                    case 0x02:
                        if (binmode_debug)
                        {
                            printf("\r\nAVR BULK READ");
                        }

                        memset(tmpbuf, 0, TMPBUFF_SIZE);

                        while (!read_buff(tmpbuf, 8, DEFAULT_MAX_TRIES));

                        uint32_t addr = (tmpbuf[0] << 24) | (tmpbuf[1] << 16) | (tmpbuf[2] << 8) | tmpbuf[3];
                        uint32_t len = (tmpbuf[4] << 24) | (tmpbuf[5] << 16) | (tmpbuf[6] << 8) | tmpbuf[7];

                        if (binmode_debug)
                        {
                            printf("\r\naddr: 0x%08X, len: 0x%08X", addr, len);
                        }

                        if ((addr > 0xFFFF) || (len > 0xFFFF) || ((addr + len) > 0xFFFF))
                        {
                            // error
                            CDC_SEND_STR(1, "\x00");
                            break;
                        }
                        CDC_SEND_STR(1, "\x01");

                        if (binmode_debug)
                        {
                            printf("\r\n>> ");
                        }
                        while (len > 0)
                        {
                            hwspi_write_read(0x20); // AVR_FETCH_LOW_BYTE_COMMAND
                            hwspi_write_read((addr >> 8) & 0xFF);
                            hwspi_write_read(addr & 0xFF);
                            uint8_t byte_flash = hwspi_write_read(0x00);
                            if (binmode_debug)
                            {
                                printf("\r\n0x%02X", byte_flash);
                            }
                            tud_cdc_n_write_char(1, byte_flash); // Send the readed byte
                            tud_cdc_n_write_flush(1);
                            len--;
                            if (len > 0)
                            {
                                hwspi_write_read(0x28); // AVR_FETCH_HIGH_BYTE_COMMAND
                                hwspi_write_read((addr >> 8) & 0xFF);
                                hwspi_write_read(addr & 0xFF);
                                uint8_t byte_flash = hwspi_write_read(0x00);
                                if (binmode_debug)
                                { 
                                    printf("\r\n0x%02X", byte_flash);
                                }
                                tud_cdc_n_write_char(1, byte_flash);  // Send the readed byte
                                tud_cdc_n_write_flush(1);
                                len--;
                            }
                            addr++;
                        }
                        if (binmode_debug)
                        {
                            printf("\r\n");
                        }
                    break;

                    default:
                        // error
                        CDC_SEND_STR(1, "\x00");
                    break;
                }

            break;
        }
    }
}


// handler needs to be cooperative multitasking until mode is enabled
void legacy4third_mode(void){
    static uint32_t mode_active = 0;
    if (mode_active == 0)
    {
        mode_active++;
        // enable_debug_legacy();
        system_config.binmode_usb_rx_queue_enable=false; 
        system_config.binmode_usb_tx_queue_enable=false; 
    }
    else if (mode_active == 1)
    {
        mode_active++;

        prompt_result result = { 0 };

        printf("\r\n%sPower supply\r\nVolts (0.80V-5.00V)%s", ui_term_color_info(), ui_term_color_reset());
        
        float volts = 0.0f;
        ui_prompt_float(&result, 0.8f, 5.0f, 3.3f, true, &volts, false);
        
        volts_integer = (uint8_t)floorf(volts);
        volts_decimal = (uint8_t)((volts - floorf(volts)) * 100);

        if (binmode_debug)
        {
            printf("\r\nVolts: int = %u, dec = %u\n", volts_integer, volts_decimal);
        }
        
        float current = 0.0f;
        printf("\r\n%sMaximum current (0mA-500mA), <enter> for none%s", ui_term_color_info(), ui_term_color_reset());
        ui_prompt_float(&result, 0.0f, 500.0f, 100.0f, true, &current, true);

        current_integer = (uint16_t)floorf(current);
        current_decimal = (uint8_t)((current - floorf(current)) * 100);

        if (binmode_debug)
        {
            printf("\r\nCurrent: int = %u, dec = %u\n", current_integer, current_decimal);
        }
        
        cdc_buff = (uint8_t*) mem_alloc(CDCBUFF_SIZE + TMPBUFF_SIZE, 0);
        if (binmode_debug)
        {
            printf("\r\ncdc_buff: 0x%08X\r\n", cdc_buff);
        }
        printf("\r\nDone! Just execute flashrom or avrdude using the binary com port\r\n");
        tmpbuf = cdc_buff + CDCBUFF_SIZE;
        memset(cdc_buff, 0, CDCBUFF_SIZE);
        memset(tmpbuf, 0, TMPBUFF_SIZE);
        remain_bytes = 0;
        cdc_full_flush(1);
        legacy_protocol();
        system_config.binmode_usb_rx_queue_enable=true; 
        system_config.binmode_usb_tx_queue_enable=true; 
        mem_free(cdc_buff);
        reset_legacy();
        /*
            uint8_t binmode_args = 0;
            binmode_reset_buspirate(&binmode_args);
        */
    }
}

 
 



