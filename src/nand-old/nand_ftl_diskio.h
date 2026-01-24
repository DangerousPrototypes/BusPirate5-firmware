/**
 * @file		nand_ftl_diskio.h
 * @author		Andrew Loebs
 * @brief		Header file of the nand ftl diskio module
 *
 * Glue layer between fatfs diskio and dhara ftl.
 *
 */

#ifndef __NAND_FTL_DISKIO_H
#define __NAND_FTL_DISKIO_H

// #include "../fatfs/diskio.h" // types from the diskio driver
// #include "../fatfs/ff.h"     // BYTE type

DSTATUS diskio_initialize(BYTE drv);
DSTATUS diskio_status(BYTE drv);
DRESULT diskio_read(BYTE drv, BYTE* buff, LBA_t sector, UINT count);
DRESULT diskio_write(BYTE drv, const BYTE* buff, LBA_t sector, UINT count);
DRESULT diskio_ioctl(BYTE drv, BYTE cmd, void* buff);

#endif // __NAND_FTL_DISKIO_H
