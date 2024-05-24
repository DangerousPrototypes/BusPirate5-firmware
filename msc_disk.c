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
//#include "fatfs/tf_card.h"
#include "tusb.h"
#include "assert.h"

#if CFG_TUD_MSC

// whether host does safe-eject
bool ejected = false;
//latch the ejected status until read by the host
static bool latch_ejected = false;

static bool writable = true;

enum
{
  MSC_DEMO_DISK_BLOCK_NUM  = 16, // 8KB is the smallest size that windows allow to mount
  // should this be MSC_DEMO_DISK_SECTOR_SIZE?
  // ..._BLOCK_SIZE refers to an erase size (e.g., for 4k physical, 512b logical sectors)
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
      sizeof(README_CONTENTS)-1, 0x00, 0x00, 0x00 // readme's files size (4 Bytes)
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

  // NOTE: tinyusb (class/msc/msc_device.c) sets all values
  //       to spaces by default, so safe to only copy changes
  memcpy(vendor_id  , vid, strlen(vid));
  memcpy(product_id , pid, strlen(pid));
  memcpy(product_rev, rev, strlen(rev));
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g TF flash card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
  (void) lun;

  // RAM disk is ready until ejected
  if (ejected || latch_ejected) {
    tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
    latch_ejected = false;
    return false;
  }
  tud_msc_set_sense(lun, 0x00, 0x00, 0x00);

  return true;
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

  if ( load_eject )
  {
    if (start)
    {
    	ejected = false;
      // load disk storage
    }else
    {
      // unload disk storage
      latch_ejected = true;
      ejected = true;
    }
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
    case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
      // Host is about to read/write etc ... better not to disconnect disk
      resplen = 0;
    break;

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
  return writable;
}

//eject and insert the usbms drive to force the host to sync its contents
void refresh_usbmsdrive(void)
{
  // eject the usb drive
  tud_msc_start_stop_cb(0, 0, 0, 1);
  // insert the drive back
  tud_msc_start_stop_cb(0, 0, 1, 1);
}
void make_usbmsdrive_readonly(void)
{
  if (!writable)
    return;
  // eject the usb drive
  tud_msc_start_stop_cb(0, 0, 0, 1);
  // make sure the storage is synced
  disk_ioctl(0, CTRL_SYNC, 0);
  writable = false;
  // insert the drive back
  tud_msc_start_stop_cb(0, 0, 1, 1);
}
void make_usbmsdrive_writable(void)
{
  if(writable)
    return;
  // eject the usb drive
  tud_msc_start_stop_cb(0, 0, 0, 1);
  //make sure the storage is synced
  disk_ioctl(0, CTRL_SYNC, 0);
  writable = true;
  //insert the drive back
  tud_msc_start_stop_cb(0, 0, 1, 1);
}

#endif
