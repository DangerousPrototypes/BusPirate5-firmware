#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "hardware/timer.h"
#include "fatfs/ff.h"
//#include "fatfs/tf_card.h"
#include "bio.h"
#include "ui/ui_prompt.h"
#include "ui/ui_parse.h"
#include "ui/ui_term.h"
#include "mjson/mjson.h"
#include "storage.h"
#include "mem.h"
#include "ui/ui_cmdln.h"
#include "ui/ui_args.h"


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

void storage_init(void)
{
    // Output Pins
    gpio_set_function(FLASH_STORAGE_CS, GPIO_FUNC_SIO);
    gpio_put(FLASH_STORAGE_CS, 1);
    gpio_set_dir(FLASH_STORAGE_CS, GPIO_OUT);    
}



void storage_unmount(void)
{
    system_config.storage_available=false;
    system_config.storage_mount_error=0;
    printf("Storage removed\r\n");
}

bool storage_detect(void)
{        
    #if BP5_REV==8 || BP5_REV==9
    //TF flash card detect is measured through the analog mux for lack of IO pins....
    //if we have low, storage not previously available, and we didn't error out, try to mount
    if(hw_adc_raw[HW_ADC_MUX_CARD_DETECT]<100 && 
        system_config.storage_available==false && 
        system_config.storage_mount_error==0
    )
    {
        if(storage_mount())
        {
            printf("Storage mounted: %7.2f GB %s\r\n\r\n", system_config.storage_size,storage_fat_type_labels[system_config.storage_fat_type-1]);
        }
        else
        {
            printf("Mount error %d\r\n", system_config.storage_mount_error);
        }
    }

    //card removed, unmount, look for fresh insert next time
    if(hw_adc_raw[HW_ADC_MUX_CARD_DETECT]>=100 &&
        (system_config.storage_available==true || system_config.storage_mount_error!=0)
    )
    {
        storage_unmount();

    }
    #endif
    return true;
}


bool storage_mount(void)
{
    fr = f_mount(&fs, "", 1);
    if (fr != FR_OK) {
        system_config.storage_available=0;
        system_config.storage_mount_error=fr;
        return false;
    }
    else
    {
        system_config.storage_available=1;
        system_config.storage_fat_type=fs.fs_type;
        system_config.storage_size=fs.csize * fs.n_fatent * BP_FLASH_DISK_BLOCK_SIZE * 1E-9; // 2048E-9; //512E-9;
        return true;
    }

}

