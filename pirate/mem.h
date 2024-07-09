/**
 * @file		mem.h
 * @author		Andrew Loebs
 * @brief		Header file of the memory management module
 *
 * A simple module to manage allocation of a single nand page buffer.
 *
 * This was made in response to many modules (shell_command, spi_nand),
 * needing temporary access to a page-sized array, and not wanting to
 * create that array on the stack or allocate it statically for the lifetime
 * of the application. This is not the page buffer used by the flash translation
 * layer.
 *
 *
 */

#ifndef __MEM_H
#define __MEM_H

#include <stdint.h>
#include <stdlib.h>

enum big_buffer_owners{
	BP_BIG_BUFFER_NONE=0,
	BP_BIG_BUFFER_SCOPE,
	BP_BIG_BUFFER_LA,
	BP_BIG_BUFFER_DISKFORMAT,
};

/// @brief Attempts to allocate a nand page buffer.
/// @return Pointer to the buffer if available, NULL if not available
/// @note Return value should always be checked against null.
/// @note Max size: SPI_NAND_PAGE_SIZE + SPI_NAND_OOB_SIZE
uint8_t *mem_alloc(size_t size, uint32_t owner);

/// @brief Frees the allocated nand page buffer
/// @param ptr pointer to the nand page buffer
void mem_free(uint8_t *ptr);

#endif // __MEM_H
