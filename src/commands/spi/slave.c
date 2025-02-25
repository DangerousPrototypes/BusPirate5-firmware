#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pirate.h"
#include "command_struct.h"
#include "bytecode.h"
#include "mode/hwspi.h"
#include "ui/ui_cmdln.h"
// #include "ui/ui_prompt.h"
// #include "ui/ui_const.h"
#include "ui/ui_help.h"
#include "system_config.h"
#include "pirate/amux.h"
#include "usb_rx.h"
#include "pirate/bio.h"


static const char* const usage[] = {
    "This command has no help text. (yet)"
    /*    "flash [init|probe|erase|write|read|verify|test]\r\n\t[-f <file>] [-e(rase)] [-v(verify)] [-h(elp)]",
        "Initialize and probe: flash probe",
        "Erase and program, with verify: flash write -f example.bin -e -v",
        "Read to file: flash read -f example.bin",
        "Verify with file: flash verify -f example.bin",
        "Test chip (full erase/write/verify): flash test",
        "Force dump: flash read -o -b <bytes> -f <file>"*/
};

static const struct ui_help_options options[] = {
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


void slave_handler(struct command_result* res) {
    uint32_t value;
    char file[13];

    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
        return;
    }
    #define SPI_MODE (1) // Cant use SPI Mode 0 without toggling the CS line for every byte otherwise the receiving Pico in Mode 0 does not receive the data.

    // deinit SPI mode
    spi_deinit(M_SPI_PORT);
    bio_init();
    // set buffers to correct position
    bio_buf_input(M_SPI_CLK); // sck
    bio_buf_input(M_SPI_CDI);  
    bio_buf_output(M_SPI_CDO); 
    bio_buf_input(M_SPI_CS);   // cs
    //spi_set_format(M_SPI_PORT, data_bits, cpol, cpha, SPI_MSB_FIRST);
    spi_cpol_t cpol; // Clock Polarity
    spi_cpha_t cpha; // Clock Phase
    cpol = SPI_CPOL_0;
    cpha = SPI_CPHA_1;

    spi_init(M_SPI_PORT, 1000 * 1000);
    spi_set_format(M_SPI_PORT, 8, cpol, cpha, SPI_MSB_FIRST);
    spi_set_slave(M_SPI_PORT, true);

    // assign spi functon to io pins
    bio_set_function(M_SPI_CLK, GPIO_FUNC_SPI); // sck
    bio_set_function(M_SPI_CDO, GPIO_FUNC_SPI); // tx
    bio_set_function(M_SPI_CDI, GPIO_FUNC_SPI); // rx
    bio_set_function(M_SPI_CS, GPIO_FUNC_SPI);

    uint8_t out_buf[12] = {0x00, 0x01, 0x02};
    uint8_t in_buf[12] = {0x00};
    printf("Starting SPI slave\r\n");
    printf("SPI slave example using SPI Mode: %d\r\n", SPI_MODE);
    while(true){
        spi_write_read_blocking(M_SPI_PORT, out_buf, in_buf, 3);
        printf("Read: %02X, Wrote: %02X\r\n", in_buf[0], out_buf[0]);
        printf("Read: %02X, Wrote: %02X\r\n", in_buf[1], out_buf[1]);
        printf("Read: %02X, Wrote: %02X\r\n", in_buf[2], out_buf[2]);
        out_buf[0]++;
        out_buf[1]++;
        out_buf[2]++;
    }

}
