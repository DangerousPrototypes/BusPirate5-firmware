#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include <string.h>
#include "pirate.h"
#include "ui/ui_term.h"
#include "command_struct.h"
#include "ui/ui_help.h"
#include "ui/ui_cmdln.h"
#include "binmode/fala.h"
#include "fatfs/ff.h"       // File system related
#include "ui/ui_hex.h" // Hex display related
//#include "ui/ui_progress_indicator.h" // Progress indicator related
//#include "commands/i2c/eeprom.h"
#include "pirate/file.h" // File handling related
#include "pirate/hwspi.h" // SPI related functions
#include "eeprom_base.h"
#include "pirate/bio.h" // Bio related functions

#define SPI_EEPROM_READ_CMD 0x03 // Read command for EEPROM
#define SPI_EEPROM_WRITE_CMD 0x02 // Write command for EEPROM   
#define SPI_EEPROM_WRDI_CMD 0x04 // Write Disable command for EEPROM
#define SPI_EEPROM_WREN_CMD 0x06 // Write Enable command for EEPROM
#define SPI_EEPROM_RDSR_CMD 0x05 // Read Status Register command for EEPROM
#define SPI_EEPROM_WRSR_CMD 0x01 // Write Status Register command

enum eeprom_actions_enum {
    EEPROM_DUMP=0,
    EEPROM_ERASE,
    EEPROM_WRITE,
    EEPROM_READ,
    EEPROM_VERIFY,
    EEPROM_TEST,
    EEPROM_LIST,
    EEPROM_PROTECT
};

static const struct cmdln_action_t eeprom_actions[] = {
    { EEPROM_DUMP, "dump" },
    { EEPROM_ERASE, "erase" },
    { EEPROM_WRITE, "write" },
    { EEPROM_READ, "read" },
    { EEPROM_VERIFY, "verify" },
    { EEPROM_TEST, "test" },
    { EEPROM_LIST, "list"  },
    { EEPROM_PROTECT, "protect"}
};


static const char* const usage[] = {
    "eeprom [dump|erase|write|read|verify|test|list|protect]\r\n\t[-d <device>] [-f <file>] [-v(verify)] [-s <start address>] [-b <bytes>] [-t(test)] [-p <protection blocks>] [-w <WPEN>] [-h(elp)]",
    "List available EEPROM devices:%s eeprom list",
    "Display contents:%s eeprom dump -d 25x020",
    "Display 16 bytes starting at address 0x60:%s eeprom dump -d 25x020 -s 0x60 -b 16",
    "Erase, verify:%s eeprom erase -d 25x020 -v",
    "Write from file, verify:%s eeprom write -d 25x020 -f example.bin -v",
    "Read to file, verify:%s eeprom read -d 25x020 -f example.bin -v",
    "Verify against file:%s eeprom verify -d 25x020 -f example.bin",
    "Test chip (full erase/write/verify):%s eeprom test -d 25x020",
    "Probe Status Register block protection:%s eeprom protect -d 25x020",
    "Test for chip block protection features:%s eeprom protect -d 25x020 -t",
    "Disable all block protection bits (BP1, BP0):%s eeprom protect -d 25x020 -p 0b00",
    "Disable Write Pin ENable (WPEN):%s eeprom protect -d 25x020 -w 0",
};

static const struct ui_help_options options[] = {
    { 1, "", T_HELP_SPI_EEPROM },               // command help
    { 0, "dump", T_HELP_EEPROM_DUMP },  
    { 0, "erase", T_HELP_EEPROM_ERASE },    // erase
    { 0, "write", T_HELP_EEPROM_WRITE },    // write
    { 0, "read", T_HELP_EEPROM_READ },      // read
    { 0, "verify", T_HELP_EEPROM_VERIFY },  // verify
    { 0, "test", T_HELP_EEPROM_TEST },      // test
    { 0, "list", T_HELP_EEPROM_LIST},      // list devices
    { 0, "protect", T_HELP_EEPROM_PROTECT }, // protect
    { 0, "-d", T_HELP_EEPROM_DEVICE_FLAG }, // device to use
    { 0, "-f", T_HELP_EEPROM_FILE_FLAG },   // file to read/write/verify
    { 0, "-v", T_HELP_EEPROM_VERIFY_FLAG }, // with verify (after write)
    { 0, "-s", UI_HEX_HELP_START }, // start address for dump
    { 0, "-b", UI_HEX_HELP_BYTES }, // bytes to dump
    { 0, "-q", UI_HEX_HELP_QUIET}, // quiet mode, disable address and ASCII columns
    { 0, "-c", T_HELP_DISK_HEX_PAGER_OFF },
    { 0, "-t", T_HELP_EEPROM_SPI_TEST_FLAG },   // test chip for block protection features
    { 0, "-p", T_HELP_EEPROM_PROTECT_FLAG }, // set block protection bits (BP1, BP0)
    { 0, "-w", T_HELP_EEPROM_SPI_WPEN_FLAG },   // set Write Pin ENable (WPEN)
    { 0, "-h", T_HELP_FLAG },   // help
};

