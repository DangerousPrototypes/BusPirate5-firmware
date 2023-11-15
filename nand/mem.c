/**
 * @file		mem.c
 * @author		Andrew Loebs
 * @brief		Implementation file of the memory management module
 *
 */

#include "mem.h"

#include <stdlib.h>

#include "spi_nand.h"

// private variables
static uint8_t mem_buffer[SPI_NAND_PAGE_SIZE + SPI_NAND_OOB_SIZE];
static bool allocated = false;

// public function definitions
uint8_t *mem_alloc(size_t size)
{
    if (!allocated && size <= sizeof(mem_buffer)) {
        allocated = true;
        return mem_buffer;
    }
    else {
        return NULL;
    }
}

void mem_free(uint8_t *ptr)
{
    if (ptr == mem_buffer) {
        allocated = false;
    }
}