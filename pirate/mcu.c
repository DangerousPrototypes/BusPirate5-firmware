#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "pico/unique_id.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"

uint64_t mcu_get_unique_id(void){
	pico_unique_board_id_t id;
	pico_get_unique_board_id(&id);
    return *((uint64_t*)(id.id));
};

void mcu_reset(void){
 	watchdog_enable(1, 1);
	while(1);
}

void mcu_jump_to_bootloader(void){
	/* \param usb_activity_gpio_pin_mask 0 No pins are used as per a cold boot. Otherwise a single bit set indicating which
	*                               GPIO pin should be set to output and raised whenever there is mass storage activity
	*                               from the host.
	* \param disable_interface_mask value to control exposed interfaces
	*  - 0 To enable both interfaces (as per a cold boot)
	*  - 1 To disable the USB Mass Storage Interface
	*  - 2 To disable the USB PICOBOOT Interface
	*/
	reset_usb_boot(0x00,0x00);
}

uint8_t mcu_detect_revision(void){
	gpio_set_function(23, GPIO_FUNC_SIO);
    gpio_set_dir(23,true);
    gpio_put(23,true);
    busy_wait_ms(100);
    gpio_set_dir(23, false);
    busy_wait_us(1);
    if(gpio_get(23)){
        return 10;
    }else{
        return 8;
    }
}