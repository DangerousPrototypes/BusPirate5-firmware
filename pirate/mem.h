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

typedef enum _big_buffer_owners {
	BP_BIG_BUFFER_OWNER_NONE=0,
	BP_BIG_BUFFER_OWNER_SCOPE,
	BP_BIG_BUFFER_OWNER_LA,
	BP_BIG_BUFFER_OWNER_DISKFORMAT,

	MAXIMUM_BP_BIG_BUFFER_OWNER,
} big_buffer_owner_t;
static_assert(sizeof(big_buffer_owner_t) <= 4);
// Ensures 16-bits or less; Allows to overload value stored in tracking structure
static_assert(MAXIMUM_BP_BIG_BUFFER_OWNER <= 0x10000);

typedef struct _big_buffer_general_state {
    // uintptr_t allows cleaner addition/subtraction between pointers
    static_assert(sizeof(uintptr_t) >= sizeof(size_t));
    uintptr_t buffer;
    size_t    total_size;
    uintptr_t high_watermark; // end of allocatable memory
    uintptr_t low_watermark;
    uintptr_t long_lived_limit;
    uint16_t  temp_allocations_count;
    uint16_t  long_lived_allocations_count;
} big_buffer_general_state_t;

typedef struct _big_buffer_allocation_instance {
    uintptr_t   result;
    size_t      requested_size;
    size_t      requested_alignment;
    uint32_t    owner_tag : 16; // big_buffer_owner_t ... limited to 16-bits
	uint32_t    was_long_lived_allocation : 1;
	uint32_t    : 15; // rfu
} big_buffer_allocation_instance_t;

#define MAXIMUM_SUPPORTED_ALLOCATION_COUNT 32

typedef struct _big_buffer_state {
	big_buffer_general_state_t general;
	big_buffer_allocation_instance_t allocation[MAXIMUM_SUPPORTED_ALLOCATION_COUNT];
} big_buffer_state_t;

/// @brief Initializes the BigBuffer module.
/// @details Must be called before any other BigBuffer API.
/// @note Called during system init, so in some respects this is an implementation detail.
void BigBuffer_Initialize(void);

/// @brief Verifies that all /temporary/ allocations have been freed,
///        and that state matches expectations.
/// @note it is expected that all temporary allocations have been freed
///       prior to calling this API.  This is STRONGLY recommended before
///       initializing a new mode to help avoid bugs that are essentially
///       impossible to debug (dangling pointers --> memory corruption).
void BigBuffer_VerifyNoTemporaryAllocations(void);

/// @brief Allocates temporary buffer of the specified size and alignment.
/// @note The memory returned by this API /MUST/ be released prior to calling
///       BigBuffer_PartialReset().
void* BigBuffer_AllocateTemporary(size_t countOfBytes, size_t requiredAlignment, big_buffer_owner_t owner);

/// @brief Allocates a long-lived buffer of the specified size and alignment, if possible.
/// @details The memory returned by this API is /NOT/ required to be released prior to
///          calling BigBuffer_PartialReset().
/// @note Total long-lived buffer space is currently limited to 8k.
void* BigBuffer_AllocateLongLived(size_t countOfBytes, size_t requiredAlignment, big_buffer_owner_t owner);

/// @brief Frees a buffer previously allocated by `BigBuffer_AllocateTemporary()`.
/// @note The owner must match ... this helps avoid a class of bugs that are otherwise
///       more difficult to catch / debug / track down.
void BigBuffer_FreeTemporary(void* ptr, big_buffer_owner_t owner);

/// @brief Frees a buffer previously allocated by `BigBuffer_AllocateLongLived()`.
/// @note The owner must match ... this helps avoid a class of bugs that are otherwise
///       more difficult to catch / debug / track down.
void BigBuffer_FreeLongLived(void* ptr, big_buffer_owner_t owner);

/// @brief How many bytes of temporary memory are available with a given alignment?
size_t BigBuffer_GetAvailableTemporaryMemory(size_t requiredAlignment);

/// @brief How many bytes of long-lived memory are available with a given alignment?
size_t BigBuffer_GetAvailableLongLivedMemory(size_t requiredAlignment);

////////////////////////////////////////////////////////////////////////////////
// Debug APIs ... not really intended for general use, but may be useful during
//                development / debugging.
////////////////////////////////////////////////////////////////////////////////


/// @brief provide statistics on current BigBuffer usage
/// @details Provides overview of BigBuffer state (high / low water marks, etc.)
/// @note This API is /NOT/ performance-critical, as it is intended for
///       interactive display of information.
bool BigBuffer_DebugGetStatistics( big_buffer_general_state_t * general_state_out );

/// @brief provide detailed information on current BigBuffer usage
/// @details The goal of this API is to help troubleshoot memory usage issues.
///          Example question and answer:
///          Why is this mode failing to initialize?
///          The mode requires 130k buffer, which reaches into the long-lived
///          allocation space, but the entire 8k of the long-lived buffer is
///          allocated by XXXX.
/// @note This API is /NOT/ performance-critical, as it is intended for
///       interactive display of information.
bool BigBuffer_DebugGetDetailedStatistics( big_buffer_state_t * state_out );

/// @brief Prints the current state of the BigBuffer module
/// @details This is intended for debugging purposes.  It will print the
///          current internal `big_buffer_general_state_t`.  If the `verbose`
///          parameter is true, it will also print all currently allocated
///          memory allocations (one allocation per line).
/// @note This API is /NOT/ performance-critical, as it is intended for
///       interactive display of information.
void BigBuffer_DebugDumpCurrentState(bool verbose);

// End of debug APIs
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// LEGACY APIs ... these are deprecated and will soon be removed
//
// Stage 0: define the new APIs (above)
// Stage 1: migrate all code to use the new APIs
// Stage 2: mark the functions with deprecated attribute
// Stage 3: wait appropriate time for braches to be updated
// Stage 4: remove the deprecated functions entirely

//#define DEPRECATE_MEM_ALLOC
//#define DEPRECATE_MEM_FREE
#define DEPRECATE_MEM_ALLOC __attribute__((deprecated("use BigBuffer_AllocateTemporary() instead")))
#define DEPRECATE_MEM_FREE __attribute__((deprecated("use BigBuffer_FreeTemporary() instead")))

/// @brief Attempts to allocate a nand page buffer.
/// @return Pointer to the buffer if available, NULL if not available
/// @note Return value should always be checked against null.
/// @note Max size: SPI_NAND_PAGE_SIZE + SPI_NAND_OOB_SIZE
uint8_t* mem_alloc(size_t size, big_buffer_owner_t owner) DEPRECATE_MEM_ALLOC;
/// @brief Frees the allocated nand page buffer
/// @param ptr pointer to the nand page buffer
void     mem_free(uint8_t *ptr)                           DEPRECATE_MEM_FREE;

// End of legacy memory allocation APIs
////////////////////////////////////////////////////////////////////////////////



#endif // __MEM_H
