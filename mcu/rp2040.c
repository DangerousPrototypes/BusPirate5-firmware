#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "pico/unique_id.h"
#include "hardware/watchdog.h"

uint64_t mcu_get_unique_id(void)
{
	pico_unique_board_id_t id;
	pico_get_unique_board_id(&id);
    return *((uint64_t*)(id.id));
};

void mcu_reset_args(opt_args (*args), struct command_result *res)
{
 	watchdog_enable(1, 1);
	while(1);
}

void mcu_reset(struct command_attributes *attributes, struct command_response *response)
{
 	watchdog_enable(1, 1);
	while(1);
}
