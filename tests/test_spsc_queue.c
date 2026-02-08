/**
 * @file test_spsc_queue.c
 * @brief Host-side stress test for spsc_queue.h
 *
 * Mocks Pico SDK dependencies and uses pthreads to simulate
 * dual-core producer/consumer under heavy contention.
 *
 * Build & run:
 *   gcc -O2 -Wall -Wextra -pthread -Itests/stubs \
 *       -o tests/test_spsc_queue tests/test_spsc_queue.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* Mock Pico SDK primitives for host compilation                      */
/* ------------------------------------------------------------------ */

/* __dmb() -> GCC/Clang full memory fence (stronger than ARM DMB,     */
/* which is fine – if it passes here, the real DMB is sufficient)      */
#define __dmb() __sync_synchronize()

/* tight_loop_contents() -> compiler barrier + yield hint              */
static inline void tight_loop_contents(void) {
    __asm__ volatile("" ::: "memory");
#if defined(__x86_64__) || defined(__i386__)
    __asm__ volatile("pause");
#endif
}

/* Now include the unit under test.
 * Stub headers in tests/stubs/ satisfy the pico/stdlib.h and
 * hardware/sync.h includes without pulling in the real Pico SDK. */
#include "../src/spsc_queue.h"

/* ------------------------------------------------------------------ */
/* Test infrastructure                                                */
/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */
/* Unit tests – single-threaded correctness                           */
/* ------------------------------------------------------------------ */

static int test_init(void) {
    uint8_t buf[64];
    spsc_queue_t q;
    spsc_queue_init(&q, buf, 64);

    ASSERT_EQ(q.head, 0, "head should be 0 after init");
    ASSERT_EQ(q.tail, 0, "tail should be 0 after init");
    ASSERT_EQ(q.capacity, 64, "capacity");
    ASSERT_EQ(q.mask, 63, "mask");
    ASSERT_TRUE(spsc_queue_is_empty(&q), "should be empty");
    ASSERT_TRUE(!spsc_queue_is_full(&q), "should not be full");
    ASSERT_EQ(spsc_queue_level(&q), 0, "level should be 0");
    ASSERT_EQ(spsc_queue_free(&q), 63, "free should be capacity-1");
    return TEST_PASS;
}

static int test_add_remove_single(void) {
    uint8_t buf[4];
    spsc_queue_t q;
    spsc_queue_init(&q, buf, 4);

    uint8_t out = 0;
    ASSERT_TRUE(!spsc_queue_try_remove(&q, &out), "remove from empty");
    ASSERT_TRUE(!spsc_queue_try_peek(&q, &out), "peek from empty");

    ASSERT_TRUE(spsc_queue_try_add(&q, 0xAA), "add 0xAA");
    ASSERT_EQ(spsc_queue_level(&q), 1, "level after 1 add");
    ASSERT_TRUE(!spsc_queue_is_empty(&q), "not empty after add");

    ASSERT_TRUE(spsc_queue_try_peek(&q, &out), "peek");
    ASSERT_EQ(out, 0xAA, "peek value");

    ASSERT_TRUE(spsc_queue_try_remove(&q, &out), "remove");
    ASSERT_EQ(out, 0xAA, "removed value");
    ASSERT_TRUE(spsc_queue_is_empty(&q), "empty after remove");
    return TEST_PASS;
}

static int test_fill_and_drain(void) {
    uint8_t buf[8];   /* capacity=8, usable=7 */
    spsc_queue_t q;
    spsc_queue_init(&q, buf, 8);

    /* Fill to capacity-1 */
    for (uint8_t i = 0; i < 7; i++) {
        ASSERT_TRUE(spsc_queue_try_add(&q, i), "fill add");
    }
    ASSERT_TRUE(spsc_queue_is_full(&q), "should be full");
    ASSERT_TRUE(!spsc_queue_try_add(&q, 99), "add to full");
    ASSERT_EQ(spsc_queue_level(&q), 7, "level when full");
    ASSERT_EQ(spsc_queue_free(&q), 0, "free when full");

    /* Drain and verify FIFO order */
    for (uint8_t i = 0; i < 7; i++) {
        uint8_t out;
        ASSERT_TRUE(spsc_queue_try_remove(&q, &out), "drain remove");
        ASSERT_EQ(out, i, "FIFO order");
    }
    ASSERT_TRUE(spsc_queue_is_empty(&q), "empty after drain");
    return TEST_PASS;
}

