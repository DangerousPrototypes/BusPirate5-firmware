#pragma once

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

typedef enum _nand_volume_state_t {
    NAND_VOLUME_STATE_EJECTED = 0,
    NAND_VOLUME_STATE_SHARED_READONLY,
    NAND_VOLUME_STATE_FW_EXCLUSIVE,
    NAND_VOLUME_STATE_HOST_EXCLUSIVE,
} nand_volume_state_t;

// transitions to new media state ... handles setting MCN flag when needed
void set_nand_volume_state(nand_volume_state_t new_state, bool media_change_notification_required);
nand_volume_state_t get_nand_volume_state(void);

// indicates that the host requires a media change notification 
void set_usbms_media_change_notification_required(void);

