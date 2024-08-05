/**
 * @file		mem.c
 * @author		Henry Gabryjelski
 * @brief		Implementation file of the memory management module
 * @details     Big Buffer Memory Layout:
 *              ( high )  __BIG_BUFFER_END__     -> Pointer just past the end of Big Buffer
 *              ( ...  )  ...                    -> Long-lived allocations (if any)
 *              ( v--^ )  s_State.high_watermark -> Pointer just past the last unallocated
 *                                                  memory, equal to low water mark if all
 *                                                  all memory allocated, or __BIG_BUFFER_END__
 *                                                  -8k (long-lived allocations)
 *              ( ...  )  ...                    -> Unallocated memory ...
 *                                                Long-lived allocations grow from top
 *                                                while temporary allocations grow from bottom
 *              ( v--^ )  s_State.low_watermark  -> Pointer to the first unused byte of memory
 *              ( ...  )  ...                    -> Temporary memory allocations
 *              ( low  )  __BIG_BUFFER_START__   -> Guaranteed to be 32k aligned
 **/


// Modified by Ian Lesnet 18 Dec 2023 for Bus Pirate 5
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // memset()
#include <stdint.h> // uint32_t, uintptr_t, ...
#include "pico/stdlib.h"
#include "pirate.h"
#include "pirate/mem.h"
#include "system_config.h"

#define DEBUG_BIGBUFFER

// 136k ... 128k contiguous / 32k aligned required by current LA due to DMA engine configuration
// Plus an extra 8k that are generally safe for long-term allocations
// Plus an extra 2k for future tracking of allocations

#define BIG_BUFFER_TEMPORARY_BUFFER_KB (128u)
#define BIG_BUFFER_LONGLIVED_BUFFER_KB (  8u)
#define BIG_BUFFER_ALIGNMENT_KB        ( 32u)
#define BIG_BUFFER_SIZE ((BIG_BUFFER_TEMPORARY_BUFFER_KB + BIG_BUFFER_LONGLIVED_BUFFER_KB) * 1024u)

