#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pirate.h"
#include "opt_args.h"
#include "fatfs/ff.h"
#include "pirate/storage.h"
#include "bytecode.h"
#include "mode/hwspi.h"
#include "lib/sfud/inc/sfud.h"
#include "lib/sfud/inc/sfud_def.h"
#include "spiflash.h"
#include "ui/ui_cmdln.h"
//#include "ui/ui_prompt.h"
//#include "ui/ui_const.h"
#include "ui/ui_help.h"
#include "system_config.h"
#include "pirate/amux.h"

static const char * const usage[]= {
    "flash [init|probe|erase|write|read|verify|test]\r\n\t[-f <file>] [-e(rase)] [-v(verify)] [-h(elp)]",
    "Initialize and probe: flash probe",
    "Erase and program, with verify: flash write -f example.bin -e -v",
    "Read to file: flash read -f example.bin",
    "Verify with file: flash verify -f example.bin",    
    "Test chip (full erase/write/verify): flash test"
};

static const struct ui_help_options options[]= {
{1,"", T_HELP_FLASH}, //flash command help
    {0,"init", T_HELP_FLASH_INIT}, //init
    {0,"probe", T_HELP_FLASH_PROBE}, //probe
    {0,"erase", T_HELP_FLASH_ERASE}, //erase
    {0,"write", T_HELP_FLASH_WRITE}, //write
    {0,"read",T_HELP_FLASH_READ}, //read
    {0,"verify",T_HELP_FLASH_VERIFY}, //verify   
    {0,"test",T_HELP_FLASH_TEST}, //test
    {0,"-f",T_HELP_FLASH_FILE_FLAG}, //file to read/write/verify    
    {0,"-e",T_HELP_FLASH_ERASE_FLAG}, //with erase (before write)
    {0,"-v",T_HELP_FLASH_VERIFY_FLAG},//with verify (after write)
};

enum flash_actions {
    FLASH_INIT=0,
    FLASH_PROBE,
    FLASH_ERASE,
    FLASH_WRITE,
    FLASH_READ,
    FLASH_VERIFY,
    FLASH_TEST
};

void flash(struct command_result *res){
    uint32_t value;
    char file[13];

/* Some notes on automating the command line parsing a bit more
    enum arg_types {
        ARG_NONE=0,
        ARG_STRING,
        ARG_UINT32T
    };

    typedef struct arg_item_struct
    {
        char flag;
        uint8_t type;
        bool arg_required;
        bool val_required; //if the arg is present, is the value mandatory? TODO: think through
        uint32_t def_val;
    }arg_item_t;

    const arg_item_t options[]={
        {'e', ARG_NONE, false, false, 0}, //erase
        {'v', ARG_NONE, false, false, 0}, //verify
    };
*/
    if(ui_help_show(res->help_flag,usage,count_of(usage), &options[0],count_of(options) )) return;

    if(system_config.mode!=4){
        printf("flash command is currently only available in SPI mode\r\n");
        return;
    }

    if(!ui_help_check_vout_vref()){
        return;
    }

    char action_str[9];
    bool init=false, probe=false, erase=false, verify=false, read=false, write=false, test=false;
    //action is the first argument (read/write/probe/erase/etc)
    if(cmdln_args_string_by_position(1, sizeof(action_str), action_str)){
        if(strcmp(action_str, "init")==0) init=true;
        if(strcmp(action_str, "probe")==0) probe=true;
        if(strcmp(action_str, "erase")==0) erase=true;  
        if(strcmp(action_str, "verify")==0) verify=true;    
        if(strcmp(action_str, "read")==0) read=true;    
        if(strcmp(action_str, "write")==0) write=true;
        if(strcmp(action_str, "test")==0) test=true;
    }

    //erase_flag
    bool erase_flag = cmdln_args_find_flag('e'|0x20);
    //verify_flag
    bool verify_flag = cmdln_args_find_flag('v'|0x20);
    //file to read/write/verify
    command_var_t arg;
    bool file_flag = cmdln_args_find_flag_string('f'|0x20, &arg, sizeof(file), file);
    if( (read||write||verify) && !file_flag ){
        printf("Missing file name (-f)\r\n");
        return;
    }

    //start and end rage? bytes to write/dump???
    sfud_flash flash_info= {.name = "SPI_FLASH", .spi.name = "SPI1"};
    uint8_t data[256];
    uint32_t start_address=0;

    if(!spiflash_init(&flash_info)) return;
    uint32_t end_address=flash_info.sfdp.capacity; //TODO: handle capacity in the flashtable when we pass from command line
    if(probe){
        spiflash_probe();
        return;
    }

    if(erase||erase_flag||test){
        if(!spiflash_erase(&flash_info)) return;
        if(verify_flag||test){
            if(!spiflash_erase_verify(start_address, end_address, sizeof(data), data, &flash_info)) return;
        }
    }

    if(test){
        if(!spiflash_write_test(start_address, end_address, sizeof(data), data, &flash_info)) return;
        if(!spiflash_write_verify(start_address, end_address, sizeof(data), data, &flash_info)) return;
    }

    if(write){
        if(!spiflash_load(start_address, end_address, sizeof(data), data, &flash_info, file)) return;
        if(verify_flag){
            uint8_t data2[256];
            if(!spiflash_verify(start_address, end_address, sizeof(data), data, data2, &flash_info, file)) return;
        }
    }

    if(read){
        if(!spiflash_dump(start_address, end_address, sizeof(data), data, &flash_info, file)) return;
    }  

    if(verify){
        if(!spiflash_verify(start_address, end_address, sizeof(data), data, data, &flash_info, file)) return; 
    }  
}