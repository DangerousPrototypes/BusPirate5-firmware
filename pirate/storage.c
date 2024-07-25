#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "hardware/timer.h"
#include "fatfs/ff.h"
#include "fatfs/diskio.h"
#include "pirate/bio.h"
#include "ui/ui_prompt.h"
#include "ui/ui_parse.h"
#include "ui/ui_term.h"
#include "mjson/mjson.h"
#include "pirate/storage.h"
#include "pirate/mem.h"
#include "ui/ui_cmdln.h"

FATFS fs;		/* FatFs work area needed for each volume */
//FIL fil;			/* File object needed for each open file */
//FRESULT fr;     /* FatFs return code */
// Size of read/write.
#define BUF_SIZE 512
// Insure 4-byte alignment.
uint32_t buf32[(BUF_SIZE + 3)/4];
uint8_t* buf = (uint8_t*)buf32;
UINT br;
UINT bw;

const char storage_fat_type_labels[][8] = {
    "FAT12",
    "FAT16",
    "FAT32",
    "EXFAT",
    "UNKNOWN"
};

const char *fresult_msg[]={
	[FR_OK]="ok",                               /* (0) Succeeded */
	[FR_DISK_ERR]="disk error",                 /* (1) A hard error occurred in the low level disk I/O layer */
	[FR_INT_ERR]="assertion failed",            /* (2) Assertion failed */
	[FR_NOT_READY]="drive not ready",			/* (3) The physical drive cannot work */
	[FR_NO_FILE]="file not found",				/* (4) Could not find the file */
	[FR_NO_PATH]="path not found",				/* (5) Could not find the path */
	[FR_INVALID_NAME]="invalid path",		/* (6) The path name format is invalid */
	[FR_DENIED]="access denied",				/* (7) Access denied due to prohibited access or directory full */
	[FR_EXIST]="access denied",				/* (8) Access denied due to prohibited access */
	[FR_INVALID_OBJECT]="invalid object",		/* (9) The file/directory object is invalid */
	[FR_WRITE_PROTECTED]="write prohibited",		/* (10) The physical drive is write protected */
	[FR_INVALID_DRIVE]="invalid drive",		/* (11) The logical drive number is invalid */
	[FR_NOT_ENABLED]="drive not enabled",			/* (12) The volume has no work area */
	[FR_NO_FILESYSTEM]="filesystem not found",		/* (13) There is no valid FAT volume */
	[FR_MKFS_ABORTED]="format aborted",		/* (14) The f_mkfs() aborted due to any problem */
	[FR_TIMEOUT]="timeout",				/* (15) Could not get a grant to access the volume within defined period */
	[FR_LOCKED]="file locked",				/* (16) The operation is rejected according to the file sharing policy */
	[FR_NOT_ENOUGH_CORE]="buffer full",		/* (17) LFN working buffer could not be allocated */
	[FR_TOO_MANY_OPEN_FILES]="too many open files",	/* (18) Number of open files > FF_FS_LOCK */
	[FR_INVALID_PARAMETER]="invalid parameter"	/* (19) Given parameter is invalid */
};

void storage_file_error(uint8_t res){
    if(res>0){
        printf("%sError:%s %s%s", ui_term_color_error(),ui_term_color_info(), fresult_msg[res], ui_term_color_reset());
    }
}

void storage_init(void){
    // Output Pins
    gpio_set_function(FLASH_STORAGE_CS, GPIO_FUNC_SIO);
    gpio_put(FLASH_STORAGE_CS, 1);
    gpio_set_dir(FLASH_STORAGE_CS, GPIO_OUT);    
}

uint8_t storage_mount(void){
    FRESULT fr;     /* FatFs return code */
    fr = f_mount(&fs, "", 1);
    if (fr != FR_OK) {
        system_config.storage_available=0;
        system_config.storage_mount_error=fr;
    }else{
        system_config.storage_available=1;
        system_config.storage_fat_type=fs.fs_type;
        system_config.storage_size=fs.csize * fs.n_fatent * BP_FLASH_DISK_BLOCK_SIZE * 1E-9; // 2048E-9; //512E-9;
    }
    return fr;
}

void storage_unmount(void){
    //make sure the low level storage is consistent
    disk_ioctl(fs.pdrv, CTRL_SYNC, 0);
    f_unmount("");
    system_config.storage_available=false;
    system_config.storage_mount_error=0;
#if BP5_REV == 8 || BP5_REV == 9
    printf("Storage removed\r\n");
#endif
}