// TODO: define DEBUG_BIGBUFFER to bitmask, to enable specific types of output at compilation
#if defined(DEBUG_BIGBUFFER)
    #define X_FN_ENTRY(fmt, ...)
    #define X_FN_EXIT(fmt, ...)

    // #define X_FN_ENTRY(fmt, ...) printf("BB @ %3d: >>>>> %s" fmt "\n", __LINE__, __func__, ##__VA_ARGS__)
    // #define X_FN_EXIT(fmt, ...)  printf("BB @ %3d: <<<<< %s" fmt "\n", __LINE__, __func__, ##__VA_ARGS__)
    #define X_DBGP(fmt, ...)     printf("BB @ %3d: " fmt "\n", __LINE__, ##__VA_ARGS__)
    #define X_SUCCESS(fmt, ...)  printf("BB @ %3d: SUCCESS " fmt "\n", __LINE__, ##__VA_ARGS__)
    #define X_FAILURE(fmt, ...)  printf("BB @ %3d: *FAILED* " fmt "\n", __LINE__, ##__VA_ARGS__)

    #define X_CHECK_INVARIANTS()                                                                                         \
        do {                                                                                                             \
            big_buffer_invariant_error_flags_t error_flags = BigBuffer_InvariantsFailed();                               \
            static_assert(BIG_BUFFER_INVARIANT_NONE == 0);                                                               \
            while (error_flags != 0) {                                                                                   \
                printf("BB @ %3d: *FATAL* Invariant failed from %s:%s:%d\n", error_flags, __FILE__, __func__, __LINE__); \
                BigBuffer_DebugDumpCurrentState(true);                                                                   \
                assert(false);                                                                                           \
            }                                                                                                            \
        } while (0)

    #define X_FATAL(fmt, ...)                                               \
        do {                                                                \
            printf("BB @ %3d: *FATAL* " fmt "\n", __LINE__, ##__VA_ARGS__); \
            BigBuffer_DebugDumpCurrentState(true);                          \
            assert(false);                                                  \
        } while (1)

#else
    #define X_FN_ENTRY(fmt, ...)
    #define X_FN_EXIT(fmt, ...)
    #define X_DBGP(fmt, ...)
    #define X_SUCCESS(fmt, ...)
    #define X_FAILURE(fmt, ...)

    #define X_CHECK_INVARIANTS()
    #define X_FATAL(fmt, ...) assert(false)
#endif


typedef enum _big_buffer_invariant_error_flags {
    BIG_BUFFER_INVARIANT_NONE = 0,
    BIG_BUFFER_INVARIANT_CONSTANT_BIGBUF_POINTER           = 0x0001,
    BIG_BUFFER_INVARIANT_CONSTANT_BIGBUF_SIZE              = 0x0002,
    BIG_BUFFER_INVARIANT_CONSTANT_LONGTERM_MARKER          = 0x0004,
    BIG_BUFFER_INVARIANT_WATERMARKS_CROSSED                = 0x0008,
    BIG_BUFFER_INVARIANT_LOW_WATERMARK_VALUE_TOO_LOW       = 0x0010,
    BIG_BUFFER_INVARIANT_LOW_WATERMARK_VALUE_TOO_HIGH      = 0x0020,
    BIG_BUFFER_INVARIANT_HIGH_WATERMARK_VALUE_TOO_LOW      = 0x0040,
    BIG_BUFFER_INVARIANT_HIGH_WATERMARK_VALUE_TOO_HIGH     = 0x0080,

    BIG_BUFFER_INVARIANT_LOW_WATERMARK_AT_ZERO_TEMP_ALLOCS = 0x0100,
    BIG_BUFFER_INVARIANT_HIGH_WATERMARK_WITH_NO_ALLOCS     = 0x0200,

    BIG_BUFFER_INVARIANT_UNINITIALIZED_BUT_STORING_VALUES  = 0x8000,
} big_buffer_invariant_error_flags_t;


// Why?  Currently, the way the LA DMA works, it requires 4x 32k-aligned, contiguously allocated buffers.
// AKA: a 128k contiguous, 32k-aligned buffer
static_assert(BIG_BUFFER_TEMPORARY_BUFFER_KB >= 128u, "BigBuffer size must be at least 128k for LA DMA architecture");
static_assert(BIG_BUFFER_ALIGNMENT_KB        >=  32u, "BigBuffer alignment must be at least 32k for LA DMA architecture");

static uint8_t s_BigBufMemory[BIG_BUFFER_SIZE] __attribute__((aligned(BIG_BUFFER_ALIGNMENT_KB * 1024u))); 


static bool s_BigBufInitialized = false;
static uint8_t s_ReservedForFutureAllocationTracking[1024u];
static big_buffer_general_state_t s_State = {0};
static big_buffer_allocation_instance_t s_Allocation[MAXIMUM_SUPPORTED_ALLOCATION_COUNT] = {0};


static big_buffer_invariant_error_flags_t BigBuffer_InvariantsFailed(void) {
    uint32_t result_flags = 0u;
    if (s_BigBufInitialized) {
        // constant values
        if (s_State.buffer != (uintptr_t)(s_BigBufMemory)) {
            result_flags |= BIG_BUFFER_INVARIANT_CONSTANT_BIGBUF_POINTER;
        }
        if (s_State.total_size != count_of(s_BigBufMemory)) {
            result_flags |= BIG_BUFFER_INVARIANT_CONSTANT_BIGBUF_SIZE;
        }
        if (s_State.long_lived_limit != s_State.buffer + s_State.total_size - (BIG_BUFFER_LONGLIVED_BUFFER_KB * 1024u)) {
            result_flags |= BIG_BUFFER_INVARIANT_CONSTANT_LONGTERM_MARKER;
        }

        // Verify reasonable values for high/low watermarks
        if (s_State.low_watermark < s_State.buffer) {
            result_flags |= BIG_BUFFER_INVARIANT_LOW_WATERMARK_VALUE_TOO_LOW;
        }
        if (s_State.low_watermark > s_State.buffer + s_State.total_size) {
            result_flags |= BIG_BUFFER_INVARIANT_LOW_WATERMARK_VALUE_TOO_HIGH;
        }
        if (s_State.high_watermark < s_State.long_lived_limit) {
            result_flags |= BIG_BUFFER_INVARIANT_HIGH_WATERMARK_VALUE_TOO_LOW;
        }
        if (s_State.high_watermark > s_State.buffer + s_State.total_size) {
            result_flags |= BIG_BUFFER_INVARIANT_HIGH_WATERMARK_VALUE_TOO_HIGH;
        }
        if (s_State.low_watermark > s_State.high_watermark) {
            result_flags |= BIG_BUFFER_INVARIANT_WATERMARKS_CROSSED;
        }
        // if there are no temporary allocations...
        if (s_State.temp_allocations_count == 0) {
            // then low water mark should always point to start of big buffer
            if (s_State.low_watermark != s_State.buffer) {
                result_flags |= BIG_BUFFER_INVARIANT_LOW_WATERMARK_AT_ZERO_TEMP_ALLOCS;
            }
        }
        // if neither short nor long-lived allocations...
        if ((s_State.temp_allocations_count       == 0) &&
            (s_State.long_lived_allocations_count == 0)  ) {
            // high water mark should point to end of big buffer
            if (s_State.high_watermark != s_State.buffer + s_State.total_size) {
                result_flags |= BIG_BUFFER_INVARIANT_HIGH_WATERMARK_WITH_NO_ALLOCS;
            }
        }
    } else {
        if((s_State.buffer                       != 0u) ||
           (s_State.total_size                   != 0u) ||
           (s_State.high_watermark               != 0u) ||
           (s_State.low_watermark                != 0u) ||
           (s_State.long_lived_limit             != 0u) ||
           (s_State.temp_allocations_count       != 0u) ||
           (s_State.long_lived_allocations_count != 0u)  ) {
            result_flags |= BIG_BUFFER_INVARIANT_UNINITIALIZED_BUT_STORING_VALUES;
        }
    }
    return result_flags;
}
static void SortAllocationTrackingData(void) {
    // Only 32 maximum entries, so N*N algorithm is only 1024 iterations.
    // If max allocation count is increased, consider a more efficient sorting algorithm.
    static_assert(MAXIMUM_SUPPORTED_ALLOCATION_COUNT <= 32);

    for (size_t dest = 0; dest < MAXIMUM_SUPPORTED_ALLOCATION_COUNT - 1; ++dest) {
        // find the smallest remaining non-null index
        // Sort by allocated address ()
        size_t smallest = dest;
        for (size_t i = dest + 1; i < MAXIMUM_SUPPORTED_ALLOCATION_COUNT; ++i) {
            if (s_Allocation[i].result == 0u) {
                // ignore NULL pointers (non-allocated)
            } else if (s_Allocation[i].result > s_Allocation[smallest].result) {
                // ignore because not a smaller address
            } else if (s_Allocation[i].result < s_Allocation[smallest].result) {
                smallest = i; // update smallest index because it's smaller
            } else {
                // two allocations at the same address should never happen ...
                assert(false);
            }
        }
        // swap the dest and smallest indices
        if (smallest != dest) {
            big_buffer_allocation_instance_t tmp = s_Allocation[dest];
            s_Allocation[dest] = s_Allocation[smallest];
            s_Allocation[smallest] = tmp;
        }
    }
    return;
}

static void DumpGeneralStateHeader(void) {
    printf("\n");
    printf("BB: Buffer*  | TotalSize | HighWater | LowWater | TmpCnt | LongCnt\n");
    printf("    ---------|-----------|-----------|----------|--------|--------\n");
    //intf("BB: ..xxxx.. |  ..xxxx.. |  ..xxxx.. | ..xxxx.. |   .xx. |    .xx.\n");
    return;
}
static void DumpGeneralState(const big_buffer_general_state_t * general_state) {
    printf("BB: %08x |  %08x |  %08x | %08x |   %04x |    %04x\n",
           general_state->buffer,
           general_state->total_size,
           general_state->high_watermark,
           general_state->low_watermark,
           general_state->temp_allocations_count,
           general_state->long_lived_allocations_count
           );
    return;
}
static void DumpAllocationInstanceHeader(void) {
    printf("    ------------------------------------------------------\n");
    printf("    Buffer*  |  Req_Size | Req_Align | OwnerTag | Type\n");
    //intf("    ..xxxx.. |  ..xxxx.. |  ..xxxx.. | ..xxxx.. | .ss.\n");
    return;
}
static void DumpAllocationInstance(const big_buffer_allocation_instance_t* alloc_state) {
    printf(
        "    %08x |  %08x |  %08x | %08x | %4s\n",
        alloc_state->result,
        alloc_state->requested_size,
        alloc_state->requested_alignment,
        alloc_state->owner_tag,
        (alloc_state->was_long_lived_allocation) ? "long" : "temp"
        );
    return;
}


static size_t FindUnusedTrackingEntryIndex(void) {
    // This should never fail, because successful allocations are limited to MAXIMUM_SUPPORTED_ALLOCATION_COUNT
    for (size_t i = 0; i < MAXIMUM_SUPPORTED_ALLOCATION_COUNT; ++i) {
        if (s_Allocation[i].result == 0u) {
            return i;
        }
    }
    do { // Should never reach this point ... loop infinitely, dumping state every 5 seconds
        assert(false);
        sleep_ms(5000);
        BigBuffer_DebugDumpCurrentState(true);
    } while (1);
}



void BigBuffer_Initialize(void) {
    X_FN_ENTRY("");

    X_CHECK_INVARIANTS();
    printf("BB: Init()\n");
    if (s_BigBufInitialized) {
        assert(false); // should only call this once
    } else {
        memset(&s_State, 0, sizeof(s_State));
        s_State.buffer                       = (uintptr_t)s_BigBufMemory;
        s_State.total_size                   = count_of(s_BigBufMemory);
        s_State.high_watermark               = s_State.buffer + s_State.total_size;
        s_State.low_watermark                = s_State.buffer;
        s_State.long_lived_limit             = s_State.high_watermark - (BIG_BUFFER_LONGLIVED_BUFFER_KB * 1024u);
        s_State.temp_allocations_count       = 0u;
        s_State.long_lived_allocations_count = 0u;

        printf("BB: BB @ %p, size %zu, high %p, low %p\n",
            s_State.buffer, s_State.total_size, s_State.high_watermark, s_State.low_watermark
            );
        memset(s_BigBufMemory, 0xAA, s_State.total_size);

        // Initialize memory tracking buffer also
        memset(&s_Allocation[0], 0, sizeof(s_Allocation));

        s_BigBufInitialized = true;
    }
    X_CHECK_INVARIANTS();
    X_FN_EXIT("");
}

void BigBuffer_VerifyNoTemporaryAllocations(void) {
    X_FN_ENTRY("");

    if (!s_BigBufInitialized) {
        assert(false); // this is recoverable in this instance, but still violates the API contract
        BigBuffer_Initialize(); 
    }
    X_CHECK_INVARIANTS();
    assert(s_State.temp_allocations_count == 0);
    // TODO: also verify no allocation tracking data lists a temporary allocation?
    X_FN_EXIT("");
}
// TODO: macro to call BigBuffer_InvariantsFailed() and assert if non-zero result
//       this will simplify compiling this to nothing on release builds, and allow file / func / line numbers

static uintptr_t BigBuffer_DetermineNewLowWaterMark(void) {
    if (s_State.temp_allocations_count == 0) {
        return s_State.buffer;
    }
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
    assert(allocations_found == s_State.temp_allocations_count);
    return new_low_water_mark;
}
static uintptr_t BigBuffer_DetermineNewHighWaterMark(void) {
    if (s_State.long_lived_allocations_count == 0) {
        return s_State.buffer + s_State.total_size;
    }
    // find the lowest address allocated to tracked long-lived allocations
    uint16_t allocations_found = 0u;
    uintptr_t new_high_water_mark = s_State.buffer + s_State.total_size; // highest value ... when no long-lived allocations
    for (size_t i = 0; i < MAXIMUM_SUPPORTED_ALLOCATION_COUNT; ++i) {
        if (s_Allocation[i].result == 0u) continue;
        if (!s_Allocation[i].was_long_lived_allocation) continue;
        ++allocations_found;
        uintptr_t start_of_allocation = (uintptr_t)(s_Allocation[i].result);
        if (start_of_allocation < new_high_water_mark) {
            new_high_water_mark = start_of_allocation;
        }
    }
    assert(allocations_found == s_State.long_lived_allocations_count);
    return new_high_water_mark;
}

void* BigBuffer_AllocateTemporary(size_t countOfBytes, size_t requiredAlignment, big_buffer_owner_t owner) {
    X_FN_ENTRY("(%zu, %zu, %d)", countOfBytes, requiredAlignment, owner);
    X_CHECK_INVARIANTS();

    uintptr_t result = 0;
    if (requiredAlignment == 0) {
        requiredAlignment = 1; // fixup common API error
    }
    size_t alignmentMask = requiredAlignment - 1;


    if (!s_BigBufInitialized) {
        printf("BB_AllocTemp: big buffer is not initialized?\n");
        assert(false); // BigBuffer_Initialize() must be called before attempting to allocate memory
    } else if ((requiredAlignment & alignmentMask) != 0u) {
        printf("BB_AllocTemp: alignment (%#zx) must be a power of 2\n", requiredAlignment);
        assert(false); // alignment must be a power of 2
    } else if (requiredAlignment > 128u * 1024u) {
        printf("BB_AllocLongLived: required (%#zx) alignment too large\n", requiredAlignment);
    } else if (s_State.long_lived_allocations_count + s_State.temp_allocations_count >= MAXIMUM_SUPPORTED_ALLOCATION_COUNT) {
        printf("BB_AllocTemp: too many allocations\n");
    } else if (countOfBytes == 0) {
        printf("BB_AllocTemp: returning NULL for zero-byte allocation request");
    } else if (countOfBytes > s_State.high_watermark - s_State.low_watermark) {
        // exit early if the request number of bytes is larger than what's left
        // N.B. It might still not fit due to alignment requirements ... checked later.
        printf("BB_AllocTemp: returning NULL because requested %zu bytes > %zu bytes available\n", countOfBytes, s_State.high_watermark - s_State.low_watermark);
    } else {
        // allocate temporary buffers from the lower end of the address space
        result = s_State.low_watermark;
        if ((result & alignmentMask) != 0u) {
            // adjust the allocation so that it's properly aligned ... move result to larger address
            result = (result + requiredAlignment) & ~alignmentMask;
            if (s_State.high_watermark - result < countOfBytes) {
                printf("BB_AllocTemp: insufficient space due to alignment requirement (%#zx bytes @ %#zx align)\n", countOfBytes, requiredAlignment);
                result = 0;
            }
        }
    }

    if (result != 0) {
        // adjust tracking data
        s_State.low_watermark = result + countOfBytes;
        ++s_State.temp_allocations_count;
        size_t idx = FindUnusedTrackingEntryIndex();
        memset(&s_Allocation[idx], 0, sizeof(big_buffer_allocation_instance_t));
        s_Allocation[idx].result = result;
        s_Allocation[idx].requested_size = countOfBytes;
        s_Allocation[idx].requested_alignment = requiredAlignment;
        s_Allocation[idx].owner_tag = owner;
        s_Allocation[idx].was_long_lived_allocation = false;

        // finally, zero the memory before returning it.
        memset((void*)result, 0, countOfBytes);
    }

    X_CHECK_INVARIANTS();
    X_FN_EXIT("(%zu, %zu, %d) -> %zx", countOfBytes, requiredAlignment, owner, result);
    return (void*)result;
}
void* BigBuffer_AllocateLongLived(size_t countOfBytes, size_t requiredAlignment, big_buffer_owner_t owner) {
    X_FN_ENTRY("(%zu, %zu, %d)", countOfBytes, requiredAlignment, owner);
    X_CHECK_INVARIANTS();

    uintptr_t result = 0;
    if (requiredAlignment == 0) {
        requiredAlignment = 1; // fixup common API error
    }
    size_t alignmentMask = requiredAlignment - 1;
    // limit lower bound of the allocation
    uintptr_t lower_limit = s_State.long_lived_limit;
    if (s_State.low_watermark > lower_limit) {
        lower_limit = s_State.low_watermark;
    }

    if (!s_BigBufInitialized) {
        printf("BB_AllocLongLived: big buffer is not initialized?\n");
        assert(false); // BigBuffer_Initialize() must be called before attempting to allocate memory
    } else if ((requiredAlignment & alignmentMask) != 0u) {
        printf("BB_AllocLongLived: alignment (%#zx) must be a power of 2\n", requiredAlignment);
        assert(false); // alignment must be a power of 2
    } else if (requiredAlignment > 128u * 1024u) {
        printf("BB_AllocLongLived: required alignment (%#zx) too large\n", requiredAlignment);
    } else if (s_State.long_lived_allocations_count + s_State.temp_allocations_count >= MAXIMUM_SUPPORTED_ALLOCATION_COUNT) {
        printf("BB_AllocLongLived: too many allocations\n");
    } else if (countOfBytes == 0) {
        printf("BB_AllocTemp: returning NULL for zero-byte allocation request");
    } else if (countOfBytes > s_State.high_watermark - lower_limit) {
        // exit early if the request number of bytes is larger than what's left
        // N.B. It might still not fit due to alignment requirements ... checked later.
        printf("BB_AllocLongLived: returning NULL because requested %zu bytes > %zu bytes available\n", countOfBytes, s_State.high_watermark - lower_limit);
    } else {
        result = s_State.high_watermark - countOfBytes;
        if ((result & alignmentMask) != 0u) {
            // adjust the allocation so that it's properly aligned ... move result to SMALLER address
            result = result & ~alignmentMask;
            if (lower_limit > result) {
                printf("BB_AllocLongLived: insufficient space due to alignment requirement (%#zx bytes @ %#zx align)\n", countOfBytes, requiredAlignment);
                result = 0;
            }
        }
    }

    if (result != 0u) {
        // adjust tracking data
        s_State.high_watermark = result;
        ++s_State.long_lived_allocations_count;
        size_t idx = FindUnusedTrackingEntryIndex();
        memset(&s_Allocation[idx], 0, sizeof(big_buffer_allocation_instance_t));
        s_Allocation[idx].result = result;
        s_Allocation[idx].requested_size = countOfBytes;
        s_Allocation[idx].requested_alignment = requiredAlignment;
        s_Allocation[idx].owner_tag = owner;
        s_Allocation[idx].was_long_lived_allocation = true;

        // finally, zero the memory before returning it.
        memset((void*)result, 0, countOfBytes);
    }
    X_CHECK_INVARIANTS();
    X_FN_EXIT("(%zu, %zu, %d) -> %zx", countOfBytes, requiredAlignment, owner, result);
    return (void*)result;
}

static big_buffer_allocation_instance_t* BigBuffer_FreeCommon(uintptr_t p, big_buffer_owner_t owner) {

    if (!s_BigBufInitialized) {
        printf("BB_Free: big buffer is not initialized?\n");
        assert(false); // BigBuffer_Initialize() must be called before attempting to free memory
        return NULL;
    }
    if (p == 0u) { // permit the free'ing of a nullptr ...
        //printf("BB_Free: NULL pointer\n");
        return NULL;
    }
    if (p < s_State.buffer) {
        printf("BB_Free: pointer %#010zx is before the start of the Big Buffer %#010zx\n", p, s_State.buffer);
        assert(false); // ptr is before the start of the Big Buffer
        return NULL;
    }
    if (p >= (s_State.total_size + s_State.buffer)) {
        printf("BB_Free: pointer %#010zx is after the end of the Big Buffer %#010zx\n", p, s_State.buffer + s_State.total_size);
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
        printf("BB_Free: pointer %#010zx was not allocated from BigBuffer (%#010zx .. %#010zx)\n", p, s_State.buffer, s_State.buffer + s_State.total_size);
        assert(false); // ptr was not allocated by BigBuffer
        return NULL;
    }
    if (allocated->owner_tag != owner) {
        printf("BB_Free: owner tag mismatch: point %#010zx allocated by %d, free attempted by %d\n", p, allocated->owner_tag, owner);
        assert(false); // owner tag mismatch
        return NULL;
    }

    // all checks passed.  do NOT modify tracking data here, as it depends on type of allocation
    return allocated;
}
void BigBuffer_FreeTemporary(const void * ptr, big_buffer_owner_t owner) {
    X_FN_ENTRY("(%p, %d)", ptr, owner);
    X_CHECK_INVARIANTS();

    big_buffer_allocation_instance_t* allocation = BigBuffer_FreeCommon((uintptr_t)ptr, owner);
    if (allocation == NULL) {
        // already asserted in BigBuffer_FreeCommon(), or was free'ing NULL
    } else if (allocation->was_long_lived_allocation) {
        printf("BB_FreeTemp: pointer %#010zx was allocated as long-lived allocation.\n");
        assert(false); // ptr was not a temporary allocation
    } else {
        // zero this one's tracking data, which free's the memory and prevents it from affecting calculation of new watermarks.
        // Then, scan all the allocations to find a new low water mark.
        memset(allocation, 0, sizeof(big_buffer_allocation_instance_t));
        --s_State.temp_allocations_count;
        s_State.low_watermark = BigBuffer_DetermineNewLowWaterMark();
    }

    X_CHECK_INVARIANTS();
    X_FN_EXIT("(%p, %d)", ptr, owner);
    return;
}
void BigBuffer_FreeLongLived(const void * ptr, big_buffer_owner_t owner) {
    X_FN_ENTRY("(%p, %d)", ptr, owner);
    X_CHECK_INVARIANTS();

    big_buffer_allocation_instance_t* allocation = BigBuffer_FreeCommon((uintptr_t)ptr, owner);
    if (allocation == NULL) {
        // already asserted in BigBuffer_FreeCommon(), or was free'ing NULL
    } else if (!allocation->was_long_lived_allocation) {
        printf("BB_FreeLongLived: pointer %#010zx was allocated as temporary allocation.\n");
        assert(false); // ptr was not a long-lived allocation
    } else {
        // zero this one's tracking data, which free's the memory and prevents it from affecting calculation of new watermarks.
        // Then, scan all the allocations to find a new high water mark.
        memset(allocation, 0, sizeof(big_buffer_allocation_instance_t));
        --s_State.long_lived_allocations_count;
        s_State.high_watermark = BigBuffer_DetermineNewHighWaterMark();
    }

    X_CHECK_INVARIANTS();
    X_FN_EXIT("(%p, %d)", ptr, owner);
    return;
}

void BigBuffer_DebugGetStatistics( big_buffer_general_state_t * general_state_out ) {
    X_FN_ENTRY("(%p)", general_state_out);
    X_CHECK_INVARIANTS();

    static_assert(sizeof(s_State) == sizeof(big_buffer_general_state_t));
    X_CHECK_INVARIANTS();

    memcpy(general_state_out, &s_State, sizeof(big_buffer_general_state_t));

    X_CHECK_INVARIANTS();
    X_FN_EXIT("(%p)", general_state_out);
    return;
}
void BigBuffer_DebugGetDetailedStatistics( big_buffer_state_t * state_out ) {
    X_FN_ENTRY("(%p)", state_out);
    X_CHECK_INVARIANTS();

    static_assert(sizeof(s_State) == sizeof(state_out->general));
    static_assert(sizeof(s_Allocation) == sizeof(state_out->allocation));
    X_CHECK_INVARIANTS();

    memcpy(&(state_out->general), &s_State, sizeof(big_buffer_general_state_t));
    memcpy(&(state_out->allocation[0]), &(s_Allocation[0]), sizeof(state_out->allocation));

    X_CHECK_INVARIANTS();
    X_FN_EXIT("(%p)", state_out);
    return;
}
void BigBuffer_DebugDumpSummary(const big_buffer_general_state_t * state) {
    X_FN_ENTRY("(%p)", state);
    X_CHECK_INVARIANTS();

    DumpGeneralStateHeader();
    DumpGeneralState(state);

    X_CHECK_INVARIANTS();
    X_FN_EXIT("(%p)", state);
}
void BigBuffer_DebugDumpState(const big_buffer_state_t * state) {
    X_FN_ENTRY("(%p)", state);
    X_CHECK_INVARIANTS();

    DumpGeneralStateHeader();
    DumpGeneralState(&(state->general));
    DumpAllocationInstanceHeader();
    for (size_t j = 0; j < MAXIMUM_SUPPORTED_ALLOCATION_COUNT; ++j) {
        if (state->allocation[j].result != 0u) {
            DumpAllocationInstance(&(state->allocation[j]));
        }
    }
    X_CHECK_INVARIANTS();
    X_FN_EXIT("(%p)", state);
}
void BigBuffer_DebugDumpCurrentState(bool verbose) {
    X_FN_ENTRY("(%s)", verbose ? "true" : "false");
    X_CHECK_INVARIANTS();

    DumpGeneralStateHeader();
    DumpGeneralState(&s_State);
    if (verbose && ((s_State.temp_allocations_count > 0) || (s_State.long_lived_allocations_count > 0))) {
        SortAllocationTrackingData();
        DumpAllocationInstanceHeader();
        for (size_t j = 0; j < MAXIMUM_SUPPORTED_ALLOCATION_COUNT; ++j) {
            if (s_Allocation[j].result != 0u) {
                DumpAllocationInstance(&s_Allocation[j]);
            }
        }
    }
    X_CHECK_INVARIANTS();
    X_FN_EXIT("(%s)", verbose ? "true" : "false");
}

size_t BigBuffer_GetAvailableTemporaryMemory(size_t requiredAlignment) {
    X_FN_ENTRY("(%zu)", requiredAlignment);
    X_CHECK_INVARIANTS();

    size_t result = 0u;
    size_t alignmentMask = requiredAlignment - 1;
    uintptr_t aligned_lower = (s_State.low_watermark + (requiredAlignment - 1)) & (~alignmentMask);

    if ((requiredAlignment & alignmentMask) != 0u) {
        printf("BB_GetAvailableTempMemory(%#zx): alignment must be a power of 2\n", requiredAlignment);
        assert(false); // alignment must be a power of 2
    } else if (aligned_lower >= s_State.high_watermark) {
        printf("BB_GetAvailableTempMemory(%#zx): aligned_lower (%#zx) >= high_watermark (%#zx)\n", requiredAlignment, aligned_lower, s_State.high_watermark);
        result = 0u;
    } else {
        result = s_State.high_watermark - aligned_lower;
    }

    X_CHECK_INVARIANTS();
    X_FN_EXIT("(%zu) -> %zu (%zx)", requiredAlignment, result, result);
    return result;
}
size_t BigBuffer_GetAvailableLongLivedMemory(size_t requiredAlignment) {
    X_FN_ENTRY("(%zu)", requiredAlignment);
    X_CHECK_INVARIANTS();

    size_t result; // leave uninitialized to ensure each path sets the value (compiler error if used w/o being set)
    size_t alignmentMask = requiredAlignment - 1;
    uintptr_t lower = s_State.long_lived_limit;
    if (s_State.low_watermark > lower) {
        lower = s_State.low_watermark;
    }
    uintptr_t aligned_lower = (lower + (requiredAlignment - 1)) & (~alignmentMask);


    if ((requiredAlignment & alignmentMask) != 0u) {
        printf("BB_GetAvailableTempMemory(%#zx): alignment must be a power of 2\n", requiredAlignment);
        assert(false); // alignment must be a power of 2
        result = 0u;
    } else if (aligned_lower >= s_State.high_watermark) {
        printf("BB_GetAvailableTempMemory(%#zx): aligned_lower (%#zx) >= high_watermark (%#zx)\n", requiredAlignment, aligned_lower, s_State.high_watermark);
        result = 0u;
    } else {
        result = s_State.high_watermark - aligned_lower;
    }

    X_CHECK_INVARIANTS();
    X_FN_EXIT("(%zu) -> %zu (%zx)", requiredAlignment, result, result);
    return result;
}



// /////////////////////////////////////////////////////////////////////////////
// TODO: replace the below API with BigBuffer_xxxx() API.
static big_buffer_owner_t legacy_owner = BP_BIG_BUFFER_OWNER_NONE;
uint8_t *mem_alloc(size_t size, big_buffer_owner_t owner)
{
    X_FN_ENTRY("(%zu, %d)", size, owner);

    uint8_t * result = BigBuffer_AllocateTemporary(size, 1u, owner);
    if (result != NULL) {
        legacy_owner = owner;
    }

    X_FN_EXIT("(%zu, %d) -> %p", size, owner, result);
    return result;
}
void mem_free(uint8_t const * ptr)
{
    X_FN_ENTRY("(%p)", ptr);

    BigBuffer_FreeTemporary(ptr, legacy_owner);
    BigBuffer_VerifyNoTemporaryAllocations();

    X_FN_EXIT("(%p)", ptr);
}
