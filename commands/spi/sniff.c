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
#include "hardware/pio.h"
#include "spisnif.pio.h"
#include "usb_rx.h"

static const char * const usage[]= {
/*    "flash [init|probe|erase|write|read|verify|test]\r\n\t[-f <file>] [-e(rase)] [-v(verify)] [-h(elp)]",
    "Initialize and probe: flash probe",
    "Erase and program, with verify: flash write -f example.bin -e -v",
    "Read to file: flash read -f example.bin",
    "Verify with file: flash verify -f example.bin",    
    "Test chip (full erase/write/verify): flash test",
    "Force dump: flash read -o -b <bytes> -f <file>"*/
};

static const struct ui_help_options options[]= {
/*{1,"", T_HELP_FLASH}, //flash command help
    {0,"init", T_HELP_FLASH_INIT}, //init
    {0,"probe", T_HELP_FLASH_PROBE}, //probe
    {0,"erase", T_HELP_FLASH_ERASE}, //erase
    {0,"write", T_HELP_FLASH_WRITE}, //write
    {0,"read",T_HELP_FLASH_READ}, //read
    {0,"verify",T_HELP_FLASH_VERIFY}, //verify   
    {0,"test",T_HELP_FLASH_TEST}, //test
    {0,"-f",T_HELP_FLASH_FILE_FLAG}, //file to read/write/verify    
    {0,"-e",T_HELP_FLASH_ERASE_FLAG}, //with erase (before write)
    {0,"-v",T_HELP_FLASH_VERIFY_FLAG},//with verify (after write)*/
};

static PIO pio;
static uint pio_state_machine;
static uint pio_loaded_offset;

static PIO pio;
static uint pio_state_machine_d1;
static uint pio_loaded_offset_d1;

bool pio_read(uint32_t *raw){
    if(pio_sm_is_rx_fifo_empty(pio, pio_state_machine)){
        return false;
    }
    // 8-bit read from the uppermost byte of the FIFO, as data is left-justified
    (*raw) = pio->rxf[pio_state_machine];
    //TODO: change this based on UART settings
    //Detect parity error?
    return true;
}

bool pio_read_d1(uint32_t *raw){
    if(pio_sm_is_rx_fifo_empty(pio, pio_state_machine_d1)){
        return false;
    }
    // 8-bit read from the uppermost byte of the FIFO, as data is left-justified
    (*raw) = pio->rxf[pio_state_machine_d1];
    //TODO: change this based on UART settings
    //Detect parity error?
    return true;
}

void sniff_handler(struct command_result *res){
    uint32_t value;
    char file[13];

    if(ui_help_show(res->help_flag,usage,count_of(usage), &options[0],count_of(options) )) return;
    pio=M_UART_PIO;
    pio_state_machine=M_UART_PIO_SM;
    pio_loaded_offset = pio_add_program(pio, &spisnif_program);
    spisnif_program_init(pio, pio_state_machine, pio_loaded_offset, bio2bufiopin[BIO3], bio2bufiopin[BIO0], bio2bufiopin[BIO2]);

    pio_state_machine_d1=M_UART_PIO_SM-1;
    pio_loaded_offset_d1 = pio_add_program(pio, &spisnif_2_program);
    spisnif_2_program_init(pio, pio_state_machine_d1, pio_loaded_offset_d1, bio2bufiopin[BIO3], bio2bufiopin[BIO1], bio2bufiopin[BIO2]);



    static const char pin_labels[][5] = {"DAT0","DAT1","SCLK","SCS"}; 
    
    system_bio_claim(true, BIO0, BP_PIN_MODE, pin_labels[0]);
    system_bio_claim(true, BIO1, BP_PIN_MODE, pin_labels[1]);
    system_bio_claim(true, BIO2, BP_PIN_MODE, pin_labels[2]);
    system_bio_claim(true, BIO3, BP_PIN_MODE, pin_labels[3]);
    printf("Any key to exit\r\n");
    while(true){
        if(pio_read(&value)){
            printf("%d ", value);
        }
        if(pio_read_d1(&value)){
            printf("(%d) ", value);
        }
        char c;
        if(rx_fifo_try_get(&c)) break;
    }
    pio_remove_program(pio, &spisnif_program, pio_loaded_offset);
    pio_remove_program(pio, &spisnif_2_program, pio_loaded_offset_d1);
    system_bio_claim(false, BIO0, BP_PIN_MODE, 0);
    system_bio_claim(false, BIO1, BP_PIN_MODE, 0);
    system_bio_claim(false, BIO2, BP_PIN_MODE, 0);
    system_bio_claim(false, BIO3, BP_PIN_MODE, 0);
}

