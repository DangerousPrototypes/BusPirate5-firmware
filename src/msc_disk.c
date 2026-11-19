/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "fatfs/ff.h"
#include "fatfs/diskio.h"
// #include "fatfs/tf_card.h"
#include "tusb.h"
#include "assert.h"

#if CFG_TUD_MSC

enum medium_state {
    // this is a transient duplicate of md_state_not_present
    // It prevents the program view to get ahead of the USB traffic
    // since the Mode sense info is sent to the USB later, in response to
    // another SCSCI command (REQUEST_SENSE)
    md_state_not_present_stage1,
    md_state_not_present,
    md_state_reset,
    md_state_medium_changed,
    // this is a transient duplicate of md_state_ready
    // It prevents the program view to get ahead of the USB traffic
    // since the Mode sense info is sent to the USB later, in response to
    // another SCSCI command (REQUEST_SENSE)
    md_state_ready_stage1,
    md_state_ready
};

static volatile uint32_t medium_state = md_state_ready;
static volatile uint32_t next_medium_state = md_state_ready;

static volatile bool eject_request = false;
static volatile bool insert_request = false;
static volatile bool cmd_ack = false;

static volatile bool writable = true;
static volatile bool host_ejected = false;

// Optional host traffic counters, shown by 'nand info'. Written on core 1, read on core 0.
// Costs ~56 bytes of RAM; enable with -DBP_MSC_DISK_STATS=1 when debugging USB MSC behaviour.
#ifndef BP_MSC_DISK_STATS
#define BP_MSC_DISK_STATS 0
#endif
#if BP_MSC_DISK_STATS
#define MSC_STAT(expr) do { expr; } while (0)
static volatile struct {
    uint32_t tur;
    uint32_t read10;
    uint32_t write10;
    uint32_t write10_errors;      // disk_write() failed
    uint32_t write_protect_hits;  // WRITE(10) refused because the drive was read-only
    uint32_t start_stop;
    uint32_t host_eject;
    uint32_t prevent_removal;
    uint32_t sync_cache;
    uint32_t unknown_scsi;
    uint8_t last_unknown_opcode;
    uint32_t inserts;
    uint32_t ejects;
    uint32_t reenumerations;
} msc_stats;
#else
#define MSC_STAT(expr) do { } while (0)
#endif

enum {
    MSC_DEMO_DISK_BLOCK_NUM = 16,  // 8KB is the smallest size that windows allow to mount
    MSC_DEMO_DISK_BLOCK_SIZE = 512 // 512
};

// Some MCU doesn't have enough 8KB SRAM to store the whole disk
// We will use Flash as read-only disk with board that has
// CFG_EXAMPLE_MSC_READONLY defined

#define README_CONTENTS                                                                                                \
    "No storage mounted.\r\n\
Kind regards,\r\n\
Ian and Chris\r\n\r\n\
https://buspirate.com/"

static_assert(sizeof(README_CONTENTS) <= MSC_DEMO_DISK_BLOCK_SIZE, "README_CONTENTS too large for single-sector file");

