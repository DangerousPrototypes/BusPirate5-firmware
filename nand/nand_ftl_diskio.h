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

//#include "../fatfs/diskio.h" // types from the diskio driver
//#include "../fatfs/ff.h"     // BYTE type

DSTATUS nand_ftl_diskio_initialize(void);
DSTATUS nand_ftl_diskio_status(void);
DRESULT nand_ftl_diskio_read(BYTE *buff, LBA_t sector, UINT count);
DRESULT nand_ftl_diskio_write(const BYTE *buff, LBA_t sector, UINT count);
DRESULT nand_ftl_diskio_ioctl(BYTE cmd, void *buff);

#endif // __NAND_FTL_DISKIO_H
