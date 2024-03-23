#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/spi.h"
#include "pirate.h"
#include "pirate/bio.h"

void hwspi_init(void){

}

// low typically selects a chip
void hwspi_select(void){
	bio_put(M_SPI_CS, 0);
}

//high typically deselects a chip
void hwspi_deselect(void){
	bio_put(M_SPI_CS, 1);
}

// this is taken from the SDK 
void hwspi_write(uint32_t data){
    // Write to TX FIFO whilst ignoring RX, then clean up afterward. When RX
    // is full, PL022 inhibits RX pushes, and sets a sticky flag on
    // push-on-full, but continues shifting. Safe if SSPIMSC_RORIM is not set.
	while(!spi_is_writable(M_SPI_PORT)) tight_loop_contents();

	spi_get_hw(M_SPI_PORT)->dr = (uint32_t)data;

    // Drain RX FIFO, then wait for shifting to finish (which may be *after*
    // TX FIFO drains), then drain RX FIFO again
    while(spi_is_readable(M_SPI_PORT)){
        (void)spi_get_hw(M_SPI_PORT)->dr;
	}

    while(spi_get_hw(M_SPI_PORT)->sr & SPI_SSPSR_BSY_BITS){
        tight_loop_contents();
	}

    while(spi_is_readable(M_SPI_PORT)){
        (void)spi_get_hw(M_SPI_PORT)->dr;
	}

    // Don't leave overrun flag set
    spi_get_hw(M_SPI_PORT)->icr = SPI_SSPICR_RORIC_BITS;
}

void hwspi_write_n(uint8_t *data, uint8_t count){
    for(uint8_t i=0; i<count; i++){
        hwspi_write(data[i]);
    }
}

//writes <count> bytes of a 32bit <data> to the SPI port
void hwspi_write_32(const uint32_t data, uint8_t count){
    uint8_t sent=0;
    for(uint8_t i=4; i>0; i--){
        hwspi_write(data>>(8*(i-1)));
        sent++;
        if(sent==count) return;
    }
}

uint32_t hwspi_read(void){
	while(!spi_is_writable(M_SPI_PORT));
	spi_get_hw(M_SPI_PORT)->dr = (uint32_t)0xff;
    while(!spi_is_readable(M_SPI_PORT));	
	return (uint8_t)spi_get_hw(M_SPI_PORT)->dr;
}

void hwspi_read_n(uint8_t *data, uint8_t count){
    for(uint8_t i=0; i<count; i++){
        data[i]=hwspi_read();
    }
}



