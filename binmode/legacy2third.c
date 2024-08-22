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

//#include <stdio.h>
#include <string.h>
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


const char legacy2third_mode_name[]="Legacy Binary Mode For Third Parties (Flashrom and AVRdude)";

uint8_t* cdc_buff;
uint32_t remain_bytes;
#define DEFAULT_MAX_TRIES 100000

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

#define CDC_SEND_STR(cdc_n, str) tud_cdc_n_write(cdc_n, (uint8_t*)str, sizeof(str) - 1); tud_cdc_n_write_flush(1);

void cdc_full_flush(uint32_t cdc_id)
{
    tud_cdc_n_read_flush(cdc_id);
    tud_cdc_n_write_flush(cdc_id);
    remain_bytes = 0;
}

void legacy_protocol(void)
{
    uint8_t op_byte;
    uint8_t extended_info;
    uint8_t count_zero = 0;
    uint32_t spi_speed = 0;

    cdc_full_flush(1);

    while (1)
    {
        op_byte = 0;
        extended_info = 0;
        tud_task();
        while (!read_buff(&op_byte, 1, DEFAULT_MAX_TRIES));
        printf("\r\n-\r\nop_byte=0x%02X", op_byte);
        printf(", extended_info=0x%02X", extended_info);

        if (op_byte)
        {
            count_zero = 0; // ugly, but simple
            if (op_byte >= 0x60 && op_byte <= 0x67) // this must be the first
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
        
        printf("\r\nop_byte=0x%02X", op_byte);
        printf(", extended_info=0x%02X", extended_info);
        switch (op_byte)
        {
            case 0x00:
                if (!count_zero)
                {
                    printf("\r\nBBIO1->");
                    CDC_SEND_STR(1, "BBIO1");
                    spi_speed = 0;
                    hwspi_deinit();
                    psu_disable();
                }
                else if (count_zero > 15)
                {
                    count_zero = 0;
                }
                count_zero++;
            break;

            case 0x0F:
                printf("\r\nBus Pirate CLI prompt->");
                // ugly hack for fixed baudarate (look flashrom src!):
                CDC_SEND_STR(1, "Bus Pirate v2.5\r\nCommunity Firmware v7.1\r\nHiZ>"); 
            break;

            case 0x01:
                printf("\r\nSPI1->");
                CDC_SEND_STR(1, "SPI1");
            break;

            case 0x40:
                printf("\r\npsu...");
                if (extended_info & 0b00001000)
                {
                    uint8_t args[] = { 0x03, 0x21, 0x00, 0x80 };
                    uint32_t result = binmode_psu_enable(args);
                    if(result)
                    {
                        printf("\r\nPSU ERROR CODE %d", result);
                        CDC_SEND_STR(1, "\x00");
                    }
                    else
                    {
                        printf("\r\nPSU Enabled");
                        CDC_SEND_STR(1, "\x01");
                    }
                }
            break;

            case 0x60:
                printf("\r\nspi_speed");
                spi_speed = 1000000;
                CDC_SEND_STR(1, "\x01");
            break;

            case 0x80:
                printf("\r\nhwspi_init");
                uint8_t data_bits = 8;
                uint8_t cpol = 0;
                uint8_t cpha = 0;
                static const char mpin_labels[][5]={
                    "CLK",
                    "MOSI",
                    "MISO",
                    "CS"
                };

                spi_init(SPI1_BASE, 1000000); // 1MHz
                hwspi_init(data_bits, cpol, cpha);
                system_bio_claim(true, 6, 1, mpin_labels[0]);
                system_bio_claim(true, 7, 1, mpin_labels[1]);
                system_bio_claim(true, 4, 1, mpin_labels[2]);
                system_bio_claim(true, 5, 1, mpin_labels[3]);
                CDC_SEND_STR(1, "\x01");
            break;

            case 0x03:
                printf("\r\nhwspi_select");
                hwspi_select();
                CDC_SEND_STR(1, "\x01");
            break;

            case 0x04:
                uint16_t bytes_to_read = 0;
                uint16_t bytes_to_write = 0;
                static uint8_t tmpbuf[0x2000];

                memset(tmpbuf, 0, sizeof(tmpbuf));

                while (!read_buff(tmpbuf, 4, DEFAULT_MAX_TRIES));

                printf("\r\nbytes_to_write H: 0x%02X", tmpbuf[0]);
                printf("\r\nbytes_to_write L: 0x%02x", tmpbuf[1]);
                printf("\r\nbytes_to_read H: 0x%02X", tmpbuf[2]);
                printf("\r\nbytes_to_read L: 0x%02x", tmpbuf[3]);

                bytes_to_write = (tmpbuf[0] << 8) | tmpbuf[1];
                bytes_to_read = (tmpbuf[2] << 8) | tmpbuf[3];
                printf("\r\nbytes_to_write: %d", bytes_to_write);
                printf("\r\nbytes_to_read: %d", bytes_to_read);

                while (!read_buff(tmpbuf, bytes_to_write, DEFAULT_MAX_TRIES));

                hwspi_select();
                printf("\r\n>> "); 
                int j = 0;
                uint32_t total_bytes_spi = bytes_to_write > bytes_to_read ? bytes_to_write : bytes_to_read;
                if (bytes_to_read)
                {
                    total_bytes_spi++;
                }
                while (j < total_bytes_spi)
                {
                    if (j >= bytes_to_write)
                    {
                        tmpbuf[j] = 0x00;
                    }
                    printf("\r\n[%d] 0x%02X -> | ", j, tmpbuf[j]);
                    tmpbuf[j] = hwspi_write_read(tmpbuf[j]);
                    printf("<- 0x%02X", tmpbuf[j]);
                    j++;
                }
                hwspi_deselect();

                tmpbuf[0] = '\x01';

                int bytes_sent = 0;
                int chunk_size = 32;
                int total_bytes = bytes_to_read + 1;
                int total_cdc_bytes_sended = 0;
                tud_cdc_n_read_flush(1);
                while (bytes_sent < total_bytes) 
                {
                    int bytes_left = total_bytes - bytes_sent;
                    int current_chunk_size = (bytes_left < chunk_size) ? bytes_left : chunk_size;
                    while (tud_cdc_n_write_available(1) < current_chunk_size) 
                    {
                        tud_task();
                        tud_cdc_n_write_flush(1);
                    }
                    total_cdc_bytes_sended += tud_cdc_n_write(1, tmpbuf + bytes_sent, current_chunk_size);
                    tud_cdc_n_write_flush(1);
                    bytes_sent += current_chunk_size;
                }

                printf("\r\ntotal_cdc_bytes_sended: %d", total_cdc_bytes_sended);
                tud_task();
            break;
        }
    }
}

// handler needs to be cooperative multitasking until mode is enabled
void legacy2third_mode(void){
    static uint32_t mode_active=0;
    if (mode_active == 0){
        mode_active++;
        uint8_t binmode_args = 1;
        binmode_debug_level(&binmode_args);
        script_enabled();
        cdc_buff = (uint8_t*) mem_alloc(0x2000);
        remain_bytes = 0;
        legacy_protocol();
        mem_free(cdc_buff);
        hwspi_deinit();
        psu_disable();
    }
}

 
 



