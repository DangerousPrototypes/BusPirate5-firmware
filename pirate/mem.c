/**
 * @file		mem.c
 * @author		Andrew Loebs
 * @brief		Implementation file of the memory management module
 *
 */
// Modified by Ian Lesnet 18 Dec 2023 for Bus Pirate 5
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // memset()
#include <stdint.h> // uint32_t, uintptr_t, ...
#include "pico/stdlib.h"
#include "pirate.h"
#include "pirate/mem.h"
#include "system_config.h"

// 138k ... 128k contiguous / 32k aligned required by current LA due to DMA engine configuration
#define BIG_BUFFER_SIZE (138 * 1024) 
static uint8_t s_BigBufMemory[BIG_BUFFER_SIZE] __attribute__((aligned(32768))); 



static uint8_t * s_BigBuf                = NULL; // cannot use until initialized
static size_t    s_BigBufSize            = 0u;    // cannot use until initialized
static uint8_t * s_BigBufHighWaterMark   = NULL; // cannot use until initialized
static uint8_t * s_BigBufLowWaterMark    = NULL; // cannot use until initialized
static size_t    s_BigBufAllocationCount = 0u;

/* Big Buffer Memory Layout:
   (high)  __BIG_BUFFER_END__    -> Pointer just past the end of Big Buffer
   (... )  ...                   -> Allocated memory, if any
   (... )  s_BigBufHighWaterMark -> Pointer just past the last free memory
   (... )  ...                   -> Unused memory ... could be allocated from top (small) or bottom (aligned)
   (... )  s_BigBufLowWaterMark  -> Pointer to the first unused byte of memory
   (... )  ...                   -> Large (>1k), or high-alignment requirement (1k+) allocated memory
   (low )  __BIG_BUFFER_START__  -> Guaranteed to be 32k aligned
*/

void BigBuffer_Initialize(void) {
    if (s_BigBufAllocationCount != 0u) {
        assert(false); // prior allocation is still outstanding?
    }

    s_BigBuf                = (uint8_t *)s_BigBufMemory;
    s_BigBufSize            = count_of(s_BigBufMemory);
    s_BigBufHighWaterMark   = (uint8_t *)s_BigBufMemory + s_BigBufSize;
    s_BigBufLowWaterMark    = (uint8_t *)s_BigBufMemory;
    s_BigBufAllocationCount = 0u;

    memset(s_BigBufMemory, 0xAA, s_BigBufSize);
}
void BigBuffer_Finalize(void) {
    if (s_BigBufAllocationCount != 0u) {
        assert(false); // prior allocation is still outstanding?
    }
    // clear all the pointers to prevent accidental use after finalization
    s_BigBuf                = NULL;
    s_BigBufSize            = 0;
    s_BigBufHighWaterMark   = NULL;
    s_BigBufLowWaterMark    = NULL;
    s_BigBufAllocationCount = 0;
}