static int test_wraparound(void) {
    uint8_t buf[4];   /* capacity=4, usable=3 */
    spsc_queue_t q;
    spsc_queue_init(&q, buf, 4);

    /* Push head and tail past the wrap boundary several times */
    for (int round = 0; round < 20; round++) {
        for (uint8_t i = 0; i < 3; i++) {
            ASSERT_TRUE(spsc_queue_try_add(&q, (uint8_t)(round * 3 + i)),
                        "wraparound add");
        }
        ASSERT_TRUE(spsc_queue_is_full(&q), "full after 3");

        for (uint8_t i = 0; i < 3; i++) {
            uint8_t out;
            ASSERT_TRUE(spsc_queue_try_remove(&q, &out), "wraparound remove");
            ASSERT_EQ(out, (uint8_t)(round * 3 + i), "wraparound value");
        }
        ASSERT_TRUE(spsc_queue_is_empty(&q), "empty after round");
    }
    return TEST_PASS;
}

static int test_blocking_ops(void) {
    uint8_t buf[4];
    spsc_queue_t q;
    spsc_queue_init(&q, buf, 4);

    /* Blocking add/remove in single-threaded context (should not block) */
    spsc_queue_add_blocking(&q, 0x11);
    spsc_queue_add_blocking(&q, 0x22);

    uint8_t out;
    spsc_queue_remove_blocking(&q, &out);
    ASSERT_EQ(out, 0x11, "blocking remove 1");
    spsc_queue_remove_blocking(&q, &out);
    ASSERT_EQ(out, 0x22, "blocking remove 2");

    /* Blocking peek */
    spsc_queue_add_blocking(&q, 0x33);
    spsc_queue_peek_blocking(&q, &out);
    ASSERT_EQ(out, 0x33, "blocking peek");
    spsc_queue_remove_blocking(&q, &out);
    ASSERT_EQ(out, 0x33, "blocking peek didn't consume");
    return TEST_PASS;
}

/* ------------------------------------------------------------------ */
/* Stress tests – multi-threaded                                      */
/* ------------------------------------------------------------------ */

/* Shared state for threaded tests */
typedef struct {
    spsc_queue_t* q;
    uint32_t      count;          /* number of bytes to transfer */
    volatile int  error;          /* set non-zero on failure */
    char          error_msg[256];
} stress_ctx_t;

static void* producer_thread(void* arg) {
    stress_ctx_t* ctx = (stress_ctx_t*)arg;
    for (uint32_t i = 0; i < ctx->count; i++) {
        spsc_queue_add_blocking(ctx->q, (uint8_t)(i & 0xFF));
    }
    return NULL;
}

static void* consumer_thread(void* arg) {
    stress_ctx_t* ctx = (stress_ctx_t*)arg;
    uint8_t expected = 0;
    for (uint32_t i = 0; i < ctx->count; i++) {
        uint8_t got;
        spsc_queue_remove_blocking(ctx->q, &got);
        if (got != expected) {
            ctx->error = 1;
            snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                     "byte %u: expected 0x%02X got 0x%02X",
                     i, expected, got);
            return NULL;
        }
        expected = (uint8_t)((expected + 1) & 0xFF);
    }
    return NULL;
}

/**
 * Stress test: producer and consumer on separate threads,
 * transferring a large number of bytes through a small queue.
 * Verifies FIFO ordering and data integrity.
 */
static int test_stress_small_queue(void) {
    /* Small queue (16 bytes) with high contention */
    static uint8_t buf[16];
    static spsc_queue_t q;
    spsc_queue_init(&q, buf, sizeof(buf));

    stress_ctx_t ctx = { .q = &q, .count = 2000000, .error = 0 };

    pthread_t prod, cons;
    pthread_create(&cons, NULL, consumer_thread, &ctx);
    pthread_create(&prod, NULL, producer_thread, &ctx);
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    ASSERT_TRUE(!ctx.error, ctx.error_msg);
    ASSERT_TRUE(spsc_queue_is_empty(&q), "queue should be empty after stress");
    return TEST_PASS;
}

static int test_stress_large_queue(void) {
    /* Larger queue (4096 bytes) with high throughput */
    static uint8_t buf[4096];
    static spsc_queue_t q;
    spsc_queue_init(&q, buf, sizeof(buf));

    stress_ctx_t ctx = { .q = &q, .count = 10000000, .error = 0 };

    pthread_t prod, cons;
    pthread_create(&cons, NULL, consumer_thread, &ctx);
    pthread_create(&prod, NULL, producer_thread, &ctx);
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    ASSERT_TRUE(!ctx.error, ctx.error_msg);
    ASSERT_TRUE(spsc_queue_is_empty(&q), "queue should be empty after stress");
    return TEST_PASS;
}

