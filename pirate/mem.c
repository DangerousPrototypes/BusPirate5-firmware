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

// 136k ... 128k contiguous / 32k aligned required by current LA due to DMA engine configuration
// Plus an extra 8k that are generally safe for long-term allocations
// Plus an extra 2k for future tracking of allocations

#define BIG_BUFFER_TEMPORARY_BUFFER_KB (128u)
#define BIG_BUFFER_LONGLIVED_BUFFER_KB (  8u)
#define BIG_BUFFER_ALIGNMENT_KB        ( 32u)

// Why?  Currently, the way the LA DMA works, it requires 4x 32k-aligned, contiguously allocated buffers.
// AKA: a 128k contiguous, 32k-aligned buffer
static_assert(BIG_BUFFER_TEMPORARY_BUFFER_KB >= 128u, "BigBuffer size must be at least 128k for LA DMA architecture");
static_assert(BIG_BUFFER_ALIGNMENT_KB        >=  32u, "BigBuffer alignment must be at least 32k for LA DMA architecture");

#define BIG_BUFFER_SIZE ((BIG_BUFFER_TEMPORARY_BUFFER_KB + BIG_BUFFER_LONGLIVED_BUFFER_KB) * 1024u)
static uint8_t s_BigBufMemory[BIG_BUFFER_SIZE] __attribute__((aligned(BIG_BUFFER_ALIGNMENT_KB * 1024u))); 

// This 2k is going to be used for tracking allocations, and RFU (Reserved For Use) for future memory allocator features.
// For now, mark as requiring 8k alignment solely to have it "nearby" in the memory map.
#define REMAINING_RESERVED_2K_BUFFER ()

static uint8_t s_ReservedForAllocationTracking[1024u];
static big_buffer_general_state_t s_State = {0};
static big_buffer_allocation_instance_t s_Allocation[MAXIMUM_SUPPORTED_ALLOCATION_COUNT] = {0};

static void SortAllocationTrackingData(void) {
    // Sort the allocation tracking data by allocated address.
    return;
}

void DumpGeneralStateHeader(void) {
    return;
}
void DumpGeneralState(big_buffer_general_state_t * general_state) {
    return;
}
void DumpAllocationInstanceHeader(void) {
    return;
}
void DumpAllocationInstance(big_buffer_allocation_instance_t* alloc_state) { // TODO: type of this parameter TBD
    return;
}
void DumpFullMemoryState(void) {
    DumpGeneralStateHeader();
    DumpGeneralState(&s_State);
    SortAllocationTrackingData();
    DumpAllocationInstanceHeader();
    for (size_t j = 0; j < MAXIMUM_SUPPORTED_ALLOCATION_COUNT; ++j) {
        if (s_Allocation[j].result != 0u) {
            DumpAllocationInstance(&s_Allocation[j]);
        }
    }
}
size_t FindUnusedTrackingEntryIndex(void) {
    // This should never fail, because successful allocations are limited to MAXIMUM_SUPPORTED_ALLOCATION_COUNT
    for (size_t i = 0; i < MAXIMUM_SUPPORTED_ALLOCATION_COUNT; ++i) {
        if (s_Allocation[i].result == 0u) {
            return i;
        }
    }
    do { // Should never reach this point ... loop infinitely, dumping state every 5 seconds
        assert(false);
        sleep_ms(5000);
        DumpFullMemoryState();
    } while (1);
}


// uintptr_t allows cleaner addition/subtraction between pointers
static size_t    s_BigBufSize            = 0u; // cannot use until initialized
static uintptr_t s_BigBufHighWaterMark   = 0u; // cannot use until initialized
static uintptr_t s_BigBufLowWaterMark    = 0u; // cannot use until initialized
static uintptr_t s_BigBufLongTermMarker  = 0u; // cannot use until initialized

static uint16_t    s_BigBufTempAllocCount      = 0u;
static uint16_t    s_BigBufLongLivedAllocCount = 0u;
static bool        s_BigBufInitialized         = false;

