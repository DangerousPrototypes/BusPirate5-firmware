/**
 * @file disk.c
 * @author 
 * @brief implements all the disk cli commands
 * @version 0.1
 * @date 2024-05-11
 * 
 * @copyright Copyright (c) 2024
 * Modified by Lior Shalmay Copyright (c) 2024
 */
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
#include "pirate/storage.h"

#define DEF_ROW_SIZE    8
#define PRINTABLE(_c)   (_c>0x1f && _c<0x7f ? _c : '.')

static const char * const hex_usage[]= {
    "hex <file> [-d(address)] [-a(ascii)] [-s <size>]",
    "Print file contents in HEX: hex example.bin -d -a -s 8",
    "press 'x' to quit pager"
};
static const struct ui_help_options hex_options[]= {
{1,"", T_HELP_DISK_HEX}, //section heading
    {0,"<file>", T_HELP_DISK_HEX_FILE},
    {0,"-d", T_HELP_DISK_HEX_ADDR},
    {0,"-a", T_HELP_DISK_HEX_ASCII},
    {0,"-s <size>", T_HELP_DISK_HEX_SIZE},
    {0,"-t <off>", T_HELP_DISK_HEX_OFF},
    {0,"-c", T_HELP_DISK_HEX_PAGER_OFF}
};

// Show flags
#define HEX_NONE   0x00
#define HEX_ADDR   0x01
#define HEX_ASCII  0x02

// shown_off:  starting shown offset, used for display only
// page_lines: number of lines per page. 0 means no paging
// row_size:   row size in bytes
// flags:      show flags (address and ascii only for now)
static uint32_t hex_dump(FIL *fil, uint32_t shown_off, const uint16_t page_lines,
        const uint16_t row_size, const uint8_t flags)
{
    const bool flag_addr = flags & HEX_ADDR;
    const bool flag_ascii = flags & HEX_ASCII;
    const uint32_t page_size = page_lines ? page_lines * row_size : (uint32_t)-1;
    char buf[512];
    uint32_t buf_off = 0;
    uint32_t line_start_off = 0;
    uint32_t tot_read = 0;
    bool print_addr = false;

    if (flag_addr)
        print_addr = true;
    UINT bytes_read = 0;
    while (true) {
        f_read(fil, &buf, MIN(sizeof(buf), page_size), &bytes_read);
        tot_read += bytes_read;
        if (!bytes_read) {
            // Flush last line
            if (flag_ascii) {
                uint8_t rem = buf_off % row_size;
                if (rem) {
                    for (uint8_t j=0; j<row_size-rem; j++)
                        printf("   ");
                    printf(" |");
                    for (uint8_t j=0; j<rem; j++)
                        printf("%c", PRINTABLE(buf[line_start_off+j]));
                    printf("|");
                }
            }
            break;
        }
        for (uint16_t i=0; i<bytes_read; i++) {
            if (print_addr) {
                print_addr = false;
                printf("%04x  ", shown_off);
            }
            printf("%02x ", buf[i]);
            buf_off++;
            shown_off++;
            if (!(buf_off % row_size)) {
                if (flag_ascii) {
                    printf(" |");
                    for (uint8_t j=0; j<row_size; j++)
                        printf("%c", PRINTABLE(buf[line_start_off+j]));
                    printf("|");
                }
                printf("\r\n");
                if (flag_addr)
                    print_addr = true;
                line_start_off = buf_off;
            }
        }
        if (tot_read >= page_size)
            break;
    }
    return bytes_read;
}

