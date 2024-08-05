#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "bigbuffer_test.h"
#include "pirate/mem.h"




#define X_FN_ENTRY()     printf("bb_test @ %3d: >>>>> %s\n", __LINE__, __func__)
#define X_FN_EXIT()      printf("bb_test @ %3d: <<<<< %s\n", __LINE__, __func__)
#define X_DBGP(fmt, ...) printf("bb_test @ %3d: " fmt "\n", __LINE__, ##__VA_ARGS__)

static bool _ensure_no_allocations(void) {
    X_FN_ENTRY();

    big_buffer_general_state_t state = {0};
    bool result = BigBuffer_DebugGetStatistics(&state);
    if (result) {
        if (state.temp_allocations_count != 0) {
            X_DBGP("ERROR: Temp allocations not freed\n");
            result = false;
        }
        if (state.long_lived_allocations_count != 0) {
            X_DBGP("ERROR: Long-lived allocations not freed\n");
            result = false;
        }
    }

    X_FN_EXIT();
}

static bool _test_temporary_allocations_1(bool reversed_free_order) {
    X_FN_ENTRY();

    bool success = true;
    void * allocations[32] = {NULL};

    if (success) {
        success = _ensure_no_allocations();
    }

    // up to 32 allocations, and 136k available to allocate
    // 32 * 4k = 128k in "spacer" allocations
    // This leaves 8k for the other 32 allocations
    // 8k / 32 = 256 bytes per allocation
    for (int i = 0; success && (i < 32); ++i) {
        BigBuffer_DebugDumpCurrentState(false); printf("\n");
        void* spacer = BigBuffer_AllocateTemporary(4096, 1, BP_BIG_BUFFER_OWNER_SELFTEST);
        if (spacer == NULL) {
            X_DBGP("ERROR: Failed to allocate spacer buffer idx %d / 32\n", i);
            BigBuffer_DebugDumpCurrentState(true); printf("\n");
            success = false;
            break; // out of for loop ...
        }
        allocations[i] = BigBuffer_AllocateTemporary(320, 1, BP_BIG_BUFFER_OWNER_SELFTEST);
        // allocations[i] = BigBuffer_AllocateTemporary(256, 1, BP_BIG_BUFFER_OWNER_SELFTEST);
        if (allocations[i] == NULL) {
            X_DBGP("ERROR: Failed to allocate temporary buffer idx %d / 32\n", i);
            BigBuffer_DebugDumpCurrentState(true); printf("\n");
            success = false;
            break; // out of for loop ...
        }
        BigBuffer_FreeTemporary(spacer, BP_BIG_BUFFER_OWNER_SELFTEST);
    }
    BigBuffer_DebugDumpCurrentState(true);

    for (int j = 0; j < 32; ++j) {
        int idx = reversed_free_order ? (31 - j) : j;
        BigBuffer_FreeTemporary(allocations[idx], BP_BIG_BUFFER_OWNER_SELFTEST);
    }
    BigBuffer_DebugDumpCurrentState(true); printf("\n");

    if (success) {
        success = _ensure_no_allocations();
    }
    X_FN_EXIT();
    return success;
}

static bool _dispatch(void)
{
    bool success = true;
    X_FN_ENTRY();

    if (success) {
        success = _ensure_no_allocations();
        BigBuffer_DebugDumpCurrentState(true); printf("\n");
    }
    if (success) {
        success = _test_temporary_allocations_1(true);
    }
    if (success) {
        success = _test_temporary_allocations_1(false);
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


