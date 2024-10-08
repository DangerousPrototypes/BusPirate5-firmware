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
// #include <stdatomic.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "fatfs/ff.h"
#include "fatfs/diskio.h"
#include "msc_disk.h"
//#include "fatfs/tf_card.h"
#include "tusb.h"
#include "assert.h"

#if CFG_TUD_MSC

typedef enum _media_change_notification_step_t {
  // list these in reverse order, so that can simply "count down" to zero
  MCN_STEP_NONE = 0,
  MCN_STEP_06_28_00, // Media may have changed
  MCN_STEP_06_29_00, // Bus reset
} media_change_notification_step_t;
static const media_change_notification_step_t FIRST_MCN_STEP = MCN_STEP_06_29_00;

nand_volume_state_t g_nand_volume_state = NAND_VOLUME_STATE_EJECTED;
media_change_notification_step_t g_media_change_notification = MCN_STEP_NONE;

/*
static volatile bool cmd_ack = false;
*/

enum
{
  MSC_DEMO_DISK_BLOCK_NUM  = 16, // 8KB is the smallest size that windows allow to mount
  MSC_DEMO_DISK_BLOCK_SIZE = 512 //512
};

// Some MCU doesn't have enough 8KB SRAM to store the whole disk
// We will use Flash as read-only disk with board that has
// CFG_EXAMPLE_MSC_READONLY defined

#define README_CONTENTS \
"No storage mounted.\r\n\
Kind regards,\r\n\
Ian and Chris\r\n\r\n\
https://buspirate.com/"

static_assert(sizeof(README_CONTENTS) <= MSC_DEMO_DISK_BLOCK_SIZE, "README_CONTENTS too large for single-sector file");

#define CFG_EXAMPLE_MSC_READONLY
#ifdef CFG_EXAMPLE_MSC_READONLY
const
#endif
uint8_t msc_disk[MSC_DEMO_DISK_BLOCK_NUM][MSC_DEMO_DISK_BLOCK_SIZE] =
{
  //------------- Block0: Boot Sector -------------//
  // byte_per_sector    = DISK_BLOCK_SIZE; fat12_sector_num_16  = DISK_BLOCK_NUM;
  // sector_per_cluster = 1; reserved_sectors = 1;
  // fat_num            = 1; fat12_root_entry_num = 16;
  // sector_per_fat     = 1; sector_per_track = 1; head_num = 1; hidden_sectors = 0;
  // drive_number       = 0x80; media_type = 0xf8; extended_boot_signature = 0x29;
  // filesystem_type    = "FAT12   "; volume_serial_number = 0x1234; volume_label = "TinyUSB MSC";
  // FAT magic code at offset 510-511
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
      0xF8, 0xFF,      // first 2 entries must be F8 and FF (legacy reasons)
      0xFF,            // third entry: allocated to the readme file, and indicates end-of-cluster-chain
      0xFF,            // fourth entry: end-of-cluster-chain
      0x0F,            // ???
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

  //------------- Block3: Readme Content -------------//
  {
    README_CONTENTS
  },
};

// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
  (void) lun;

  const char vid[] = "BP5";
  const char pid[] = "Storage";
  const char rev[] = "1.0";

  // NOTE: Relies on behavior of TinyUSB, which sets all strings to 0x20 (space)
  //       before calling this function.  See class/msc/msc_device.c.
  memcpy(vendor_id  , vid, strlen(vid));
  memcpy(product_id , pid, strlen(pid));
  memcpy(product_rev, rev, strlen(rev));
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g TF flash card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
    (void)lun;
    bool return_value = false;

    nand_volume_state_t state = get_nand_volume_state();
    if ((state == NAND_VOLUME_STATE_EJECTED) ||
        (state == NAND_VOLUME_STATE_FW_EXCLUSIVE)) {
        // Do not allow access to the storage
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
        return_value = false;
    } else if (state == NAND_VOLUME_STATE_HOST_EXCLUSIVE) {
        // Do not interfere with host-exclusive access
        tud_msc_set_sense(lun, 0, 0, 0);
        return_value = true;
    } else if (state == NAND_VOLUME_STATE_SHARED_READONLY) {
        //         
        if (g_media_change_notification == MCN_STEP_NONE) {
            tud_msc_set_sense(lun, 0, 0, 0);
            return_value = true;
        } else if (g_media_change_notification == MCN_STEP_06_29_00) {
            --g_media_change_notification;
            tud_msc_set_sense(lun, 6, 29, 00);
            return_value = false;
        } else if (g_media_change_notification == MCN_STEP_06_28_00) {
            --g_media_change_notification;
            tud_msc_set_sense(lun, 6, 28, 00);
            return_value = false;
        }

    } else {
        // invalid state ... assert and set to FW_EXCLUSIVE mode?
        assert(false);
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
        set_nand_volume_state(NAND_VOLUME_STATE_FW_EXCLUSIVE, false);
        return_value = false;
    }
    return return_value;
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size)
{
  DRESULT res;
  (void) lun;

	if(system_config.storage_available)
	{
		if((res=disk_ioctl(0, GET_SECTOR_COUNT, block_count))){
			//printf(" blockcount = unknown (storage inserted?) \r\n", *block_count);
	  }
    *block_size  = BP_FLASH_DISK_BLOCK_SIZE;
  }
  else
  {
		*block_count = MSC_DEMO_DISK_BLOCK_NUM;
	  *block_size  = MSC_DEMO_DISK_BLOCK_SIZE;
  }

}

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
    (void) lun;
    (void) power_condition;

    tud_msc_set_sense(lun, 0, 0, 0);

    nand_volume_state_t state = get_nand_volume_state();
    if (!start && load_eject) {
        // TODO: Consider if this should be permitted in NAND_VOLUME_STATE_HOST_EXCLUSIVE, as way to exit that mode?
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 24, 0); // INVALID_FIELD_IN_CDB
    }
    if (!start) { // spin down disk (stop) or eject media (depending on load_eject bit)
        disk_ioctl(0, CTRL_SYNC, 0); // flush any cached sector writes
    }
    return true;
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
  DRESULT res;
  (void) lun;
	//printf(" READ lba %d +%d siz %d\r\n", lba, offset, bufsize);


	if(system_config.storage_available)
	{
		if(res=disk_read(0, buffer, lba, (bufsize/BP_FLASH_DISK_BLOCK_SIZE)))		// assume no offset
		{
			printf(" READ ERROR %d \r\n", res);
			bufsize=0;
		}
	}
	else
	{

		uint8_t const* addr = msc_disk[lba] + offset;
		memcpy(buffer, addr, bufsize);
	}
	
  return bufsize;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize)
{
  DRESULT res;
  (void) lun;


  //printf(" WRITE lba %d +%d siz %d\r\n", lba, offset, bufsize);

	if(system_config.storage_available)
	{	
		if(res=disk_write(0, buffer, lba, (bufsize/BP_FLASH_DISK_BLOCK_SIZE)))		// assume no offset
		{
			printf(" WRITE ERROR %d \r\n", res);
			bufsize=0;
		}
	}
	else
	{

#ifndef CFG_EXAMPLE_MSC_READONLY
		uint8_t* addr = msc_disk[lba] + offset;
  		memcpy(addr, buffer, bufsize);
#else
  		(void) lba; (void) offset; (void) buffer;
#endif
	
	}

  return bufsize;
}