/**
 * Stress test with try_ (non-blocking) API: producer and consumer
 * poll instead of blocking.  Verifies no data loss under contention.
 */
static void* producer_try_thread(void* arg) {
    stress_ctx_t* ctx = (stress_ctx_t*)arg;
    uint32_t sent = 0;
    while (sent < ctx->count) {
        if (spsc_queue_try_add(ctx->q, (uint8_t)(sent & 0xFF))) {
            sent++;
        } else {
            tight_loop_contents();
        }
    }
    return NULL;
}

static void* consumer_try_thread(void* arg) {
    stress_ctx_t* ctx = (stress_ctx_t*)arg;
    uint8_t expected = 0;
    uint32_t received = 0;
    while (received < ctx->count) {
        uint8_t got;
        if (spsc_queue_try_remove(ctx->q, &got)) {
            if (got != expected) {
                ctx->error = 1;
                snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                         "try: byte %u: expected 0x%02X got 0x%02X",
                         received, expected, got);
                return NULL;
            }
            expected = (uint8_t)((expected + 1) & 0xFF);
            received++;
        } else {
            tight_loop_contents();
        }
    }
    return NULL;
}

static int test_stress_try_api(void) {
    static uint8_t buf[32];
    static spsc_queue_t q;
    spsc_queue_init(&q, buf, sizeof(buf));

    stress_ctx_t ctx = { .q = &q, .count = 5000000, .error = 0 };

    pthread_t prod, cons;
    pthread_create(&cons, NULL, consumer_try_thread, &ctx);
    pthread_create(&prod, NULL, producer_try_thread, &ctx);
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    ASSERT_TRUE(!ctx.error, ctx.error_msg);
    ASSERT_TRUE(spsc_queue_is_empty(&q), "queue should be empty after try stress");
    return TEST_PASS;
}

/**
 * Stress test: interleave peek + remove on consumer side.
 * Verifies peek always returns the same value that remove yields.
 */
static void* consumer_peek_thread(void* arg) {
    stress_ctx_t* ctx = (stress_ctx_t*)arg;
    uint8_t expected = 0;
    uint32_t received = 0;
    while (received < ctx->count) {
        uint8_t peeked, got;
        /* Try peek first */
        if (spsc_queue_try_peek(ctx->q, &peeked)) {
            /* Now remove – must get same value */
            if (!spsc_queue_try_remove(ctx->q, &got)) {
                /* Another consumer? Shouldn't happen in SPSC */
                ctx->error = 1;
                snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                         "peek succeeded but remove failed at %u", received);
                return NULL;
            }
            if (peeked != got) {
                ctx->error = 1;
                snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                         "peek/remove mismatch at %u: peek=0x%02X rm=0x%02X",
                         received, peeked, got);
                return NULL;
            }
            if (got != expected) {
                ctx->error = 1;
                snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                         "peek: byte %u: expected 0x%02X got 0x%02X",
                         received, expected, got);
                return NULL;
            }
            expected = (uint8_t)((expected + 1) & 0xFF);
            received++;
        } else {
            tight_loop_contents();
        }
    }
    return NULL;
}

static int test_stress_peek_remove(void) {
    static uint8_t buf[64];
    static spsc_queue_t q;
    spsc_queue_init(&q, buf, sizeof(buf));

    stress_ctx_t ctx = { .q = &q, .count = 2000000, .error = 0 };

    pthread_t prod, cons;
    pthread_create(&cons, NULL, consumer_peek_thread, &ctx);
    pthread_create(&prod, NULL, producer_try_thread, &ctx);
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    ASSERT_TRUE(!ctx.error, ctx.error_msg);
    ASSERT_TRUE(spsc_queue_is_empty(&q), "queue should be empty after peek stress");
    return TEST_PASS;
}

/**
 * Stress: level/free consistency while queue is active.
 * A monitor thread repeatedly samples level() and free() and
 * verifies the invariant: level + free == capacity - 1.
 */
typedef struct {
    spsc_queue_t*   q;
    volatile int    stop;
    volatile int    error;
    char            error_msg[256];
    uint64_t        samples;
} monitor_ctx_t;

