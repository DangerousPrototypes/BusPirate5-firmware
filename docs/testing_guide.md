+++
weight = 90419
title = 'Host-Side Testing'
+++

# Host-Side Testing

> Run pure-logic unit tests on the host machine without embedded hardware.

---

## Current State

Only one test file exists: `tests/test_spsc_queue.c`. It stress-tests the SPSC
(single-producer, single-consumer) lock-free queue using pthreads to simulate
the dual-core RP2040 environment on a host PC.

---

## Test Infrastructure

The framework lives inside each test file. From `tests/test_spsc_queue.c`:

```c
#define TEST_PASS  0
#define TEST_FAIL  1

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define RUN_TEST(fn)                                                    \
    do {                                                                \
        tests_run++;                                                    \
        printf("  [RUN]  %s\n", #fn);                                  \
        if ((fn)() == TEST_PASS) {                                      \
            tests_passed++;                                             \
            printf("  [PASS] %s\n", #fn);                               \
        } else {                                                        \
            tests_failed++;                                             \
            printf("  [FAIL] %s\n", #fn);                               \
        }                                                               \
    } while (0)

#define ASSERT_TRUE(cond, msg)                                          \
    do {                                                                \
        if (!(cond)) {                                                  \
            printf("    ASSERT FAILED: %s (%s:%d)\n",                   \
                   msg, __FILE__, __LINE__);                            \
            return TEST_FAIL;                                           \
        }                                                               \
    } while (0)

#define ASSERT_EQ(a, b, msg)                                            \
    do {                                                                \
        if ((a) != (b)) {                                               \
            printf("    ASSERT_EQ FAILED: %s  (%u != %u) (%s:%d)\n",   \
                   msg, (unsigned)(a), (unsigned)(b),                   \
                   __FILE__, __LINE__);                                 \
            return TEST_FAIL;                                           \
        }                                                               \
    } while (0)
```

---

## SDK Mocking

Pico SDK primitives are replaced with host equivalents. From
`tests/test_spsc_queue.c`:

```c
/* __dmb() -> GCC/Clang full memory fence */
#define __dmb() __sync_synchronize()

/* tight_loop_contents() -> compiler barrier + yield hint */
static inline void tight_loop_contents(void) {
    __asm__ volatile("" ::: "memory");
#if defined(__x86_64__) || defined(__i386__)
    __asm__ volatile("pause");
#endif
}
```

Stub headers in `tests/stubs/` satisfy Pico SDK `#include` directives:

| Stub header | Replaces |
|-------------|----------|
| `tests/stubs/pico/stdlib.h` | `pico/stdlib.h` |
| `tests/stubs/hardware/sync.h` | `hardware/sync.h` |

---

## Building and Running Tests

Quickest path:

```bash
cd tests && ./test_spsc_queue.sh
```

Or manually:

```bash
gcc -O2 -Wall -Wextra -pthread -Itests/stubs \
    -o tests/test_spsc_queue tests/test_spsc_queue.c
./tests/test_spsc_queue
```

---

## Test File Structure

```
tests/
├── test_spsc_queue.c      # SPSC queue stress test
├── test_spsc_queue.sh     # Build & run script
└── stubs/                 # Mock Pico SDK headers
    ├── pico/
    │   └── stdlib.h       # Mock pico/stdlib.h
    └── hardware/
        └── sync.h         # Mock hardware/sync.h
```

---

## Writing a New Test

Pattern from `test_spsc_queue.c`:

1. Write test functions returning `TEST_PASS` or `TEST_FAIL`.
2. Use `ASSERT_TRUE(cond, msg)` and `ASSERT_EQ(a, b, msg)` macros.
3. Register in `main()` with `RUN_TEST(test_function_name)`.
4. Print a summary at the end.

Example:

```c
static int test_basic_operation(void) {
    spsc_queue_t q;
    uint8_t buf[16];
    spsc_queue_init(&q, buf, sizeof(buf));

    ASSERT_TRUE(spsc_queue_is_empty(&q), "queue should be empty");
    ASSERT_TRUE(spsc_queue_try_add(&q, 0x42), "add should succeed");

    uint8_t val;
    ASSERT_TRUE(spsc_queue_try_remove(&q, &val), "remove should succeed");
    ASSERT_EQ(val, 0x42, "value should match");

    return TEST_PASS;
}
```

---

## What Can Be Tested on Host

| Testable | Examples |
|----------|---------|
| Pure logic | Queue operations, parsers, formatters |
| Data structures | SPSC queue, ring buffers |
| Command-line parsing | `bp_cmd` parser functions |
| Validators | Range checks, string matching |

| Needs Hardware | Examples |
|----------------|---------|
| Protocol IO | SPI, I2C, UART transactions |
| Pin operations | `bio_put`, `bio_get` |
| ADC/AMUX | Voltage measurements |
| Display | LCD updates |

---

## Extending to Other Subsystems

To test a new module:

1. Create `tests/test_mymodule.c`.
2. Mock any Pico SDK dependencies (add stubs to `tests/stubs/` if needed).
3. Include the source under test: `#include "../src/mymodule.h"`.
4. Create `tests/test_mymodule.sh` build script.
5. Compile with `gcc -pthread -Itests/stubs`.

---

## Related Documentation

- [dual_core_guide.md](dual_core_guide.md) — SPSC queue design
- [build_system_guide.md](build_system_guide.md) — Build system
- Source: `tests/test_spsc_queue.c`, `tests/test_spsc_queue.sh`
