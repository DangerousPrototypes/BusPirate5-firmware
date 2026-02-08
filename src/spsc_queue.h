/**
 * @file spsc_queue.h
 * @brief Lock-free Single-Producer Single-Consumer queue implementation
 * 
 * This provides a lock-free ring buffer optimized for the RP2040/RP2350
 * dual-core architecture. It replaces the spinlock-based queue which had
 * disabled spinlocks causing potential race conditions.
 * 
 * Key features:
 * - Lock-free: Uses memory barriers instead of spinlocks
 * - Static allocation: No malloc/free required
 * - Power-of-2 buffer sizes for efficient modulo via bitmask
 * - Single producer, single consumer safe across cores
 * 
 * Usage pattern in Bus Pirate:
 * - rx_fifo: Core1 produces (USB/UART/RTT input), Core0 consumes
 * - tx_fifo: Core0 produces (printf output), Core1 consumes
 * 
 * @author Bus Pirate Project
 * @date 2026
 * @license BSD-3-Clause
 */

#ifndef SPSC_QUEUE_H
#define SPSC_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "pico/stdlib.h"      // For tight_loop_contents()
#include "hardware/sync.h"    // For __dmb()

/**
 * @brief SPSC queue structure
 * 
 * The head and tail pointers are kept on separate cache lines where possible
 * to avoid false sharing. On RP2040/RP2350 with unified memory, this is less
 * critical but still good practice.
 * 
 * Invariants:
 * - head: Next position to write (modified only by producer)
 * - tail: Next position to read (modified only by consumer)
 * - Queue is empty when head == tail
 * - Queue is full when (head + 1) & mask == tail
 */
typedef struct {
    volatile uint32_t head;      /**< Write position (producer only) */
    volatile uint32_t tail;      /**< Read position (consumer only) */
    uint8_t* buffer;             /**< Pointer to data buffer */
    uint32_t capacity;           /**< Total buffer size (must be power of 2) */
    uint32_t mask;               /**< capacity - 1, for fast modulo */
} spsc_queue_t;

/**
 * @brief Initialize an SPSC queue with an external buffer
 * 
 * @param q         Pointer to queue structure
 * @param buffer    Pointer to buffer storage (caller-provided)
 * @param capacity  Size of buffer in bytes (MUST be power of 2)
 * 
 * @pre capacity must be a power of 2
 * @pre buffer must be at least capacity bytes
 * 
 * Example:
 * @code
 * static uint8_t my_buffer[256];
 * static spsc_queue_t my_queue;
 * spsc_queue_init(&my_queue, my_buffer, sizeof(my_buffer));
 * @endcode
 */
static inline void spsc_queue_init(spsc_queue_t* q, uint8_t* buffer, uint32_t capacity) {
    // Verify capacity is power of 2
    #ifndef NDEBUG
    assert((capacity & (capacity - 1)) == 0 && capacity > 0);
    #endif
    q->head = 0;
    q->tail = 0;
    q->buffer = buffer;
    q->capacity = capacity;
    q->mask = capacity - 1;
}

/**
 * @brief Try to add a byte to the queue (non-blocking)
 * 
 * @param q     Pointer to queue
 * @param data  Byte to add
 * @return true if byte was added, false if queue is full
 * 
 * @pre Must be called from producer core only
 * @note Lock-free, uses memory barrier for visibility
 */
static inline bool spsc_queue_try_add(spsc_queue_t* q, uint8_t data) {
    uint32_t head = q->head;
    uint32_t next_head = (head + 1) & q->mask;
    
    // Check if queue is full
    // Stale tail is safe: may falsely report full, caller retries
    if (next_head == q->tail) {
        return false;  // Full
    }
    
    // Write data to buffer
    q->buffer[head] = data;
    
    // Release barrier: ensure data write is visible before head update
    // Pairs with acquire barrier in consumer's try_remove/try_peek
    __dmb();
    
    // Publish new head position
    q->head = next_head;
    
    return true;
}

/**
 * @brief Add a byte to the queue (blocking)
 * 
 * @param q     Pointer to queue
 * @param data  Byte to add
 * 
 * @pre Must be called from producer core only
 * @warning This will spin-wait if queue is full - use with caution
 */
