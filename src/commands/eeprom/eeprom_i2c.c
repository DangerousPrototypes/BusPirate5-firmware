#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include <string.h>
#include "pirate.h"
#include "ui/ui_term.h"
#include "command_struct.h"
#include "ui/ui_help.h"
#include "lib/bp_args/bp_cmd.h"
#include "binmode/fala.h"
#include "fatfs/ff.h"       // File system related
#include "ui/ui_hex.h" // Hex display related
#include "ui/ui_progress_indicator.h" // Progress indicator related
#include "pirate/file.h" // File handling related
#include "pirate/hwi2c_pio.h"
#include "eeprom_base.h"
#include "bytecode.h"
#include "mode/hwi2c.h"

#define I2C_EEPROM_DEFAULT_ADDRESS 0x50 // Default I2C address for EEPROMs

enum eeprom_actions_enum {
    EEPROM_DUMP=0,
    EEPROM_ERASE,
    EEPROM_WRITE,
    EEPROM_READ,
    EEPROM_VERIFY,
    EEPROM_TEST,
    EEPROM_LIST
};

static const bp_command_action_t eeprom_i2c_action_defs[] = {
    { EEPROM_DUMP,   "dump",   T_HELP_EEPROM_DUMP },
    { EEPROM_ERASE,  "erase",  T_HELP_EEPROM_ERASE },
    { EEPROM_WRITE,  "write",  T_HELP_EEPROM_WRITE },
    { EEPROM_READ,   "read",   T_HELP_EEPROM_READ },
    { EEPROM_VERIFY, "verify", T_HELP_EEPROM_VERIFY },
    { EEPROM_TEST,   "test",   T_HELP_EEPROM_TEST },
    { EEPROM_LIST,   "list",   T_HELP_EEPROM_LIST },
};

static const bp_command_opt_t eeprom_i2c_opts[] = {
    { "device",   'd', BP_ARG_REQUIRED, "<device>", T_HELP_EEPROM_DEVICE_FLAG },
    { "file",     'f', BP_ARG_REQUIRED, "<file>",   T_HELP_EEPROM_FILE_FLAG },
    { "verify",   'v', BP_ARG_NONE,     NULL,        T_HELP_EEPROM_VERIFY_FLAG },
    { "start",    's', BP_ARG_REQUIRED, "<addr>",   UI_HEX_HELP_START },
    { "bytes",    'b', BP_ARG_REQUIRED, "<count>",  UI_HEX_HELP_BYTES },
    { "quiet",    'q', BP_ARG_NONE,     NULL,        UI_HEX_HELP_QUIET },
    { "nopager",  'c', BP_ARG_NONE,     NULL,        T_HELP_DISK_HEX_PAGER_OFF },
    { "address",  'a', BP_ARG_REQUIRED, "<i2caddr>",T_HELP_EEPROM_ADDRESS_FLAG },
    { "yes",      'y', BP_ARG_NONE,     NULL,        T_HELP_FLASH_YES_OVERRIDE },
    { 0 }
};

static const char* const usage[] = {
    "eeprom [dump|erase|write|read|verify|test|list]\r\n\t[-d <device>] [-f <file>] [-v(verify)] [-s <start address>] [-b <bytes>] [-a <i2c address>] [-h(elp)]",
    "List available EEPROM devices:%s eeprom list",
    "Display contents (x to exit):%s eeprom dump -d 24x02",
    "Display 16 bytes starting at address 0x60:%s eeprom dump -d 24x02 -s 0x60 -b 16",
    "Erase, verify:%s eeprom erase -d 24x02 -v",
    "Write from file, verify:%s eeprom write -d 24x02 -f example.bin -v",
    "Read to file, verify:%s eeprom read -d 24x02 -f example.bin -v",
    "Verify against file:%s eeprom verify -d 24x02 -f example.bin",
    "Test chip (full erase/write/verify):%s eeprom test -d 24x02",
    "Use alternate I2C address (0x50 default):%s eeprom dump -d 24x02 -a 0x53",
};

