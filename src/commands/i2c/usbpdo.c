/*
 * Portions of this code are adapted from:
 * https://github.com/Ralim/usb-pd
 *
 * PD Buddy Firmware Library - USB Power Delivery for everyone
 * Copyright 2017-2018 Clayton G. Hobbs
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include <string.h>
#include "pirate.h"
#include "pirate/hwi2c_pio.h"
#include "ui/ui_term.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"
#include "mode/hwi2c.h"
#include "ui/ui_help.h"
#include "ui/ui_cmdln.h"
#include "binmode/fala.h"
#include "commands/i2c/usbpdo.h"

// FUSB302 I2C address and register definitions
#define FUSB302_I2C_ADDR 0x22

// FUSB302 Register Map
#define FUSB302_REG_DEVICE_ID 0x01
#define FUSB302_REG_SWITCHES0 0x02
#define FUSB302_REG_SWITCHES1 0x03
#define FUSB302_REG_MEASURE 0x04
#define FUSB302_REG_SLICE 0x05
#define FUSB302_REG_CONTROL0 0x06
#define FUSB302_REG_CONTROL1 0x07
#define FUSB302_REG_CONTROL2 0x08
#define FUSB302_REG_CONTROL3 0x09
#define FUSB302_REG_MASK 0x0A
#define FUSB302_REG_POWER 0x0B
#define FUSB302_REG_RESET 0x0C
#define FUSB302_REG_OCPREG 0x0D
#define FUSB302_REG_MASKA 0x0E
#define FUSB302_REG_MASKB 0x0F
#define FUSB302_REG_CONTROL4 0x10
#define FUSB302_REG_STATUS0A 0x3C
#define FUSB302_REG_STATUS1A 0x3D
#define FUSB302_REG_INTERRUPTA 0x3E
#define FUSB302_REG_INTERRUPTB 0x3F
#define FUSB302_REG_STATUS0 0x40
#define FUSB302_REG_STATUS1 0x41
#define FUSB302_REG_INTERRUPT 0x42
#define FUSB302_REG_FIFOS 0x43

// FUSB302 Control/Configuration Values
#define FUSB302_POWER_ALL 0x0F        // Enable all power
#define FUSB302_RESET_SW 0x01         // Software reset
#define FUSB302_SWITCHES0_CC1_EN 0x07 // Enable CC1 pull-up and monitoring
#define FUSB302_SWITCHES0_CC2_EN 0x0B // Enable CC2 pull-up and monitoring
#define FUSB302_CONTROL2_MODE 0x02    // DFP mode
#define FUSB302_SLICE_SDAC_HYS 0x37   // Typical slicing threshold

// USB PD message tokens
#define PD_TOKEN_SOP 0xE0
#define PD_TOKEN_EOP 0xFE

// PD message header masks
#define PD_HEADER_NUM_DATA_OBJS_MASK 0x7000
#define PD_HEADER_NUM_DATA_OBJS_SHIFT 12
#define PD_HEADER_MSG_TYPE_MASK 0x000F

// PD message types
#define PD_MSG_TYPE_SOURCE_CAPABILITIES 1

// PDO types
#define PDO_TYPE_FIXED 0
#define PDO_TYPE_BATTERY 1
#define PDO_TYPE_VARIABLE 2

typedef struct {
    uint32_t pdo;
    uint8_t type;
    uint16_t voltage_mv;
    uint16_t current_ma;
    uint16_t power_mw;
} pdo_info_t;

static uint8_t pdo_msg_id = 0;

// FUSB302 I2C read/write helper functions
static bool fusb_write_reg(uint8_t reg, uint8_t value) {
    if (i2c_write(FUSB302B_ADDR, (uint8_t[]){ reg, value }, 2u)) {
        return true;
    }
    return false;
}

static bool fusb_read_reg(uint8_t reg, uint8_t* value) {
    // setup the register to read
    if (i2c_write(FUSB302B_ADDR, (uint8_t[]){ reg }, 1u)) {
        return true;
    }
    // read the register
    if (i2c_read((FUSB302B_ADDR) | 1, value, 1u)) {
        return true;
    }
    return false;
}

static bool fusb_read_fifo(uint8_t* buffer, uint8_t length) {
    if (i2c_write(FUSB302B_ADDR, (uint8_t[]){ FUSB302_REG_FIFOS }, 1u)) {
        return true;
    }

    if (i2c_read((FUSB302B_ADDR) | 1, buffer, length)) {
        return true;
    }
    return false;
}

bool fusb_write_fifo(const uint8_t* buffer, uint8_t length) {
    uint8_t data[65];

    if (length + 1 > sizeof(data)) {
        return true; // Prevent overflow
    }

    data[0] = FUSB302_REG_FIFOS;
    memcpy(&data[1], buffer, length);
    if (i2c_write(FUSB302B_ADDR, data, length + 1)) {
        return true;
    }
    return false;
}

static void fusb302_decode_pdo(uint32_t pdo, pdo_info_t* info) {
    info->pdo = pdo;
    info->type = (pdo >> 30) & 0x3;

    switch (info->type) {
        case PDO_TYPE_FIXED:
            info->voltage_mv = ((pdo >> 10) & 0x3FF) * 50; // Voltage in 50mV units
            info->current_ma = (pdo & 0x3FF) * 10;         // Current in 10mA units
            info->power_mw = (info->voltage_mv * info->current_ma) / 1000;
            break;

        case PDO_TYPE_VARIABLE:
            // For variable PDOs, we show max voltage and current
            info->voltage_mv = ((pdo >> 20) & 0x3FF) * 50; // Max voltage
            info->current_ma = (pdo & 0x3FF) * 10;         // Current
            info->power_mw = (info->voltage_mv * info->current_ma) / 1000;
            break;

        case PDO_TYPE_BATTERY:
            // For battery PDOs
            info->voltage_mv = ((pdo >> 20) & 0x3FF) * 50; // Max voltage
            info->power_mw = (pdo & 0x3FF) * 250;          // Power in 250mW units
            info->current_ma = (info->power_mw * 1000) / info->voltage_mv;
            break;

        default:
            info->voltage_mv = 0;
            info->current_ma = 0;
            info->power_mw = 0;
            break;
    }
}

static void fusb302_print_pdo(uint8_t index, pdo_info_t* pdo) {
    const char* type_names[] = { "Fixed", "Battery", "Variable", "Reserved" };

    printf("  %d: %s Supply - %u.%03uV @ %u.%03uA (%u.%03uW)\r\n",
           index,
           type_names[pdo->type],
           pdo->voltage_mv / 1000,
           pdo->voltage_mv % 1000,
           pdo->current_ma / 1000,
           pdo->current_ma % 1000,
           pdo->power_mw / 1000,
           pdo->power_mw % 1000);
}

#if 0
static bool fusb302_read_source_capabilities(pdo_info_t* pdos, uint8_t* num_pdos) {
    uint8_t status, interrupt;
    uint8_t fifo_data[64];
    *num_pdos = 0;

    // Clear interrupts
    fusb_read_reg(FUSB302_REG_INTERRUPT, &interrupt);
    fusb_read_reg(FUSB302_REG_INTERRUPTA, &interrupt);
    fusb_read_reg(FUSB302_REG_INTERRUPTB, &interrupt);

    printf("Waiting for Source Capabilities message...\r\n");

    // Wait for a message (timeout after ~2 seconds)
    uint32_t timeout = 200;
    while (timeout-- > 0) {
        fusb_read_reg(FUSB302_REG_STATUS1, &status);

        if (!(status & 0x20)) { // RX_Empty bit
            // Read the message from FIFO
            uint8_t token1, token2;
            if (fusb_read_fifo(&token1, 1) || token1 != PD_TOKEN_SOP) {
                printf("Error: Invalid SOP token\r\n");
                continue;
            }

            // Read header
            uint8_t header[2];
            if (fusb_read_fifo(header, 2)) {
                printf("Error: Failed to read message header\r\n");
                continue;
            }

            uint16_t msg_header = (header[1] << 8) | header[0];
            uint8_t msg_type = msg_header & PD_HEADER_MSG_TYPE_MASK;
            uint8_t num_data_objs = (msg_header & PD_HEADER_NUM_DATA_OBJS_MASK) >> PD_HEADER_NUM_DATA_OBJS_SHIFT;

            if (msg_type == PD_MSG_TYPE_SOURCE_CAPABILITIES) {
                printf("Received Source Capabilities with %d PDOs:\r\n", num_data_objs);

                // Read PDOs
                for (uint8_t i = 0; i < num_data_objs && i < 7; i++) {
                    uint8_t pdo_bytes[4];
                    if (fusb_read_fifo(pdo_bytes, 4)) {
                        printf("Error: Failed to read PDO %d\r\n", i);
                        continue;
                    }

                    uint32_t pdo = (pdo_bytes[3] << 24) | (pdo_bytes[2] << 16) | (pdo_bytes[1] << 8) | pdo_bytes[0];

                    fusb302_decode_pdo(pdo, &pdos[i]);
                    (*num_pdos)++;
                }

                // Read CRC and EOP
                uint8_t crc[4];
                fusb_read_fifo(crc, 4);
                // fusb_read_fifo(&token2, 1);
                return true;
            }
        }

        sleep_ms(10);
    }
    return false;
}
#endif



// PD message types
#define PD_MSG_TYPE_GET_SOURCE_CAP 0b111

bool fusb_send_get_source_cap() {
    /* Token sequences for the FUSB302B */
    static uint8_t sop_seq[5] = {
        FUSB_FIFO_TX_SOP1, FUSB_FIFO_TX_SOP1, FUSB_FIFO_TX_SOP1, FUSB_FIFO_TX_SOP2, FUSB_FIFO_TX_PACKSYM
    };
    static const uint8_t eop_seq[4] = { FUSB_FIFO_TX_JAM_CRC, FUSB_FIFO_TX_EOP, FUSB_FIFO_TX_TXOFF, FUSB_FIFO_TX_TXON };

    /* Get_Source_Cap is a control message with no data objects */
    uint8_t msg_len = 2; // Just header
    uint8_t header[2];

    // Build header for Get_Source_Cap message
    header[0] = 0b10 << 6; // version: PD 3.0
    header[0] |= PD_MSG_TYPE_GET_SOURCE_CAP; // Message type
    header[1] = (pdo_msg_id & 0x7) << 1;    // Message ID, no data objects

    /* Set the number of bytes to be transmitted in the packet */
    sop_seq[4] = FUSB_FIFO_TX_PACKSYM | msg_len;

    /* Write all three parts of the message to the TX FIFO */
    if (fusb_write_fifo((uint8_t*)sop_seq, 5)) {
        return true;
    }
    if (fusb_write_fifo(header, msg_len)) {
        return true;
    }
    if (fusb_write_fifo((uint8_t*)eop_seq, 4)) {
        return true;
    }

    // increment message ID
    pdo_msg_id = (pdo_msg_id + 1) & 0x7;

    return false;
}