//-----------------------------------------------------------------------
// SPI EEPROM hardware abstraction layer functions
//-----------------------------------------------------------------------

//poll for busy status, return false if write is complete, true if timeout
static bool eeprom_25x_poll_busy(struct eeprom_info *eeprom){
    uint8_t reg;
    for(uint32_t i=0; i<0xfffff; i++) {
        hwspi_write_read_cs((uint8_t[]){SPI_EEPROM_RDSR_CMD}, 1, &reg, 1); // send the read status command
        if((reg & 0x01) == 0) { // check if WIP bit is clear
            return false; // write is complete
        }
    }
    //printf("Error: EEPROM write timeout\r\n");
    return true;
}

static bool eeprom_25x_read(struct eeprom_info *eeprom, uint32_t address, uint32_t read_bytes, uint8_t *buf) {
    // get the address for the current byte
    uint8_t block_select_bits = 0;
    uint8_t address_array[3];
    if(eeprom->device->hal->get_address(eeprom, address, &block_select_bits, address_array)){ 
        return true; // error getting address
    }

    // read the data from the EEPROM
    hwspi_select(); // select the EEPROM chip
    hwspi_write((SPI_EEPROM_READ_CMD|block_select_bits)); // send the read command with block select bits
    hwspi_write_n(address_array, eeprom->device->address_bytes); // send the address bytes
    hwspi_read_n(buf, read_bytes); // read 16 bytes from the EEPROM
    hwspi_deselect(); // deselect the EEPROM chip
    return false;
}

static bool eeprom_25x_write_page(struct eeprom_info *eeprom, uint32_t address, uint8_t *buf, uint32_t page_write_size){
    //get address
    uint8_t block_select_bits = 0;
    uint8_t address_array[3];
    if(eeprom->device->hal->get_address(eeprom, address, &block_select_bits, address_array))return true; // get the address   
    //printf("BS: 0x%02X, Address: 0x%02X 0x%02X 0x%02X, Page Write Size: %d\r\n", block_select_bits, address_array[2], address_array[1], address_array[0], page_write_size);
    //need to do a partial page write
    //first read the existing page from the eeprom
    //then update with the new data
    //finally write the updated page back to the eeprom
    if(page_write_size < eeprom->device->page_bytes) {
        // if the page write size is less than the device page size, we need to read the existing page first
        uint8_t existing_page[EEPROM_ADDRESS_PAGE_SIZE];
        if(eeprom_25x_read(eeprom, address, eeprom->device->page_bytes, existing_page)) {
            return true; // error reading existing page
        }
        // update the existing page with the new data
        memcpy(&buf[page_write_size], &existing_page[page_write_size], eeprom->device->page_bytes - page_write_size);
    }

    hwspi_write_read_cs((uint8_t[]){SPI_EEPROM_WREN_CMD}, 1, NULL, 0); // enable write
    hwspi_select(); // select the EEPROM chip
    hwspi_write((SPI_EEPROM_WRITE_CMD|block_select_bits)); // send the read command with block select bits
    hwspi_write_n(address_array, eeprom->device->address_bytes); // send the address bytes
    for(uint32_t i = 0; i < eeprom->device->page_bytes; i++) {
        hwspi_write(buf[i]);
    }
    //hwspi_write_n(buf, eeprom->device->page_bytes); // write the page data, use the specified page write size
    hwspi_deselect(); // deselect the EEPROM chip
    
    if(eeprom_25x_poll_busy(eeprom))return true; // poll for write complete, return true if timeout
    return false; // write is complete
}