/* Big Buffer Memory Layout:
   (high)  __BIG_BUFFER_END__    -> Pointer just past the end of Big Buffer
   (... )  ...                   -> Long-lived allocations (if any)
   (... )  s_BigBufHighWaterMark -> Pointer just past the last unallocated memory,
                                    or equal to low water mark if all memory allocated
   (... )  ...                   -> Unallocated memory ...
                                    Long-lived allocations grow from top
                                    while temporary allocations grow from bottom
   (... )  s_BigBufLowWaterMark  -> Pointer to the first unused byte of memory
   (... )  ...                   -> Temporary memory allocations
   (low )  __BIG_BUFFER_START__  -> Guaranteed to be 32k aligned
*/


void BigBuffer_Initialize(void) {
    printf("BB: Init()\n");
    if (s_BigBufInitialized) {
        assert(false); // should only call this once
        return;
    }

    s_State.buffer              = (uintptr_t)s_BigBufMemory;
    s_BigBufSize                = count_of(s_BigBufMemory);
    s_BigBufHighWaterMark       = s_State.buffer + s_BigBufSize;
    s_BigBufLowWaterMark        = s_State.buffer;
    s_BigBufLongTermMarker      = s_BigBufHighWaterMark - (BIG_BUFFER_LONGLIVED_BUFFER_KB * 1024u);
    s_BigBufTempAllocCount      = 0u;
    s_BigBufLongLivedAllocCount = 0u;

    printf("BB: BB @ %p, size %zu, high %p, low %p\n",
           s_State.buffer, s_BigBufSize, s_BigBufHighWaterMark, s_BigBufLowWaterMark
           );
    memset(s_BigBufMemory, 0xAA, s_BigBufSize);

    // TODO: initialize memory tracking buffer also
    memset(s_ReservedForAllocationTracking, 0x00, count_of(s_ReservedForAllocationTracking));

    s_BigBufInitialized = true;
}

typedef enum _big_buffer_invariant_error_flags {
    BIG_BUFFER_INVARIANT_NONE = 0,
    BIG_BUFFER_INVARIANT_CONSTANT_BIGBUF_POINTER           = 0x0001,
    BIG_BUFFER_INVARIANT_CONSTANT_BIGBUF_SIZE              = 0x0002,
    BIG_BUFFER_INVARIANT_CONSTANT_LONGTERM_MARKER          = 0x0004,
    BIG_BUFFER_INVARIANT_LOW_WATERMARK_VALUE_TOO_LOW       = 0x0008,
    BIG_BUFFER_INVARIANT_LOW_WATERMARK_VALUE_TOO_HIGH      = 0x0010,
    BIG_BUFFER_INVARIANT_HIGH_WATERMARK_VALUE_TOO_LOW      = 0x0020,
    BIG_BUFFER_INVARIANT_HIGH_WATERMARK_VALUE_TOO_HIGH     = 0x0040,
    BIG_BUFFER_INVARIANT_WATERMARKS_CROSSED                = 0x0080,

    BIG_BUFFER_INVARIANT_LOW_WATERMARK_AT_ZERO_TEMP_ALLOCS = 0x0100,
    BIG_BUFFER_INVARIANT_HIGH_WATERMARK_WITH_NO_ALLOCS     = 0x0200,

    BIG_BUFFER_INVARIANT_UNINITIALIZED_BUT_STORING_VALUES  = 0x8000,
} big_buffer_invariant_error_flags_t;

