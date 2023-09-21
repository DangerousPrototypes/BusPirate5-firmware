#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pirate.h"
#include "system_config.h"
#include "hardware/timer.h"
#include "fatfs/ff.h"
#include "fatfs/tf_card.h"
#include "bio.h"
#include "storage.h"
#include "mjson/mjson.h"

FATFS fs;		/* FatFs work area needed for each volume */
FIL fil;			/* File object needed for each open file */
// Size of read/write.
#define BUF_SIZE 512
// Insure 4-byte alignment.
uint32_t buf32[(BUF_SIZE + 3)/4];
uint8_t* buf = (uint8_t*)buf32;

FRESULT fr;     /* FatFs return code */
UINT br;
UINT bw;

void storage_init(void)
{
    // Output Pins
    gpio_set_function(SDCARD_CS, GPIO_FUNC_SIO);
    gpio_put(SDCARD_CS, 1);
    gpio_set_dir(SDCARD_CS, GPIO_OUT);    
}

bool storage_detect(void)
{        
    //SD card detect is measued through the analog mux for lack of IO pins....
    //if we have low, storage not previously available, and we didn't error out, try to mount
    if(hw_adc_raw[HW_ADC_MUX_CARD_DETECT]<100 && 
        system_config.storage_available==false && 
        system_config.storage_mount_error==0
    )
    {
        //show a status message (SD card mounted: XXGB FAT32)
        fr = f_mount(&fs, "", 1);
        if (fr != FR_OK) {
            system_config.storage_available=false;
            system_config.storage_mount_error=fr;
            printf("Mount error %d\r\n", fr);
        }
        else
        {
            system_config.storage_available=true;
            system_config.storage_fat_type=fs.fs_type;
            system_config.storage_size=fs.csize * fs.n_fatent * 512E-9;
            printf("SD Card mounted: %7.2f GB %s\r\n\r\n", system_config.storage_size,storage_fat_type_labels[system_config.storage_fat_type-1]);
        }        

    }

    //card removed, unmount, look for fresh insert next time
    if(hw_adc_raw[HW_ADC_MUX_CARD_DETECT]>=100 &&
        (system_config.storage_available==true || system_config.storage_mount_error!=0)
    )
    {
        system_config.storage_available=false;
        system_config.storage_mount_error=0;
        printf("SD Card removed\r\n");

    }
    return true;
}

bool storage_mount(void)
{
    fr = f_mount(&fs, "", 1);
    if (fr != FR_OK) {
        //printf("Mount error %d\r\n", fr);
        system_config.storage_available=0;
        system_config.storage_mount_error=fr;
        return false;
    }
    else
    {
        system_config.storage_available=1;
        system_config.storage_fat_type=fs.fs_type;
        system_config.storage_size=fs.csize * fs.n_fatent * 512E-9;
        return true;
        /*printf("Mount ok\r\n");

        switch (fs.fs_type) {
            case FS_FAT12:
                printf("Type is FAT12\r\n");
                break;
            case FS_FAT16:
                printf("Type is FAT16\r\n");
                break;
            case FS_FAT32:
                printf("Type is FAT32\r\n");
                break;
            case FS_EXFAT:
                printf("Type is EXFAT\r\n");
                break;
            default:
                printf("Type is unknown\r\n");
                break;
        }

        printf("Card size: %7.2f GB (GB = 1E9 bytes)\r\n\r\n", fs.csize * fs.n_fatent * 512E-9);
        */
        
    }

}

uint32_t storage_new_file(void){

    fr = f_open(&fil, "newfile.txt", FA_WRITE | FA_CREATE_ALWAYS);	
    if (fr == FR_OK) {
        f_write(&fil, "It works!\r\n", 11, &bw);	
        fr = f_close(&fil);							
        if (fr == FR_OK && bw == 11) {		
            return 1;
        }else{
            return 0;
        }
    }
}

uint32_t getint(const char *buf, int len, const char *path, uint32_t defvalue, int *found) {
  double dv;

  *found = mjson_get_number(buf, len, path, &dv);
  
  if(found)
  {
      return dv;
  }
  else
  {
      return defvalue;
  }

}

