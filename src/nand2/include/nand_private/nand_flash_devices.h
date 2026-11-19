/*
 * SPDX-FileCopyrightText: 2015-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

// Manufacturer IDs (MI) for supported nand flash devices
#define SPI_NAND_FLASH_GIGADEVICE_MI  0xC8
#define SPI_NAND_FLASH_ALLIANCE_MI    0x52
#define SPI_NAND_FLASH_WINBOND_MI     0xEF
#define SPI_NAND_FLASH_MICRON_MI      0x2C
#define SPI_NAND_FLASH_ZETTA_MI       0xBA
#define SPI_NAND_FLASH_XTX_MI         0x0B

// Device ID (DI) for supported nand flash devices

#define GIGADEVICE_DI_51              0x51
#define GIGADEVICE_DI_41              0x41
#define GIGADEVICE_DI_31              0x31
#define GIGADEVICE_DI_21              0x21
#define GIGADEVICE_DI_52              0x52
#define GIGADEVICE_DI_42              0x42
#define GIGADEVICE_DI_32              0x32
#define GIGADEVICE_DI_22              0x22
#define GIGADEVICE_DI_55              0x55
#define GIGADEVICE_DI_45              0x45
#define GIGADEVICE_DI_35              0x35
#define GIGADEVICE_DI_25              0x25
#define GIGADEVICE_DI_95              0x95
#define GIGADEVICE_DI_85              0x85
#define GIGADEVICE_DI_92              0x92
#define GIGADEVICE_DI_82              0x82

#define ALLIANCE_DI_25                0x25   //AS5F31G04SND-08LIN
#define ALLIANCE_DI_2E                0x2E   //AS5F32G04SND-08LIN
#define ALLIANCE_DI_8E                0x8E   //AS5F12G04SND-10LIN
#define ALLIANCE_DI_2F                0x2F   //AS5F34G04SND-08LIN
#define ALLIANCE_DI_8F                0x8F   //AS5F14G04SND-10LIN
#define ALLIANCE_DI_2D                0x2D   //AS5F38G04SND-08LIN
#define ALLIANCE_DI_8D                0x8D   //AS5F18G04SND-10LIN

#define WINBOND_DI_AA20               0xAA20
#define WINBOND_DI_BA20               0xBA20
#define WINBOND_DI_AA21               0xAA21
#define WINBOND_DI_BA21               0xBA21
#define WINBOND_DI_BC21               0xBC21
#define WINBOND_DI_AA22               0xAA22
#define WINBOND_DI_AA23               0xAA23

#define MICRON_DI_34                  0x34
#define MICRON_DI_14                  0x14
#define MICRON_DI_15                  0x15
#define MICRON_DI_24                  0x24   // MT29F2G

#define ZETTA_DI_71                   0x71

#define XTX_DI_37                     0x37