static inline void spsc_queue_add_blocking(spsc_queue_t* q, uint8_t data) {
    while (!spsc_queue_try_add(q, data)) {
        // Spin-wait - could add __wfe() for power saving on ARM
        tight_loop_contents();
    }
}

/**
 * @brief Try to remove a byte from the queue (non-blocking)
 * 
 * @param q     Pointer to queue
 * @param data  Pointer to store removed byte
 * @return true if byte was removed, false if queue is empty
 * 
 * @pre Must be called from consumer core only
 * @note Lock-free, uses memory barrier for visibility
 */
static inline bool spsc_queue_try_remove(spsc_queue_t* q, uint8_t* data) {
    uint32_t tail = q->tail;
    
    // Check if queue is empty
    // Stale head is safe: may falsely report empty, caller retries
    if (tail == q->head) {
        return false;  // Empty
    }
    
    // Acquire barrier: ensure we see data written before head was updated
    // Pairs with release barrier in producer's try_add
    __dmb();
    
    // Read data from buffer
    *data = q->buffer[tail];
    
    // Release barrier: ensure data read completes before tail update is visible
    // Prevents producer from seeing free slot and overwriting before read finishes
    __dmb();
    
    // Publish new tail position
    q->tail = (tail + 1) & q->mask;
    
    return true;
}

/**
 * @brief Remove a byte from the queue (blocking)
 * 
 * @param q     Pointer to queue
 * @param data  Pointer to store removed byte
 * 
 * @pre Must be called from consumer core only
 * @warning This will spin-wait if queue is empty - use with caution
 */
static inline void spsc_queue_remove_blocking(spsc_queue_t* q, uint8_t* data) {
    while (!spsc_queue_try_remove(q, data)) {
        // Spin-wait - could add __wfe() for power saving on ARM
        tight_loop_contents();
    }
}

/**
 * @brief Try to peek at the next byte without removing (non-blocking)
 * 
 * @param q     Pointer to queue
 * @param data  Pointer to store peeked byte
 * @return true if byte available, false if queue is empty
 * 
 * @pre Must be called from consumer core only
 */
static inline bool spsc_queue_try_peek(spsc_queue_t* q, uint8_t* data) {
    uint32_t tail = q->tail;
    
    // Check if queue is empty
    if (tail == q->head) {
        return false;  // Empty
    }
    
    // Memory barrier: ensure we read data after confirming not empty
    __dmb();
    
    // Read data without advancing tail
    *data = q->buffer[tail];
    
    return true;
}

/**
 * @brief Peek at the next byte (blocking)
 * 
 * @param q     Pointer to queue
 * @param data  Pointer to store peeked byte
 * 
 * @pre Must be called from consumer core only
 * @warning This will spin-wait if queue is empty
 */
static inline void spsc_queue_peek_blocking(spsc_queue_t* q, uint8_t* data) {
    while (!spsc_queue_try_peek(q, data)) {
        tight_loop_contents();
    }
}

/**
 * @brief Get number of bytes available to read
 * 
 * @param q     Pointer to queue
 * @return Number of bytes in queue
 * 
 * @note Safe to call from either core, but result may be stale
 */
static inline uint32_t spsc_queue_level(spsc_queue_t* q) {
    uint32_t head = q->head;
    uint32_t tail = q->tail;
    
    // Handle wrap-around using mask
    return (head - tail) & q->mask;
}

/**
 * @brief Get number of bytes free for writing
 * 
 * @param q     Pointer to queue
 * @return Number of free bytes (capacity - 1 when empty due to full detection)
 * 
 * @note Safe to call from either core, but result may be stale
 */
static inline uint32_t spsc_queue_free(spsc_queue_t* q) {
    // One slot is always kept empty to distinguish full from empty
    return q->capacity - 1 - spsc_queue_level(q);
}

/**
 * @brief Check if queue is empty
 * 
 * @param q     Pointer to queue
 * @return true if empty
 * 
 * @note Safe to call from either core, but result may be stale
 */
static inline bool spsc_queue_is_empty(spsc_queue_t* q) {
    return q->head == q->tail;
}

/**
 * @brief Check if queue is full
 * 
 * @param q     Pointer to queue
 * @return true if full
 * 
 * @note Safe to call from either core, but result may be stale
 */
static inline bool spsc_queue_is_full(spsc_queue_t* q) {
    return ((q->head + 1) & q->mask) == q->tail;
}

#endif // SPSC_QUEUE_H
