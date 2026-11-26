/*
 * SPDX-FileCopyrightText: 2015-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileContributor: Added Dosilicon DS35Q1GA support
 * SPDX-FileContributor: Added GigaDevice GD5F1GM7 support
 */

#pragma once

#include <stdint.h>

// Manufacturer IDs (MI) for supported nand flash devices
#define SPI_NAND_FLASH_GIGADEVICE_MI  0xC8
#define SPI_NAND_FLASH_ALLIANCE_MI    0x52
#define SPI_NAND_FLASH_WINBOND_MI     0xEF
#define SPI_NAND_FLASH_MICRON_MI      0x2C
#define SPI_NAND_FLASH_ZETTA_MI       0xBA
#define SPI_NAND_FLASH_XTX_MI         0x0B
#define SPI_NAND_FLASH_DOSILICON_MI   0xE5

typedef struct {
    uint8_t manufacturer_id;
    uint16_t device_id;
    const char *model_name;
} spi_nand_flash_device_info_t;

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
#define GIGADEVICE_DI_91              0x91
#define GIGADEVICE_DI_81              0x81
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

#define DOSILICON_DI_71               0x71

// Model names for the IDs accepted by the vendor-specific initialization routines.
#define SPI_NAND_FLASH_DEVICE_TABLE                                                                                \
{                                                                                                                  \
    {SPI_NAND_FLASH_GIGADEVICE_MI, GIGADEVICE_DI_51, "GD5F1GQ5UExxG"},                                           \
    {SPI_NAND_FLASH_GIGADEVICE_MI, GIGADEVICE_DI_41, "GD5F1GQ5RExxG"},                                           \
    {SPI_NAND_FLASH_GIGADEVICE_MI, GIGADEVICE_DI_31, "GD5F1GQ5UExxH"},                                           \
    {SPI_NAND_FLASH_GIGADEVICE_MI, GIGADEVICE_DI_21, "GD5F1GQ5RExxH"},                                           \
    {SPI_NAND_FLASH_GIGADEVICE_MI, GIGADEVICE_DI_52, "GD5F2GQ5UExxG"},                                           \
    {SPI_NAND_FLASH_GIGADEVICE_MI, GIGADEVICE_DI_42, "GD5F2GQ5RExxG"},                                           \
    {SPI_NAND_FLASH_GIGADEVICE_MI, GIGADEVICE_DI_32, "GD5F2GQ5UExxH"},                                           \
    {SPI_NAND_FLASH_GIGADEVICE_MI, GIGADEVICE_DI_22, "GD5F2GQ5xExxH"},                                            \
    {SPI_NAND_FLASH_GIGADEVICE_MI, GIGADEVICE_DI_55, "GD5F4GQ6UExxG"},                                           \
    {SPI_NAND_FLASH_GIGADEVICE_MI, GIGADEVICE_DI_45, "GD5F4GQ6RExxG"},                                           \
    {SPI_NAND_FLASH_GIGADEVICE_MI, GIGADEVICE_DI_35, "GigaDevice 4 Gbit (ID 0x35)"},                              \
    {SPI_NAND_FLASH_GIGADEVICE_MI, GIGADEVICE_DI_25, "GigaDevice 4 Gbit (ID 0x25)"},                              \
    {SPI_NAND_FLASH_GIGADEVICE_MI, GIGADEVICE_DI_91, "GD5F1GM7UExxG"},                                           \
    {SPI_NAND_FLASH_GIGADEVICE_MI, GIGADEVICE_DI_81, "GD5F1GM7RExxG"},                                           \
    {SPI_NAND_FLASH_GIGADEVICE_MI, GIGADEVICE_DI_95, "GD5F4GM8UExxG"},                                           \
    {SPI_NAND_FLASH_GIGADEVICE_MI, GIGADEVICE_DI_85, "GD5F4GM8RExxG"},                                           \
    {SPI_NAND_FLASH_GIGADEVICE_MI, GIGADEVICE_DI_92, "GD5F2GM7UExxG"},                                           \
    {SPI_NAND_FLASH_GIGADEVICE_MI, GIGADEVICE_DI_82, "GD5F2GM7RExxG"},                                           \
    {SPI_NAND_FLASH_ALLIANCE_MI, ALLIANCE_DI_25, "AS5F31G04SND-08LIN"},                                           \
    {SPI_NAND_FLASH_ALLIANCE_MI, ALLIANCE_DI_2E, "AS5F32G04SND-08LIN"},                                           \
    {SPI_NAND_FLASH_ALLIANCE_MI, ALLIANCE_DI_8E, "AS5F12G04SND-10LIN"},                                           \
    {SPI_NAND_FLASH_ALLIANCE_MI, ALLIANCE_DI_2F, "AS5F34G04SND-08LIN"},                                           \
    {SPI_NAND_FLASH_ALLIANCE_MI, ALLIANCE_DI_8F, "AS5F14G04SND-10LIN"},                                           \
    {SPI_NAND_FLASH_ALLIANCE_MI, ALLIANCE_DI_2D, "AS5F38G04SND-08LIN"},                                           \
    {SPI_NAND_FLASH_ALLIANCE_MI, ALLIANCE_DI_8D, "AS5F18G04SND-10LIN"},                                           \
    {SPI_NAND_FLASH_WINBOND_MI, WINBOND_DI_AA20, "W25N512GVxIG/IT"},                                              \
    {SPI_NAND_FLASH_WINBOND_MI, WINBOND_DI_BA20, "W25N512GWxxR/T"},                                               \
    {SPI_NAND_FLASH_WINBOND_MI, WINBOND_DI_AA21, "W25N01GVxxxG/T/R"},                                             \
    {SPI_NAND_FLASH_WINBOND_MI, WINBOND_DI_BA21, "W25N01GWxxxG/T"},                                               \
    {SPI_NAND_FLASH_WINBOND_MI, WINBOND_DI_BC21, "W25N01JWxxxG/T"},                                               \
    {SPI_NAND_FLASH_WINBOND_MI, WINBOND_DI_AA22, "W25N02KVxxIR/U"},                                               \
    {SPI_NAND_FLASH_WINBOND_MI, WINBOND_DI_AA23, "W25N04KVxxIR/U"},                                               \
    {SPI_NAND_FLASH_MICRON_MI, MICRON_DI_34, "MT29F4G01ABAFD"},                                                   \
    {SPI_NAND_FLASH_MICRON_MI, MICRON_DI_14, "MT29F1G01ABAFD"},                                                   \
    {SPI_NAND_FLASH_MICRON_MI, MICRON_DI_15, "MT29F1G01ABAFD (1.8 V)"},                                           \
    {SPI_NAND_FLASH_MICRON_MI, MICRON_DI_24, "MT29F2G01ABAGD"},                                                   \
    {SPI_NAND_FLASH_ZETTA_MI, ZETTA_DI_71, "ZD35Q1GC"},                                                           \
    {SPI_NAND_FLASH_XTX_MI, XTX_DI_37, "XT26G08D"},                                                               \
    {SPI_NAND_FLASH_DOSILICON_MI, DOSILICON_DI_71, "DS35Q1GA"},                                                   \
}

const char *spi_nand_flash_model_name(uint8_t manufacturer_id, uint16_t device_id);