#define CFG_EXAMPLE_MSC_READONLY
#ifdef CFG_EXAMPLE_MSC_READONLY
const
#endif
    uint8_t msc_disk[MSC_DEMO_DISK_BLOCK_NUM][MSC_DEMO_DISK_BLOCK_SIZE] = {
        //------------- Block0: Boot Sector -------------//
        // byte_per_sector    = DISK_BLOCK_SIZE; fat12_sector_num_16  = DISK_BLOCK_NUM;
        // sector_per_cluster = 1; reserved_sectors = 1;
        // fat_num            = 1; fat12_root_entry_num = 16;
        // sector_per_fat     = 1; sector_per_track = 1; head_num = 1; hidden_sectors = 0;
        // drive_number       = 0x80; media_type = 0xf8; extended_boot_signature = 0x29;
        // filesystem_type    = "FAT12   "; volume_serial_number = 0x1234; volume_label = "TinyUSB MSC";
        // FAT magic code at offset 510-511
        // clang-format off
    {
        0xEB, 0x3C, 0x90, 0x4D, 0x53, 0x44, 0x4F, 0x53, 0x35, 0x2E, 0x30, 0x00, 0x02, 0x01, 0x01, 0x00,
        0x01, 0x10, 0x00, 0x10, 0x00, 0xF8, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x29, 0x34, 0x12, 0x00, 0x00, 'T' , 'i' , 'n' , 'y' , 'U' ,
        'S' , 'B' , ' ' , 'M' , 'S' , 'C' , 0x46, 0x41, 0x54, 0x31, 0x32, 0x20, 0x20, 0x20, 0x00, 0x00,

        // Zeroes until the 2 last bytes, which contain the FAT magic code
        [MSC_DEMO_DISK_BLOCK_SIZE-2] = 0x55, 0xAA
    },

    //------------- Block1: FAT12 Table -------------//
    {
        0xF8, 0xFF, 0xFF, 0xFF, 0x0F // // first 2 entries must be F8FF, third entry is cluster end of readme file
    },

    //------------- Block2: Root Directory -------------//
    {
        // first entry is volume label
        'B' , 'u' , 's' , ' ' , 'P' , 'i' , 'r' , 'a' , 't' , 'e' , ' ' , 0x08, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4F, 0x6D, 0x65, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // second entry is readme file
        'R' , 'E' , 'A' , 'D' , 'M' , 'E' , ' ' , ' ' , 'T' , 'X' , 'T' , 0x20, 0x00, 0xC6, 0x52, 0x6D,
        0x65, 0x43, 0x65, 0x43, 0x00, 0x00, 0x88, 0x6D, 0x65, 0x43, 0x02, 0x00,
        
        (sizeof(README_CONTENTS)-1 >> (8*0)), // readme's files size (4 Bytes, little-endian)
        (sizeof(README_CONTENTS)-1 >> (8*1)), // readme's files size (4 Bytes, little-endian)
        (sizeof(README_CONTENTS)-1 >> (8*2)), // readme's files size (4 Bytes, little-endian)
        (sizeof(README_CONTENTS)-1 >> (8*3)), // readme's files size (4 Bytes, little-endian)
    },
        // clang-format on

        //------------- Block3: Readme Content -------------//
        { README_CONTENTS },
    };

// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {
    (void)lun;

    
    #if (BP_VER == 5)
        const char vid[] = "BP5";
    #elif (BP_VER == XL5)
        const char vid[] = "BP5XL";
    #elif (BP_VER == 6)
        const char vid[] = "BP6";
    #elif (BP_VER == 7)
        const char vid[] = "BP7";
    #else
        #error "Unknown Bus Pirate version"
    #endif
    const char pid[] = "Storage";
    const char rev[] = "1.0";

    // NOTE: Relies on behavior of TinyUSB, which sets all strings to 0x20 (space)
    //       before calling this function.  See class/msc/msc_device.c.
    memcpy(vendor_id, vid, strlen(vid));
    memcpy(product_id, pid, strlen(pid));
    memcpy(product_rev, rev, strlen(rev));
}