// Modify the fusb302_read_source_capabilities function to accept a parameter
static bool fusb302_read_source_capabilities(pdo_info_t* pdos, uint8_t* num_pdos, bool send_request) {
    uint8_t status, interrupt;
    *num_pdos = 0;

    // Clear interrupts
    fusb_read_reg(FUSB302_REG_INTERRUPT, &interrupt);
    fusb_read_reg(FUSB302_REG_INTERRUPTA, &interrupt);
    fusb_read_reg(FUSB302_REG_INTERRUPTB, &interrupt);

    // Flush RX buffer first
    fusb_write_reg(FUSB_CONTROL1, FUSB_CONTROL1_RX_FLUSH);

    if (send_request) {
        printf("Sending Get_Source_Cap message...\r\n");
        if (fusb_send_get_source_cap()) {
            printf("Error: Failed to send Get_Source_Cap message\r\n");
            return false;
        }
    }

    printf("Waiting for Source Capabilities message...\r\n");

    // Wait for a message (timeout after ~2 seconds)
    uint32_t timeout = 200;
    while (timeout-- > 0) {
        fusb_read_reg(FUSB302_REG_STATUS1, &status);

        if (!(status & 0x20)) { // RX_Empty bit
            // Read the message from FIFO
            uint8_t token1;
            if (fusb_read_fifo(&token1, 1) || token1 != PD_TOKEN_SOP) {
                printf("Error: Invalid SOP token (0x%02X)\r\n", token1);
                // Flush and continue
                fusb_write_reg(FUSB_CONTROL1, FUSB_CONTROL1_RX_FLUSH);
                continue;
            }

            // Read header
            uint8_t header[2];
            if (fusb_read_fifo(header, 2)) {
                printf("Error: Failed to read message header\r\n");
                continue;
            }

            uint16_t msg_header = (header[1] << 8) | header[0];
            uint8_t msg_type = msg_header & PD_HEADER_MSG_TYPE_MASK;
            uint8_t num_data_objs = (msg_header & PD_HEADER_NUM_DATA_OBJS_MASK) >> PD_HEADER_NUM_DATA_OBJS_SHIFT;

            printf("Received message type: %d, data objects: %d\r\n", msg_type, num_data_objs);

            if (msg_type == PD_MSG_TYPE_SOURCE_CAPABILITIES) {
                printf("Received Source Capabilities with %d PDOs:\r\n", num_data_objs);

                // Read PDOs
                for (uint8_t i = 0; i < num_data_objs && i < 7; i++) {
                    uint8_t pdo_bytes[4];
                    if (fusb_read_fifo(pdo_bytes, 4)) {
                        printf("Error: Failed to read PDO %d\r\n", i);
                        continue;
                    }

                    uint32_t pdo = (pdo_bytes[3] << 24) | (pdo_bytes[2] << 16) | (pdo_bytes[1] << 8) | pdo_bytes[0];

                    fusb302_decode_pdo(pdo, &pdos[i]);
                    (*num_pdos)++;
                }

                // Read CRC and EOP
                uint8_t crc[4];
                fusb_read_fifo(crc, 4);
                return true;
            } else {
                // Skip this message - read remaining data objects and CRC
                for (uint8_t i = 0; i < num_data_objs; i++) {
                    uint8_t dummy[4];
                    fusb_read_fifo(dummy, 4);
                }
                uint8_t crc[4];
                fusb_read_fifo(crc, 4);
            }
        }

        sleep_ms(10);
    }
    return false;
}

