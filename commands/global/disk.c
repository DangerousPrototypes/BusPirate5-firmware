#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "fatfs/ff.h"
#include "ui/ui_prompt.h"
#include "ui/ui_parse.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "pirate/storage.h"
#include "pirate/mem.h"
#include "ui/ui_cmdln.h"

#define DEF_ROW_SIZE    8
#define PRINTABLE(_c)   (_c>0x1f && _c<0x7f ? _c : '.')

static const char * const hex_usage[]= {
    "hex <file> [-d(address)] [-a(ascii)] [-s <row size>]",
    "Print file contents in HEX: hex example.bin",
};
static const struct ui_help_options hex_options[]= {
{1,"", T_HELP_DISK_HEX}, //section heading
    {0,"<file>", T_HELP_DISK_HEX_FILE}, 
};
void disk_hex_handler(struct command_result *res){
    //check help
    if(ui_help_show(res->help_flag,hex_usage,count_of(hex_usage), &hex_options[0],count_of(hex_options) )) return;

    FIL fil;        /* File object needed for each open file */
    FRESULT fr;     /* FatFs return code */
    char file[512];
    char location[32];
    uint32_t off = 0;
    uint32_t tmp_off = 0;
    uint32_t row_size = DEF_ROW_SIZE;
    bool flag_addr = false;
    bool flag_ascii = false;
    command_var_t arg;

    cmdln_args_string_by_position(1, sizeof(location), location);
    fr = f_open(&fil, location, FA_READ);
    if (fr != FR_OK) {
        storage_file_error(fr);
        res->error=true;
        return;
    }

    flag_addr = cmdln_args_find_flag('d'|0x20);
    flag_ascii = cmdln_args_find_flag('a'|0x20);
    if (!cmdln_args_find_flag_uint32('s'|0x20, &arg, &row_size))
        row_size = DEF_ROW_SIZE;

    printf("\r\n");
    if (flag_addr)
        printf("%04x  ", off);
    while (true) {
        UINT bytes_read;
        f_read(&fil, &file, sizeof(file),&bytes_read);
        if (!bytes_read) {
            // Flush last line
            if (flag_ascii) {
                uint8_t rem = off % row_size;
                if (rem) {
                    for (uint8_t j=0; j<row_size-rem; j++)
                        printf("   ");
                    printf(" |");
                    for (uint8_t j=0; j<rem; j++)
                        printf("%c", PRINTABLE(file[tmp_off+j]));
                    printf("|");
                }
            }
            break;
        }
        for (uint16_t i=0; i<bytes_read; i++) {
            printf("%02x ", file[i]);
            off++;
            if (!(off % row_size)) {
                if (flag_ascii) {
                    printf(" |");
                    for (uint8_t j=0; j<row_size; j++)
                        printf("%c", PRINTABLE(file[tmp_off+j]));
                    printf("|");
                }
                printf("\r\n");
                if (flag_addr)
                    printf("%04x  ", off);
                tmp_off = off;
            }
        }
    }
    printf("\r\n");
    /* Close the file */
    f_close(&fil);
}

