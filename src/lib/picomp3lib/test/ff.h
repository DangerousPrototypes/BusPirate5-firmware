// Minimal shim layer to allow FatFS commands on Linux machine
#pragma once
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef struct FIL 
{
    FILE* fs;
} FIL;

typedef char TCHAR;
typedef const char BYTE;
typedef uint32_t UINT;
typedef uint32_t FSIZE_t;

typedef enum 
{
    FR_OK = 0,
    FR_NOT_OK,
} FRESULT;

#define FA_READ             0x01
#define FA_WRITE            0x02
#define FA_OPEN_EXISTING    0x00
#define FA_CREATE_NEW       0x04
#define FA_CREATE_ALWAYS    0x08
#define FA_OPEN_ALWAYS      0x10
#define FA_OPEN_APPEND      0x30

/* Open or create a file */
extern FRESULT f_open(FIL* fp, const TCHAR* path, BYTE mode);

/* Close an open file object */
extern FRESULT f_close (FIL* fp);

/* Read data from the file */
extern FRESULT f_read (FIL* fp, void* buff, UINT btr, UINT* br);

/* Write data to the file */
extern FRESULT f_write (FIL* fp, const void* buff, UINT btw, UINT* bw);

/* Move file pointer of the file object */
extern FRESULT f_lseek (FIL* fp, FSIZE_t ofs);

extern const char *FRESULT_str(FRESULT i);