bool fusb_reset() {
    /* Flush the TX buffer */
    if (fusb_write_reg(FUSB_CONTROL0, 0x44)) {
        return true;
    }
    /* Flush the RX buffer */
    if (fusb_write_reg(FUSB_CONTROL1, FUSB_CONTROL1_RX_FLUSH)) {
        return true;
    }
    /* Reset the PD logic */
    // This triggers the source to send capabilities
    if (fusb_write_reg(FUSB_RESET, FUSB_RESET_PD_RESET)) {
        return true;
    }

    return false;
}

bool fusb_read_id(uint8_t* version) {
    // Return true if read of the revision ID is sane
    *version = 0;
    if (fusb_read_reg(FUSB_DEVICE_ID, version)) {
        return true;
    }
    if (*version == 0 || *version == 0xFF) {
        return true;
    }
    return false;
}

bool runCCLineSelection() {
    uint8_t cc1, cc2;

    /* Measure CC1 */
    if (fusb_write_reg(FUSB_SWITCHES0, 0x07)) {
        return true;
    }
    busy_wait_ms(10);
    if (fusb_read_reg(FUSB_STATUS0, &cc1)) {
        return true;
    }

    /* Measure CC2 */
    if (fusb_write_reg(FUSB_SWITCHES0, 0x0B)) {
        return true;
    }
    busy_wait_ms(10);
    if (fusb_read_reg(FUSB_STATUS0, &cc2)) {
        return true;
    }

    if ((cc1 & FUSB_STATUS0_BC_LVL) == 0 && (cc2 & FUSB_STATUS0_BC_LVL) == 0) {
        printf("No CC line detected!\r\n");
        return true;
    }

    /* Select the correct CC line for BMC signaling; also enable AUTO_CRC */
    if ((cc1 & FUSB_STATUS0_BC_LVL) > (cc2 & FUSB_STATUS0_BC_LVL)) {
        // TX_CC1|AUTO_CRC|SPECREV0
        if (fusb_write_reg(FUSB_SWITCHES1, 0x25)) {
            return true;
        }
        // PWDN1|PWDN2|MEAS_CC1
        if (fusb_write_reg(FUSB_SWITCHES0, 0x07)) {
            return true;
        }
        printf("Using CC1\r\n");
    } else {
        // TX_CC2|AUTO_CRC|SPECREV0
        if (fusb_write_reg(FUSB_SWITCHES1, 0x26)) {
            return true;
        }
        // PWDN1|PWDN2|MEAS_CC2
        if (fusb_write_reg(FUSB_SWITCHES0, 0x0B)) {
            return true;
        }
        printf("Using CC2\r\n");
    }
    return false;
}

