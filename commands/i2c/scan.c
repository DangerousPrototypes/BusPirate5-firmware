
#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "i2c.pio.h"
#include "mode/pio_i2c.h"
#include "pirate/storage.h"
#include "ui/ui_term.h"
#include "lib/i2c_address_list/dev_i2c_addresses.h"

bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

bool i2c_search_check_addr(uint8_t address){
	uint16_t ack;
	uint32_t error;

	error=pio_i2c_start_timeout(pio, pio_state_machine, 0xfff);
	if(error){
		pio_i2c_resume_after_error(pio, pio_state_machine);
	}
	ack=pio_i2c_write_timeout(pio, pio_state_machine, address, 0xfff);

	if(ack){
		pio_i2c_resume_after_error(pio, pio_state_machine);
	}

	//if read address then read one and NACK
	if(!ack && (address&0x1)){
		error=pio_i2c_read_timeout(pio, pio_state_machine, &error, false, 0xfff);
		if(error)
		{
			pio_i2c_resume_after_error(pio, pio_state_machine);
		}	
	} 
	
	error=pio_i2c_stop_timeout(pio, pio_state_machine, 0xfff);
	if(error){
		pio_i2c_resume_after_error(pio, pio_state_machine);
	}	
	
	return (!ack);	
}

static void i2c_search_addr(bool verbose){
	bool color = false;
	uint16_t device_count=0;
	uint16_t device_pairs=0;

	/*if(checkshort()){
		printf("No pullup or short\r\n");
		system_config.error=1;
		return;
	}*/

	printf("I2C address search:\r\n");

	pio_i2c_rx_enable(pio, pio_state_machine, false);

	for(uint16_t i=0; i<256; i=i+2){

		bool i2c_w=i2c_search_check_addr(i);
		bool i2c_r=i2c_search_check_addr(i+1);

		if(i2c_w||i2c_r){
			device_count+=(i2c_w+i2c_r); //add any new devices
			if(i2c_w&&i2c_r) device_pairs++;
		
			color=!color;
			if(color||verbose)
					ui_term_color_text_background(hw_pin_label_ordered_color[0][0],hw_pin_label_ordered_color[0][1]);

			printf("0x%02X",i>>1);
			if(i2c_w) printf(" (0x%02X W)",i);
			if(i2c_r) printf(" (0x%02X R)",i+1);
			if(color||verbose){
				printf("%s", ui_term_color_reset());
			}	
			printf("\r\n");	
			if(verbose){
				printf("%s\r\n", dev_i2c_addresses[i>>1]);
			}			
		}
	}	

    printf("%s\r\nFound %d addresses, %d W/R pairs.\r\n",ui_term_color_reset(), device_count, device_pairs);
}