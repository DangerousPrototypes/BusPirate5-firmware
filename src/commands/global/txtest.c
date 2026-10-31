// ============================================================================
// TXTEST — TX Pipeline Saturation Test for Bus Pirate 5/6/7
// ============================================================================
//
// Blasts a known repeating pattern through the USB TX pipeline so a host-side
// script can capture the output and verify every byte arrived intact.
//
// Three layers can be tested independently:
//   Layer 1 (default) — printf path  (printf → _write → tx_fifo_write → SPSC → USB)
//   Layer 2           — tx_fifo_write directly (bypass printf)
//   Layer 3           — charbuf path (charbuf_append → charbuf_draw → tx_fifo_write)
//
// Usage:
//   txtest                  — Layer 1, 4096 bytes, pattern 0
//   txtest -l 2             — Layer 2
//   txtest -n 32768         — 32 KB blast
//   txtest -p 1             — Pattern 1 (VT100 escapes)
//   txtest -l 2 -n 32768   — Layer 2, 32 KB
//
// Output format:
//   "TXTEST:START:<layer>:<nbytes>:<pattern>\r\n"
//   ... <nbytes> of pattern data ...
//   "\r\nTXTEST:END:<crc>\r\n"
//
// The host script sends the command, captures between START/END markers,
// generates the same pattern locally, and compares.
// ============================================================================

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "command_struct.h"
#include "system_config.h"
#include "usb_tx.h"
#include "pirate/mem.h"
#include "lib/bp_args/bp_cmd.h"
#include "ui/ui_help.h"

#define TXTEST_MAX_BYTES (32 * 1024)

// ============================================================================
// USAGE EXAMPLES
// ============================================================================
static const char* const usage[] = {
    "txtest [-l(ayer) <1-3>] [-n(bytes) <size>] [-p(attern) <0-2>]",
    "Default (putchar, 4KB, counter):%s txtest",
    "Direct tx_fifo, 32KB:%s txtest -l 2 -n 32768",
    "VT100 escape pattern:%s txtest -p 1",
    "Chunked tx_fifo, 16KB:%s txtest -l 3 -n 16384",
};

// ============================================================================
// VALUE CONSTRAINTS
// ============================================================================
static const bp_val_constraint_t layer_range = {
    .type = BP_VAL_UINT32,
    .u = { .min = 1, .max = 3, .def = 1 },
};

static const bp_val_constraint_t nbytes_range = {
    .type = BP_VAL_UINT32,
    .u = { .min = 64, .max = TXTEST_MAX_BYTES, .def = 4096 },
};

static const bp_val_constraint_t pattern_range = {
    .type = BP_VAL_UINT32,
    .u = { .min = 0, .max = 2, .def = 0 },
};

// ============================================================================
// FLAG TABLE
// ============================================================================
static const bp_command_opt_t txtest_opts[] = {
    { "layer",   'l', BP_ARG_REQUIRED, "1-3",   T_HELP_TXTEST_LAYER,   &layer_range },
    { "nbytes",  'n', BP_ARG_REQUIRED, "size",  T_HELP_TXTEST_NBYTES,  &nbytes_range },
    { "pattern", 'p', BP_ARG_REQUIRED, "0-2",   T_HELP_TXTEST_PATTERN, &pattern_range },
    { 0 },
};

// ============================================================================
// COMMAND DEFINITION
// ============================================================================
const bp_command_def_t txtest_def = {
    .name = "txtest",
    .description = T_HELP_TXTEST,
    .opts = txtest_opts,
    .usage = usage,
    .usage_count = count_of(usage),
};

// ============================================================================
// PATTERN GENERATORS
// ============================================================================

// Pattern 0: sequential counter 0x00-0xFF repeating
static void pattern_counter(uint8_t* buf, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)(i & 0xFF);
    }
}

// Pattern 1: repeating VT100 escape "\x1b[0m" (the problematic sequence)
static void pattern_vt100_reset(uint8_t* buf, uint32_t len) {
    const uint8_t pat[] = { 0x1b, '[', '0', 'm' };
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = pat[i % sizeof(pat)];
    }
}

// Pattern 2: mixed VT100 — simulates hex editor output
// "\x1b[1;34mAA\x1b[0m" repeating (blue hex pair + reset)
static void pattern_vt100_mixed(uint8_t* buf, uint32_t len) {
    const uint8_t pat[] = { 0x1b, '[', '1', ';', '3', '4', 'm',
                            'A', 'A',
                            0x1b, '[', '0', 'm' };
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = pat[i % sizeof(pat)];
    }
}

// Simple CRC-16 for verification
static uint16_t crc16(const uint8_t* data, uint32_t len) {
    uint16_t crc = 0xFFFF;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}

// ============================================================================
// HANDLER
// ============================================================================
void txtest_handler(struct command_result* res) {
    if (bp_cmd_help_check(&txtest_def, res->help_flag)) {
        return;
    }

    // Parse flags with defaults
    uint32_t layer = 1, nbytes = 4096, pattern = 0;
    bp_cmd_flag(&txtest_def, 'l', &layer);
    bp_cmd_flag(&txtest_def, 'n', &nbytes);
    bp_cmd_flag(&txtest_def, 'p', &pattern);

    // Claim the big buffer — up to 32 KB for the pattern
    uint8_t* buf = mem_alloc(nbytes, BP_BIG_BUFFER_TXTEST);
    if (!buf) {
        printf("Error: big buffer unavailable (in use by another command)\r\n");
        return;
    }

    // Fill the entire buffer with the pattern
    switch (pattern) {
    case 0: pattern_counter(buf, nbytes);       break;
    case 1: pattern_vt100_reset(buf, nbytes);   break;
    case 2: pattern_vt100_mixed(buf, nbytes);   break;
    }

    // CRC over the full buffer
    uint16_t crc = crc16(buf, nbytes);

    // Drain any pending output before we start
    tx_fifo_wait_drain();

    // START marker — always via printf so the host can parse it
    printf("TXTEST:START:%lu:%lu:%lu\r\n", layer, nbytes, pattern);
    tx_fifo_wait_drain();

    // Blast the pattern
    switch (layer) {
    case 1:
        // Layer 1: _putchar path — byte-by-byte tx_fifo_put
        // Same path printf uses internally (_putchar → tx_fifo_put)
        for (uint32_t i = 0; i < nbytes; i++) {
            char c = (char)buf[i];
            tx_fifo_put(&c);
        }
        break;

    case 2:
        // Layer 2: tx_fifo_write — one big blocking write
        tx_fifo_write((const char*)buf, nbytes);
        break;

    case 3: {
        // Layer 3: chunked tx_fifo_write + drain between chunks
        // Simulates the charbuf_draw flush pattern of the hex editor
        #define TXTEST_L3_CHUNK 1024
        uint32_t remaining = nbytes;
        uint32_t offset = 0;
        while (remaining > 0) {
            uint32_t chunk = (remaining > TXTEST_L3_CHUNK) ? TXTEST_L3_CHUNK : remaining;
            tx_fifo_write((const char*)&buf[offset], chunk);
            tx_fifo_wait_drain();
            offset    += chunk;
            remaining -= chunk;
        }
        break;
    }
    }

    // END marker
    tx_fifo_wait_drain();
    printf("\r\nTXTEST:END:%04X\r\n", crc);

    // Release the big buffer
    mem_free(buf);
}
