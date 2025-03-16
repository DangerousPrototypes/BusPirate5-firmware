/**
    @file sfe_psram.c

  */

#ifndef __psram_h__
#define __psram_h__

#include <stddef.h>

//void runtime_init_setup_psram(void);

size_t sfe_setup_psram(uint32_t psram_cs_pin);

void psram_reinit_timing(void);
void *__psram_malloc(size_t size);
void __psram_free(void *ptr);

void *__psram_realloc(void *ptr, size_t size);
void *__psram_calloc(size_t num, size_t size);

size_t __psram_largest_free_block(void);
size_t __psram_total_space(void);
size_t __psram_total_used(void);

size_t __psram_get_size(void);

#endif /* __psram_h__ */
