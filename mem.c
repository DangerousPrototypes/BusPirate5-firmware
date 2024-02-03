/**
 * @file		mem.c
 * @author		Andrew Loebs
 * @brief		Implementation file of the memory management module
 *
 */
// Modified by Ian Lesnet 18 Dec 2023 for Bus Pirate 5
#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "mem.h"
#include <stdlib.h>
#include "system_config.h"

// private variables
static uint8_t mem_buffer[BIG_BUFFER_SIZE] __attribute__((aligned(32768)));
static bool allocated = false;

// public function definitions
uint8_t *mem_alloc(size_t size, uint32_t owner)
{
    if (!allocated && size <= sizeof(mem_buffer)) {
        allocated = true;
        system_config.big_buffer_owner=owner;
        return mem_buffer;
    }
    else 
    {
        printf("Error: the big buffer is already allocated to #%d", system_config.big_buffer_owner);
        return NULL;
    }
}

void mem_free(uint8_t *ptr)
{
    if (ptr == mem_buffer) {
        allocated = false;
        system_config.big_buffer_owner=BP_BIG_BUFFER_NONE;
    }
}