static void eeprom_25x_status_reg_print(uint8_t reg) {
    // print the status register in a human readable format
    printf("Status Register: 0x%02X\r\n", reg);
    //printf("Write Enable Latch (WEL): %s\r\n", (reg & 0x02) ? "Enabled" : "Disabled");
    //printf("Write In Progress (WIP): %s\r\n", (reg & 0x01) ? "In Progress" : "Idle");
    printf("Write Pin ENable (WPEN): %s\r\n", (reg & 0x80) ? "Enabled" : "Disabled");
    printf("Block Protect Bits (BP1, BP0): %d, %d\r\n", (reg >> 3) & 0x01, (reg >> 2) & 0x01);
    printf("Protection range: ");
    if((reg & 0b1100) == 0b00) {
        printf("None\r\n");
    } else if((reg & 0b1100) == 0b0100) {
        printf("Upper 1/4\r\n");
    } else if((reg & 0b1100) == 0b1000) {
        printf("Upper 1/2\r\n");
    } else if((reg & 0b1100) == 0b1100) {
        printf("All\r\n");
    } else {
        printf("Unknown\r\n");
    }
    
}

static uint8_t eeprom_25x_read_status_register(struct eeprom_info *eeprom) {
    // read the status register
    uint8_t status_reg;
    hwspi_write_read_cs((uint8_t[]){SPI_EEPROM_RDSR_CMD}, 1, &status_reg, 1); // send the read status command
    return status_reg; // return the status register value
}

static bool eeprom_25x_write_status_register(struct eeprom_info *eeprom, uint8_t value) {
    // write the status register
    hwspi_write_read_cs((uint8_t[]){SPI_EEPROM_WREN_CMD}, 1, NULL, 0); // enable write
    hwspi_write_read_cs((uint8_t[]){SPI_EEPROM_WRSR_CMD, value}, 2, NULL, 0); // send the write status command with value
    if(eeprom_25x_poll_busy(eeprom)) return true; // poll for write complete, return true if timeout
    return false; // write is complete
}

static bool eeprom_25x_is_write_protect(struct eeprom_info *eeprom) {
    // check if the write protect bits are set
    uint8_t status_reg = eeprom_25x_read_status_register(eeprom); // read the status register
    if(status_reg & 0b1100) { // check if WEL bit is set
        printf("Error: block write protection enabled, write will fail!\r\n\r\n");
        eeprom_25x_status_reg_print(status_reg); // print the status register
        printf("\r\nDisable block write protection first: eeprom protect -d %s -p 0b00\r\n", eeprom->device->name);
        return true; // write protect bits are set
    }
    return false; // write protect bits are not set
}