static bool command_acknowledged() {
    return cmd_ack;
}
// This function will be called by core0
static bool is_ejected() {
    // stage1 counts too: the host has already been told the medium is gone, a further eject request
    // could never be acknowledged (the state machine only ejects from md_state_ready).
    return medium_state == md_state_not_present || medium_state == md_state_not_present_stage1;
}
// This function will be called by core0
static bool is_inserted() {
    return medium_state == md_state_ready;
}
// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g TF flash card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    (void)lun;
    bool return_value = true;
    MSC_STAT(msc_stats.tur++);
    medium_state = next_medium_state;
    if (host_ejected) {
        medium_state = md_state_not_present;
        next_medium_state = medium_state;
        host_ejected = false;
    } else if (insert_request && medium_state == md_state_not_present) {
        medium_state = md_state_medium_changed;
        insert_request = false; // insert is fire-and-forget, completed here
        cmd_ack = true;
    } else if (eject_request && medium_state == md_state_ready) {
        medium_state = md_state_not_present_stage1;
        cmd_ack = true;
    }
    switch (medium_state) {
        case md_state_not_present_stage1:
        case md_state_not_present:
            tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
            next_medium_state = md_state_not_present;
            return_value = false;
            break;
        // This state isn't implemented by my SD card reader
        // case md_state_reset:
        //     tud_msc_set_sense(lun, SCSI_SENSE_UNIT_ATTENTION, 0x29, 0x00);
        //     medium_state = md_state_medium_changed;
        //     return_value = false;
        //     break;
        case md_state_medium_changed:
            tud_msc_set_sense(lun, SCSI_SENSE_UNIT_ATTENTION, 0x28, 0x00);
            next_medium_state = md_state_ready_stage1;
            return_value = false;
            break;
        case md_state_ready_stage1:
        case md_state_ready:
            tud_msc_set_sense(lun, 0x00, 0x00, 0x00);
            next_medium_state = md_state_ready;
            return_value = true;
            break;
    }
    return return_value;
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) {
    DRESULT res;
    (void)lun;

    if (system_config.storage_available) {
        if ((res = disk_ioctl(0, GET_SECTOR_COUNT, block_count))) {
            // printf(" blockcount = unknown (storage inserted?) \r\n", *block_count);
        }
        *block_size = BP_FLASH_DISK_BLOCK_SIZE;
    } else {
        *block_count = MSC_DEMO_DISK_BLOCK_NUM;
        *block_size = MSC_DEMO_DISK_BLOCK_SIZE;
    }
}

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject) {
    (void)lun;
    (void)power_condition;

    MSC_STAT(msc_stats.start_stop++);
    if (load_eject) {
        if (start) {
            host_ejected = false;
            // load disk storage
        } else {
            MSC_STAT(msc_stats.host_eject++);
            host_ejected = true;
            if (writable) {
                disk_ioctl(0, CTRL_SYNC, 0);
            }
        }
    }
    return true;
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    (void)lun;
    MSC_STAT(msc_stats.read10++);
    // printf(" READ lba %d +%d siz %d\r\n", lba, offset, bufsize);

    if (system_config.storage_available) {
        // Runs on core1 (tud_task): no printf here, the TX FIFO is core0-only.
        // Return -1 on error: 0 means "busy" to TinyUSB and would retry forever, starving core1.
        if (disk_read(0, buffer, lba, (bufsize / BP_FLASH_DISK_BLOCK_SIZE))) // assume no offset
        {
            return -1;
        }
    } else {

        uint8_t const* addr = msc_disk[lba] + offset;
        memcpy(buffer, addr, bufsize);
    }

    return bufsize;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    (void)lun;
    MSC_STAT(msc_stats.write10++);

    // printf(" WRITE lba %d +%d siz %d\r\n", lba, offset, bufsize);

    if (system_config.storage_available) {
        // See tud_msc_read10_cb: must not printf on core1 and must not return 0 on error.
        if (disk_write(0, buffer, lba, (bufsize / BP_FLASH_DISK_BLOCK_SIZE))) // assume no offset
        {
            MSC_STAT(msc_stats.write10_errors++);
            return -1;
        }
    } else {

#ifndef CFG_EXAMPLE_MSC_READONLY
        uint8_t* addr = msc_disk[lba] + offset;
        memcpy(addr, buffer, bufsize);
#else
        (void)lba;
        (void)offset;
        (void)buffer;
#endif
    }

    return bufsize;
}

