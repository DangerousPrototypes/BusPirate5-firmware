/**
 * @file		sys_time.c
 * @author		Andrew Loebs
 * @brief		Implementation file of the sys time module
 *
 */
#include <stdint.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "pico/time.h"
#include "nand/sys_time.h"
// #include "../st/ll/stm32l4xx_ll_utils.h"
// #include "../st/stm32l4xx.h"
// #include "../st/system_stm32l4xx.h"

// defines
#define SYSTICK_PREEMPT_PRIORITY 0
#define SYSTICK_SUB_PRIORITY 0

// private variables
uint32_t sys_time_ms = 0;

bool _sys_time_increment(repeating_timer_t* mst) {
    sys_time_ms++;
    return true;
}

// public function definitions
void sys_time_init(void) {
    sys_time_ms = 0;

    // setup 1 ms sys tick
    static repeating_timer_t mst;
    add_repeating_timer_ms(-1, _sys_time_increment, NULL, &mst);
}
/*
void _sys_time_increment(void)
{
    sys_time_ms++;
}*/

uint32_t sys_time_get_ms(void) {
    return sys_time_ms;
}

uint32_t sys_time_get_elapsed(uint32_t start) {
    return sys_time_ms - start;
}

// TODO: add unit test for this function
//  normal case
//  current time overlapped, end time not overlapped
//  current time overlapped, end time overlapped
//  current time not overlapped, end time overlapped
//  all above cases when duration is 0
bool sys_time_is_elapsed(uint32_t start, uint32_t duration_ms) {
    return (sys_time_get_elapsed(start) >= duration_ms);
}

void sys_time_delay(uint32_t duration_ms) {
    uint32_t start = sys_time_ms;
    while (!sys_time_is_elapsed(start, duration_ms))
        ;
}