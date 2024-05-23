#include "pirate/intercore_helpers.h"
#include "pirate.h"
#include "assert.h"


// This wrapper is a response to manual code review noting
// at least two coding patterns, neither of which is all
// that safe...
// Using a wrapper also makes it easier to later
// change to a more robust cross-core communication
// with minimal code changes elsewhere.


void icm_core0_send_message(uint8_t message_id) {
    static uint8_t static_message_count = 0u;

    // NOTE: although split into multiple lines for readability,
    //       the compiler optimizes this quite well!

    // encode a message counter into the value,
    // but keep top byte as zero to avoid overlap with RP2040 valid memory locations
    uint32_t icm_value = ++static_message_count;

    // shift those bits to avoid overlap with the encoded message_id parameter
    static_assert(sizeof(message_id) < sizeof(icm_value));
    icm_value <<= sizeof(message_id)*8;

    // add in the message_id
    icm_value |= message_id;

    multicore_fifo_push_blocking(icm_value);

    do {
        uint32_t response = multicore_fifo_pop_blocking();
        // in the current design, any message other than
        // the one sent indicates a serious de-synchronization
        assert(response == message_id);
        if (response == message_id) return;
    } while (1);
}