#define BIT_EQUAL(a, b, bitpos) ((((a) >> (bitpos)) & 1) == (((b) >> (bitpos)) & 1))
static bool eeprom_25x_probe_block_protect(struct eeprom_info *eeprom) {   
    //test for write protect bits....
    //|7|6|5|4|3|2|1|0|
    //|-|-|-|-|-|-|-|-|
    //|WPEN|X|X|X|BP1|BP0|WEL|WIP|

    uint8_t reg, reg_old;
    reg_old = eeprom_25x_read_status_register(eeprom); // read the status register
    eeprom_25x_status_reg_print(reg_old); // print the status register

    if(eeprom->protect_test_flag){      
        printf("\r\nTesting support for Write Protect blocks (BP0, BP1) and Write Pin ENable (WPEN)\r\n");
        //now write 0x00 to the status register to disable write protect
        printf("\r\nDisabling write protect: ");
        if(eeprom_25x_write_status_register(eeprom, 0x00)) return true; // write 0x00 to the status register
        reg= eeprom_25x_read_status_register(eeprom); // read the status register again
        if(reg != 0x00) {
            printf("0x%02X, FAILED!\r\n", reg);
        }else {
            printf("0x%02X, OK\r\n", reg);
            
            //write WPEN, BP1, BP0, then read back to see the status
            printf("\r\nTesting for BP0, BP1, WPEN...");
            if(eeprom_25x_write_status_register(eeprom, 0b10001100)) return true;
            reg = eeprom_25x_read_status_register(eeprom);
            printf("wrote: 0x%02X, read: 0x%02X\r\n", 0b10001100, reg);
            printf("BP0: %s\r\n", (reg & 0b100) ? "Present" : "Not detected");
            printf("BP1: %s\r\n", (reg & 0b1000) ? "Present" : "Not detected");
            printf("WPEN: %s\r\n", (reg & 0x80) ? "Present" : "Not detected");
        }

        //restore old settings
        printf("\r\nRestoring original status register: 0x%02X...", reg_old);
        if(eeprom_25x_write_status_register(eeprom, reg_old)) return true;
        reg = eeprom_25x_read_status_register(eeprom);
        if(reg!=reg_old){
            printf("\r\nError: Failed to restore status register, wrote: 0x%02X, read: 0x%02X\r\n", reg_old, reg);
        }else{
            printf("Done :)\r\n");
        }
        eeprom_25x_status_reg_print(reg);
    }
    
    if(eeprom->protect_blocks_flag || eeprom->protect_wpen_flag) {
        printf("\r\n");
        reg = reg_old = eeprom_25x_read_status_register(eeprom); // read the status register
        uint8_t protection_bits_aligned;
        if(eeprom->protect_blocks_flag){
            protection_bits_aligned = (eeprom->protect_bits & 0b11)<<2;
            reg=reg&~0b1100; //clear existing
            reg=reg|protection_bits_aligned;
            printf("New Block protect bits (BP1, BP0): %d, %d\r\n", (reg&0b1000)?1:0, (reg&0b100)?1:0);
        }
        if(eeprom->protect_wpen_flag){
            reg=reg&~0x80; //clear existing
            reg=reg|((eeprom->protect_wpen_bit&0x01)<<7); //set the WPEN bit
            printf("New Write Pin ENable bit (WPEN): %d\r\n", (reg&0x80)?1:0);
        }

        printf("Updating status register (0x%02X): ", reg);
        if(eeprom_25x_write_status_register(eeprom, reg)){
            printf("Error: Failed to write status register\r\n");
            return true;
        }
        uint8_t reg_updated = eeprom_25x_read_status_register(eeprom);
        //test just the updated bits
        bool bp0=1, bp1=1, wpen=1;
        if(eeprom->protect_blocks_flag){
            bp0 = BIT_EQUAL(reg_updated, protection_bits_aligned, 2);
            bp1 = BIT_EQUAL(reg_updated, protection_bits_aligned, 3);
        }
        if(eeprom->protect_wpen_flag){
            wpen = BIT_EQUAL(reg_updated, ((eeprom->protect_wpen_bit&0x01)<<7), 7);
        }
        if(!bp0||!bp1||!wpen){
            printf("\r\nError updating (0x%02X):%s%s%s\r\n",reg_updated, !bp1?" BP1":"", !bp0?" BP0":"", !wpen?" WPEN":"");
            printf("Does this chip support all the protection bits?\r\n");
            printf("To test protection bit support try: eeprom protect -d %s -t\r\n\r\n", eeprom->device->name);
            //eeprom_25x_status_reg_print(reg);
            //return true;
        }else{
            printf("Done :)\r\n\r\n");
        }        
        eeprom_25x_status_reg_print(reg_updated);
    }
    return false;
}

static struct eeprom_hal_t eeprom_25x_hal = {
    .get_address = eeprom_get_address,
    .read = eeprom_25x_read,
    .write_page = eeprom_25x_write_page,
    .is_write_protected = eeprom_25x_is_write_protect,
    .probe_protect = eeprom_25x_probe_block_protect
};
//---------------------------------------------------------------------------

//-----------------------------------------------------------------------
// 93X SPI EEPROM hardware abstraction layer functions
//-----------------------------------------------------------------------
#define E93_READ_CMD 0b110
#define E93_EWEN_CMD 0b10011
#define E93_WRITE_CMD 0b101

//poll for busy status, return false if write is complete, true if timeout
static bool eeprom_93x_poll_busy(struct eeprom_info *eeprom){
    uint8_t reg;

    //wait 250ns, then raise CS
    busy_wait_us(1);
    hwspi_select(); // select the EEPROM chip
    busy_wait_us(1);
    //wait for DO to go low->high
    for(uint32_t i=0; i<0xfffff; i++) {
        if(bio_get(M_SPI_CDI)) {
            return false; // write is complete
        }
    }
    //printf("Error: EEPROM write timeout\r\n");
    return true;
}