bool fusb_send_hardrst() {
    /* Send a hard reset */
    if (fusb_write_reg(FUSB_CONTROL3, 0x07 | FUSB_CONTROL3_SEND_HARD_RESET)) {
        return true;
    }
    busy_wait_ms(100);
    return false;
}

bool fusb_setup(void) {
    /* Fully reset the FUSB302B */
    if (fusb_write_reg(FUSB_RESET, FUSB_RESET_SW_RES)) {
        return true;
    }

    busy_wait_ms(10);
    uint8_t tries = 0;
    uint8_t version = 0;
    while (fusb_read_id(&version)) {
        busy_wait_ms(10);
        tries++;
        if (tries > 5) {
            return true; // Welp :(
        }
    }

    /* Turn on all power */
    if (fusb_write_reg(FUSB_POWER, 0x0F)) {
        return true;
    }

    /* Set interrupt masks */
    // Setting to 0 so interrupts are allowed
    if (fusb_write_reg(FUSB_MASK1, 0x00)) {
        return true;
    }
    if (fusb_write_reg(FUSB_MASKA, 0x00)) {
        return true;
    }
    if (fusb_write_reg(FUSB_MASKB, 0x00)) {
        return true;
    }
    if (fusb_write_reg(FUSB_CONTROL0, 0b11 << 2)) {
        return true;
    }

    /* Enable automatic retransmission */
    if (fusb_write_reg(FUSB_CONTROL3, 0x07)) {
        return true;
    }
    // set defaults
    if (fusb_write_reg(FUSB_CONTROL2, 0x00)) {
        return true;
    }
    /* Flush the RX buffer */
    if (fusb_write_reg(FUSB_CONTROL1, FUSB_CONTROL1_RX_FLUSH)) {
        return true;
    }

    if (runCCLineSelection()) {
        return true;
    }
    if (fusb_reset()) {
        return true;
    }
    pdo_msg_id = 0;
    return false;
}

