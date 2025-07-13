// Minimal shim layer to allow FatFS commands on Linux machine
#include "ff.h"

/* Open or create a file */
inline FRESULT f_open(FIL* fp, const TCHAR* path, BYTE mode)
{		
    char fmode[5];

    fmode[0] ='\0';
    
    // Parse the mode flags
    if (mode & FA_READ)
    {
        strcat(fmode, "rb");
    }

    if (mode & FA_WRITE)
    {
        strcat(fmode, "wb");
    }


    fp->fs = fopen(path, fmode);

    return (fp->fs != NULL) ? FR_OK : FR_NOT_OK;
}

/* Close an open file object */
inline FRESULT f_close (FIL* fp)
{											
    return (!fclose(fp->fs)) ? FR_OK : FR_NOT_OK;
}

/* Read data from the file */
inline FRESULT f_read (FIL* fp, void* buff, UINT btr, UINT* br)
{
    *br = fread(buff, sizeof(char) , btr, fp->fs);
    return FR_OK;
}

/* Write data to the file */
inline FRESULT f_write (FIL* fp, const void* buff, UINT btw, UINT* bw)
{
    *bw = fwrite(buff, sizeof(char), btw, fp->fs);
    return FR_OK;
}

/* Move file pointer of the file object */
inline FRESULT f_lseek (FIL* fp, FSIZE_t ofs)
{								
    return (!fseek(fp->fs, ofs, SEEK_SET)) ? FR_OK : FR_NOT_OK;
}

/* Should display error code */
const char *FRESULT_str(FRESULT i)
{
    return "Not Implemented";
}