const bp_command_def_t eeprom_i2c_def = {
    .name = "eeprom",
    .description = T_HELP_I2C_EEPROM,
    .actions = eeprom_i2c_action_defs,
    .action_count = count_of(eeprom_i2c_action_defs),
    .opts = eeprom_i2c_opts,
    .usage = usage,
    .usage_count = count_of(usage),
};
#if 0
uint32_t i2c_eeprom_determineSize(const bool debug)
{
  // try to read a byte to see if connected
  if (! isConnected()) return 0;

  uint8_t patAA = 0xAA;
  uint8_t pat55 = 0x55;

  for (uint32_t size = 128; size <= 65536; size *= 2)
  {
    bool folded = false;

    //  store old values
    bool addressSize = _isAddressSizeTwoWords;
    _isAddressSizeTwoWords = size > I2C_DEVICESIZE_24LC16;  // 2048
    uint8_t buf = readByte(size);

    //  test folding
    uint8_t count = 0;
    writeByte(size, pat55);
    if (readByte(0) == pat55) count++;
    writeByte(size, patAA);
    if (readByte(0) == patAA) count++;
    folded = (count == 2);
    if (debug)
    {
      SPRNH(size, HEX);
      SPRN('\t');
      SPRNLH(readByte(size), HEX);
    }

    //  restore old values
    writeByte(size, buf);
    _isAddressSizeTwoWords = addressSize;

    if (folded) return size;
  }
  return 0;
}
#endif
//----------------------------------------------------------------------------------
// I2C EEPROM hardware abstraction layer functions
//----------------------------------------------------------------------------------
//poll for busy status, return false if write is complete, true if timeout
static bool i2c_eeprom_poll_busy(struct eeprom_info *eeprom){
    //poll for write complete
    for(uint32_t i = 0; i<0xfffffu; i++){
        uint32_t timeout = 0xfffffu; // default timeout for I2C operations
        hwi2c_status_t i2c_result = pio_i2c_write_array_timeout(eeprom->device_address<<1, NULL, 0, 0xfffffu);
        if(!i2c_result){
            return false; // write is complete
        }
    }
    return true; // timeout, write is not complete
}

static bool i2c_eeprom_read(struct eeprom_info *eeprom, uint32_t address, uint32_t read_bytes, uint8_t *buf) {
    // get the address for the current byte
    uint8_t block_select_bits = 0;
    uint8_t address_array[3];
    if(eeprom->device->hal->get_address(eeprom, address, &block_select_bits, address_array)){ 
        return true; // error getting address
    }

    // read the data from the EEPROM
    if (i2c_transaction((eeprom->device_address|block_select_bits)<<1, address_array, eeprom->device->address_bytes, buf, read_bytes)) {
        return true; // error
    }
    return false;
}

static bool i2c_eeprom_write_page(struct eeprom_info *eeprom, uint32_t address, uint8_t *buf, uint32_t page_write_size){
    //get address
    uint8_t block_select_bits = 0;
    uint8_t address_array[3];
    if(eeprom->device->hal->get_address(eeprom, address, &block_select_bits, address_array))return true; // get the address   

    //need to do a partial page write
    //first read the existing page from the eeprom
    //then update with the new data
    //finally write the updated page back to the eeprom
    if(page_write_size < eeprom->device->page_bytes) {
        // if the page write size is less than the device page size, we need to read the existing page first
        uint8_t existing_page[EEPROM_ADDRESS_PAGE_SIZE];
        if(i2c_eeprom_read(eeprom, address, eeprom->device->page_bytes, existing_page)) {
            return true; // error reading existing page
        }
        // update the existing page with the new data
        memcpy(&buf[page_write_size], &existing_page[page_write_size], eeprom->device->page_bytes - page_write_size);
    }

    uint32_t timeout = 0xfffffu; // default timeout for I2C operations
    if(pio_i2c_start_timeout(timeout)) return true;
    hwi2c_status_t i2c_result = pio_i2c_write_timeout((eeprom->device_address|block_select_bits)<<1, timeout);
    if(i2c_result != HWI2C_OK) return true;

    for(uint8_t i = 0; i < eeprom->device->address_bytes; i++) {
        i2c_result = pio_i2c_write_timeout(address_array[i], timeout);
        if(i2c_result != HWI2C_OK) return true;
    }

    for(uint32_t i=0; i < eeprom->device->page_bytes; i++) {
        i2c_result = pio_i2c_write_timeout(buf[i], timeout);
        if(i2c_result != HWI2C_OK) return true;
    }

    if (pio_i2c_stop_timeout(timeout)) return true;
    if (pio_i2c_wait_idle_extern(timeout)) return true;

    //poll for write complete
    if(i2c_eeprom_poll_busy(eeprom))return true;
    return false; // write is complete
}

