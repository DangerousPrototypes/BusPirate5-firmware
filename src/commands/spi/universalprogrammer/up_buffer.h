/*
    memorybuffer functions for the universal programmer 'plank' (https:// xx) 
    
    (C) Chris van Dongen 2025-26

*/

static void dumpbuffer(uint32_t start, uint32_t len);
static void crcbuffer(uint32_t start, uint32_t len);
static void clearbuffer(uint32_t start, uint32_t len, uint8_t fillbyte);
static void readbuffer(uint32_t start, uint32_t fstart, uint32_t len, char *fname);
static void hexreadbuffer(char *fname);
static void writebuffer(uint32_t start, uint32_t len, char *fname);

