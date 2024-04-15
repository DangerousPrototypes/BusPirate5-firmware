/**
 * @file		fifo.h
 * @author		Andrew Loebs
 * @brief		Header file of the FIFO module
 *
 * Simple inline functions for FIFO buffer interaction
 *
 */

#ifndef __FIFO_H
#define __FIFO_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t *_front;
    uint8_t *_back;
    uint8_t *_read;
    uint8_t *_write;
} fifo_t;

/// @brief Macro for fifo static initializer
/// @param buffer Underlying memory block to be used by the fifo
/// @param len Length of the memory block
#define FIFO_STATIC_INIT(buffer, len)                                                              \
    {                                                                                              \
        ._front = (buffer), ._back = &(buffer)[(len)-1], ._read = (buffer), ._write = (buffer),    \
    }

/// @brief Inline function for checking if the fifo is empty
static inline bool fifo_is_empty(fifo_t *fifo) { return fifo->_read == fifo->_write; }

/// @brief Inline function for checking if the fifo is full
static inline bool fifo_is_full(fifo_t *fifo)
{
    bool normal_case = (fifo->_write + 1) == fifo->_read;
    bool edge_case = (fifo->_write == fifo->_back) && (fifo->_read == fifo->_front);

    return normal_case || edge_case;
}

/// @brief Inline function for enqueueing a byte into the fifo
/// @return true if enqueued successfully, false if full
static inline bool fifo_enqueue(fifo_t *fifo, uint8_t value)
{
    if (fifo_is_full(fifo)) return false;

    *fifo->_write = value;
    // circular increment
    if (fifo->_write == fifo->_back)
        fifo->_write = fifo->_front;
    else
        fifo->_write++;

    return true;
}

/// @brief Inline function for dequeueing a byte from the fifo
/// @return true if dequeued successfully, false if empty
static inline bool fifo_try_dequeue(fifo_t *fifo, uint8_t *out)
{
    if (fifo_is_empty(fifo)) return false;

    *out = *fifo->_read;
    // circular increment
    if (fifo->_read == fifo->_back)
        fifo->_read = fifo->_front;
    else
        fifo->_read++;

    return true;
}

#endif // __FIFO_H
