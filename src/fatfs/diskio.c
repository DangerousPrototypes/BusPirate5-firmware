/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/
#include <stdint.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "diskio.h" /* Declarations of disk functions */

#ifdef BP_HW_STORAGE_TFCARD
    #include "fatfs/tf_card.h"
#elif defined(BP_HW_STORAGE_NAND)
    #include "../nand2/nand_ftl_diskio.h"
#else
    #error "No storage device defined in diskio.c"
#endif

// Drive number for spi_nand device
#define PDRV_NAND_FTL 0

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/
DSTATUS disk_status(BYTE pdrv /* Physical drive number to identify the drive */
)
{
    //if (PDRV_NAND_FTL == pdrv) {
        return diskio_status(pdrv);
    //}
    //else {
    //    return STA_NODISK;
   // }
}

/*-----------------------------------------------------------------------*/
/* Initialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize(BYTE pdrv /* Physical drive nmuber to identify the drive */
)
{
    //if (PDRV_NAND_FTL == pdrv) {
        return diskio_initialize(pdrv);
    //}
    //else {
    //    return STA_NODISK;
    //}
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read(BYTE pdrv,    /* Physical drive nmuber to identify the drive */
                  BYTE *buff,   /* Data buffer to store read data */
                  LBA_t sector, /* Start sector in LBA */
                  UINT count    /* Number of sectors to read */
)
{
    //if (PDRV_NAND_FTL == pdrv) {
        return diskio_read(pdrv, buff, sector, count);
    //}
    //else {
    //    return STA_NODISK;
    //}
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write(BYTE pdrv,        /* Physical drive nmuber to identify the drive */
                   const BYTE *buff, /* Data to be written */
                   LBA_t sector,     /* Start sector in LBA */
                   UINT count        /* Number of sectors to write */
)
{
    //if (PDRV_NAND_FTL == pdrv) {
        return diskio_write(pdrv, buff, sector, count);
    //}
    //else {
    //    return STA_NODISK;
    //}
}

#endif

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl(BYTE pdrv, /* Physical drive number (0..) */
                   BYTE cmd,  /* Control code */
                   void *buff /* Buffer to send/receive control data */
)
{
    //if (PDRV_NAND_FTL == pdrv) {
        return diskio_ioctl(pdrv, cmd, buff);
    //}
    //else {
    //    return STA_NODISK;
    //}
}