// reference: https://github.com/CRImier/HaD_talking_pd
bool fusb_pdo_request(uint8_t pdo_pos, uint16_t current, uint16_t max_current) {
    /* Token sequences for the FUSB302B */
    static uint8_t sop_seq[5] = {
        FUSB_FIFO_TX_SOP1, FUSB_FIFO_TX_SOP1, FUSB_FIFO_TX_SOP1, FUSB_FIFO_TX_SOP2, FUSB_FIFO_TX_PACKSYM
    };
    static const uint8_t eop_seq[4] = { FUSB_FIFO_TX_JAM_CRC, FUSB_FIFO_TX_EOP, FUSB_FIFO_TX_TXOFF, FUSB_FIFO_TX_TXON };

    /* Get the length of the message: a two-octet header plus NUMOBJ four-octet
     * data objects */
    uint8_t obj_count = 1;
    uint8_t pdo_len = 2 + (4 * obj_count);
    uint8_t pdo[6]={0,0,0,0,0,0};

    // 2 byte header, big endian
    pdo[0] = 0b10 << 6; // version: PD 3.0
    pdo[0] |= 0b00010;   // message type: request

    pdo[1] |= obj_count << 4;        // number of data objects
    pdo[1] |= (pdo_msg_id & 0b111) << 1; // message ID

    // packing max current into fields
    uint16_t max_current_b = max_current; // 10
    uint16_t max_current_l = max_current_b & 0xff;
    uint16_t max_current_h = max_current_b >> 8;
    pdo[2] = max_current_l;
    pdo[3] |= max_current_h;

    // packing current into fields
    uint16_t current_b = current; // 10
    uint16_t current_l = current_b & 0x3f;
    uint16_t current_h = current_b >> 6;
    pdo[3] |= current_l << 2;
    pdo[4] |= current_h;

    pdo[5] |= (pdo_pos + 1) << 4; // PDO profile object position
    pdo[5] |= 0b1;                // no suspend

    /* Set the number of bytes to be transmitted in the packet */
    sop_seq[4] = FUSB_FIFO_TX_PACKSYM | pdo_len;

    /* Write all three parts of the message to the TX FIFO */
    if (fusb_write_fifo((uint8_t*)sop_seq, 5)) {
        return true;
    }
    if (fusb_write_fifo((uint8_t*)pdo, pdo_len)) {
        return true;
    }
    if (fusb_write_fifo((uint8_t*)eop_seq, 4)) {
        return true;
    }

    // increment message ID
    pdo_msg_id = (pdo_msg_id + 1) & 0b111;

    return false;
}