uint32_t storage_save_mode(const char *filename, struct _mode_config_t *config_t, uint8_t count)
{
    if(system_config.storage_available==0)
    {
        return 0;
    }

    fr = f_open(&fil, filename, FA_CREATE_ALWAYS | FA_WRITE);	
    if (fr != FR_OK) {
        //no config file or SD card
        //printf("File not found: %d\r\n", fr);
        //return (int)fr;
        return 0;
    }

    f_printf(&fil,"{\n");
    for(uint8_t i=0; i<count; i++)
    {
        f_printf(&fil, "\"%s\": %d%s\n",&config_t[i].tag[2], *config_t[i].config, (i==(count-1)?"":",") );  
    }
    f_printf(&fil,"}");
    
    
    /* Close the file */
    f_close(&fil);  

    return 1;
}

uint32_t storage_load_mode(const char *filename, struct _mode_config_t *config_t, uint8_t count)
{
    char json[512];

    if(system_config.storage_available==0)
    {
        return 0;
    }

    fr = f_open(&fil, filename, FA_READ);	
    if (fr != FR_OK) {
        //no config file or SD card
        //printf("File not found: %d\r\n", fr);
        //return (int)fr;
        return 0;
    }

    //FIL* fp,     /* [IN] File object */
    //void* buff,  /* [OUT] Buffer to store read data */
    //UINT btr,    /* [IN] Number of bytes to read */
    //UINT* br     /* [OUT] Number of bytes read */
    fr = f_read ( &fil, &json, sizeof(json), &br);

	int found;
    for(uint8_t i=0; i<count; i++)
    {
        uint32_t val=getint(json, br, config_t[i].tag, 0, &found);
        if(found)
        {
            //printf("%s: %d\r\n",config_t[i].tag, val);
            *config_t[i].config=val;         
        }
        else
        {
            //printf("%s: not found\r\n",config_t[i].tag);
        }
    }

    /* Close the file */
    f_close(&fil);  

    return 1;
}

const char system_config_file[]="bpconfig.bp";

struct _mode_config_t system_config_json[]={
    {"$.terminal_language", &system_config.terminal_language},
    {"$.terminal_ansi_color", &system_config.terminal_ansi_color},
    {"$.terminal_ansi_statusbar", &system_config.terminal_ansi_statusbar},
    {"$.display_format", &system_config.display_format},   
    {"$.lcd_screensaver_active", &system_config.lcd_screensaver_active},
    {"$.lcd_timeout", &system_config.lcd_timeout},
    {"$.led_effect", &system_config.led_effect},
    {"$.led_color", &system_config.led_color},
    {"$.led_brightness", &system_config.led_brightness},   
    {"$.terminal_usb_enable", &system_config.terminal_usb_enable},
    {"$.terminal_uart_enable", &system_config.terminal_uart_enable},
    {"$.terminal_uart_number", &system_config.terminal_uart_number},
    {"$.debug_uart_enable", &system_config.debug_uart_enable},
    {"$.debug_uart_number", &system_config.debug_uart_number},
};

uint32_t storage_save_config(void)
{
    return storage_save_mode(system_config_file, system_config_json, count_of(system_config_json));
}

uint32_t storage_load_config(void)
{
 	return storage_load_mode(system_config_file, system_config_json, count_of(system_config_json));
}


#if 0
// ls/dir directory file list
    FRESULT res;
    DIR dir;
    UINT i;
    static FILINFO fno;
    char path[256];

        path[0]= '/';
        path[1]=0x00;

    res = f_opendir(&dir, path);                       /* Open the directory */
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno);                   /* Read a directory item */
            if (res != FR_OK || fno.fname[0] == 0) break;  /* Break on error or end of dir */
            if (fno.fattrib & AM_DIR) {                    /* It is a directory */
                /*i = strlen(path);
                sprintf(&path[i], "/%s", fno.fname);
                res = scan_files(path);                   
                if (res != FR_OK) break;
                path[i] = 0;*/
            } else {                                       /* It is a file. */
                printf("%s/%s\r\n", path, fno.fname);
            }
        }
        f_closedir(&dir);
    }
    else
    {
        printf("Directory error: %d", res);
    }

#endif