static const char * const cat_usage[]= {
    "cat <file>",
    "Print file contents: cat example.txt",
};
static const struct ui_help_options cat_options[]= {
{1,"", T_HELP_DISK_CAT}, //section heading
    {0,"<file>", T_HELP_DISK_CAT_FILE}, 
};
void disk_cat_handler(struct command_result *res){
    //check help
   	if(ui_help_show(res->help_flag,cat_usage,count_of(cat_usage), &cat_options[0],count_of(cat_options) )) return;

    FIL fil;		/* File object needed for each open file */
    FRESULT fr;     /* FatFs return code */    
    char file[512];
    char location[32];
    cmdln_args_string_by_position(1, sizeof(location), location);    
    fr = f_open(&fil, location, FA_READ);	
    if (fr != FR_OK) {
        storage_file_error(fr);
        res->error=true;
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
}

static const char * const mkdir_usage[]= {
    "mkdir <dir>",
    "Create directory: mkdir dir",
};
static const struct ui_help_options mkdir_options[]= {
{1,"", T_HELP_DISK_MKDIR}, //section heading
    {0,"<dir>", T_HELP_DISK_MKDIR_DIR}, 
};
void disk_mkdir_handler(struct command_result *res){
    //check help
   	if(ui_help_show(res->help_flag,mkdir_usage,count_of(mkdir_usage), &mkdir_options[0],count_of(mkdir_options) )) return;

    FRESULT fr;
    char location[32];
    cmdln_args_string_by_position(1, sizeof(location), location);  
    fr = f_mkdir(location);
    if(fr != FR_OK){
        storage_file_error(fr);
        res->error=true;
    }
}

static const char * const cd_usage[]= {
    "cd <dir>",
    "Change directory: cd dir",
};
static const struct ui_help_options cd_options[]= {
{1,"", T_HELP_DISK_CD}, //section heading
    {0,"<dir>", T_HELP_DISK_CD_DIR}, 
};
void disk_cd_handler(struct command_result *res){
    //check help
   	if(ui_help_show(res->help_flag,cd_usage,count_of(cd_usage), &cd_options[0],count_of(cd_options) )) return;

    FRESULT fr;
    char location[32];
    cmdln_args_string_by_position(1, sizeof(location), location);  
    fr = f_chdir(location);
    if(fr != FR_OK){
        storage_file_error(fr);
        res->error=true;
        return;
    }

    TCHAR str[128];
    fr = f_getcwd(str, 128);  /* Get current directory path */
    printf("%s\r\n",str);
}

static const char * const rm_usage[]= {
    "rm [<file>|<dir>]",
    "Delete file: rm example.txt",
    "Delete directory: rm dir",
};
static const struct ui_help_options rm_options[]= {
{1,"", T_HELP_DISK_RM}, //section heading
    {0,"<file>", T_HELP_DISK_RM_FILE}, 
    {0,"<dir>", T_HELP_DISK_RM_DIR}, 
};
void disk_rm_handler(struct command_result *res){
    //check help
   	if(ui_help_show(res->help_flag,rm_usage,count_of(rm_usage), &rm_options[0],count_of(rm_options) )) return;

    FRESULT fr;
    char location[32];
    cmdln_args_string_by_position(1, sizeof(location), location);  
    fr = f_unlink(location);
    if(fr != FR_OK){
        storage_file_error(fr);
        res->error=true;
    }
}

static const char * const ls_usage[]= {
    "ls <dir>",
    "Show current directory contents: ls",
    "Show directory contents: ls /dir",
};
static const struct ui_help_options ls_options[]= {
{1,"", T_HELP_DISK_LS}, //section heading
    {0,"<dir>", T_HELP_DISK_LS_DIR}, 
};
void disk_ls_handler(struct command_result *res){
    //check help
   	if(ui_help_show(res->help_flag,ls_usage,count_of(ls_usage), &ls_options[0],count_of(ls_options) )) return;

    FRESULT fr;
    DIR dir;
    FILINFO fno;
    int nfile, ndir; 

    //is there a trailing path to ls?
    char location[32];
    cmdln_args_string_by_position(1, sizeof(location), location);  
    fr = f_opendir(&dir, location);                       /* Open the directory */
    if (fr != FR_OK) {
        storage_file_error(fr);
        res->error=true;
        return;
    }

    nfile = ndir = 0;
    for(;;){
        fr = f_readdir(&dir, &fno);                   /* Read a directory item */
        if(fr != FR_OK || fno.fname[0] == 0) break;  /* Error or end of dir */
        strlwr(fno.fname); //FAT16 is only UPPERCASE, make it lower to be easy on the eyes...
        if(fno.fattrib & AM_DIR){   /* Directory */
            printf("%s   <DIR>   %s%s%s\r\n",ui_term_color_prompt(), ui_term_color_info(), fno.fname, ui_term_color_reset());
            ndir++;
        }else{   /* File */
            printf("%s%10u %s%s%s\r\n", 
            ui_term_color_prompt(),fno.fsize, ui_term_color_info(),fno.fname, ui_term_color_reset());
            nfile++;
        }
    }
    f_closedir(&dir);
    printf("%s%d dirs, %d files%s\r\n", ui_term_color_info(), ndir, nfile, ui_term_color_reset());
}

uint8_t disk_format(void){
    // make the file system
    FRESULT fr = storage_format();
    if(fr==FR_NOT_ENOUGH_CORE){
        printf("Error: Buffer not available. Is the scope or logic analyzer running?\r\n");
    }

    if (FR_OK != fr) {
        storage_file_error(fr);
        printf("Error: Format failed...\r\n", fr); // fs make failure
        return fr;
    }
    printf("Format success!\r\n"); // fs make success
    
    // retry mount
    fr=storage_mount();
    if(fr!=FR_OK) {
        storage_file_error(fr);
        printf("Mount error %d\r\n", system_config.storage_mount_error);
        return fr;
    }
    printf("Storage mounted: %7.2f GB %s\r\n\r\n", system_config.storage_size,storage_fat_type_labels[system_config.storage_fat_type-1]);
    return fr;
}

bool disk_format_confirm(void){
    uint32_t confirm;
    do{
        confirm=ui_prompt_yes_no();
    }while(confirm>1);
    return confirm;
}

static const char * const format_usage[]= {
    "format",
    "Format storage: format",
};
static const struct ui_help_options format_options[]= {
{1,"", T_HELP_DISK_FORMAT}, //section heading
    {0,"format", T_HELP_DISK_FORMAT_CMD}, 
};

void disk_format_handler(struct command_result *res){
    //check help
   	if(ui_help_show(res->help_flag,format_usage,count_of(format_usage), &format_options[0],count_of(format_options) )) return;

    cmdln_next_buf_pos();
    printf("Erase the internal storage?\r\ny/n> ");
    if(disk_format_confirm()==false) return;
    printf("\r\nAre you sure?\r\ny/n> ");
    if(disk_format_confirm()==false) return;
    printf("\r\n\r\nFormatting...\r\n");
    uint8_t format_status=disk_format();
    if(format_status!=FR_OK){
        storage_file_error(format_status);
        res->error=true;        
    }
}