bool tud_msc_prevent_allow_medium_removal_cb(uint8_t lun, uint8_t prohibit_removal, uint8_t control)
{
    (void)lun;
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
int32_t tud_msc_scsi_cb (uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize)
{
  // read10 & write10 has their own callback and MUST not be handled here

  void const* response = NULL;
  uint16_t resplen = 0;

  // most scsi handled is input
  bool in_xfer = true;

  switch (scsi_cmd[0])
  {

    default:
    // Set Sense = Invalid Command Operation
    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

    // negative means error -> tinyusb could stall and/or response with failed status
    resplen = -1;
    break;
  }

  // return resplen must not larger than bufsize
  if ( resplen > bufsize ) resplen = bufsize;

  if ( response && (resplen > 0) )
  {
    if(in_xfer)
    {
      memcpy(buffer, response, resplen);
    }else
    {
      // SCSI output
    }
  }

  return resplen;
}

bool tud_msc_is_writable_cb(uint8_t lun)
{
    // the ONLY state where the host is allowed to write
    // to the volume is when the host is the exclusive owner
    nand_volume_state_t state = get_nand_volume_state();
    return state == NAND_VOLUME_STATE_HOST_EXCLUSIVE;
}

void set_nand_volume_state(nand_volume_state_t new_state, bool media_change_notification_required)
{
    // This function encodes the following state transition table,
    // to ensure the complexity of ensuring MCN occurs at least in
    // the following cases:
    //
    //
    //  | from \ to    | `ejected` | `shared R/O` | `FW Exc` | `Host Exc` |
    //  |--------------|----------:|-------------:|---------:|-----------:|
    //  | `ejected`    |    `Y`    |      `M`     |    `Y`   |     `M`    |
    //  | `shared R/O` |    `Y`    |      `Y`     |    `Y`   |     `M`    |
    //  | `FW Exc`     |    `Y`    |      `M`     |    `Y`   |     `M`    |
    //  | `Host Exc`   |    `Y`    |      `M`     |    `Y`   |     `Y`    |
    //
    // Where `Y` indicates the state change is allowed w/o MCN,
    // and `M` indicates the state change is allowed, but must set MCN.
    // The `X` is a transition which will be evaluated at a later date
    // and is currently disallowed.

    static uint8_t set_nand_volume_state_called_from = 0u;
    // TODO: If this can this be called from both cores, then needs a mutex
    //       track this.
    set_nand_volume_state_called_from |= 1u << get_core_num();

    nand_volume_state_t old_state = g_nand_volume_state;

    // check if the state transition requires a media change notification
    if (media_change_notification_required) {
        // Caller has explicitly requested a media change notification,
        // even if not required by the state transition.
    } else if (old_state == new_state) {
        // no actual state change, so no media change notification required
    } else if (new_state == NAND_VOLUME_STATE_SHARED_READONLY) {
        // change from anything else to shared_readonly requires
        // notifying the host of a media change
        media_change_notification_required = true;
    } else if (new_state == NAND_VOLUME_STATE_HOST_EXCLUSIVE) {
        // change from anything else to host_exclusive requires
        // notifying the host of a media change
        media_change_notification_required = true;
    }

    g_nand_volume_state = new_state;
    if (media_change_notification_required) {
        set_usbms_media_change_notification_required();
    }

}
nand_volume_state_t get_nand_volume_state(void)
{
    return g_nand_volume_state;
}

// indicates that the host requires a media change notification 
void set_usbms_media_change_notification_required(void)
{
    g_media_change_notification = FIRST_MCN_STEP;
}

#endif