//function to return the block select, address given a byte address
bool eeprom_93x_get_address(struct eeprom_info *eeprom, uint32_t address, uint8_t command, uint8_t *address_array) {
    // check if the address is valid
    if (address >= eeprom->device->size_bytes) {
        printf("Error: Address out of range\r\n");
        return true; // invalid address
    }

    uint8_t address_bits = eeprom->device->block_select_bits;
    if(eeprom->device->block_select_offset) address_bits += 1;
    uint16_t cmd = (command << address_bits) | (address); // construct the command with block select bits
    address_array[0] = (uint8_t)(cmd >> 8); // high byte
    address_array[1] = (uint8_t)(cmd & 0xFF); // low byte
    return false; 
}

static bool eeprom_93x_read(struct eeprom_info *eeprom, uint32_t address, uint32_t read_bytes, uint8_t *buf) {
    // get the address for the current byte
    uint8_t address_array[2];

    //for 16 bit addressing we shift the address by one bit
    address = address >> (eeprom->device->page_bytes-1);

    if(eeprom_93x_get_address(eeprom, address, E93_READ_CMD, address_array)){ 
        return true; // error getting address
    }
    //printf("0x%02X 0x%02X\r\n", address_array[0], address_array[1]);
    //return false;

    // read the data from the EEPROM
    hwspi_select(); // select the EEPROM chip
    hwspi_write_n(address_array, 2); // send the address bytes
    hwspi_read_n(buf, read_bytes); // read bytes from the EEPROM
    hwspi_deselect(); // deselect the EEPROM chip

    return false;
}

static bool eeprom_93x_write_page(struct eeprom_info *eeprom, uint32_t address, uint8_t *buf, uint32_t page_write_size){
    //get address
    uint8_t address_array[2];
    //TODO: acomdate 16 bit addressing
    address = address >> (eeprom->device->page_bytes-1);
    if(eeprom_93x_get_address(eeprom, address, E93_WRITE_CMD, address_array))return true; // get the address   
    //printf("0x%02X 0x%02X\r\n", address_array[0], address_array[1]);
    //return false;

    uint8_t ewen_cmd[2];
    bool org_16=false;
    uint8_t address_bits = eeprom->device->block_select_bits;
    if(eeprom->device->block_select_offset) address_bits += 1;
    uint16_t cmd = (E93_EWEN_CMD << (address_bits-2)); // construct the command with block select bits
    ewen_cmd[0] = (uint8_t)(cmd >> 8); // high byte
    ewen_cmd[1] = (uint8_t)(cmd & 0xFF); // low byte
    hwspi_write_read_cs(ewen_cmd, 2, NULL, 0); // enable write

    hwspi_select(); // select the EEPROM chip
    hwspi_write_n(address_array, 2); // send the address bytes
    hwspi_write_n(buf, page_write_size); // write the page data
    hwspi_deselect(); // deselect the EEPROM chip
    
    if(eeprom_93x_poll_busy(eeprom))return true; // poll for write complete, return true if timeout
    return false; // write is complete
}

static bool eeprom_93x_is_write_protect(struct eeprom_info *eeprom) {
    // check if the write protect bits are set
    //93X does not have write protect bits, so always return false
    return false;
}

static struct eeprom_hal_t eeprom_93x_hal = {
    .get_address = NULL, //eeprom_get_address,
    .read = eeprom_93x_read,
    .write_page = eeprom_93x_write_page,
    .is_write_protected = eeprom_93x_is_write_protect,
    .probe_protect = NULL
};
//---------------------------------------------------------------------------