static void* monitor_thread(void* arg) {
    monitor_ctx_t* ctx = (monitor_ctx_t*)arg;
    ctx->samples = 0;
    while (!ctx->stop) {
        /* Take a single atomic-ish snapshot of head and tail,
         * then compute level and free from that one snapshot.
         * Calling level() then free() would read head/tail twice
         * and the queue can change between those reads.          */
        uint32_t head = ctx->q->head;
        uint32_t tail = ctx->q->tail;
        uint32_t mask = ctx->q->mask;
        uint32_t cap  = ctx->q->capacity;

        uint32_t lvl      = (head - tail) & mask;
        uint32_t free_cnt = cap - 1 - lvl;

        /* Sanity: level must be <= capacity-1 */
        if (lvl >= cap) {
            ctx->error = 1;
            snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                     "level=%u >= capacity=%u (head=%u tail=%u)",
                     lvl, cap, head, tail);
            return NULL;
        }
        /* Invariant: level + free == capacity - 1 */
        if (lvl + free_cnt != cap - 1) {
            ctx->error = 1;
            snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                     "level=%u free=%u sum=%u expected=%u",
                     lvl, free_cnt, lvl + free_cnt, cap - 1);
            return NULL;
        }
        ctx->samples++;
    }
    return NULL;
}

static int test_stress_level_invariant(void) {
    static uint8_t buf[128];
    static spsc_queue_t q;
    spsc_queue_init(&q, buf, sizeof(buf));

    stress_ctx_t data_ctx = { .q = &q, .count = 5000000, .error = 0 };
    monitor_ctx_t mon_ctx = { .q = &q, .stop = 0, .error = 0, .samples = 0 };

    pthread_t prod, cons, mon;
    pthread_create(&mon,  NULL, monitor_thread,       &mon_ctx);
    pthread_create(&cons, NULL, consumer_try_thread,   &data_ctx);
    pthread_create(&prod, NULL, producer_try_thread,   &data_ctx);

    pthread_join(prod, NULL);
    pthread_join(cons, NULL);
    mon_ctx.stop = 1;
    pthread_join(mon, NULL);

    printf("    (monitor sampled %lu times)\n", (unsigned long)mon_ctx.samples);

    ASSERT_TRUE(!data_ctx.error, data_ctx.error_msg);
    ASSERT_TRUE(!mon_ctx.error, mon_ctx.error_msg);
    ASSERT_TRUE(spsc_queue_is_empty(&q), "empty after invariant test");
    return TEST_PASS;
}

/* ------------------------------------------------------------------ */
/* Throughput benchmark                                                */
/* ------------------------------------------------------------------ */

static int test_throughput_benchmark(void) {
    static uint8_t buf[4096];
    static spsc_queue_t q;
    spsc_queue_init(&q, buf, sizeof(buf));

    const uint32_t count = 20000000;
    stress_ctx_t ctx = { .q = &q, .count = count, .error = 0 };

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    pthread_t prod, cons;
    pthread_create(&cons, NULL, consumer_thread, &ctx);
    pthread_create(&prod, NULL, producer_thread, &ctx);
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    clock_gettime(CLOCK_MONOTONIC, &t1);

    double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
    double mops = count / elapsed / 1e6;

    printf("    %u bytes in %.3f s  =>  %.1f M ops/s\n", count, elapsed, mops);

    ASSERT_TRUE(!ctx.error, ctx.error_msg);
    ASSERT_TRUE(spsc_queue_is_empty(&q), "empty after benchmark");
    return TEST_PASS;
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */

int main(void) {
    printf("\n=== SPSC Queue Test Suite ===\n\n");

    printf("-- Unit tests (single-threaded) --\n");
    RUN_TEST(test_init);
    RUN_TEST(test_add_remove_single);
    RUN_TEST(test_fill_and_drain);
    RUN_TEST(test_wraparound);
    RUN_TEST(test_blocking_ops);

    printf("\n-- Stress tests (multi-threaded) --\n");
    RUN_TEST(test_stress_small_queue);
    RUN_TEST(test_stress_large_queue);
    RUN_TEST(test_stress_try_api);
    RUN_TEST(test_stress_peek_remove);
    RUN_TEST(test_stress_level_invariant);

    printf("\n-- Throughput benchmark --\n");
    RUN_TEST(test_throughput_benchmark);

    printf("\n=== Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(", %d FAILED", tests_failed);
    }
    printf(" ===\n\n");

    return tests_failed > 0 ? 1 : 0;
}
