#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "bigbuffer_test.h"
#include "pirate/mem.h"


#define DEBUG_BBTEST_MASK_NONE       0x00u
#define DEBUG_BBTEST_MASK_FAILURE    0x01u
#define DEBUG_BBTEST_MASK_FAIL_STATE 0x02u
#define DEBUG_BBTEST_MASK_SUCCESS    0x10u
#define DEBUG_BBTEST_MASK_VERBOSE    0x20u
#define DEBUG_BBTEST_MASK_FN_ENTRY   0x40u
#define DEBUG_BBTEST_MASK_FN_EXIT    0x80u

#define DEBUG_BBTEST ( DEBUG_BBTEST_MASK_FAILURE | DEBUG_BBTEST_MASK_FAIL_STATE | DEBUG_BBTEST_MASK_SUCCESS )

#if 1 // debug output macros

    #if defined(DEBUG_BBTEST) && (DEBUG_BBTEST & DEBUG_BBTEST_MASK_FN_ENTRY)
        #define X_FN_ENTRY(fmt, ...) printf("bb_tst @ %3d: >>>>> %s" fmt "\n", __LINE__, __func__, ##__VA_ARGS__)
    #else
        #define X_FN_ENTRY(fmt, ...)
    #endif
    #if defined(DEBUG_BBTEST) && (DEBUG_BBTEST & DEBUG_BBTEST_MASK_FN_EXIT)
        #define X_FN_EXIT(fmt, ...)  printf("bb_tst @ %3d: <<<<< %s" fmt "\n", __LINE__, __func__, ##__VA_ARGS__)
    #else
        #define X_FN_EXIT(fmt, ...)
    #endif
    #if defined(DEBUG_BBTEST) && (DEBUG_BBTEST & DEBUG_BBTEST_MASK_VERBOSE)
        #define X_DBGP(fmt, ...)    printf("bb_tst @ %3d: " fmt "\n", __LINE__, ##__VA_ARGS__)
    #else
        #define X_DBGP(fmt, ...)
    #endif
    #if defined(DEBUG_BBTEST) && (DEBUG_BBTEST & DEBUG_BBTEST_MASK_FAILURE)
        #if (DEBUG_BBTEST & DEBUG_BBTEST_MASK_FAIL_STATE)
            #define X_FAILURE(fmt, ...)                                              \
                do {                                                                     \
                    printf("bb_tst @ %3d: *FAILED* " fmt "\n", __LINE__, ##__VA_ARGS__); \
                    BigBuffer_DebugDumpCurrentState(true); printf("\n");                 \
                } while (0);
        #else
            #define X_FAILURE(fmt, ...)                                              \
                do {                                                                     \
                    printf("bb_tst @ %3d: *FAILED* " fmt "\n", __LINE__, ##__VA_ARGS__); \
                } while (0);
        #endif
    #else
        #define X_FAILURE(fmt, ...)
    #endif
    #if defined(DEBUG_BBTEST) && (DEBUG_BBTEST & DEBUG_BBTEST_MASK_SUCCESS)
        #define X_SUCCESS(fmt, ...) printf("bb_tst @ %3d: SUCCESS " fmt "\n", __LINE__, ##__VA_ARGS__)
    #else
        #define X_SUCCESS(fmt, ...)
    #endif

    #define X_TMP(fmt, ...) printf("bb_tst @ %3d: " fmt "\n", __LINE__, ##__VA_ARGS__)

#endif

// define function pointer that matches the signature of _test_temporary_allocations_1
typedef bool (*bool_parameter_test_fn_t)(bool reversed_free_order);

typedef struct _allocation_iteration_t {
    size_t alloc; // how many bytes to allocate
    size_t align; // required alignment
    size_t count; // how many to allocate
    uintptr_t from_long_lived : 1; // if true, allocate from long-lived memory
    uintptr_t expect_failure  : 1; // if true, expect this to fail
    uintptr_t : ((sizeof(uintptr_t)*8) - 2); // explicitly RFU
} allocation_iteration_t;

typedef struct _bool_test_t {
    bool_parameter_test_fn_t test_fn;
    const char * fn_name;
} bool_test_t;
#define X_BOOL_TEST(fn) { .test_fn = fn, .fn_name = #fn }



#define X_MAXIMUM_ALLOCATION_COUNT (32u)
#define X_MAXIMUM_LONG_LIVED_BYTE_COUNT (8u * 1024u)


static bool _verify_no_allocations(void) {
    bool success = true;

    big_buffer_general_state_t state = {0};
    BigBuffer_DebugGetStatistics(&state);
    if (state.temp_allocations_count != 0) {
        X_FAILURE("Temp allocations not freed");
        success = false;
    }
    if (state.long_lived_allocations_count != 0) {
        X_FAILURE("Long-lived allocations not freed");
        success = false;
    }
    return success;
}

static bool _test_temporary_allocations_1(bool reversed_free_order) {
    X_FN_ENTRY("(%s)", reversed_free_order ? "true" : "false");

    bool success = true;
    void * allocations[32] = {NULL};
    if (success) {
        success = _verify_no_allocations();
        if (!success) {
            X_FAILURE("_verify_no_allocations() failed at function entry");
        }
    }

    // up to 32 allocations, and 136k available to allocate
    // 31 * 4k = 124k in "spacer" allocations
    // This leaves 12k for the other 32 allocations
    // 12k / 32 = 256+128 bytes per allocation
    for (int i = 0; success && (i < 32); ++i) {

        void* spacer = NULL;
        if (i != 31) {
            spacer = BigBuffer_AllocateTemporary(4096, 1, BP_BIG_BUFFER_OWNER_SELFTEST);
            if (spacer == NULL) {
                X_FAILURE("Unable to allocate spacer buffer idx %d / 32", i);
                BigBuffer_DebugDumpCurrentState(true); printf("\n");
                success = false;
                break; // out of for loop ...
            }
        }
        allocations[i] = BigBuffer_AllocateTemporary(384, 1, BP_BIG_BUFFER_OWNER_SELFTEST);
        if (allocations[i] == NULL) {
            X_FAILURE("Unable to allocate temporary buffer idx %d / 32", i);
            BigBuffer_DebugDumpCurrentState(true); printf("\n");
            success = false;
            break; // out of for loop ...
        }

        BigBuffer_FreeTemporary(spacer, BP_BIG_BUFFER_OWNER_SELFTEST);
    }

    for (int j = 0; j < 32; ++j) {
        int idx = reversed_free_order ? (31 - j) : j;
        BigBuffer_FreeTemporary(allocations[idx], BP_BIG_BUFFER_OWNER_SELFTEST);
    }

    if (success) {
        success = _verify_no_allocations();
    }

    X_FN_EXIT("(%s) --> %s", reversed_free_order ? "true" : "false", success ? "true" : "false");
    return success;
}


static bool _test_longlived_allocations_iterations_impl(bool reversed_free_order, allocation_iteration_t iter) {

    bool success = true;

    const size_t total_size = BigBuffer_GetAvailableLongLivedMemory(iter.align);
    void * allocations[X_MAXIMUM_ALLOCATION_COUNT] = {NULL};


    success = _verify_no_allocations();
    if (!success) {
        X_FAILURE("(%zd, %zd, %zd) _verify_no_allocations() failed", iter.alloc, iter.align, iter.count);
    } else if (total_size != X_MAXIMUM_LONG_LIVED_BYTE_COUNT) {
        X_FAILURE("(%zd, %zd) expected 8k of long-lived memory available, got %zd", iter.alloc, iter.align, total_size);
        success = false;
    } else {

        // Hard-coded that total available long-lived memory is 8k ... checked above
        for (size_t i = 0; success && (i < iter.count); ++i) {
            void * allocation = BigBuffer_AllocateLongLived(iter.alloc, iter.align, BP_BIG_BUFFER_OWNER_SELFTEST);
            if (allocation == NULL) {
                X_FAILURE("(%zd, %zd) Unable to allocate long-lived buffer %zd", iter.alloc, iter.align, i);
                success = false;
                break; // out of allocation for loop `i` ...
            }
            allocations[i] = allocation;
        }
    }

    // All available memory (for given alignment) should have been allocated.
    // Verify this...
    if (success) {
        size_t remaining_available = BigBuffer_GetAvailableLongLivedMemory(iter.align);
        if (remaining_available != 0) {
            X_FAILURE("(%zd, %zd, %zd) Expected no additional allocations available, got %zd", iter.alloc, iter.align, iter.count, remaining_available);
            success = false;
        }
    }

    // Free all allocations
    for (size_t i = 0; i < count_of(allocations); ++i) {
        size_t idx = reversed_free_order ? (count_of(allocations) - 1u - i) : i;
        BigBuffer_FreeLongLived(allocations[idx], BP_BIG_BUFFER_OWNER_SELFTEST);
    }

    if (success) {
        success = _verify_no_allocations();
        if (!success) {
            X_FAILURE("(%zd, %zd, %zd) _verify_no_allocations() failed", iter.alloc, iter.align, iter.count);
        }
    }
    return success;
}

static bool _test_longlived_allocations_iterations(bool reversed_free_order) {
    X_FN_ENTRY("(%s)", reversed_free_order ? "true" : "false");

    bool success = true;
    if (success) {
        success = _verify_no_allocations();
        if (!success) {
            X_FAILURE("_verify_no_allocations() failed at function entry");
        }
    }

    static const allocation_iteration_t iterations[] = {
        {  256u,    1u, 32u, true },
        {  512u,    1u, 16u, true },
        { 1024u,    1u,  8u, true },
        { 2048u,    1u,  4u, true },
        { 4096u,    1u,  2u, true },
        { 8192u,    1u,  1u, true },
    };
    void * allocations[32u] = {NULL};

    for (size_t j = 0; success && (j < count_of(iterations)); ++j) {

        allocation_iteration_t iter = iterations[j];
        assert(iter.count <= count_of(allocations));

        for (; iter.align <= (32u*1024); iter.align <<= 1) {
            if (iter.align > iter.alloc) {
                iter.count >>= 1;
            }
            if (iter.count == 0) {
                // special case for when exceed available memory space
                // because the long-lived allocations can be up to 128k aligned (even if only 8k size)
                iter.count = 1;
            }
            success = _test_longlived_allocations_iterations_impl(reversed_free_order, iter);
            // if successful, print a one-line "SUCCESS" message ... helps anchor last successful test in output
            if (success) {
                X_SUCCESS("LL_Alloc1(%s %4zd @ %6zd alignment, %zd count", reversed_free_order ? "true): " : "false):" , iter.alloc, iter.align, iter.count);
            }
        } // next alignment / count

        // end of all alignments possible for this iteration....
    }
    
    if (success) {
        X_SUCCESS("ll-alloc-1(%s)", reversed_free_order ? "true" : "false");
    } else {
        X_FAILURE("ll-alloc-1(%s)", reversed_free_order ? "true" : "false");
    }

    X_FN_EXIT("(%s) --> %s", reversed_free_order ? "true" : "false", success ? "true" : "false");
    return success;
}
static bool _test_mixed_allocations_1(bool reversed_free_order) {
    X_FN_ENTRY("(%s)", reversed_free_order ? "true" : "false");

    bool success = true;
    if (success) {
        success = _verify_no_allocations();
        if (!success) {
            X_FAILURE("ERROR: _verify_no_allocations() failed at function entry\n");
        }
    }

    // What is the interesting use case here?  128k + 8k total
    allocation_iteration_t iterations[] = {
        { .alloc = 1024u * 126u, .align = 1024u * 32u, .count = 1, .from_long_lived = false, .expect_failure = false }, //   0..125
        { .alloc = 1024u *   3u, .align =          1u, .count = 1, .from_long_lived = true,  .expect_failure = false }, // 133..135
        { .alloc = 1024u *   1u, .align =          1u, .count = 1, .from_long_lived = false, .expect_failure = false }, // 126..126
        { .alloc = 1024u *   1u, .align =          1u, .count = 1, .from_long_lived = true,  .expect_failure = false }, // 132..132
        { .alloc = 1024u *   5u, .align =          1u, .count = 1, .from_long_lived = false, .expect_failure = false }, // 127..131
        { .alloc =           1u, .align =          1u, .count = 1, .from_long_lived = true,  .expect_failure = true  }, // all memory already allocated
        { .alloc =           1u, .align =          1u, .count = 1, .from_long_lived = false, .expect_failure = true  }, // all memory already allocated
    };
    void * p[count_of(iterations)] = {NULL};

    for (size_t i = 0; success && (i < count_of(iterations)); ++i) {
        allocation_iteration_t * iter = &iterations[i];
        if (iter->from_long_lived) {
            p[i] = BigBuffer_AllocateLongLived(iter->alloc, iter->align, BP_BIG_BUFFER_OWNER_SELFTEST);
        } else {
            p[i] = BigBuffer_AllocateTemporary(iter->alloc, iter->align, BP_BIG_BUFFER_OWNER_SELFTEST);
        }
        if (iter->expect_failure) {
            if (p[i] != NULL) {
                X_FAILURE("Expected allocation failure for iteration %zd", i);
                success = false;
                break; // out of for loop ...
            }
        } else {
            if (p[i] == NULL) {
                X_FAILURE("Unexpected allocation failure for iteration %zd", i);
                success = false;
                break; // out of for loop ...
            }
        }
    }

    // reset state by free'ing all the allocations 
    for (size_t i = 0; i < count_of(iterations); ++i) {
        size_t idx = reversed_free_order ? (count_of(iterations) - 1u - i) : i;
        if (iterations[idx].from_long_lived) {
            BigBuffer_FreeLongLived(p[idx], BP_BIG_BUFFER_OWNER_SELFTEST);
        } else {
            BigBuffer_FreeTemporary(p[idx], BP_BIG_BUFFER_OWNER_SELFTEST);
        }
        p[idx] = NULL;
    }

    X_FN_EXIT("(%s) --> %s", reversed_free_order ? "true" : "false", success ? "true" : "false");
    return success;
}
static bool _test_longlived_allocations_3(bool reversed_free_order) {
    X_FN_ENTRY("(%s)", reversed_free_order ? "true" : "false");

    bool success = true;
    if (success) {
        success = _verify_no_allocations();
        if (!success) {
            X_FAILURE("_verify_no_allocations() failed at function entry");
        }
    }

    X_FN_EXIT("(%s) --> %s", reversed_free_order ? "true" : "false", success ? "true" : "false");
    return success;
}


static const bool_test_t bool_tests[] = {
    X_BOOL_TEST(_test_temporary_allocations_1),
    X_BOOL_TEST(_test_longlived_allocations_iterations),
    X_BOOL_TEST(_test_mixed_allocations_1),
};

static bool _dispatch(void)
{
    bool success = true;
    X_FN_ENTRY("");

    if (success) {
        success = _verify_no_allocations();
        if (success) {
            X_DBGP();
            X_DBGP("Initial State:");
            BigBuffer_DebugDumpCurrentState(true);
            X_DBGP();
        }
    }
    
    for (size_t idx = 0; idx < count_of(bool_tests); ++idx) {
        const bool_test_t * t = &bool_tests[idx];
        if (success) {
            success = t->test_fn(true);
            if (success) {
                X_SUCCESS("%s(%s)", t->fn_name, "true");
            } else {
                X_FAILURE("%s(%s)", t->fn_name, "true");
            }
        }
        if (success) {
            success = t->test_fn(false);
            if (success) {
                X_SUCCESS("%s(%s)", t->fn_name, "false");
            } else {
                X_FAILURE("%s(%s)", t->fn_name, "false");
            }
        }
    }

    X_FN_EXIT("(%s) --> %s", reversed_free_order ? "true" : "false", success ? "true" : "false");
    return success;
}




void bigbuff_test_handler(command_result_t *res_out)
{
    X_FN_ENTRY("()");

    // This function is called when the user types the command "bigbuff_test"
    // It is a simple test function that exercises the big buffer allocation
    // and free functions, specifically focusing on ensuring edge cases are
    // handled.
    memset(res_out, 0, sizeof(command_result_t));
    bool success = _dispatch();
    res_out->success = success;
    res_out->error   = !success;

    X_FN_EXIT("() --> %s", success ? "true" : "false");
    return;
}


