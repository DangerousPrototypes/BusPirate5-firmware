/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Modified for Bus Pirate:
 *  2^n length buffers only: we use bit masking for faster rollover checking
 *  This also helps match the behavior of DMA when set to ring buffer mode
 *  Only handles 8 bit queues, type is fixed at car
 *
 */

#include <stdlib.h>
#include <string.h>
#include "queue.h"

#define DISABLE_SPINLOCK 1

void queue2_init_with_spinlock(queue_t* q, uint8_t* buf, uint element_count, uint spinlock_num) {
    #ifndef DISABLE_SPINLOCK
    lock_init(&q->core, spinlock_num);
    #endif
    q->data = buf; //(uint8_t *)calloc(element_count, sizeof(char));
    q->element_count = (uint16_t)element_count;
    q->wptr = 0;
    q->rptr = 0;
}

void queue_available_bytes_unsafe(queue_t* q, uint16_t* cnt) {
    uint16_t wptr_saved = q->wptr;
    uint16_t rptr_saved = q->rptr; // we're in charge of read pointer so this shouldnt change

    if (rptr_saved > wptr_saved) {
        (*cnt) = (q->element_count - rptr_saved) + wptr_saved;
    } else {
        (*cnt) = wptr_saved - rptr_saved;
    }
}

void queue_available_bytes(queue_t* q, uint16_t* cnt) {
    #ifndef DISABLE_SPINLOCK
    uint32_t save = spin_lock_blocking(q->core.spin_lock);
    #endif
    if (q->rptr > q->wptr) {
        (*cnt) = (q->element_count - q->rptr) + q->wptr;
    } else {
        (*cnt) = q->wptr - q->rptr;
    }
    #ifndef DISABLE_SPINLOCK
    lock_internal_spin_unlock_with_notify(&q->core, save);
    #endif
}

void queue_update_read_pointer(queue_t* q, uint16_t* cnt) {
    #ifndef DISABLE_SPINLOCK
    uint32_t save = spin_lock_blocking(q->core.spin_lock);
    #endif
    q->rptr = (q->rptr + (*cnt)) & (q->element_count - 1);
    #ifndef DISABLE_SPINLOCK
    lock_internal_spin_unlock_with_notify(&q->core, save);
    #endif
}

static bool queue_add_internal(queue_t* q, const char* data, bool block) {
    do {
        #ifndef DISABLE_SPINLOCK
        uint32_t save = spin_lock_blocking(q->core.spin_lock);
        #endif
        // if (queue2_get_level_unsafe(q) != q->element_count) {
        if (((q->wptr + 1) & (q->element_count - 1)) != q->rptr) {
            // memcpy(element_ptr(q, q->wptr), data, q->element_size);
            q->data[q->wptr] = (*data);
            // q->wptr = inc_index(q, q->wptr);
            q->wptr = (q->wptr + 1) & (q->element_count - 1);
            #ifndef DISABLE_SPINLOCK
            lock_internal_spin_unlock_with_notify(&q->core, save);
            #endif
            return true;
        }
        if (block) {
            #ifndef DISABLE_SPINLOCK
            lock_internal_spin_unlock_with_wait(&q->core, save);
            #endif
        } else {
            #ifndef DISABLE_SPINLOCK
            spin_unlock(q->core.spin_lock, save);
            #endif
            return false;
        }
    } while (true);
}

static bool queue_remove_internal(queue_t* q, char* data, bool block) {
    do {
        #ifndef DISABLE_SPINLOCK
        uint32_t save = spin_lock_blocking(q->core.spin_lock);
        #endif
        // if (queue2_get_level_unsafe(q) != 0) {
        if (((q->rptr) & (q->element_count - 1)) != q->wptr) {
            // memcpy(data, element_ptr(q, q->rptr), q->element_size);
            (*data) = q->data[q->rptr];
            // q->rptr = inc_index(q, q->rptr);
            q->rptr = (q->rptr + 1) & (q->element_count - 1);
            #ifndef DISABLE_SPINLOCK
            lock_internal_spin_unlock_with_notify(&q->core, save);
            #endif
            return true;
        }
        if (block) {
            #ifndef DISABLE_SPINLOCK
            lock_internal_spin_unlock_with_wait(&q->core, save);
            #endif
        } else {
            #ifndef DISABLE_SPINLOCK
            spin_unlock(q->core.spin_lock, save);
            #endif
            return false;
        }
    } while (true);
}

static bool queue_peek_internal(queue_t* q, char* data, bool block) {
    do {
        #ifndef DISABLE_SPINLOCK
        uint32_t save = spin_lock_blocking(q->core.spin_lock);
        #endif
        // if (queue2_get_level_unsafe(q) != 0) {
        if (((q->rptr) & (q->element_count - 1)) != q->wptr) {
            // memcpy(data, element_ptr(q, q->rptr), q->element_size);
            (*data) = q->data[q->rptr];
            #ifndef DISABLE_SPINLOCK
            lock_internal_spin_unlock_with_notify(&q->core, save);
            #endif
            return true;
        }
        if (block) {
            #ifndef DISABLE_SPINLOCK
            lock_internal_spin_unlock_with_wait(&q->core, save);
            #endif
        } else {
            #ifndef DISABLE_SPINLOCK
            spin_unlock(q->core.spin_lock, save);
            #endif  
            return false;
        }
    } while (true);
}

bool queue2_try_add(queue_t* q, const char* data) {
    return queue_add_internal(q, data, false);
}

bool queue2_try_remove(queue_t* q, char* data) {
    return queue_remove_internal(q, data, false);
}

bool queue2_try_peek(queue_t* q, char* data) {
    return queue_peek_internal(q, data, false);
}

void queue2_add_blocking(queue_t* q, const char* data) {
    queue_add_internal(q, data, true);
}

void queue2_remove_blocking(queue_t* q, char* data) {
    queue_remove_internal(q, data, true);
}

void queue2_peek_blocking(queue_t* q, char* data) {
    queue_peek_internal(q, data, true);
}