static const struct eeprom_device_t eeprom_devices[] = {
    { "25X010",    128,     1, 0, 0,   8, 10000, &eeprom_25x_hal}, //8 and 16 byte page variants
    //{ "25X010",    128,     1, 0, 0,  16 },//use the lowest common page size
    { "25X020",    256,     1, 0, 0,   8, 10000, &eeprom_25x_hal },
    //{ "25X020",    256,     1, 0, 0,  16 },
    { "25X040",    512,     1, 1, 3,   8, 10000, &eeprom_25x_hal },
    //{ "25X040",    512,     1, 1, 3,  16 },
    { "25X080",   1024,     2, 0, 0,  16, 10000, &eeprom_25x_hal },
    //{ "25X080",   1024,     2, 0, 0,  32 },
    { "25X160",   2048,     2, 0, 0,  16, 10000, &eeprom_25x_hal },
    //{ "25X160",   2048,     2, 0, 0,  32 },
    { "25X320",    4096,     2, 0, 0,  32, 10000, &eeprom_25x_hal },
    { "25X640",    8192,     2, 0, 0,  32, 10000, &eeprom_25x_hal },
    { "25X128",  16384,     2, 0, 0,  64, 10000, &eeprom_25x_hal },
    { "25X256",  32768,     2, 0, 0,  64, 10000, &eeprom_25x_hal },
    { "25X512",   65536,     2, 0, 0, 128, 10000, &eeprom_25x_hal },
    { "25X1024",  131072,     3, 0, 0, 256, 10000, &eeprom_25x_hal },
    { "25XM01",  131072,     3, 0, 0, 256, 10000, &eeprom_25x_hal },
    { "25XM02",  262144,     3, 0, 0, 256, 5000, &eeprom_25x_hal }, //5MHz
    { "25XM04",  524288,     3, 0, 0, 256, 8000, &eeprom_25x_hal },
    {"93X46", 128, 2, 7, 0,   1, 2000, &eeprom_93x_hal }, 
    {"93X56", 256, 2, 8, 1,   1, 2000, &eeprom_93x_hal }, 
    {"93X66", 512, 2, 9, 0,   1, 2000, &eeprom_93x_hal }, 
    {"93X76", 1024, 2, 10, 1,  1, 2000, &eeprom_93x_hal },
    {"93X86", 2048, 2, 11, 0,  1, 2000, &eeprom_93x_hal },
    {"93X46-16", 128, 2, 6, 0,   2, 2000, &eeprom_93x_hal }, 
    {"93X56-16", 256, 2, 7, 1,   2, 2000, &eeprom_93x_hal }, 
    {"93X66-16", 512, 2, 8, 0,   2, 2000, &eeprom_93x_hal }, 
    {"93X76-16", 1024, 2, 9, 1,  2, 2000, &eeprom_93x_hal },
    {"93X86-16", 2048, 2, 10, 0,  2, 2000, &eeprom_93x_hal },   
};


static bool eeprom_get_args(struct eeprom_info *args) {
    command_var_t arg;
    char arg_str[9];
    
    // common function to parse the command line verb or action
    if(cmdln_args_get_action(eeprom_actions, count_of(eeprom_actions), &args->action)){
        ui_help_show(true, usage, count_of(usage), &options[0], count_of(options)); // show help if requested
        return true;
    }

    if(args->action == EEPROM_LIST) {
        eeprom_display_devices(eeprom_devices, count_of(eeprom_devices)); // display devices if list action
        printf("\r\nCompatible with most 25X/95X SPI EEPROMs: AT25, M95x, 25C/LC/AA/CS, etc.\r\n");
        printf("For STM M95x chips use the equivalent 25X chip name: M95128 = 25X128, etc.\r\n");
        printf("93X are supported in 8 and 16 bit address modes.\r\n93X chips have no write protect bits.\r\n");
        printf("3.3volts is suitable for most devices.\r\n");
        return true; // no error, just listing devices
    }
    
    if(!cmdln_args_find_flag_string('d', &arg, sizeof(arg_str), arg_str)){
        printf("Missing EEPROM device name: -d <device name>\r\n");
        eeprom_display_devices(eeprom_devices, count_of(eeprom_devices));
        return true;
    }

    // we have a device name, find it in the list
    uint8_t eeprom_type = 0xFF; // invalid by default
    strupr(arg_str);
    for(uint8_t i = 0; i < count_of(eeprom_devices); i++) {
        if(strcmp(arg_str, eeprom_devices[i].name) == 0) {
            eeprom_type = i; // found the device
            break;
        }
    }

    if(eeprom_type == 0xFF) {
        printf("Invalid EEPROM device name: %s\r\n", arg_str);
        eeprom_display_devices(eeprom_devices, count_of(eeprom_devices));
        return true; // error
    }
 
    args->device = &eeprom_devices[eeprom_type];

    // file to read/write/verify
    if ((args->action == EEPROM_READ || args->action == EEPROM_WRITE || args->action==EEPROM_VERIFY)) {
        if(file_get_args(args->file_name, sizeof(args->file_name))) return true;
    }

    // verify_flag
    args->verify_flag = cmdln_args_find_flag('v' | 0x20);

    if(args->device->hal->is_write_protected==NULL) {
        // check if the device is write protected
        return false;
    }

    // test block protect bits
    args->protect_test_flag = cmdln_args_find_flag('t' | 0x20);
    // program block protect bits
    
    args->protect_blocks_flag=cmdln_args_find_flag_uint32('p' | 0x20, &arg, &args->protect_bits);
    if(arg.has_arg && !arg.has_value){
        printf("Specify block protect bits (0-3, 0b00-0b11)\r\n");
        return true;
    }
    if(args->protect_blocks_flag){
        if (args->protect_bits>=4) {
            printf("Block write protect bits out of range: -p 0-3 or 0b00-0b11: %d\r\n", args->protect_bits);
            return true; // error
        }
    }
    uint32_t temp;
    args->protect_wpen_flag=cmdln_args_find_flag_uint32('w' | 0x20, &arg, &temp);
    if(arg.has_arg && !arg.has_value){
        printf("Specify Write Pin ENable: -w 0 or 1\r\n");
        return true;
    }
    if(args->protect_wpen_flag){
        args->protect_wpen_bit = (temp)?1:0;
    }

    // let hex editor parse its own arguments (handled in the dump function)
    //if(ui_hex_get_args(args->device->size_bytes, &args->start_address, &args->user_bytes)) return true;

    return false;
}

