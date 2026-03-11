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
#include "eeprom_i2c_gui.h"
#include "ui/ui_mem_gui.h"
#include "bytecode.h"
#include "mode/hwi2c.h"

#define I2C_EEPROM_DEFAULT_ADDRESS 0x50 // Default I2C address for EEPROMs

/* CLI UI ops: explicit printf/print_progress for command-line path */
static void cli_progress(uint32_t cur, uint32_t total, void *ctx) { (void)ctx; print_progress(cur, total); }
static void cli_message(const char *msg, void *ctx)  { (void)ctx; printf("%s", msg); }
static void cli_error(const char *msg, void *ctx)    { (void)ctx; printf("%s", msg); }
static void cli_warning(const char *msg, void *ctx)  { (void)ctx; printf("%s", msg); }
static const eeprom_ui_ops_t eeprom_cli_ops = {
    .progress = cli_progress,
    .message  = cli_message,
    .error    = cli_error,
    .warning  = cli_warning,
    .ctx      = NULL,
};

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
    { "device",   'd', BP_ARG_REQUIRED, "device", T_HELP_EEPROM_DEVICE_FLAG },
    { "file",     'f', BP_ARG_REQUIRED, "file",   T_HELP_EEPROM_FILE_FLAG },
    { "verify",   'v', BP_ARG_NONE,     NULL,        T_HELP_EEPROM_VERIFY_FLAG },
    { "start",    's', BP_ARG_REQUIRED, "addr",   UI_HEX_HELP_START },
    { "bytes",    'b', BP_ARG_REQUIRED, "count",  UI_HEX_HELP_BYTES },
    { "quiet",    'q', BP_ARG_NONE,     NULL,        UI_HEX_HELP_QUIET },
    { "nopager",  'c', BP_ARG_NONE,     NULL,        T_HELP_DISK_HEX_PAGER_OFF },
    { "address",  'a', BP_ARG_REQUIRED, "i2caddr",T_HELP_EEPROM_ADDRESS_FLAG },
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

/* probe devices: size_bytes = 0xFFFFFFFF disables bounds check in get_address,
 * allowing reads at any address without triggering the out-of-range error. */
static const struct eeprom_device_t detect_probe1 = {
    "probe", 0xFFFFFFFFu, 1, 0, 0, 8, 400, &i2c_eeprom_hal
};
static const struct eeprom_device_t detect_probe2 = {
    "probe", 0xFFFFFFFFu, 2, 0, 0, 8, 400, &i2c_eeprom_hal
};

/* send START + 8-bit address + STOP (no data). returns true if device ACKed. */
static bool i2c_probe_ack(uint8_t addr_7bit) {
    return !pio_i2c_write_array_timeout(addr_7bit << 1, NULL, 0, 0xfffffu);
}

/* read len bytes at address from i2c_addr using probe device. */
static bool detect_read(uint8_t i2c_addr, const struct eeprom_device_t *probe,
                         uint32_t address, uint8_t *buf, uint32_t len) {
    struct eeprom_info ei;
    memset(&ei, 0, sizeof(ei));
    ei.device_address = i2c_addr;
    ei.device = probe;
    ei.ui = NULL;
    return probe->hal->read(&ei, address, len, buf);
}

/* return the first device table index matching size_bytes.
 * if bso >= 0, also requires block_select_offset == bso. */
static int find_device(const struct eeprom_device_t *devices, uint8_t count,
                        uint32_t size_bytes, int bso) {
    for (uint8_t i = 0; i < count; i++) {
        if (devices[i].size_bytes != size_bytes) continue;
        if (bso >= 0 && devices[i].block_select_offset != (uint8_t)bso) continue;
        return (int)i;
    }
    return -2;
}

/* sequential wrap test: reads 8 bytes starting at address 252 from i2c_addr
 * using 1-byte addressing. for a chip with exactly 256B accessible at this
 * I2C address, the read wraps at byte 255 and bytes[4..7] equal the chip's
 * bytes at addresses 0..3. for a 2-byte address chip (64KB+), the 1-byte
 * probe mis-points the internal address counter, and no wrap is seen.
 * returns true if the block is 256B (wrap detected). */
static bool block_wraps_256(uint8_t i2c_addr) {
    uint8_t s[8], w[8];
    if (detect_read(i2c_addr, &detect_probe1, 0, s, 8)) return false;
    if (detect_read(i2c_addr, &detect_probe1, 252, w, 8)) return false;
    return (memcmp(&w[4], s, 4) == 0);
}