// Command usage and help
static const char* const usage[] = {
    "fusb302 init", "fusb302 id", "fusb302 status", "fusb302 scan", "fusb302 request <profile>"
};

static const struct ui_help_options options[] = {
    /*    {0,"", T_HELP_FUSB302_INIT},
        {0,"", T_HELP_FUSB302_ID},
        {0,"", T_HELP_FUSB302_STATUS},
        {0,"", T_HELP_FUSB302_SCAN},
        {0,"", T_HELP_FUSB302_REQUEST}*/
};

enum fusb302_actions_enum {
    FUSB302_INIT = 0,
    FUSB302_ID,
    FUSB302_STATUS,
    FUSB302_SCAN,
    FUSB302_REQUEST
};

static const struct cmdln_action_t fusb302_actions[] = { { FUSB302_INIT, "init" },
                                                         { FUSB302_ID, "id" },
                                                         { FUSB302_STATUS, "status" },
                                                         { FUSB302_SCAN, "scan" },
                                                         { FUSB302_REQUEST, "request" } };

static pdo_info_t available_pdos[7];
static uint8_t num_available_pdos = 0;

void fusb302_handler(struct command_result* res) {
    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
        return;
    }
    if (!ui_help_sanity_check(true, 0x00)) {
        return;
    }

    uint32_t action;
    if (cmdln_args_get_action(fusb302_actions, count_of(fusb302_actions), &action)) {
        ui_help_show(true, usage, count_of(usage), &options[0], count_of(options));
        return;
    }

    fala_start_hook();

    switch (action) {

        case FUSB302_ID: {
            uint8_t device_id;
            if (!fusb_read_id(&device_id)) {
                printf("Device ID: 0x%02X\r\n", device_id);
                if ((device_id & 0xF0) == 0x90) {
                    printf("Chip: FUSB302\r\n");
                    printf("Revision: %d\r\n", device_id & 0x0F);
                } else {
                    printf("Warning: Unexpected device ID\r\n");
                }
            } else {
                printf("Error: Failed to read device ID\r\n");
            }
            break;
        }

        case FUSB302_STATUS: {
            uint8_t status0, status1;
            if (!fusb_read_reg(FUSB302_REG_STATUS0, &status0) && !fusb_read_reg(FUSB302_REG_STATUS1, &status1)) {
                printf("FUSB302 Status:\r\n");
                printf("  STATUS0: 0x%02X\r\n", status0);
                printf("  STATUS1: 0x%02X\r\n", status1);
                printf("  CC1: %s\r\n", (status0 & 0x01) ? "Connected" : "Disconnected");
                printf("  CC2: %s\r\n", (status0 & 0x02) ? "Connected" : "Disconnected");
                printf("  VBUS OK: %s\r\n", (status0 & 0x80) ? "Yes" : "No");
            } else {
                printf("Error: Failed to read status registers\r\n");
            }
            break;
        }

        case FUSB302_SCAN: {
            printf("Scanning for USB PD source capabilities...\r\n");

            if (fusb_setup()) {
                printf("FUSB302 setup failed\r\n");
                break;
            }

            if (fusb302_read_source_capabilities(available_pdos, &num_available_pdos, false)) {
                printf("\r\nAvailable Power Profiles:\r\n");
                for (uint8_t i = 0; i < num_available_pdos; i++) {
                    fusb302_print_pdo(i + 1, &available_pdos[i]);
                }
                uint8_t profile = 0;
                pdo_info_t* selected_pdo = &available_pdos[0];
                printf("Requesting initial profile %d: %u.%03uV @ %u.%03uA\r\n",
                       1,
                       selected_pdo->voltage_mv / 1000,
                       selected_pdo->voltage_mv % 1000,
                       selected_pdo->current_ma / 1000,
                       selected_pdo->current_ma % 1000);
                
                if (fusb_pdo_request(0, selected_pdo->current_ma / 10, selected_pdo->current_ma / 10)) {
                    printf("Failed to send PD message\r\n");
                } else {
                    printf("PD Request message sent\r\n");
                }

                busy_wait_ms(500); //delay
                selected_pdo = &available_pdos[1];
                printf("Requesting second profile %d: %u.%03uV @ %u.%03uA\r\n",
                       2,
                       selected_pdo->voltage_mv / 1000,
                       selected_pdo->voltage_mv % 1000,
                       selected_pdo->current_ma / 1000,
                       selected_pdo->current_ma % 1000);

                if (fusb_pdo_request(1, selected_pdo->current_ma / 10, selected_pdo->current_ma / 10)) {
                    printf("Failed to send PD message\r\n");
                } else {
                    printf("PD Request message sent\r\n");
                }     
                
                busy_wait_ms(500); //delay
                selected_pdo = &available_pdos[2];
                printf("Requesting second profile %d: %u.%03uV @ %u.%03uA\r\n",
                       3,
                       selected_pdo->voltage_mv / 1000,
                       selected_pdo->voltage_mv % 1000,
                       selected_pdo->current_ma / 1000,
                       selected_pdo->current_ma % 1000);

                if (fusb_pdo_request(2, selected_pdo->current_ma / 10, selected_pdo->current_ma / 10)) {
                    printf("Failed to send PD message\r\n");
                } else {
                    printf("PD Request message sent\r\n");
                }                     

                busy_wait_ms(500); //delay
                printf("Requesting source capabilities...\r\n");
                
                // Check if device is already initialized
                uint8_t device_id;
                if (fusb_read_id(&device_id)) {
                    printf("Error: FUSB302 not detected. Run 'fusb302 scan' first.\r\n");
                    break;
                }

                // Request capabilities without reset
                if (fusb302_read_source_capabilities(available_pdos, &num_available_pdos, true)) {
                    printf("\r\nAvailable Power Profiles:\r\n");
                    for (uint8_t i = 0; i < num_available_pdos; i++) {
                        fusb302_print_pdo(i + 1, &available_pdos[i]);
                    }
                } else {
                    printf("Failed to get source capabilities\r\n");
                }                


            } else {
                printf("No USB PD source detected\r\n");
            }
            break;
        }

        case FUSB302_REQUEST: {
            uint32_t profile_num;
            /*if (!cmdln_args_get_uint(&profile_num)) {
                printf("Error: Profile number required\r\n");
                printf("Usage: fusb302 request <profile>\r\n");
                break;
            }*/

            if (num_available_pdos == 0) {
                printf("Error: No PDOs available. Run 'fusb302 scan' first.\r\n");
                break;
            }

            if (profile_num < 1 || profile_num > num_available_pdos) {
                printf("Error: Invalid profile number. Available profiles: 1-%d\r\n", num_available_pdos);
                break;
            }

            pdo_info_t* selected_pdo = &available_pdos[profile_num - 1];
            printf("Requesting profile %d: %u.%03uV @ %u.%03uA\r\n",
                   (int)profile_num,
                   selected_pdo->voltage_mv / 1000,
                   selected_pdo->voltage_mv % 1000,
                   selected_pdo->current_ma / 1000,
                   selected_pdo->current_ma % 1000);

            // TODO: Implement actual PD request message
            printf("Note: PD request implementation is not yet complete.\r\n");
            printf("This would send a Request message for the selected PDO.\r\n");
            break;
        }

        default:
            printf("Unknown action\r\n");
            break;
    }

    // we manually control any FALA capture
    fala_stop_hook();
    fala_notify_hook();
}