bool storage_save_binary_blob_rollover(char *data, uint32_t ptr,uint32_t size, uint32_t rollover)
{

    fr = f_open(&fil, "la.bin", FA_WRITE | FA_CREATE_ALWAYS);	
    if (fr == FR_OK) {
        
        //middle to end
        if(ptr+size<=rollover)
        {     
            f_write(&fil, &data[ptr], size, &bw);	
        }
        else
        {
            f_write(&fil, &data[ptr], rollover-ptr, &bw);	
            f_write(&fil, &data[0], size-(rollover-ptr), &bw);	
        }
        
        
        fr = f_close(&fil);							
        if (fr == FR_OK && bw == 11) {		
            return true;
        }else{
            return false;
        }
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
        //no config file or TF flash card
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

void file_error(FRESULT res)
{
    if(res>0)
    {
        printf("%sError:%s %s%s", ui_term_color_error(),ui_term_color_info(), fresult_msg[res], ui_term_color_reset());
    }
}

void hex(opt_args (*args), struct command_result *res)
{
    char file[512];

    /*if(system_config.storage_available==0)
    {
        return;
    }*/
    arg_var_t arg;
    char location[32];
    ui_args_find_string(&arg, sizeof(location), location);    

    fr = f_open(&fil, location, FA_READ);	
    if (fr != FR_OK) {
        //no config file or TF flash card
        file_error(fr);
        //return (int)fr;
        return;
    }

    uint8_t grouping=0;
    while(true)
    {
        UINT bytes_read;
        f_read(&fil, &file, sizeof(file),&bytes_read);
        if(bytes_read)
        {
            for(uint16_t i=0; i<bytes_read; i++)
            {
                printf(" 0x%02u ", file[i]);
                grouping++;
                if(grouping>=8)
                {
                    grouping=0;
                    printf("\r\n");
                }
            }

        }
        else
        {
            break;
        }
    }

    printf("\r\n");
   
    /* Close the file */
    f_close(&fil);  

    return;
}



void cat(opt_args (*args), struct command_result *res)
{
    char file[512];

    /*if(system_config.storage_available==0)
    {
        return;
    }*/
    arg_var_t arg;
    char location[32];
    ui_args_find_string(&arg, sizeof(location), location);    

    fr = f_open(&fil, location, FA_READ);	
    if (fr != FR_OK) {
        //no config file or TF flash card
        file_error(fr);
        //return (int)fr;
        return;
    }

    /* Read every line and display it */
    while (f_gets(file, sizeof(file), &fil)) {
        printf(file);
        printf("\r"); //usually this comes first, but hopefully makes text files display better
    }

    printf("\r\n");
   
    /* Close the file */
    f_close(&fil);  

    return;
}


void make_dir(opt_args (*args), struct command_result *res)
{
    FRESULT fr;
    
    arg_var_t arg;
    char location[32];
    ui_args_find_string(&arg, sizeof(location), location);

    fr = f_mkdir(location);
    file_error(fr);
}
void change_dir(opt_args (*args), struct command_result *res)
{
    FRESULT fr;
    
    arg_var_t arg;
    char location[32];
    ui_args_find_string(&arg, sizeof(location), location);

    fr = f_chdir(location);

    if(fr)
    {
        file_error(fr);
    }
    else
    {
        TCHAR str[128];
        fr = f_getcwd(str, 128);  /* Get current directory path */
        printf("%s\r\n",str);
    }
}
void storage_unlink(opt_args (*args), struct command_result *res)
{
    FRESULT fr;
    arg_var_t arg;
    char location[32];
    ui_args_find_string(&arg, sizeof(location), location);    
    fr = f_unlink(location);
    file_error(fr);
}

void list_dir(opt_args (*args), struct command_result *res)
{
    FRESULT fr;
    DIR dir;
    FILINFO fno;
    int nfile, ndir; 

    arg_var_t arg;
    char location[32];
    ui_args_find_string(&arg, sizeof(location), location);
    fr = f_opendir(&dir, location);                       /* Open the directory */
    //TCHAR str[128];
    //fr = f_getcwd(str, 128);  /* Get current directory path */
    //printf("%s\r\n",str);

    if (fr == FR_OK) {
        nfile = ndir = 0;
        for(;;)
        {
            fr = f_readdir(&dir, &fno);                   /* Read a directory item */
            if(fr != FR_OK || fno.fname[0] == 0) break;  /* Error or end of dir */
            strlwr(fno.fname);
            if(fno.fattrib & AM_DIR) 
            {   /* Directory */
                printf("%s   <DIR>   %s%s%s\r\n",ui_term_color_prompt(), ui_term_color_info(), fno.fname, ui_term_color_reset());
                ndir++;
            }
            else
            {   /* File */
                printf("%s%10u %s%s%s\r\n", 
                ui_term_color_prompt(),fno.fsize, ui_term_color_info(),fno.fname, ui_term_color_reset());
                nfile++;
            }
        }
        f_closedir(&dir);
        printf("%s%d dirs, %d files%s\r\n", ui_term_color_info(), ndir, nfile, ui_term_color_reset());
    } else {
        //printf("Failed to open \"%s\". (%s)\r\n", args[0].c, fresult_msg[fr]);
        file_error(fr);
    }


    return; // (uint32_t) res;
}

bool storage_format_confirm(void)
{
    uint32_t confirm;
    do{
        confirm=ui_prompt_yes_no();
    }while(confirm>1);

    return confirm;
}

void storage_format(opt_args (*args), struct command_result *res)
{
    cmdln_next_buf_pos();
    printf("Erase the internal storage?\r\ny/n> ");
    if(storage_format_confirm()==false) return;

    printf("\r\nAre you sure?\r\ny/n> ");
    if(storage_format_confirm()==false) return;
    printf("\r\n\r\nFormatting...\r\n");

    storage_format_base();
}

bool storage_format_base(void)
{

    uint8_t *work_buffer = mem_alloc(FF_MAX_SS, BP_BIG_BUFFER_DISKFORMAT);
    if (!work_buffer) {
        printf("Unable to allocate format work buffer. File system not created.\r\n");
        return false;
    }

    // make the file system
    fr = f_mkfs("", 0, work_buffer, FF_MAX_SS);
    if (FR_OK != fr) {
        printf("Format failed, result: %d\r\n", fr); // fs make failure
    }
    else {
        printf("Format succeeded!\r\n"); // fs make success
        // retry mount
        if(storage_mount())
        {
            printf("Storage mounted: %7.2f GB %s\r\n\r\n", system_config.storage_size,storage_fat_type_labels[system_config.storage_fat_type-1]);
        }
        else
        {
            printf("Mount error %d\r\n", system_config.storage_mount_error);
        }
    }

    mem_free(work_buffer);

    return (FR_OK == fr);

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
        //no config file or TF flash card
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