/// @brief Allocates a block of memory from the Big Buffer
/// @param elementCount Number of elements to be allocated
/// @param elementSize Size of each element (e.g., sizeof(T))
/// @param alignment Alignment required for the allocation (e.g., _Alignof(T))
/// @param owner_tag A debugging helper ... allocations can be tracked by owner
void* BigBuffer_calloc(size_t elementCount, size_t elementSize, size_t alignment, uint32_t owner_tag) {
    if (s_BigBuf == NULL) {
        assert(false); // BigBuffer_Initialize() must be called before BigBuffer_calloc()
        return NULL;
    }
    if ((alignment & (alignment - 1)) != 0u) {
        assert(false); // alignment must be a power of 2
        return NULL;
    }
    if (elementCount == 0 || elementSize == 0) {
        assert(false); // calloc is undefined for zero elements or zero size
        return NULL;
    }
    if (alignment == 0) {
        alignment = 1; // fixup common API error
    }
    if (elementCount > (SIZE_MAX / elementSize)) {
        // calculating total size would overflow
        return NULL;
    }
    size_t totalRequestedBytes = elementCount * elementSize;
    // exit early if the request number of bytes is larger than what's left
    // Note: it might still not fit due to alignment requirements ... checked later.
    if (totalRequestedBytes > (size_t)(s_BigBufHighWaterMark - s_BigBufLowWaterMark)) {
        return NULL;
    }

    // Are we allocating from the bottom (large alignment) or from the top (small alignment)?
    bool allocateFromBottom = (alignment >= 1024) || (totalRequestedBytes >= 1024);
    uint8_t * result =
        allocateFromBottom ?
        s_BigBufLowWaterMark :
        s_BigBufHighWaterMark - totalRequestedBytes;

    // If result pointer is NOT already aligned, adjust it.
    size_t alignmentMask = alignment - 1;
    if (((size_t)result & alignmentMask) != 0) {
        if (allocateFromBottom) {
            // Allocation must be moved to larger address
            result = (uint8_t*)( ((size_t)result + alignment) & ~alignmentMask );
            if ((size_t)s_BigBufHighWaterMark - (size_t)result < totalRequestedBytes) {
                // insufficient space to allocate, due to alignment requirement
                return NULL;
            }
        } else {
            // Allocation must be moved to smaller address
            result = (uint8_t*)( (size_t)result & ~alignmentMask );
            if (result < s_BigBufLowWaterMark) {
                // insufficient space to allocate, due to alignment requirement
                return NULL;
            }
        }
    }

    // result now points to a valid, aligned, unallocated memory block of appropriate size.
    // update high/low water marks, and return the buffer.
    if (allocateFromBottom) {
        s_BigBufLowWaterMark = result + totalRequestedBytes;
    } else {
        s_BigBufHighWaterMark = result;
    }

    // finally, zero the memory before returning it.
    memset(result, 0, totalRequestedBytes);
    ++s_BigBufAllocationCount;
    return result;
}

/// @brief Frees a block of memory that was allocated from the Big Buffer
/// @param ptr Pointer to the memory block to be free'd.
/// @note At least initially, the memory is not returned for re-use.
/// @todo Allow the most-recent allocated memory to actually be free'd.
///       This will likely require a more complex allocation scheme:
///       Maybe just keep two arrays of what was allocated?
///       Only allow the most recent large and most recent small allocations
///       to be free'd (which then makes a new prior allocation able to be free'd)?
///       Thus, so long as order of free() is exact inverse of alloc(), this
///       would allow reverting to initial state?
void BigBuffer_free(void* ptr, uint32_t owner_tag) {
    // at a minimum, assert that the pointer is somewhere within the Big Buffer space
    // NOTE: Technically, comparisons of pointers is undefined behavior in C.
    if ((uintptr_t)ptr < (uintptr_t)s_BigBuf) {
        assert(false); // ptr is before the start of the Big Buffer
    }
    else if ((uintptr_t)ptr >= (uintptr_t)s_BigBufHighWaterMark) {
        assert(false); // ptr is after the end of the Big Buffer
    }
    // TODO: when tracking allocations, validate owner_tag matches (or parameter is zero)
    // else if (owner_tag shows a mismatch)
    // TODO: when tracking allocations, validate that ptr was allocated by BigBuffer_calloc()
    // else if (not a pointer allocated by BigBuffer_calloc())
    else {
        --s_BigBufAllocationCount;
    }
}

// TODO: replace the below API with BigBuffer_xxxx() API.
uint8_t *mem_alloc(size_t size, uint32_t owner)
{
    return BigBuffer_calloc(size, 1u, 1u, owner);
}
void mem_free(uint8_t *ptr)
{
    BigBuffer_free(ptr, 0);
    // mem_alloc()/mem_free() only allowed a single allocation.
    // To match existing behavior, reset the memory pool when
    // any allocation is free()'d by this API.
    BigBuffer_Initialize();
}