static struct eeprom_hal_t i2c_eeprom_hal = {
    .get_address = eeprom_get_address,
    .read = i2c_eeprom_read,
    .write_page = i2c_eeprom_write_page,
    .is_write_protected = NULL, // I2C EEPROMs do not have write protection
    .probe_protect = NULL // I2C EEPROMs do not have write protection
};

static const struct eeprom_device_t eeprom_devices[] = {
    { "24X01",   128,    1, 0, 0, 8, 400, &i2c_eeprom_hal},
    { "24X02",   256,    1, 0, 0, 8, 400, &i2c_eeprom_hal   },
    { "24X04",   512,    1, 1, 0, 16, 400, &i2c_eeprom_hal  },
    { "24X08",   1024,   1, 2, 0, 16, 400, &i2c_eeprom_hal  },
    { "24X16",   2048,   1, 3, 0, 16, 400, &i2c_eeprom_hal  },
    { "24X32",   4096,   2, 0, 0, 32, 400, &i2c_eeprom_hal  },
    { "24X64",   8192,   2, 0, 0, 32, 400, &i2c_eeprom_hal  },
    { "24X128",  16384,  2, 0, 0, 64, 400, &i2c_eeprom_hal  },
    { "24X256",  32768,  2, 0, 0, 64, 400, &i2c_eeprom_hal  },
    { "24X512",  65536,  2, 0, 0, 128, 400, &i2c_eeprom_hal },
    { "24X1025", 131072, 2, 1, 3, 128, 400, &i2c_eeprom_hal },
    { "24X1026", 131072, 2, 1, 0, 128, 400, &i2c_eeprom_hal },
    { "24XM01",  131072, 2, 1, 0, 256, 400, &i2c_eeprom_hal },    
    { "24XM02",  262144, 2, 2, 0, 256, 400, &i2c_eeprom_hal }
};

static bool eeprom_get_args(struct eeprom_info *args) {
    // Parse action
    if (!bp_cmd_get_action(&eeprom_i2c_def, &args->action)) {
        bp_cmd_help_show(&eeprom_i2c_def);
        return true;
    }

    // List action: just display devices and info
    if(args->action == EEPROM_LIST) {
        eeprom_display_devices(eeprom_devices, count_of(eeprom_devices));
        printf("Compatible with most common 24X I2C EEPROMs: AT24C, 24C/LC/AA/FC, etc.\r\n");
        printf("3.3volts is suitable for most devices.\r\n");
        return true;
    }

    // Device name (required)
    char dev_name[9] = {0};
    if (!bp_cmd_get_string(&eeprom_i2c_def, 'd', dev_name, sizeof(dev_name))) {
        printf("Missing EEPROM device name: -d <device name>\r\n");
        eeprom_display_devices(eeprom_devices, count_of(eeprom_devices));
        return true;
    }
    strupr(dev_name);
    uint8_t eeprom_type = 0xFF;
    for(uint8_t i = 0; i < count_of(eeprom_devices); i++) {
        if(strcmp(dev_name, eeprom_devices[i].name) == 0) {
            eeprom_type = i;
            break;
        }
    }
    if(eeprom_type == 0xFF) {
        printf("Invalid EEPROM device name: %s\r\n", dev_name);
        eeprom_display_devices(eeprom_devices, count_of(eeprom_devices));
        return true;
    }
    args->device = &eeprom_devices[eeprom_type];

    // I2C address (optional, default)
    uint32_t i2c_address = I2C_EEPROM_DEFAULT_ADDRESS;
    if (bp_cmd_get_uint32(&eeprom_i2c_def, 'a', &i2c_address)) {
        if (i2c_address > 0x7F) {
            printf("Invalid I2C address: %d\r\n", i2c_address);
            return true;
        }
    }
    args->device_address = i2c_address;

    // verify_flag
    args->verify_flag = bp_cmd_find_flag(&eeprom_i2c_def, 'v');

    // file to read/write/verify
    if ((args->action == EEPROM_READ || args->action == EEPROM_WRITE || args->action == EEPROM_VERIFY)) {
        if(file_get_args(args->file_name, sizeof(args->file_name))) return true;
    }

    // let hex editor parse its own arguments (future)
    //if(ui_hex_get_args(args->device->size_bytes, &args->start_address, &args->user_bytes)) return true;

    return false;
}


