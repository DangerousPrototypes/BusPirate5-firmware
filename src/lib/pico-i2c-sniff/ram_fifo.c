/**
 * FIFO in RAM.
 * (C) Juan Schiavoni 2021
 */
#include <stdlib.h>
#include "ram_fifo.h"

static uint32_t *capture_buf = NULL;

static uint32_t capture_set = 0;
static uint32_t capture_get = 0;
static uint32_t capture_count = 0;
static uint32_t capture_size = 0;

bool ram_fifo_init(size_t size) {
    capture_buf  = malloc(size * sizeof(uint32_t));
    capture_size = size;
    capture_set = 0;
    capture_get = 0;
    capture_count = 0;

    return  (capture_buf != NULL);
}

bool ram_fifo_is_empty(void){
    return (capture_count == 0);   
}

bool ram_fifo_set(uint32_t item) {
    if (capture_set >= capture_size){
        capture_set = 0;    
    }

    if (capture_count < capture_size) {
        capture_buf[capture_set++] = item;

        capture_count++;
        return true;
    }

    return false;
}

uint32_t ram_fifo_get(void) {
    if (capture_get >= capture_size){
        capture_get = 0;    
    }

    capture_count--;

    return capture_buf[capture_get++];
}