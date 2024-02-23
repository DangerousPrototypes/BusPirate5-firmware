#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/spi.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "bytecode.h"
#include "bio.h"
#include "ui/ui_prompt.h"
#include "ui/ui_term.h"
#include "storage.h"

#define M_SPI_PORT spi1
#define M_SPI_CLK BIO6
#define M_SPI_CDO BIO7
#define M_SPI_CDI BIO4
#define M_SPI_CS BIO5

#define M_SPI_SELECT 0
#define M_SPI_DESELECT 1

uint32_t flash_read(void);
void flash_write_32(const uint32_t data, uint8_t count);
void flash_write(const uint32_t data);
void flash_start(void);
void flash_stop(void);

void flash_not_found()
{
    printf("not found\r\n");
}

void flash_probe()
{
    printf("Probing:\r\n Resume ID (0xAB): ");  
    //DP 0xB9: deep power down and then RDP 0xAB, 3 dummy bytes, 1 RES ID byte (release and read ID)
    //deep sleep command
    flash_start();
    flash_write(0xab);
    flash_stop();
    //resume and read ID
    busy_wait_ms(10);
    flash_start();
    flash_write_32(0xab000000, 4);
    uint8_t RESID = flash_read();
    flash_stop();

    if(RESID!=0x00 && RESID!=0xff)
    {
        printf("%02x\r\n", RESID);
    }
    else
    {
        flash_not_found();
    }

    printf(" REMS ID (0x90): ");
    //0x90: REMS  Read Electronic Manufacturer ID & Device ID (REMS)
    // 0x90, 0x00:3, 1 Manuf ID, 1 Device ID
    flash_start();
    flash_write_32(0x90000000,4);
    uint8_t REMS_MANUFID=flash_read();
    uint8_t REMS_DEVID=flash_read();
    flash_stop();
    if(REMS_MANUFID!=0x00 && REMS_MANUFID!=0xff) //TODO: manuf ID has a checksum bit or something, list of man and dev ids?
    {
        printf(" Manufacturer ID: %02x, Device ID: %02x\r\n", REMS_MANUFID, REMS_DEVID);
    }
    else
    {
        flash_not_found();
    }

    printf(" Read ID (0x9f): ");
    //0x9f: RDID  Read Identification (RDID)
    // 0x9f, 1 manuf ID, 1 memory type, 1 capacity
    flash_start();
    flash_write(0x9f);
    uint8_t RDID_MANUFID=flash_read();
    uint8_t RDID_MEMTYPE=flash_read();
    uint8_t RDID_MEMCAP=flash_read();
    flash_stop();
    if(RDID_MANUFID!=0x00 && RDID_MANUFID!=0xff)//TODO: is there a standard coding?
    {
        printf(" Manufacturer ID: %02x, Type: %02x, Capacity: %02x\r\n", RDID_MANUFID, RDID_MEMTYPE, RDID_MEMCAP);
    }
    else
    {
        flash_not_found();
    }

    //now grab Serial Flash Discoverable Parameter (SFDP)
    // 0x5a 3 byte address, dummy byte, read first 24bytes
    //check code
    //version
    //JEDEC info and jump
    //Manuf info and jump
    // Read JEDEC location and length
    // Read Manuf location and length




}

void flash_set_cs(uint8_t cs)
{

	if(cs==M_SPI_SELECT) // 'start'
	{
		if(true) bio_put(M_SPI_CS, 0);
			else bio_put(M_SPI_CS, 1);
	}
	else			// 'stop' 
	{
		if(true) bio_put(M_SPI_CS, 1);
			else bio_put(M_SPI_CS, 0);
	}
}

uint8_t flash_xfer(const uint8_t out)
{
	uint8_t spi_in;
	spi_write_read_blocking(M_SPI_PORT, &out,&spi_in, 1);
	return spi_in;
}

void flash_start(void)
{
	flash_set_cs(M_SPI_SELECT);
}

void flash_stop(void)
{

	flash_set_cs(M_SPI_DESELECT);
}

void flash_write_32(const uint32_t data, uint8_t count)
{
    uint8_t sent=0;
    for(uint8_t i=4; i>0; i--)
    {
        flash_write(data>>(8*(i-1)));
        sent++;
        if(sent==count) return;
    }
}

void flash_write(uint32_t data)
{
    // Write to TX FIFO whilst ignoring RX, then clean up afterward. When RX
    // is full, PL022 inhibits RX pushes, and sets a sticky flag on
    // push-on-full, but continues shifting. Safe if SSPIMSC_RORIM is not set.
	while(!spi_is_writable(M_SPI_PORT))
	{
		tight_loop_contents();
	}

	spi_get_hw(M_SPI_PORT)->dr = (uint32_t)data;

    // Drain RX FIFO, then wait for shifting to finish (which may be *after*
    // TX FIFO drains), then drain RX FIFO again
    while(spi_is_readable(M_SPI_PORT))
	{
        (void)spi_get_hw(M_SPI_PORT)->dr;
	}

    while(spi_get_hw(M_SPI_PORT)->sr & SPI_SSPSR_BSY_BITS)
	{
        tight_loop_contents();
	}

    while(spi_is_readable(M_SPI_PORT))
	{
        (void)spi_get_hw(M_SPI_PORT)->dr;
	}

    // Don't leave overrun flag set
    spi_get_hw(M_SPI_PORT)->icr = SPI_SSPICR_RORIC_BITS;
}

uint32_t flash_read(void)
{
	while(!spi_is_writable(M_SPI_PORT));
	
	spi_get_hw(M_SPI_PORT)->dr = (uint32_t)0xff;

    while(!spi_is_readable(M_SPI_PORT));
	
	return (uint8_t)spi_get_hw(M_SPI_PORT)->dr;
}

