#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pirate.h"
#include "opt_args.h"
#include "fatfs/ff.h"
#include "storage.h"
#include "bytecode.h"
#include "mode/hwspi.h"
#include "../lib/sfud/inc/sfud.h"
#include "../lib/sfud/inc/sfud_def.h"
#include "mode/spiflash.h"
#include "ui/ui_cmdln.h"
#include "ui/ui_prompt.h"
#include "ui/ui_const.h"
#include "ui/ui_args.h"
#include "ui/ui_help.h"
#include "system_config.h"
#include "amux.h"

const char * const flash_usage[]= 
{
    "flash [-i] [-p] [-e] [-w <file>] [-r <file>] [-v] [-t] [-h]",
    "Initialize and probe: flash -i -p ",
    "Erase and program, with verify: flash -e -w example.bin -v",
    "Read to file: flash -r example.bin",
    "Test chip: flash -t"
};

const struct ui_info_help help_flash[]= 
{
{1,"", T_HELP_FLASH}, //flash command help
    {0,"-i", T_HELP_FLASH_INIT}, //init
    {0,"-p", T_HELP_FLASH_PROBE}, //probe
    {0,"-e", T_HELP_FLASH_ERASE}, //erase
    {0,"-w", T_HELP_FLASH_WRITE}, //write
    {0,"-r",T_HELP_FLASH_READ}, //read
    {0,"-v",T_HELP_FLASH_VERIFY}, //verify   
    {0,"-t",T_HELP_FLASH_TEST}, //verify     
};

void flash(struct command_result *res)
{
    arg_var_t arg;
    char read_file[13];
    char write_file[13];
    uint32_t value;

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

    bool help = ui_args_find_flag_novalue('h'|0x20, &arg);

    if(arg.has_arg)
    {
        ui_help_usage(flash_usage,count_of(flash_usage));
        ui_help_options(&help_flash[0],count_of(help_flash));
        return;
    }

    if(system_config.mode!=4)
    {
        printf("flash command is currently only available in SPI mode\r\n");
        return;
    }

    if(!amux_check_vout_vref())
    {
        return;
    }

    //init chip? (default action)
    //bool init = ui_args_find_flag_novalue('i'|0x20, &arg);
    //probe chip? 
    bool probe = ui_args_find_flag_novalue('p'|0x20, &arg);
    //erase chip?
    bool erase = ui_args_find_flag_novalue('e'|0x20, &arg);
    //verify chip?
    bool verify = ui_args_find_flag_novalue('v'|0x20, &arg);
    //read?
    bool read = ui_args_find_flag_string('r'|0x20, &arg, sizeof(read_file), read_file);
    //to file?
    if(read && !arg.has_value)
    {
        printf("Missing dump file name\r\n");
        return;
    }
    //write
    //from file?
    bool write = ui_args_find_flag_string('w'|0x20, &arg, sizeof(write_file), write_file);
    if(write && !arg.has_value)
    {
        printf("Missing load file name\r\n");
        return;
    }    

    //test chip? erase, write, verify
    bool test = ui_args_find_flag_novalue('t'|0x20, &arg);

    //start and end rage? bytes to write/dump???

    sfud_flash flash_info= {.name = "SPI_FLASH", .spi.name = "SPI1"};
    uint8_t data[256];
    uint32_t start_address=0;

    if(!spiflash_init(&flash_info)) return;
    uint32_t end_address=flash_info.sfdp.capacity; //TODO: handle capacity in the flashtable when we pass from command line

    if(probe)
    {
        spiflash_probe();
        return;
    }


    if(erase||test)
    {
        if(!spiflash_erase(&flash_info)) return;
        if(verify||test)
        {
            if(!spiflash_erase_verify(start_address, end_address, sizeof(data), data, &flash_info)) return;
        }
    }

    if(test)
    {
        if(!spiflash_write_test(start_address, end_address, sizeof(data), data, &flash_info)) return;
        if(!spiflash_write_verify(start_address, end_address, sizeof(data), data, &flash_info)) return;
    }

    if(!test && write)
    {
        if(!spiflash_load(start_address, end_address, sizeof(data), data, &flash_info, write_file)) return;
        if(verify)
        {
            uint8_t data2[256];
            if(!spiflash_verify(start_address, end_address, sizeof(data), data, data2, &flash_info, write_file)) return;
        }
    }

    if(read)
    {
        if(!spiflash_dump(start_address, end_address, sizeof(data), data, &flash_info, read_file)) return;
    }    
}