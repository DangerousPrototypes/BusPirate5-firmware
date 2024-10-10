#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/spi.h"
#include "pirate.h"
#include "pirate/bio.h"
#include "pirate/hwspi.h"

void hwspi_init(uint8_t data_bits, uint8_t cpol, uint8_t cpha){
	//set buffers to correct position
	bio_buf_output(M_SPI_CLK); //sck
	bio_buf_output(M_SPI_CDO); //tx
	bio_buf_input(M_SPI_CDI); //rx
    spi_set_format(M_SPI_PORT, data_bits, cpol, cpha, SPI_MSB_FIRST);
	//assign spi functon to io pins
	bio_set_function(M_SPI_CLK, GPIO_FUNC_SPI); //sck
	bio_set_function(M_SPI_CDO, GPIO_FUNC_SPI); //tx
	bio_set_function(M_SPI_CDI, GPIO_FUNC_SPI); //rx
	//cs
	bio_set_function(M_SPI_CS, GPIO_FUNC_SIO);
	bio_output(M_SPI_CS);
	hwspi_deselect();
}

void hwspi_deinit(void){
	// disable peripheral
	spi_deinit(M_SPI_PORT);
	// reset all pins to safe mode (done before mode change, but we do it here to be safe)
	bio_init();
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

    while(spi_is_busy(M_SPI_PORT)); //wait for idle
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

uint32_t hwspi_write_read(uint8_t data){
	while(!spi_is_writable(M_SPI_PORT));
	spi_get_hw(M_SPI_PORT)->dr = (uint32_t)data;
    while(!spi_is_readable(M_SPI_PORT));	
    while(spi_is_busy(M_SPI_PORT)); //wait for idle
	return (uint8_t)spi_get_hw(M_SPI_PORT)->dr;
}

uint32_t hwspi_read(void){
	return hwspi_write_read(0xff);
}

void hwspi_read_n(uint8_t *data, uint32_t count){
    for(uint32_t i=0; i<count; i++){
        data[i]=hwspi_write_read(0xff);
    }
}