void disk_hex_handler(struct command_result *res){
    //check help
    if(ui_help_show(res->help_flag,hex_usage,count_of(hex_usage), &hex_options[0],count_of(hex_options) )) return;

    FIL fil;        /* File object needed for each open file */
    FRESULT fr;     /* FatFs return code */
    char location[32];
    uint32_t off = 0;
    uint32_t row_size = DEF_ROW_SIZE;
    uint8_t flags = HEX_NONE;
    uint32_t bytes_read = 0;
    uint16_t page_lines = 0;
    uint32_t seek_off = 0;
    uint32_t pager_off = 0;
    command_var_t arg;
    char recv_char;

    cmdln_args_string_by_position(1, sizeof(location), location);
    fr = f_open(&fil, location, FA_READ);
    if (fr != FR_OK) {
        storage_file_error(fr);
        res->error=true;
        return;
    }

    if (cmdln_args_find_flag('d'|0x20))
       flags |= HEX_ADDR;
    if (cmdln_args_find_flag('a'|0x20))
        flags |= HEX_ASCII;
    if (!cmdln_args_find_flag_uint32('s'|0x20, &arg, &row_size))
        row_size = DEF_ROW_SIZE;
    if (!cmdln_args_find_flag_uint32('t'|0x20, &arg, &seek_off))
        seek_off = 0;
    if (cmdln_args_find_flag('c'|0x20))
        pager_off = 1;

    page_lines = system_config.terminal_ansi_rows;
    f_lseek(&fil, seek_off);
    off = seek_off;
    printf("\r\n");
    while ( (bytes_read = hex_dump(&fil, off, page_lines, row_size, flags)) > 0) {
        off += bytes_read;

        if (pager_off) continue;

        recv_char = ui_term_cmdln_wait_char('\0');
        switch (recv_char) {
            // give the user the ability to bail out
            case 'x':
                goto exit_hex_dump_early;
                break;
            // anything else just keep going
            default:
                break;
        }
    }

exit_hex_dump_early:
    f_close(&fil);
    printf("\r\n");
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

    if (!storage_ls(location, NULL, LS_ALL)) {
        storage_file_error(fr);
        res->error=true;
        return;
    }
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

static const char * const label_usage[]= {
    "label <get|set> [label name]",
    "Get flash storage label name: label get",
    "Set flash storage label name: label set <name>"
};

static const struct ui_help_options label_options[]= {
{1,"", T_HELP_DISK_LABEL}, 
    {0,"get", T_HELP_DISK_LABEL_GET},
    {0,"set", T_HELP_DISK_LABEL_SET},
};

typedef enum label_sub_commands {
    GET_LABEL,
    SET_LABEL,
    INVALID_LABEL_CMD,
} label_sub_cmd_e;
#define MAX_LABEL_LENGTH (11)

void disk_label_handler(struct command_result *res) {
    //check help
   	if(ui_help_show(res->help_flag,label_usage,count_of(label_usage), &label_options[0],count_of(label_options) )) return;

    char command_string[4];
    char label_string[MAX_LABEL_LENGTH + 2]; // maximum label length for fat12/16 is 11 characters
    FRESULT f_result;
    label_sub_cmd_e command = INVALID_LABEL_CMD;
    DWORD label_id;
    res->error=true;

    if(!cmdln_args_string_by_position(1, sizeof(command_string), command_string)){
        printf("Missing command argument, please provide either get or set commands\r\n");
        return;
    }
    if(strcmp(command_string, "get")==0) command = GET_LABEL;    
    else if (strcmp(command_string, "set")==0) command = SET_LABEL;

    switch (command)
    {
    case GET_LABEL:
        f_result = f_getlabel("", label_string, &label_id);
        if(f_result == FR_OK){
            printf("disk label: %s", label_string);
        } else {
            storage_file_error(f_result);
            return;
        }
        break;
    
    case SET_LABEL:
        if(!cmdln_args_string_by_position(2, sizeof(label_string), label_string)){
            printf("Missing label argument, please provide a name to set the label to\r\n");
            return;
        }
        if(strlen(label_string) > MAX_LABEL_LENGTH){
            printf("label is too long, maximum label length is %d characters\r\n", MAX_LABEL_LENGTH);
            return;
        }
        f_result = f_setlabel(label_string);
        if (f_result != FR_OK){
            storage_file_error(f_result);
            return;
        }
        break;

    default:
        printf("Invalid command: '%s'\r\n", command_string);
        return;
    }
    
    res->error=false;
    return;
}