big_buffer_invariant_error_flags_t BigBuffer_InvariantsFailed(void) {
    uint32_t result_flags = 0u;
    if (s_BigBufInitialized) {
        // constant values
        if (s_State.buffer != (uintptr_t)(s_BigBufMemory)) {
            result_flags |= BIG_BUFFER_INVARIANT_CONSTANT_BIGBUF_POINTER;
        }
        if (s_BigBufSize != count_of(s_BigBufMemory)) {
            result_flags |= BIG_BUFFER_INVARIANT_CONSTANT_BIGBUF_SIZE;
        }
        if (s_BigBufLongTermMarker != s_State.buffer + s_BigBufSize - (BIG_BUFFER_LONGLIVED_BUFFER_KB * 1024u)) {
            result_flags |= BIG_BUFFER_INVARIANT_CONSTANT_LONGTERM_MARKER;
        }

        // Verify reasonable values for high/low watermarks
        if (s_BigBufLowWaterMark < s_State.buffer) {
            result_flags |= BIG_BUFFER_INVARIANT_LOW_WATERMARK_VALUE_TOO_LOW;
        }
        if (s_BigBufLowWaterMark > s_State.buffer + s_BigBufSize) {
            result_flags |= BIG_BUFFER_INVARIANT_LOW_WATERMARK_VALUE_TOO_HIGH;
        }
        if (s_BigBufHighWaterMark < s_BigBufLongTermMarker) {
            result_flags |= BIG_BUFFER_INVARIANT_HIGH_WATERMARK_VALUE_TOO_LOW;
        }
        if (s_BigBufHighWaterMark > s_State.buffer + s_BigBufSize) {
            result_flags |= BIG_BUFFER_INVARIANT_HIGH_WATERMARK_VALUE_TOO_HIGH;
        }
        if (s_BigBufLowWaterMark > s_BigBufHighWaterMark) {
            result_flags |= BIG_BUFFER_INVARIANT_WATERMARKS_CROSSED;
        }
        // if there are no temporary allocations...
        if (s_BigBufTempAllocCount == 0) {
            // then low water mark should always point to start of big buffer
            if (s_BigBufLowWaterMark != s_State.buffer) {
                result_flags |= BIG_BUFFER_INVARIANT_LOW_WATERMARK_AT_ZERO_TEMP_ALLOCS;
            }
        }
        // if neither short nor long-lived allocations...
        if ((s_BigBufTempAllocCount == 0) && (s_BigBufLongLivedAllocCount == 0)) {
            // high water mark should point to end of big buffer
            if (s_BigBufHighWaterMark != s_State.buffer + s_BigBufSize) {
                result_flags |= BIG_BUFFER_INVARIANT_HIGH_WATERMARK_WITH_NO_ALLOCS;
            }
        }
    } else {
        if((s_State.buffer              != 0u) ||
           (s_BigBufSize                != 0u) ||
           (s_BigBufHighWaterMark       != 0u) ||
           (s_BigBufLowWaterMark        != 0u) ||
           (s_BigBufLongTermMarker      != 0u) ||
           (s_BigBufTempAllocCount      != 0u) ||
           (s_BigBufLongLivedAllocCount != 0u)  ) {
            result_flags |= BIG_BUFFER_INVARIANT_UNINITIALIZED_BUT_STORING_VALUES;
        }
    }
    return result_flags;
}
void BigBuffer_VerifyNoTemporaryAllocations(void) {
    if (!s_BigBufInitialized) {
        assert(false); // this is recoverable in this instance, but still violates the API contract
        BigBuffer_Initialize(); 
    }
    big_buffer_invariant_error_flags_t error_flags = BigBuffer_InvariantsFailed();
    assert(error_flags == 0);
    assert(s_BigBufTempAllocCount == 0);
    // TODO: also verify no allocation tracking data lists a temporary allocation?
}
// TODO: macro to call BigBuffer_InvariantsFailed() and assert if non-zero result
//       this will simplify compiling this to nothing on release builds, and allow file / func / line numbers

uintptr_t BigBuffer_DetermineNewLowWaterMark(void) {
    // find the highest address allocated to tracked temporary allocations
    uint16_t allocations_found = 0u;
    uintptr_t new_low_water_mark = 0u;
    for (size_t i = 0; i < MAXIMUM_SUPPORTED_ALLOCATION_COUNT; ++i) {
        if (s_Allocation[i].result == 0u) continue;
        if (s_Allocation[i].was_long_lived_allocation) continue;
        ++allocations_found;
        uintptr_t end_of_allocation = ((uintptr_t)s_Allocation[i].result) + s_Allocation[i].requested_size;
        if (end_of_allocation > new_low_water_mark) {
            new_low_water_mark = end_of_allocation;
        }
    }
    assert(allocations_found == s_BigBufTempAllocCount);
    return new_low_water_mark;
}
uintptr_t BigBuffer_DetermineNewHighWaterMark(void) {
    // find the lowest address allocated to tracked long-lived allocations
    uint16_t allocations_found = 0u;
    uintptr_t new_high_water_mark = s_State.buffer + s_BigBufSize; // highest value ... when no long-lived allocations
    for (size_t i = 0; i < MAXIMUM_SUPPORTED_ALLOCATION_COUNT; ++i) {
        if (s_Allocation[i].result == 0u) continue;
        if (!s_Allocation[i].was_long_lived_allocation) continue;
        ++allocations_found;
        uintptr_t start_of_allocation = (uintptr_t)(s_Allocation[i].result);
        if (start_of_allocation < new_high_water_mark) {
            new_high_water_mark = start_of_allocation;
        }
    }
    assert(allocations_found == s_BigBufLongLivedAllocCount);
    return new_high_water_mark;
}