bool tud_msc_prevent_allow_medium_removal_cb(uint8_t lun, uint8_t prohibit_removal, uint8_t control) {
    (void)lun;
    if (prohibit_removal) {
        MSC_STAT(msc_stats.prevent_removal++);
    }
    if (prohibit_removal != 0) {
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x24, 0x0);
        return false;
    } else {
        tud_msc_set_sense(lun, 0, 0, 0);
        return true;
    }
}
// Callback invoked when received an SCSI command not in built-in list below
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
// - READ10 and WRITE10 has their own callbacks
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize) {
    // read10 & write10 has their own callback and MUST not be handled here

    void const* response = NULL;
    uint16_t resplen = 0;

    // most scsi handled is input
    bool in_xfer = true;

    switch (scsi_cmd[0]) {
        case 0x35: // SYNCHRONIZE CACHE(10)
        case 0x91: // SYNCHRONIZE CACHE(16)
            // Hosts flush before dismount and after metadata updates; the FTL is log-structured, so a
            // real sync matters. Rejecting this as "illegal request" makes Windows treat flushes as
            // failed, which it swallows silently (e.g. a delete that never lands).
            MSC_STAT(msc_stats.sync_cache++);
            if (system_config.storage_available && writable) {
                if (disk_ioctl(0, CTRL_SYNC, 0) != RES_OK) {
                    tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, 0x03, 0x00);
                    return -1;
                }
            }
            resplen = 0;
            break;

        default:
            MSC_STAT(msc_stats.unknown_scsi++);
            MSC_STAT(msc_stats.last_unknown_opcode = scsi_cmd[0]);
            // Set Sense = Invalid Command Operation
            tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

            // negative means error -> tinyusb could stall and/or response with failed status
            resplen = -1;
            break;
    }

    // return resplen must not larger than bufsize
    if (resplen > bufsize) {
        resplen = bufsize;
    }

    if (response && (resplen > 0)) {
        if (in_xfer) {
            memcpy(buffer, response, resplen);
        } else {
            // SCSI output
        }
    }

    return resplen;
}

#if BP_MSC_DISK_STATS
void msc_disk_print_diag(void) {
    printf("USB MSC host traffic:\r\n");
    printf("  Medium: %s, %s, host_ejected=%u\r\n",
           medium_state == md_state_ready ? "ready" : (medium_state == md_state_not_present ? "not present" : "changing"),
           writable ? "read/write" : "read-only", host_ejected);
    printf("  TUR polls: %lu, inserts: %lu, ejects: %lu, USB re-enumerations: %lu\r\n", (unsigned long)msc_stats.tur,
           (unsigned long)msc_stats.inserts, (unsigned long)msc_stats.ejects, (unsigned long)msc_stats.reenumerations);
    printf("  READ(10): %lu  WRITE(10): %lu (%lu failed, %lu refused read-only)\r\n",
           (unsigned long)msc_stats.read10, (unsigned long)msc_stats.write10,
           (unsigned long)msc_stats.write10_errors, (unsigned long)msc_stats.write_protect_hits);
    printf("  START/STOP: %lu (host eject %lu), PREVENT REMOVAL: %lu, SYNC CACHE: %lu\r\n",
           (unsigned long)msc_stats.start_stop, (unsigned long)msc_stats.host_eject,
           (unsigned long)msc_stats.prevent_removal, (unsigned long)msc_stats.sync_cache);
    printf("  Unknown SCSI commands: %lu (last opcode 0x%02X)\r\n", (unsigned long)msc_stats.unknown_scsi,
           msc_stats.last_unknown_opcode);
}
#else
void msc_disk_print_diag(void) {
    printf("USB MSC host traffic counters disabled (build with -DBP_MSC_DISK_STATS=1)\r\n");
}
#endif

bool tud_msc_is_writable_cb(uint8_t lun) {
    if (!writable) {
        MSC_STAT(msc_stats.write_protect_hits++);
    }
    return writable;
}

// Note that there are busy loop in this function
// A small sleep value is necessary to insure that
// the core can still process events. __wfe() will be called by sleep_ms
// Bounded wait for the host's TEST UNIT READY poll (acknowledged on core1). A host that stops
// polling (e.g. after storage errors) would otherwise hang core0 here and the terminal never appears.
// On timeout we carry on; writability is still enforced through tud_msc_is_writable_cb.
#define MSC_CMD_ACK_TIMEOUT_MS 3000
static void wait_command_acknowledged(void) {
    absolute_time_t deadline = make_timeout_time_ms(MSC_CMD_ACK_TIMEOUT_MS);
    while (!command_acknowledged()) {
        if (time_reached(deadline)) {
            PRINT_WARNING("USB MSC: host did not acknowledge eject/insert, continuing\n");
            return;
        }
        sleep_ms(1);
    }
}