/* scan 0x08-0x77 and warn if devices outside the EEPROM address range respond.
 * block-select EEPROMs occupy base..base+8; anything else is unexpected and
 * could cause false ACK hits during the consecutive-address scan. */
static void i2c_bus_scan_warn(uint8_t eeprom_base, const ui_mem_gui_ops_t *ops) {
    if (!ops) return;

    char buf[96];
    int  pos = 0;
    bool any = false;

    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        /* skip the EEPROM's own address block */
        if (addr >= eeprom_base && addr <= eeprom_base + 8) continue;
        if (!i2c_probe_ack(addr)) continue;

        if (!any) {
            pos = snprintf(buf, sizeof(buf), "Other I2C devices at 0x%02X", addr);
            any = true;
        } else if (pos + 6 < (int)sizeof(buf) - 32) {
            /* leave room for the suffix */
            pos += snprintf(buf + pos, sizeof(buf) - pos, ", 0x%02X", addr);
        }
    }

    if (any) {
        snprintf(buf + pos, sizeof(buf) - pos, " - ACK scan may be unreliable");
        ops->warning(buf, ops->ctx);
    }
}

int eeprom_i2c_detect_size(uint8_t i2c_addr,
                            const struct eeprom_device_t *devices,
                            uint8_t count,
                            const ui_mem_gui_ops_t *ops) {
    uint8_t sig[8], cmp[8];

    /* read 8-byte signature at address 0 - bail early if I2C is dead */
    if (detect_read(i2c_addr, &detect_probe1, 0, sig, 8)) {
        PRINT_DEBUG("eeprom detect: read sig failed (I2C error)\n");
        return -3;
    }

    PRINT_DEBUG("eeprom detect: sig = %02X %02X %02X %02X %02X %02X %02X %02X\n",
                sig[0], sig[1], sig[2], sig[3], sig[4], sig[5], sig[6], sig[7]);

    /* bus pre-scan: warn if other devices sit in the ACK scan range */
    i2c_bus_scan_warn(i2c_addr, ops);

    /* check for uniform data - set flag but don't bail yet.
     * mirror and wrap probes trivially match on uniform data,
     * but the ACK scan is data-independent and can still identify
     * block-select chips (24X16, 24X1025) even when blank. */
    bool uniform = true;
    for (uint8_t i = 1; i < 8; i++) {
        if (sig[i] != sig[0]) { uniform = false; break; }
    }
    PRINT_DEBUG("eeprom detect: uniform = %s\n", uniform ? "yes" : "no");

    /* phase 1: 24X01 (128B) - 1-byte mirror at address 128.
     * skip if uniform: mirror compare would trivially match any chip. */
    if (!uniform) {
        if (!detect_read(i2c_addr, &detect_probe1, 128, cmp, 8) &&
            memcmp(sig, cmp, 8) == 0) {
            PRINT_DEBUG("eeprom detect: 24X01 mirror@128 = match\n");
            return find_device(devices, count, 128, -1);
        }
        PRINT_DEBUG("eeprom detect: 24X01 mirror@128 = no match\n");
    }

    /* phase 2: 2-byte mirror probes for 24X32 through 24X256.
     * skip if uniform: same reason as phase 1. */
    if (!uniform) {
        for (uint8_t i = 0; i < count; i++) {
            if (devices[i].address_bytes != 2) continue;
            if (devices[i].block_select_bits > 0) continue;
            if (devices[i].size_bytes >= 65536u) continue;
            if (detect_read(i2c_addr, &detect_probe2, devices[i].size_bytes, cmp, 8)) {
                PRINT_DEBUG("eeprom detect: %s mirror@%lu = read error\n",
                            devices[i].name, (unsigned long)devices[i].size_bytes);
                continue;
            }
            if (memcmp(sig, cmp, 8) == 0) {
                PRINT_DEBUG("eeprom detect: %s mirror@%lu = match\n",
                            devices[i].name, (unsigned long)devices[i].size_bytes);
                return (int)i;
            }
            PRINT_DEBUG("eeprom detect: %s mirror@%lu = no match\n",
                        devices[i].name, (unsigned long)devices[i].size_bytes);
        }
    }

    /* phase 3: I2C ACK scan for multi-address chips.
     * scan consecutive addresses from base+1 to base+7.
     * block-select chips occupy 2, 4, or 8 consecutive I2C addresses.
     * this is data-independent and works on blank chips. */
    uint8_t ack_count = 0;
    for (uint8_t n = 1; n <= 7; n++) {
        if (i2c_probe_ack(i2c_addr + n)) ack_count++;
        else break;
    }
    PRINT_DEBUG("eeprom detect: ack scan = %d consecutive\n", ack_count);

    /* phase 4: 24X16 - 8 consecutive addresses, data-independent */
    if (ack_count >= 7) {
        PRINT_DEBUG("eeprom detect: result = 24X16 (ack_count >= 7)\n");
        return find_device(devices, count, 2048, -1);
    }

    if (ack_count == 0) {
        /* only responds at base: candidates are 24X02, 24X512, 24X1025.
         * wrap test and base+8 probe are used to distinguish. */

        if (!uniform) {
            /* sequential wrap test: 24X02 wraps at 256, 24X512 does not. */
            uint8_t wrap[8];
            if (!detect_read(i2c_addr, &detect_probe1, 252, wrap, 8) &&
                memcmp(&wrap[4], sig, 4) == 0) {
                PRINT_DEBUG("eeprom detect: wrap@252 = match -> 24X02\n");
                return find_device(devices, count, 256, -1);
            }
            PRINT_DEBUG("eeprom detect: wrap@252 = no match\n");
        }

        /* 24X1025: block select offset=3, upper bank at base+8.
         * ACK probe is data-independent, works on blank chips. */
        if (i2c_probe_ack(i2c_addr + 8)) {
            PRINT_DEBUG("eeprom detect: ACK at base+8 = yes -> 24X1025\n");
            return find_device(devices, count, 131072, 3);
        }
        PRINT_DEBUG("eeprom detect: ACK at base+8 = no\n");

        if (uniform) {
            PRINT_DEBUG("eeprom detect: uniform + ack_count=0 + no base+8 -> cannot detect\n");
            return -1;
        }

        PRINT_DEBUG("eeprom detect: result = 24X512 (elimination)\n");
        return find_device(devices, count, 65536, -1);
    }

    /* ack_count 1 or 3: need block-1 wrap test to distinguish small
     * (24X04/24X08) from large (24X1026/24XM02) chips.
     * wrap test is unreliable on uniform data. */
    if (uniform) {
        PRINT_DEBUG("eeprom detect: uniform + ack_count=%d -> cannot detect (wrap unreliable)\n", ack_count);
        return -1;
    }

    bool b1_small = block_wraps_256(i2c_addr + 1);
    PRINT_DEBUG("eeprom detect: block-1 wrap = %s\n", b1_small ? "256B (small)" : "large");

    if (ack_count == 1) {
        if (b1_small) {
            PRINT_DEBUG("eeprom detect: result = 24X04 (ack=1, small block)\n");
            return find_device(devices, count, 512, -1);
        }
        PRINT_DEBUG("eeprom detect: result = 24X1026 (ack=1, large block)\n");
        return find_device(devices, count, 131072, 0);
    }

    if (ack_count == 3) {
        if (b1_small) {
            PRINT_DEBUG("eeprom detect: result = 24X08 (ack=3, small blocks)\n");
            return find_device(devices, count, 1024, -1);
        }
        PRINT_DEBUG("eeprom detect: result = 24XM02 (ack=3, large blocks)\n");
        return find_device(devices, count, 262144, -1);
    }

    PRINT_DEBUG("eeprom detect: ack_count=%d -> ambiguous\n", ack_count);
    return -2; /* unexpected ACK pattern */
}

static bool eeprom_get_args(struct eeprom_info *args) {
    // Parse action — if no action given, launch interactive GUI
    if (!bp_cmd_get_action(&eeprom_i2c_def, &args->action)) {
        eeprom_i2c_gui(eeprom_devices, count_of(eeprom_devices), args);
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
        if(!bp_file_get_name_flag(&eeprom_i2c_def, 'f', args->file_name, sizeof(args->file_name))) return true;
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
    eeprom.ui = &eeprom_cli_ops;

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
        if (!eeprom_confirm_action(&eeprom_i2c_def)) return;
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
        eeprom_dump(&eeprom_i2c_def, &eeprom, buf, sizeof(buf));
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
