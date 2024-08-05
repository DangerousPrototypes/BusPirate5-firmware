#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "bigbuffer_test.h"
#include "pirate/mem.h"




#define X_FN_ENTRY()     printf("bb_test @ %3d: >>>>> %s\n", __LINE__, __func__)
#define X_FN_EXIT()      printf("bb_test @ %3d: <<<<< %s\n", __LINE__, __func__)
#define X_DBGP(fmt, ...) printf("bb_test @ %3d: " fmt, __LINE__, ##__VA_ARGS__)

static bool ensure_no_allocations(void)
{
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

static bool x_dispatch(void)
{
    X_FN_ENTRY();

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

    x_dispatch();

    X_FN_EXIT();
}