bool insert_or_eject_usbmsdrive(bool insert) {
    cmd_ack = false;
    if (insert) {
        // Fire-and-forget: the TEST UNIT READY handler announces the medium on the host's next poll.
        // Nothing on core 0 depends on the host having seen it.
        MSC_STAT(msc_stats.inserts++);
        insert_request = true;
    } else {
        if (is_ejected()) {
            // Already out (possibly with an insert still pending): the pending insert, if any, keeps
            // the request flag and will re-announce the medium on the host's next poll.
            return true;
        }
        // unload disk storage
        MSC_STAT(msc_stats.ejects++);
        eject_request = true;
        wait_command_acknowledged();
        eject_request = false;
    }
    return true;
}

void eject_usbmsdrive(void) {
    insert_or_eject_usbmsdrive(false);
}
void insert_usbmsdrive(void) {
    insert_or_eject_usbmsdrive(true);
}

// eject and insert the usbms drive to force the host to sync its contents
void refresh_usbmsdrive(void) {
    // eject the usb drive
    eject_usbmsdrive();
    // insert the drive back
    insert_usbmsdrive();
}

// The drive is removed but not inserted back
// because some actions (like re-init of the file system)
// may be necessary
// To re-insert, call insert_usbmsdrive()
void prepare_usbmsdrive_readonly(void) {
    if (!writable) {
        return;
    }
    // eject the usb drive
    eject_usbmsdrive();
    // make sure the storage is synced
    disk_ioctl(0, CTRL_SYNC, 0);
    writable = false;
}
// USB re-enumeration (same effect as unplug/replug), executed on core 1 which owns the TinyUSB stack.
static volatile bool reenumerate_request = false;
static absolute_time_t reenumerate_reconnect_at;
#define MSC_REENUMERATE_DISCONNECT_MS 500

void msc_disk_core1_service(void) {
    if (reenumerate_request) {
        reenumerate_request = false;
        MSC_STAT(msc_stats.reenumerations++);
        tud_disconnect();
        reenumerate_reconnect_at = make_timeout_time_ms(MSC_REENUMERATE_DISCONNECT_MS);
    } else if (!is_nil_time(reenumerate_reconnect_at) && time_reached(reenumerate_reconnect_at)) {
        reenumerate_reconnect_at = nil_time;
        tud_connect();
    }
}

void make_usbmsdrive_writable(void) {
    if (writable) {
        return;
    }
    // A mounted volume that turns write-protected and back confuses the Windows shell: it caches the
    // volume's read-only attribute, after which delete/rename from Explorer silently do nothing until the
    // device is physically re-attached (creates and cmd.exe still work). Nothing short of a fresh
    // enumeration reliably clears that, and the terminal that forced read-only mode is gone, so unless a
    // binmode client is still attached on the second CDC port we re-enumerate instead of eject/insert.
    if (!tud_cdc_n_connected(1)) {
        writable = true;
        disk_ioctl(0, CTRL_SYNC, 0);
        medium_state = md_state_ready;
        next_medium_state = md_state_ready;
        insert_request = false;
        eject_request = false;
        reenumerate_request = true;
        return;
    }
    // Allow writes *before* ejecting: when the medium disappears the host dismounts the volume, and a
    // FAT dismount writes metadata (clean-shutdown bit, cached directory/FAT updates). If those writes
    // are refused with DATA PROTECT the host carries a half-dismounted volume state into the next mount
    // (same volume serial), after which operations on existing entries (delete/rename) silently fail
    // until the device is physically re-attached. Core 0 is no longer touching the file system here.
    writable = true;
    eject_usbmsdrive();
    disk_ioctl(0, CTRL_SYNC, 0);
    insert_usbmsdrive();
}

#endif
