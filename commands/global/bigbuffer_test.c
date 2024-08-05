#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "bigbuffer_test.h"
#include "pirate/mem.h"

// define function pointer that matches the signature of _test_temporary_allocations_1
typedef bool (*bool_parameter_test_fn_t)(bool reversed_free_order);

typedef struct _allocation_iteration_t {
    size_t alloc; // how many bytes to allocate
    size_t align; // required alignment
    size_t count; // how many to allocate
} allocation_iteration_t;

typedef struct _bool_test_t {
    bool_parameter_test_fn_t test_fn;
    const char * fn_name;
} bool_test_t;
#define X_BOOL_TEST(fn) { .test_fn = fn, .fn_name = #fn }


#define X_FN_ENTRY()        printf("bb_tst @ %3d: >>>>> %s\n", __LINE__, __func__)
#define X_FN_EXIT()         printf("bb_tst @ %3d: <<<<< %s\n", __LINE__, __func__)
#define X_DBGP(fmt, ...)    printf("bb_tst @ %3d: " fmt "\n", __LINE__, ##__VA_ARGS__)
#define X_SUCCESS(fmt, ...) printf("bb_tst @ %3d: SUCCESS " fmt "\n", __LINE__, ##__VA_ARGS__)

#define X_FAILURE(fmt, ...)                                                   \
    do {                                                                      \
        printf("bb_tst @ %3d: *FAILED* " fmt "\n", __LINE__, ##__VA_ARGS__); \
        BigBuffer_DebugDumpCurrentState(true); printf("\n");                  \
    } while (0);


#define X_MAXIMUM_ALLOCATION_COUNT (32u)
#define X_MAXIMUM_LONG_LIVED_BYTE_COUNT (8u * 1024u)


static bool _verify_no_allocations(void) {
    X_FN_ENTRY();

    bool success = true;

    big_buffer_general_state_t state = {0};
    BigBuffer_DebugGetStatistics(&state);
    if (state.temp_allocations_count != 0) {
        X_FAILURE("Temp allocations not freed\n");
        success = false;
    }
    if (state.long_lived_allocations_count != 0) {
        X_FAILURE("Long-lived allocations not freed\n");
        success = false;
    }
    X_FN_EXIT();
    return success;
}

static bool _test_temporary_allocations_1(bool reversed_free_order) {
    X_FN_ENTRY();

    bool success = true;
    void * allocations[32] = {NULL};
    if (success) {
        success = _verify_no_allocations();
        if (!success) {
            X_FAILURE("_verify_no_allocations() failed at function entry\n");
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
                X_FAILURE("Unable to allocate spacer buffer idx %d / 32\n", i);
                BigBuffer_DebugDumpCurrentState(true); printf("\n");
                success = false;
                break; // out of for loop ...
            }
        }
        allocations[i] = BigBuffer_AllocateTemporary(384, 1, BP_BIG_BUFFER_OWNER_SELFTEST);
        if (allocations[i] == NULL) {
            X_FAILURE("Unable to allocate temporary buffer idx %d / 32\n", i);
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

    X_FN_EXIT();
    return success;
}


static bool _test_longlived_allocations_iterations_impl(bool reversed_free_order, allocation_iteration_t iter) {

    bool verbose = iter.alloc == 0x100u && iter.align == 0x200u;
    bool success = true;

    const size_t total_size = BigBuffer_GetAvailableLongLivedMemory(iter.align);
    void * allocations[X_MAXIMUM_ALLOCATION_COUNT] = {NULL};


    success = _verify_no_allocations();
    if (!success) {
        X_FAILURE("(%zd, %zd, %zd) _verify_no_allocations() failed\n", iter.alloc, iter.align, iter.count);
    } else if (total_size != X_MAXIMUM_LONG_LIVED_BYTE_COUNT) {
        X_FAILURE("(%zd, %zd) expected 8k of long-lived memory available, got %zd\n", iter.alloc, iter.align, total_size);
        success = false;
    } else {

        if (verbose) {
            X_DBGP("**********************************************************");
            X_DBGP("TEST CASE: %zd bytes, %zd alignment, %zd count", iter.alloc, iter.align, iter.count);
        }

        // Hard-coded that total available long-lived memory is 8k ... checked above
        for (size_t i = 0; success && (i < iter.count); ++i) {
            if (verbose) {
                printf("\n");
                BigBuffer_DebugDumpCurrentState(true);
            }
            void * allocation = BigBuffer_AllocateLongLived(iter.alloc, iter.align, BP_BIG_BUFFER_OWNER_SELFTEST);
            if (allocation == NULL) {
                X_FAILURE("(%zd, %zd) Unable to allocate long-lived buffer %zd\n", iter.alloc, iter.align, i);
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
            X_FAILURE("(%zd, %zd, %zd) Expected no additional allocations available, got %zd\n", iter.alloc, iter.align, iter.count, remaining_available);
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
            X_FAILURE("(%zd, %zd, %zd) _verify_no_allocations() failed\n", iter.alloc, iter.align, iter.count);
        }
    }
    return success;
}

static bool _test_longlived_allocations_iterations(bool reversed_free_order) {

    X_FN_ENTRY();
    bool success = true;
    if (success) {
        success = _verify_no_allocations();
        if (!success) {
            X_FAILURE("_verify_no_allocations() failed at function entry\n");
        }
    }

    static const allocation_iteration_t iterations[] = {
        {  256u,    1u, 32u },
        {  512u,    1u, 16u },
        { 1024u,    1u,  8u },
        { 2048u,    1u,  4u },
        { 4096u,    1u,  2u },
        { 8192u,    1u,  1u },
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
                X_SUCCESS("LL_Alloc1(%s %4zd @ %6zd alignment, %zd count\n", reversed_free_order ? "true): " : "false):" , iter.alloc, iter.align, iter.count);
            }
        } // next alignment / count

        // end of all alignments possible for this iteration....
    }
    
    if (success) {
        X_SUCCESS("ll-alloc-1(%s)\n", reversed_free_order ? "true" : "false");
    } else {
        X_FAILURE("ll-alloc-1(%s)\n", reversed_free_order ? "true" : "false");
    }

    X_FN_EXIT();
}
static bool _test_longlived_allocations_2(bool reversed_free_order) {

    X_FN_ENTRY();
    bool success = true;
    if (success) {
        success = _verify_no_allocations();
        if (!success) {
            X_DBGP("ERROR: _verify_no_allocations() failed at function entry\n");
        }
    }

    X_FN_EXIT();
}
static bool _test_longlived_allocations_3(bool reversed_free_order) {

    X_FN_ENTRY();
    bool success = true;
    if (success) {
        success = _verify_no_allocations();
        if (!success) {
            X_DBGP("ERROR: _verify_no_allocations() failed at function entry\n");
        }
    }

    X_FN_EXIT();
}


static const bool_test_t bool_tests[] = {
    X_BOOL_TEST(_test_temporary_allocations_1),
    X_BOOL_TEST(_test_longlived_allocations_iterations),
};

static bool _dispatch(void)
{
    bool success = true;
    X_FN_ENTRY();

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
                X_SUCCESS("%s(%s)\n", t->fn_name, "true");
            } else {
                X_FAILURE("%s(%s)\n", t->fn_name, "true");
            }
        }
        if (success) {
            success = t->test_fn(false);
            if (success) {
                X_SUCCESS("%s(%s)\n", t->fn_name, "false");
            } else {
                X_FAILURE("%s(%s)\n", t->fn_name, "false");
            }
        }
    }

    X_FN_EXIT();
}




void bigbuff_test_handler(command_result_t *res)
{
    X_FN_ENTRY();

    // This function is called when the user types the command "bigbuff_test"
    // It is a simple test function that exercises the big buffer allocation
    // and free functions, specifically focusing on ensuring edge cases are
    // handled.
    memset(res, 0, sizeof(command_result_t));
    res->success = true;
    res->error   = false;

    _dispatch();

    X_FN_EXIT();
}