bool storage_detect(void){        
    #if BP5_REV==8 || BP5_REV==9
    //TF flash card detect is measured through the analog mux for lack of IO pins....
    //if we have low, storage not previously available, and we didn't error out, try to mount
    if(hw_adc_raw[HW_ADC_MUX_CARD_DETECT]<100 && 
        system_config.storage_available==false && 
        system_config.storage_mount_error==0
    ){
        if(storage_mount()==FR_OK){
            printf("Storage mounted: %7.2f GB %s\r\n\r\n", system_config.storage_size,storage_fat_type_labels[system_config.storage_fat_type-1]);
        }else{
            printf("Mount error %d\r\n", system_config.storage_mount_error);
        }
    }

    //card removed, unmount, look for fresh insert next time
    if(hw_adc_raw[HW_ADC_MUX_CARD_DETECT]>=100 &&
        (system_config.storage_available==true || system_config.storage_mount_error!=0)
    ){
        storage_unmount();
    }
    #endif
    return true;
}

uint8_t storage_format(void) {
    FRESULT fr;     /* FatFs return code */
    uint8_t *work_buffer = BigBuffer_AllocateTemporary(FF_MAX_SS, 4, BP_BIG_BUFFER_OWNER_DISKFORMAT);
    if (!work_buffer) {
        return FR_NOT_ENOUGH_CORE;
    }

    fr = f_mkfs("", 0, work_buffer, FF_MAX_SS);
    BigBuffer_FreeTemporary(work_buffer, BP_BIG_BUFFER_OWNER_DISKFORMAT);

    if (fr == FR_OK) {
        fr = f_setlabel("Bus_Pirate5");
    }
    return fr;
}

bool storage_save_binary_blob_rollover(char *data, uint32_t ptr,uint32_t size, uint32_t rollover){
    FRESULT fr;     /* FatFs return code */
    FIL fil;			/* File object needed for each open file */
    fr = f_open(&fil, "la.bin", FA_WRITE | FA_CREATE_ALWAYS);	
    if (fr == FR_OK) {   
        //middle to end
        if(ptr+size<=rollover){     
            f_write(&fil, &data[ptr], size, &bw);	
        }else{
            f_write(&fil, &data[ptr], rollover-ptr, &bw);	
            f_write(&fil, &data[0], size-(rollover-ptr), &bw);	
        }
        fr = f_close(&fil);							
        if (fr == FR_OK) {		
            return true;
        }else{
            return false;
        }
    }
}

//TODO: this is a hack because of type issues I don't recall
//it should be able to be eliminated
uint32_t getint(const char *buf, int len, const char *path, uint32_t defvalue, int *found) {
  double dv;
  *found = mjson_get_number(buf, len, path, &dv);
  if(found){
      return dv;
  }else{
      return defvalue;
  }
}

uint32_t storage_save_mode(const char *filename, const mode_config_t *config, uint8_t count){
    if(system_config.storage_available==0){
        return 0;
    }
    FRESULT fr;     /* FatFs return code */
    FIL fil;			/* File object needed for each open file */
    fr = f_open(&fil, filename, FA_CREATE_ALWAYS | FA_WRITE);	
    if (fr != FR_OK) {
        storage_file_error(fr);
        return 0;
    }
    f_printf(&fil,"{\n");
    for (uint8_t i = 0; i < count; i++) {
        const char* tag = &config[i].tag[2]; // skip "$." prefix, which is used for loading...
        const char* comma = ( i == (count-1) ? "" : "," );
        if (config[i].formatted_as == MODE_CONFIG_FORMAT_HEXSTRING) {
            f_printf(&fil, "\"%s\": \"0x%06X\"%s\n", tag, *config[i].config, comma );
        } else {
            // fallback to decimal
            f_printf(&fil, "\"%s\": %d%s\n", tag, *config[i].config, comma );
        }
    }
    f_printf(&fil,"}");
    /* Close the file */
    f_close(&fil);  
    return 1;
}

uint32_t storage_load_mode(const char *filename, const mode_config_t *config, uint8_t count){
    char json[512];
    if (system_config.storage_available == 0) {
        return 0;
    }
    FRESULT fr;     /* FatFs return code */
    FIL fil;        /* File object needed for each open file */
    fr = f_open(&fil, filename, FA_READ);	
    if (fr != FR_OK) {
        return 0;
    }
    //FIL* fp,     /* [IN] File object */
    //void* buff,  /* [OUT] Buffer to store read data */
    //UINT btr,    /* [IN] Number of bytes to read */
    //UINT* br     /* [OUT] Number of bytes read */
    fr = f_read(&fil, &json, sizeof(json), &br);
	int found;
    for (uint8_t i = 0; i < count; i++) {
        const char* tag = config[i].tag;
        if (config[i].formatted_as == MODE_CONFIG_FORMAT_HEXSTRING) {
            char as_hexstring[11] = {}; // large enough for maximum uint32_t
            int len = mjson_get_string(json, br, tag, as_hexstring, sizeof(as_hexstring));
            if (len > 0) {
                uint32_t val = strtoul(as_hexstring, NULL, 16);
                *(config[i].config) = val;
            }
        } else { // default to reading integer
            uint32_t val = getint(json, br, tag, 0, &found);
            if (found) {
                //printf("%s: %d\r\n",config_t[i].tag, val);
                *(config[i].config) = val;
            } else {
                //printf("%s: not found\r\n",config_t[i].tag);
            }
        }
    }
    /* Close the file */
    f_close(&fil);
    return 1;
}