void* BigBuffer_AllocateTemporary(size_t countOfBytes, size_t requiredAlignment, big_buffer_owner_t owner) {
    size_t alignmentMask = requiredAlignment - 1;

    if (!s_BigBufInitialized) {
        printf("BB_AllocTemp: big buffer is not initialized?\n");
        assert(false); // BigBuffer_Initialize() must be called before attempting to allocate memory
        return NULL;
    }
    if ((requiredAlignment & alignmentMask) != 0u) {
        printf("BB_AllocTemp: alignment must be a power of 2\n");
        assert(false); // alignment must be a power of 2
        return NULL;
    }
    if (requiredAlignment == 0) {
        requiredAlignment = 1; // fixup common API error
    }
    if (requiredAlignment > 128 * 1024) {
        printf("BB_AllocLongLived: required alignment too large\n");
        return NULL;
    }
    if (s_BigBufLongLivedAllocCount + s_BigBufTempAllocCount >= MAXIMUM_SUPPORTED_ALLOCATION_COUNT) {
        printf("BB_AllocTemp: too many allocations\n");
        return NULL;
    }

    // exit early if the request number of bytes is larger than what's left
    // N.B. It might still not fit due to alignment requirements ... checked later.
    if (countOfBytes > s_BigBufHighWaterMark - s_BigBufLowWaterMark) {
        printf("BB_AllocTemp: requested %zu bytes > %zu bytes available\n", countOfBytes, s_BigBufHighWaterMark - s_BigBufLowWaterMark);
        return NULL;
    }
    
    // allocate temporary buffers from the lower end of the address space
    uintptr_t result = s_BigBufLowWaterMark;
    if ((result & alignmentMask) != 0u) {
        // adjust the allocation so that it's properly aligned ... move result to larger address
        result = (result + requiredAlignment) & ~alignmentMask;
        if (s_BigBufHighWaterMark - result < countOfBytes) {
            printf("BB_AllocTemp: insufficient space due to alignment requirement\n");
            return NULL;
        }
    }

    // adjust tracking data
    s_BigBufLowWaterMark = result + countOfBytes;
    ++s_BigBufTempAllocCount;
    size_t idx = FindUnusedTrackingEntryIndex();
    memset(&s_Allocation[idx], 0, sizeof(big_buffer_allocation_instance_t));
    s_Allocation[idx].result = result;
    s_Allocation[idx].requested_size = countOfBytes;
    s_Allocation[idx].requested_alignment = requiredAlignment;
    s_Allocation[idx].owner_tag = owner;
    s_Allocation[idx].was_long_lived_allocation = false;

    // finally, zero the memory before returning it.
    memset((void*)result, 0, countOfBytes);
    return (void*)result;
}
void* BigBuffer_AllocateLongLived(size_t countOfBytes, size_t requiredAlignment, big_buffer_owner_t owner) {
    size_t alignmentMask = requiredAlignment - 1;

    if (!s_BigBufInitialized) {
        printf("BB_AllocLongLived: big buffer is not initialized?\n");
        assert(false); // BigBuffer_Initialize() must be called before attempting to allocate memory
        return NULL;
    }
    if ((requiredAlignment & alignmentMask) != 0u) {
        printf("BB_AllocLongLived: alignment must be a power of 2\n");
        assert(false); // alignment must be a power of 2
        return NULL;
    }
    if (requiredAlignment == 0) {
        requiredAlignment = 1; // fixup common API error
    }
    if (requiredAlignment > 128 * 1024) {
        printf("BB_AllocLongLived: required alignment too large\n");
        return NULL;
    }
    if (s_BigBufLongLivedAllocCount + s_BigBufTempAllocCount >= MAXIMUM_SUPPORTED_ALLOCATION_COUNT) {
        printf("BB_AllocLongLived: too many allocations\n");
        return NULL;
    }

    // limit lower bound of the allocation
    uintptr_t lower_limit = s_BigBufLongTermMarker;
    if (s_BigBufLowWaterMark > lower_limit) {
        lower_limit = s_BigBufLowWaterMark;
    }

    // exit early if the request number of bytes is larger than what's left
    // N.B. It might still not fit due to alignment requirements ... checked later.
    if (countOfBytes > s_BigBufHighWaterMark - lower_limit) {
        printf("BB_AllocLongLived: requested %zu bytes > %zu bytes available\n", countOfBytes, s_BigBufHighWaterMark - lower_limit);
        return NULL;
    }

    uintptr_t result = s_BigBufHighWaterMark - countOfBytes;
    if ((result & alignmentMask) != 0u) {
        // adjust the allocation so that it's properly aligned ... move result to SMALLER address
        result = result & ~alignmentMask;
        if (lower_limit + countOfBytes > result) {
            printf("BB_AllocLongLived: insufficient space due to alignment requirement\n");
            return NULL;
        }
    }

    // adjust tracking data
    s_BigBufHighWaterMark = result;
    ++s_BigBufLongLivedAllocCount;
    size_t idx = FindUnusedTrackingEntryIndex();
    memset(&s_Allocation[idx], 0, sizeof(big_buffer_allocation_instance_t));
    s_Allocation[idx].result = result;
    s_Allocation[idx].requested_size = countOfBytes;
    s_Allocation[idx].requested_alignment = requiredAlignment;
    s_Allocation[idx].owner_tag = owner;
    s_Allocation[idx].was_long_lived_allocation = true;

    // finally, zero the memory before returning it.
    memset((void*)result, 0, countOfBytes);
    return (void*)result;
}

