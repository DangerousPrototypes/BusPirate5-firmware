/**
 * @file msc_disk.h
 * @brief USB Mass Storage Class disk interface.
 * @details Provides USB MSC drive management for file system access.
 */

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

/**
 * @brief Refresh USB MSC drive.
 */
void refresh_usbmsdrive(void);

/**
 * @brief Prepare USB MSC drive for read-only mode.
 */
void prepare_usbmsdrive_readonly(void);

/**
 * @brief Insert USB MSC drive.
 */
void insert_usbmsdrive(void);

/**
 * @brief Make USB MSC drive writable.
 */
void make_usbmsdrive_writable(void);

/**
 * @brief Eject USB MSC drive before bootloader jump.
 */
void eject_usbmsdrive(void);