void i2c_eeprom_handler(struct command_result* res) {
    if (bp_cmd_help_check(&eeprom_i2c_def, res->help_flag)) {
        return;
    }

    struct eeprom_info eeprom;
    if (eeprom_get_args(&eeprom)) {
        return;
    }

    if (i2c_mode_config.clock_stretch) {
        printf("Error: I2C Clock stretching is enabled.\r\nEnter I2C mode again and select clock stretching DISABLED.\r\n");
        return;
    }

    // Print chip info
    printf("%s: %d bytes,  %d block select bits, %d byte address, %d byte pages\r\n\r\n",
        eeprom.device->name, eeprom.device->size_bytes, eeprom.device->block_select_bits, eeprom.device->address_bytes, eeprom.device->page_bytes);

    char buf[EEPROM_ADDRESS_PAGE_SIZE];
    uint8_t verify_buf[EEPROM_ADDRESS_PAGE_SIZE];

    // Confirm destructive actions
    if ((eeprom.action == EEPROM_ERASE || eeprom.action == EEPROM_WRITE || eeprom.action == EEPROM_TEST)) {
        if (!eeprom_confirm_action()) return;
    }

    //we manually control any FALA capture
    fala_start_hook(); 
    
    #if 0
    if(eeprom.action == EEPROM_PROTECT){
        eeprom_probe_block_protect(&eeprom);
        goto i2c_eeprom_cleanup;
    }
    #endif

    if(eeprom.action == EEPROM_DUMP) {
        //dump the EEPROM contents
        eeprom_dump(&eeprom, buf, sizeof(buf));
        goto i2c_eeprom_cleanup; // no need to continue
    }
 
    if (eeprom.action == EEPROM_ERASE || eeprom.action == EEPROM_TEST) {
        if(eeprom_action_erase(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), eeprom.verify_flag || eeprom.action == EEPROM_TEST)) {
            goto i2c_eeprom_cleanup; // error during erase
        }
    }

    if (eeprom.action == EEPROM_TEST) {
        if(eeprom_action_test(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf))) {
            goto i2c_eeprom_cleanup; // error during test
        }
    }

    if (eeprom.action==EEPROM_WRITE) {
        if(eeprom_action_write(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), eeprom.verify_flag)) {
            goto i2c_eeprom_cleanup; // error during write
        }
    }

    if (eeprom.action==EEPROM_READ) {
        if(eeprom_action_read(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), eeprom.verify_flag)) {
            goto i2c_eeprom_cleanup; // error during read
        }
    }

    if (eeprom.action==EEPROM_VERIFY) {
        if(eeprom_action_verify(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf))){
            goto i2c_eeprom_cleanup; // error during verify
        }
    }
    printf("Success :)\r\n");

i2c_eeprom_cleanup:
    //we manually control any FALA capture
    fala_stop_hook();
    fala_notify_hook();
}
