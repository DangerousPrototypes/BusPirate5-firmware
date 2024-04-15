#ifndef _TF_CARD_H_
#define _TF_CARD_H_

/* SPI pin assignment */

#include "fatfs/ff.h" /* Obtains integer types */

DSTATUS diskio_initialize(BYTE pdrv);
DSTATUS diskio_status(BYTE pdrv);
DRESULT diskio_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count);
DRESULT diskio_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count);
DRESULT diskio_ioctl(BYTE pdrv, BYTE cmd, void *buff);

#endif // _TF_CARD_H_