void spi_eeprom_handler(struct command_result* res) {
    if(res->help_flag) {
        //eeprom_display_devices(eeprom_devices, count_of(eeprom_devices)); // display the available EEPROM devices
        ui_help_show(true, usage, count_of(usage), &options[0], count_of(options)); // show help if requested
        return; // if help was shown, exit
    }
    struct eeprom_info eeprom;
    // bus specific arguments (action, protect blocks, etc)
    if(eeprom_get_args(&eeprom)) { 
        return;
    }

    //nice full description of chip and capabilities
    printf("%s: %d bytes,  %d block select bits, %d byte address, %d byte pages\r\n\r\n", eeprom.device->name, eeprom.device->size_bytes, eeprom.device->block_select_bits, eeprom.device->address_bytes, eeprom.device->page_bytes);

    char buf[EEPROM_ADDRESS_PAGE_SIZE]; // buffer for reading/writing
    uint8_t verify_buf[EEPROM_ADDRESS_PAGE_SIZE]; // buffer for reading data from EEPROM

    //we manually control any FALA capture
    fala_start_hook(); 

    if(eeprom.action == EEPROM_PROTECT){
        if(eeprom.device->hal->probe_protect == NULL) {
            printf("Error: %s does not support block protection features\r\n", eeprom.device->name);
            goto spi_eeprom_cleanup; // error if device does not support block protection
        }
        eeprom.device->hal->probe_protect(&eeprom);
        goto spi_eeprom_cleanup;
    }

    if(eeprom.action == EEPROM_DUMP) {
        //dump the EEPROM contents
        eeprom_dump(&eeprom, buf, sizeof(buf));
        goto spi_eeprom_cleanup; // no need to continue
    }
 
    if (eeprom.action == EEPROM_ERASE || eeprom.action == EEPROM_TEST) {
        if(eeprom.device->hal->is_write_protected(&eeprom)) {
            goto spi_eeprom_cleanup; // error if write protect is enabled
        }
        if(eeprom_action_erase(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), eeprom.verify_flag || eeprom.action == EEPROM_TEST)) {
            goto spi_eeprom_cleanup; // error during erase
        }
    }

    if (eeprom.action == EEPROM_TEST) {
        if(eeprom.device->hal->is_write_protected(&eeprom)) {
            goto spi_eeprom_cleanup; // error if write protect is enabled
        }
        if(eeprom_action_test(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf))) {
            goto spi_eeprom_cleanup; // error during test
        }
    }

    if (eeprom.action==EEPROM_WRITE) {
        if(eeprom.device->hal->is_write_protected(&eeprom)) {
            goto spi_eeprom_cleanup; // error if write protect is enabled
        }        
        if(eeprom_action_write(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), eeprom.verify_flag)) {
            goto spi_eeprom_cleanup; // error during write
        }
    }

    if (eeprom.action==EEPROM_READ) {
        if(eeprom_action_read(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), eeprom.verify_flag)) {
            goto spi_eeprom_cleanup; // error during read
        }
    }

    if (eeprom.action==EEPROM_VERIFY) {
        if(eeprom_action_verify(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf))){
            goto spi_eeprom_cleanup; // error during verify
        }
    }
    printf("Success :)\r\n");

spi_eeprom_cleanup:
    //we manually control any FALA capture
    fala_stop_hook();
    fala_notify_hook();

}
