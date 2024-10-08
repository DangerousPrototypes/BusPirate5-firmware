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
#include <stdatomic.h>
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
#include "pirate/rgb.h"

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

/// @brief Checks if SCSI command block requires a media change notification,
///        and if so, sets the appropriate sense codes and updates the MCN state.
/// @param  
/// @return false if no MCN was required, true if MCN error codes were set.
static bool handled_required_media_change_notifications(uint8_t lun) {
    bool result;

    assert(get_core_num() == 1); // called only from core 1, from USB handler

    // this helper function doesn't report media is ejected....
    nand_volume_state_t state = get_nand_volume_state();
    media_change_notification_step_t step = atomic_load(&g_media_change_notification);

    // must be from a tud_msc_...() callback, so can call tud_msc_set_sense()
    if ((state != NAND_VOLUME_STATE_HOST_EXCLUSIVE) &&
        (state != NAND_VOLUME_STATE_SHARED_READONLY)) {
        // Do not allow access to the storage ... pretend media is ejected
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3A, 0x00); // MEDIUM_NOT_PRESENT
        result = true;
    } else if (step == MCN_STEP_NONE) {
        result = false;
    } else if (step == MCN_STEP_06_29_00) {
        // Media may have changed
        tud_msc_set_sense(lun, 0x06, 0x29, 0x00);
        // use atomic CAS in case value was reset again by core0
        atomic_compare_exchange_strong(&g_media_change_notification, &step, step-1);
        result = true;
    } else if (step == MCN_STEP_06_28_00) {
        // Bus reset
        tud_msc_set_sense(lun, 0x06, 0x28, 0x00);
        // use atomic CAS in case value was reset again by core0
        atomic_compare_exchange_strong(&g_media_change_notification, &step, step-1);
        result = true;
    } else {
        assert(false);
        tud_msc_set_sense(lun, 0x04, 0x3E, 0); // HARDWARE ERROR - Logical Unit Failed
        result = true;
    }
    return result;
}

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

    if (handled_required_media_change_notifications(lun)) {
        return_value = false;
    } else {
        tud_msc_set_sense(lun, 0, 0, 0);
        return_value = true;
    }
    return return_value;
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size)
{
    *block_count = 0;
    *block_size = BP_FLASH_DISK_BLOCK_SIZE;

    if (!handled_required_media_change_notifications(lun)) {
        DRESULT res = disk_ioctl(0, GET_SECTOR_COUNT, block_count);
        if (res != RES_OK) {
            //printf(" blockcount = unknown (storage inserted?) \r\n", *block_count);
        }
    }
}

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
    bool success;
    (void) power_condition;

    // BUGBUG -- Race condition vs. simultaneous firware write?
    //           Do we need a lock for any/all media access from either core?
    //           Core0 is typically firmware, Core1 handles USB requests.
    if (handled_required_media_change_notifications(lun)) {
        success = false;
    } else {
        // default here is success / no errors
        tud_msc_set_sense(lun, 0, 0, 0);
        success = true;

        if (!start && load_eject) {
            // TODO: Consider if NAND_VOLUME_STATE_HOST_EXCLUSIVE may send eject command to revert to shared mode?
            tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 24, 0); // INVALID_FIELD_IN_CDB
            success = false;
        }
        if (!start) { // spin down disk (stop) or eject media (depending on load_eject bit)
            // TODO: Is this need a lock, or is it safe to concurrently access with core0?
            disk_ioctl(0, CTRL_SYNC, 0); // flush any cached sector writes
        }
    }
    return true;
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{

  if (handled_required_media_change_notifications(lun)) {
    bufsize = 0;
  } else {
    bufsize -= bufsize % BP_FLASH_DISK_BLOCK_SIZE;
    if (bufsize != 0) {
      DRESULT res = disk_read(0, buffer, lba, (bufsize/BP_FLASH_DISK_BLOCK_SIZE));
      if (res != RES_OK) {
        //printf(" READ ERROR %d \r\n", res);
        bufsize = 0;
      } 
    }
  }
  return bufsize;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize)
{
  if (handled_required_media_change_notifications(lun)) {
    bufsize = 0;
  } else {
    bufsize -= bufsize % BP_FLASH_DISK_BLOCK_SIZE;
    if (bufsize != 0) {
      //printf(" WRITE lba %d +%d siz %d\r\n", lba, offset, bufsize);
      DRESULT res = disk_write(0, buffer, lba, (bufsize/BP_FLASH_DISK_BLOCK_SIZE));
		  if(res != RES_OK) {
			  printf(" WRITE ERROR %d \r\n", res);
			  bufsize=0;
		  }
    }
	}
  return bufsize;
}

bool tud_msc_prevent_allow_medium_removal_cb(uint8_t lun, uint8_t prohibit_removal, uint8_t control)
{
    bool was_successful = false;
    if (handled_required_media_change_notifications(lun)) {
        // do nothing more
    } else if (prohibit_removal != 0) {
        // do not support locking the media by host
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x24, 0x0);
    } else {
        tud_msc_set_sense(lun, 0, 0, 0);
        was_successful = true;
    }
    return was_successful;
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
    // at present, only Core0 should be changing the volume state.
    // Core1 can only read state (e.g., for handling SCSI commands in usb handler).
    // TODO: If this ever fires, may need to add mutex or similar...
    assert( get_core_num() == 0 );

    // This function encodes the following state transition table,
    // to ensure the complexity of ensuring MCN occurs AT LEAST for
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
    nand_volume_state_t old_state = atomic_load(&g_nand_volume_state);

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

    atomic_store(&g_nand_volume_state, new_state);
    if (media_change_notification_required) {
        set_usbms_media_change_notification_required();
    }

}
nand_volume_state_t get_nand_volume_state(void)
{
    nand_volume_state_t state = atomic_load(&g_nand_volume_state);
    return g_nand_volume_state;
}

// indicates that the host requires a media change notification 
void set_usbms_media_change_notification_required(void)
{
    atomic_store(&g_media_change_notification, FIRST_MCN_STEP);
}

#endif