bool storage_ls(const char *location, const char *ext, const uint8_t flags)
{
    FRESULT fr;
    DIR dir;
    FILINFO fno;
    int nfile, ndir;

    fr = f_opendir(&dir, location);                       /* Open the directory */
    if (fr != FR_OK) {
        return false;
    }

    nfile = ndir = 0;
    for(;;) {
        fr = f_readdir(&dir, &fno);                   /* Read a directory item */
        if (fr != FR_OK || fno.fname[0] == 0)
            break;  /* Error or end of dir */
        strlwr(fno.fname); //FAT16 is only UPPERCASE, make it lower to be easy on the eyes...
        if (ext) {
            int fname_len = strlen(fno.fname);
            int ext_len = strlen(ext);
            if (fname_len > ext_len+1) {
                if (memcmp(fno.fname+fname_len-ext_len, ext, ext_len)) {
                    continue;
                }
            }

        }
        if (fno.fattrib & AM_DIR) {   /* Directory */
            if (flags & LS_DIRS)
                printf("%s   <DIR>   %s%s%s\r\n",ui_term_color_prompt(), ui_term_color_info(), fno.fname, ui_term_color_reset());
            ndir++;
        }
        else {   /* File */
            if (flags & LS_FILES)
                if (flags & LS_SIZE)
                    printf("%s%10u ", ui_term_color_prompt(), fno.fsize);
                printf("%s%s%s\r\n", ui_term_color_info(), fno.fname, ui_term_color_reset());
            nfile++;
        }
    }
    f_closedir(&dir);
    if (flags & LS_SUMM)
        printf("%s%d dirs, %d files%s\r\n", ui_term_color_info(), ndir, nfile, ui_term_color_reset());
    return true;
}

const char system_config_file[]="bpconfig.bp";

const mode_config_t system_config_json[]={
    // clang-format off
    {"$.terminal_language",       &system_config.terminal_language,       MODE_CONFIG_FORMAT_DECIMAL,   },
    {"$.terminal_ansi_color",     &system_config.terminal_ansi_color,     MODE_CONFIG_FORMAT_DECIMAL,   },
    {"$.terminal_ansi_statusbar", &system_config.terminal_ansi_statusbar, MODE_CONFIG_FORMAT_DECIMAL,   },
    {"$.display_format",          &system_config.display_format,          MODE_CONFIG_FORMAT_DECIMAL,   },
    {"$.lcd_screensaver_active",  &system_config.lcd_screensaver_active,  MODE_CONFIG_FORMAT_DECIMAL,   },
    {"$.lcd_timeout",             &system_config.lcd_timeout,             MODE_CONFIG_FORMAT_DECIMAL,   },
    {"$.led_effect",              &system_config.led_effect_as_uint32,    MODE_CONFIG_FORMAT_DECIMAL,   },
    {"$.led_color",               &system_config.led_color,               MODE_CONFIG_FORMAT_HEXSTRING, },
    {"$.led_brightness_divisor",  &system_config.led_brightness_divisor,  MODE_CONFIG_FORMAT_DECIMAL,   },
    {"$.terminal_usb_enable",     &system_config.terminal_usb_enable,     MODE_CONFIG_FORMAT_DECIMAL,   },
    {"$.terminal_uart_enable",    &system_config.terminal_uart_enable,    MODE_CONFIG_FORMAT_DECIMAL,   },
    {"$.terminal_uart_number",    &system_config.terminal_uart_number,    MODE_CONFIG_FORMAT_DECIMAL,   },
    {"$.debug_uart_enable",       &system_config.debug_uart_enable,       MODE_CONFIG_FORMAT_DECIMAL,   },
    {"$.debug_uart_number",       &system_config.debug_uart_number,       MODE_CONFIG_FORMAT_DECIMAL,   },
    // clang-format on
};

uint32_t storage_save_config(void){
    return storage_save_mode(system_config_file, system_config_json, count_of(system_config_json));
}

uint32_t storage_load_config(void){
 	return storage_load_mode(system_config_file, system_config_json, count_of(system_config_json));
}