static big_buffer_allocation_instance_t* BigBuffer_FreeCommon(uintptr_t p, big_buffer_owner_t owner) {

    if (!s_BigBufInitialized) {
        printf("BB_Free: big buffer is not initialized?\n");
        assert(false); // BigBuffer_Initialize() must be called before attempting to free memory
        return NULL;
    }
    if (p == 0u) {
        printf("BB_Free: NULL pointer\n");
        assert(false); // NULL pointers are not valid
        return NULL;
    }
    if (p < s_State.buffer) {
        printf("BB_Free: pointer is before the start of the Big Buffer\n");
        assert(false); // ptr is before the start of the Big Buffer
        return NULL;
    }
    if ((p - s_State.buffer) >= s_BigBufSize) {
        printf("BB_Free: pointer is after the end of the Big Buffer\n");
        assert(false); // ptr is after the end of the Big Buffer
        return NULL;
    }

    // was this allocated at the given position?
    big_buffer_allocation_instance_t* allocated = NULL;
    for (size_t i = 0; i < MAXIMUM_SUPPORTED_ALLOCATION_COUNT; ++i) {
        if (s_Allocation[i].result == p) {
            allocated = &s_Allocation[i];
            break;
        }
    }
    if (allocated == NULL) {
        printf("BB_Free: pointer was not allocated from BigBuffer\n");
        assert(false); // ptr was not allocated by BigBuffer
        return NULL;
    }
    if (allocated->owner_tag != owner) {
        printf("BB_Free: owner tag mismatch: allocated by %d, free attempt by %d\n", allocated->owner_tag, owner);
        assert(false); // owner tag mismatch
        return NULL;
    }

    // all checks passed.  do NOT modify tracking data here, as it depends on type of allocation
    return allocated;
}
void BigBuffer_FreeTemporary(void* ptr, big_buffer_owner_t owner) {
    
    big_buffer_allocation_instance_t* allocation = BigBuffer_FreeCommon((uintptr_t)ptr, owner);
    if (allocation == NULL) {
        // already asserted in BigBuffer_FreeCommon()
        return;
    }
    if (allocation->was_long_lived_allocation) {
        printf("BB_FreeTemp: pointer was allocated as long-lived allocation.\n");
        assert(false); // ptr was not a temporary allocation
        return;
    }

    // zero this one's tracking data, which free's the memory and prevents it from affecting calculation of new watermarks.
    // Then, scan all the allocations to find a new low water mark.
    memset(allocation, 0, sizeof(big_buffer_allocation_instance_t));
    --s_BigBufTempAllocCount;
    s_BigBufLowWaterMark = BigBuffer_DetermineNewLowWaterMark();

    return;
}
void BigBuffer_FreeLongLived(void* ptr, big_buffer_owner_t owner) {
    big_buffer_allocation_instance_t* allocation = BigBuffer_FreeCommon((uintptr_t)ptr, owner);
    if (allocation == NULL) {
        // already asserted in BigBuffer_FreeCommon()
        return;
    }
    if (!allocation->was_long_lived_allocation) {
        printf("BB_FreeLongLived: pointer was allocated as temporary allocation.\n");
        assert(false); // ptr was not a long-lived allocation
        return;
    }

    // zero this one's tracking data, which free's the memory and prevents it from affecting calculation of new watermarks.
    // Then, scan all the allocations to find a new high water mark.
    memset(allocation, 0, sizeof(big_buffer_allocation_instance_t));
    --s_BigBufLongLivedAllocCount;
    s_BigBufHighWaterMark = BigBuffer_DetermineNewHighWaterMark();

    return;
}


bool BigBuffer_DebugGetStatistics( big_buffer_general_state_t * general_state_out ) {
    static_assert(sizeof(s_State) == sizeof(big_buffer_general_state_t));
    memcpy(general_state_out, &s_State, sizeof(big_buffer_general_state_t));
}
bool BigBuffer_DebugGetDetailedStatistics( big_buffer_state_t * state_out ) {
    static_assert(sizeof(s_State) == sizeof(state_out->general));
    static_assert(sizeof(s_Allocation) == sizeof(state_out->allocation));
    memcpy(&(state_out->general), &s_State, sizeof(big_buffer_general_state_t));
    memcpy(&(state_out->allocation[0]), &(s_Allocation[0]), sizeof(state_out->allocation));
}
size_t BigBuffer_GetAvailableTemporaryMemory(size_t requiredAlignment) {
    size_t alignmentMask = requiredAlignment - 1;
    if ((requiredAlignment & alignmentMask) != 0u) {
        printf("BB_GetAvailableTempMemory: alignment must be a power of 2\n");
        assert(false); // alignment must be a power of 2
        return 0u;
    }

    uintptr_t aligned_lower = (s_BigBufLowWaterMark + (requiredAlignment - 1)) & (~alignmentMask);
    if (aligned_lower >= s_BigBufHighWaterMark) {
        return 0u;
    }
    size_t result = s_BigBufHighWaterMark - aligned_lower;
    return result;
}
size_t BigBuffer_GetAvailableLongLivedMemory(size_t requiredAlignment) {
    size_t alignmentMask = requiredAlignment - 1;
    if ((requiredAlignment & alignmentMask) != 0u) {
        printf("BB_GetAvailableTempMemory: alignment must be a power of 2\n");
        assert(false); // alignment must be a power of 2
        return 0u;
    }
    uintptr_t lower = s_BigBufLongTermMarker;
    if (s_BigBufLowWaterMark > lower) {
        lower = s_BigBufLowWaterMark;
    }
    uintptr_t aligned_lower = (lower + (requiredAlignment - 1)) & (~alignmentMask);
    if (aligned_lower >= s_BigBufHighWaterMark) {
        return 0u;
    }
    size_t result = s_BigBufHighWaterMark - aligned_lower;
    return result;
}



// TODO: replace the below API with BigBuffer_xxxx() API.
static big_buffer_owner_t legacy_owner = BP_BIG_BUFFER_OWNER_NONE;
uint8_t *mem_alloc(size_t size, big_buffer_owner_t owner)
{
    uint8_t * result = BigBuffer_AllocateTemporary(size, 1u, owner);
    if (result != NULL) {
        legacy_owner = owner;
    }
    return result;
}
void mem_free(uint8_t *ptr)
{
    BigBuffer_FreeTemporary(ptr, legacy_owner);
    BigBuffer_VerifyNoTemporaryAllocations();
}