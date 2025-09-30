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
#include "usb_rx.h"

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
#define PDO_TYPE_AUGMENTED 3

typedef struct {
    uint32_t pdo;
    uint8_t type;
    uint8_t apdo_type;
    uint16_t voltage_mv;
    uint16_t min_voltage_mv;
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
    if (i2c_write(FUSB302B_ADDR, (uint8_t[]){ FUSB_FIFOS }, 1u)) {
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

    data[0] = FUSB_FIFOS;
    memcpy(&data[1], buffer, length);
    if (i2c_write(FUSB302B_ADDR, data, length + 1)) {
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

bool fusb_send_hardrst() {
    /* Send a hard reset */
    if (fusb_write_reg(FUSB_CONTROL3, 0x07 | FUSB_CONTROL3_SEND_HARD_RESET)) {
        return true;
    }
    busy_wait_ms(100);
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

bool fusb_cc_line_detect(uint8_t *cc_used) {
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
        //printf("No CC line detected!\r\n");
        *cc_used = 0;
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
        //printf("Using CC1\r\n");
        *cc_used = 1;
    } else {
        // TX_CC2|AUTO_CRC|SPECREV0
        if (fusb_write_reg(FUSB_SWITCHES1, 0x26)) {
            return true;
        }
        // PWDN1|PWDN2|MEAS_CC2
        if (fusb_write_reg(FUSB_SWITCHES0, 0x0B)) {
            return true;
        }
        //printf("Using CC2\r\n");
        *cc_used = 2;
    }
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
    if (fusb_write_reg(FUSB_CONTROL1, FUSB_CONTROL1_RX_FLUSH|FUSB_CONTROL1_ENSOP2DB|FUSB_CONTROL1_ENSOP1DB|FUSB_CONTROL1_ENSOP2|FUSB_CONTROL1_ENSOP1)) {
        return true;
    }
#if 0
    if (fusb_cc_line_detect()) {
        return true;
    }
    if (fusb_reset()) {
        return true;
    }
    pdo_msg_id = 0;
#endif
    return false;
}

typedef struct __attribute__((packed)) pdo_sop_header {
    uint8_t version:2;
    uint8_t port_data_role:1;
    uint8_t msg_type:5;
    uint8_t extended:1;
    uint8_t num_data_objs:3;
    uint8_t msg_id:3;
    uint8_t port_power_role:1;
} pdo_sop_header_t;


bool fusb_send_msg(uint8_t pdo_msg_type, uint8_t object_count, const uint8_t* data, uint8_t data_len) {
    /* Token sequences for the FUSB302B */
    static uint8_t sop_seq[5] = {
        FUSB_FIFO_TX_SOP1, FUSB_FIFO_TX_SOP1, FUSB_FIFO_TX_SOP1, FUSB_FIFO_TX_SOP2, FUSB_FIFO_TX_PACKSYM
    };
    static const uint8_t eop_seq[4] = { FUSB_FIFO_TX_JAM_CRC, FUSB_FIFO_TX_EOP, FUSB_FIFO_TX_TXOFF, FUSB_FIFO_TX_TXON };

    /* Get the length of the message: a two-octet header plus NUMOBJ four-octet
     * data objects */
    uint8_t msg_len = 2 + data_len; // header + data
#if 1
    uint8_t header[2];

    // Build header for message
    header[0] = 0b10 << 6; //0b10 << 6; // version: PD 3.0
    header[0] |= pdo_msg_type & 0x0F; // Message type
    header[1] = (object_count & 0x7) << 4; // Number of data objects
    header[1] |= (pdo_msg_id & 0x7) << 1;    // Message ID
#else

    pdo_sop_header_t header = {
        .version = 0b01, // PD 3.0
        .port_data_role = 0, // Sink
        .msg_type = pdo_msg_type & 0x1F,
        .extended = 0,
        .num_data_objs = object_count & 0x7,
        .msg_id = pdo_msg_id & 0x7,
        .port_power_role = 0 // Sink
    };
#endif
    /* Set the number of bytes to be transmitted in the packet */
    sop_seq[4] = FUSB_FIFO_TX_PACKSYM | msg_len;

    /* Write all three parts of the message to the TX FIFO */
    if (fusb_write_fifo((uint8_t*)sop_seq, 5)) {
        return true;
    }
    if (fusb_write_fifo((const uint8_t*)&header, 2)) {
        return true;
    }
    if (data_len > 0 && fusb_write_fifo(data, data_len)) {
        return true;
    }
    if (fusb_write_fifo((uint8_t*)eop_seq, 4)) {
        return true;
    }
    //printf("msg_id: %d\r\n", pdo_msg_id);
    // increment message ID
    pdo_msg_id = (pdo_msg_id + 1) & 0x7;  

    return false;
}

static void fusb_decode_pdo(uint32_t pdo, pdo_info_t* info) {
    info->pdo = pdo;
    info->type = (pdo >> 30) & 0x3;

    printf("PDO Raw: 0x%08X Type: %d\r\n", pdo, info->type);

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
        case PDO_TYPE_AUGMENTED:
            info->apdo_type = (pdo >> 28) & 0x3;
            //maximum volts 24:17
            info->voltage_mv = ((pdo >> 17) & 0xFF) * 100; // Max voltage in 100mV units
            info->min_voltage_mv = ((pdo >> 8) & 0xFF) * 100; // Min voltage in 100mV units
            info->current_ma = (pdo & 0x7F) * 50;          // Current in 50mA units
            info->power_mw = (info->voltage_mv * info->current_ma) / 1000;
            break;

        default:
            info->voltage_mv = 0;
            info->current_ma = 0;
            info->power_mw = 0;
            break;
    }
}

static void fusb_print_pdo(uint8_t index, pdo_info_t* pdo) {
    const char* type_names[] = { "Fixed", "Battery", "Variable", "APDO" };

    if(pdo->type == PDO_TYPE_AUGMENTED) {
        //printf("  APDO Type: %d\r\n", info->apdo_type);
        // PPS APDO
        printf("  %d: %s Supply - %u.%03uV to %u.%03uV @ %u.%03uA (%u.%03uW)\r\n",
               index,
               type_names[pdo->type],
               pdo->min_voltage_mv / 1000,
               pdo->min_voltage_mv % 1000,
               pdo->voltage_mv / 1000,
               pdo->voltage_mv % 1000,
               pdo->current_ma / 1000,
               pdo->current_ma % 1000,
               pdo->power_mw / 1000,
               pdo->power_mw % 1000);
        return;
    }

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

// PD message types
#define PD_MSG_TYPE_GET_SOURCE_CAP 0b111
bool fusb_send_get_source_cap() {
    /* Get_Source_Cap is a control message with no data objects */
    return fusb_send_msg(PD_MSG_TYPE_GET_SOURCE_CAP, 0, NULL, 0);
}

#define PD_MSG_TYPE_REQUEST 0b10
// reference: https://github.com/CRImier/HaD_talking_pd
bool fusb_pdo_request(uint8_t pdo_pos, uint16_t current, uint16_t max_current) {
    uint8_t pdo[4] = {0};

    // packing max current into fields
    uint16_t max_current_b = max_current; // 10
    uint16_t max_current_l = max_current_b & 0xff;
    uint16_t max_current_h = max_current_b >> 8;
    pdo[0] = max_current_l;
    pdo[1] |= max_current_h;

    // packing current into fields
    uint16_t current_b = current; // 10
    uint16_t current_l = current_b & 0x3f;
    uint16_t current_h = current_b >> 6;
    pdo[1] |= current_l << 2;
    pdo[2] |= current_h;

    pdo[3] |= (pdo_pos + 1) << 4; // PDO profile object position
    pdo[3] |= 0b1;                // no suspend

    /* Send the Request message with one data object */
    return fusb_send_msg(PD_MSG_TYPE_REQUEST, 1, pdo, sizeof(pdo));
}

bool fusb_pdo_hard_reset() {
    //try hard reset
    uint8_t control3;
    if(fusb_read_reg(FUSB_CONTROL3, &control3)) {
        return true;
    }
    control3 |= FUSB_CONTROL3_SEND_HARD_RESET;
    if(fusb_write_reg(FUSB_CONTROL3, control3)) {
        return true;
    }

    busy_wait_ms(10);

    pdo_msg_id = 0; //reset message ID
    return false;
}


#if 0
static bool fusb_read_source_capabilities(pdo_info_t* pdos, uint8_t* num_pdos) {
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

                    fusb_decode_pdo(pdo, &pdos[i]);
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

// Modify the fusb302_read_source_capabilities function to accept a parameter
static bool fusb_read_source_capabilities(pdo_info_t* pdos, uint8_t* num_pdos, bool send_request) {
    uint8_t status, interrupt;
    *num_pdos = 0;

    // Clear interrupts
    fusb_read_reg(FUSB_INTERRUPT, &interrupt);
    fusb_read_reg(FUSB_INTERRUPTA, &interrupt);
    fusb_read_reg(FUSB_INTERRUPTB, &interrupt);

    // Flush RX buffer first
    fusb_write_reg(FUSB_CONTROL1, FUSB_CONTROL1_RX_FLUSH);

    if (send_request) {
        printf("Sending Get_Source_Cap message...\r\n");
        if (fusb_send_get_source_cap()) {
            printf("Error: Failed to send Get_Source_Cap message\r\n");
            return false;
        }
    }

    //printf("Waiting for Source Capabilities message...\r\n");

    // Wait for a message (timeout after ~2 seconds)
    uint32_t timeout = 200;
    while (timeout-- > 0) {
        fusb_read_reg(FUSB_STATUS1, &status);

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

            printf("Header raw: 0x%02X 0x%02X\r\n", header[0], header[1]);
            uint16_t msg_header = (header[1] << 8) | header[0];
            uint8_t msg_type = msg_header & PD_HEADER_MSG_TYPE_MASK;
            uint8_t num_data_objs = (msg_header & PD_HEADER_NUM_DATA_OBJS_MASK) >> PD_HEADER_NUM_DATA_OBJS_SHIFT;
            uint8_t pdo_version = (header[0] >> 6) & 0x3;
            printf("Received message type: %d, data objects: %d, PDO version: %d\r\n", msg_type, num_data_objs, pdo_version);

            if (msg_type == PD_MSG_TYPE_SOURCE_CAPABILITIES) {
                // Read PDOs
                for (uint8_t i = 0; i < num_data_objs && i < 7; i++) {
                    uint8_t pdo_bytes[4];
                    if (fusb_read_fifo(pdo_bytes, 4)) {
                        printf("Error: Failed to read PDO %d\r\n", i);
                        continue;
                    }

                    uint32_t pdo = (pdo_bytes[3] << 24) | (pdo_bytes[2] << 16) | (pdo_bytes[1] << 8) | pdo_bytes[0];

                    fusb_decode_pdo(pdo, &pdos[i]);
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

        sleep_ms(1);
    }
    return false;
}

// Command usage and help
static const char* const usage[] = {
    "show chip ID and status info:%s fusb302 status", 
    "scan and select PDO profiles:%s fusb302 scan"
};

static const struct ui_help_options options[] = {
        {1,"", T_HELP_I2C_FUSB302},
        {0,"status", T_HELP_I2C_FUSB302_STATUS},
        {0,"scan", T_HELP_I2C_FUSB302_SCAN},
};

enum fusb_actions_enum {
    FUSB302_STATUS=0,
    FUSB302_SCAN,
};

static const struct cmdln_action_t fusb_actions[] = {{ FUSB302_STATUS, "status" },{ FUSB302_SCAN, "scan" }};

void fusb302_handler(struct command_result* res) {
    static pdo_info_t available_pdos[7];
    static uint8_t num_available_pdos = 0;

    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
        return;
    }
    if (!ui_help_sanity_check(true, 0x00)) {
        return;
    }

    uint32_t action;
    if (cmdln_args_get_action(fusb_actions, count_of(fusb_actions), &action)) {
        ui_help_show(true, usage, count_of(usage), &options[0], count_of(options));
        return;
    }

    fala_start_hook();

    switch (action) {

        case FUSB302_STATUS: {
            uint8_t device_id;
            if (!fusb_read_id(&device_id)) {
                printf("Device ID: 0x%02X\r\n", device_id);
                if ((device_id & 0xF0) == 0x90) {
                    printf("Chip: FUSB302\r\n");
                    printf("Revision: %d\r\n", device_id & 0x0F);
                } else {
                    printf("Warning: Unexpected device ID\r\n");
                    goto fusb302_cleanup;
                }
            } else {
                printf("Error: Failed to read device ID\r\n");
                goto fusb302_cleanup;
            }

            uint8_t status0, status1;
            if (!fusb_read_reg(FUSB_STATUS0, &status0) && !fusb_read_reg(FUSB_STATUS1, &status1)) {
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
            if (fusb_setup()) {
                printf("FUSB302 setup failed\r\n");
                break;
            }

            uint8_t cc_used = 0;
            char c;
            // any key to exit
            printf("Waiting for attach, x to exit...\r\n");
            while(true){
                if(rx_fifo_try_get(&c) && (c=='x')){
                    printf("Exiting scan...\r\n");
                    goto fusb302_cleanup;
                }

                if(fusb_cc_line_detect(&cc_used)){
                    continue;
                }
                printf("Attached on CC%d\r\nSending PD HARD_RESET\r\n", cc_used);

                if(fusb_pdo_hard_reset()){
                    printf("PDO hard reset failed\r\n"); //explore soft and hard reset
                    continue;
                }
                
                printf("Requesting Source Capabilities\r\n");
                if(fusb_reset()){
                    printf("FUSB302 reset failed\r\n"); 
                    continue;
                }

                if (!fusb_read_source_capabilities(available_pdos, &num_available_pdos, false)) {
                    printf("No Source Capabilities received\r\n");
                    continue;
                }

                //scan for 5v safe voltage
                for(uint8_t i = 0; i < num_available_pdos; i++) {
                    if(available_pdos[i].voltage_mv == 5000){
                        pdo_info_t* selected_pdo = &available_pdos[0];      
                        if (fusb_pdo_request(0, selected_pdo->current_ma / 10, selected_pdo->current_ma / 10)) {
                            printf("Failed to send PD message\r\n");
                            continue;
                        } else {
                            printf("Requested 5 volt profile\r\n");
                            fusb_print_pdo(i + 1, &available_pdos[i]);
                        }                            
                        break;
                    }
                }

                //interactive loop to request profiles
                while(true){
                    printf("\r\nAvailable Power Profiles:\r\n");
                    for (uint8_t i = 0; i < num_available_pdos; i++) {
                        fusb_print_pdo(i + 1, &available_pdos[i]);
                    }

                    printf("Enter profile number to request (1-%d), 0 to rescan, x to exit: ", num_available_pdos);
                    //wait for input
                    while(!rx_fifo_try_get(&c));

                    if(c == 'x'){
                        printf("\r\nExiting scan...\r\n");
                        goto fusb302_cleanup;
                    }
                    uint32_t profile_num = c - '0';
                    if(profile_num == 0){
                        printf("\r\nRescanning...\r\n");
                        break; //break to outer loop to rescan
                    }
                    if (profile_num < 1 || profile_num > (num_available_pdos)) {
                        printf("\r\nError: Invalid profile number. Available profiles: 1-%d\r\n", num_available_pdos);
                        continue;
                    }

                    pdo_info_t* selected_pdo = &available_pdos[profile_num - 1];
                    printf("\r\nRequested profile:");
                    fusb_print_pdo(profile_num, selected_pdo);
                    if(selected_pdo->voltage_mv > 20000){
                        printf("Error: %d.%dV exceeds FUSB302 limit (20V).\r\n", selected_pdo->voltage_mv/1000, selected_pdo->voltage_mv%1000);
                        continue;
                    }
                    if (fusb_pdo_request(profile_num - 1, selected_pdo->current_ma / 10, selected_pdo->current_ma / 10)) {
                        printf("Failed to send PD message\r\n");
                        continue;
                    } 
                    uint8_t statusa;
                    if(!fusb_read_reg(FUSB_STATUS0A, &statusa)){
                        if(statusa & FUSB_STATUS0A_RETRYFAIL){
                            printf("Error: Retry count exceeded\r\n");
                        }
                        if(statusa & FUSB_STATUS0A_SOFTRST){
                            printf("Error: Soft reset detected\r\n");
                        }
                        if(statusa & FUSB_STATUS0A_HARDRST){
                            printf("Error: Hard reset detected\r\n");
                        }
                    }else{
                        printf("Error: Failed to read status0a\r\n");
                    }                    
                }
                
                #if 0
                if (fusb_read_source_capabilities(available_pdos, &num_available_pdos, true)) {
                    //check the error bits in status0a
                    uint8_t statusa;
                    if(!fusb_read_reg(FUSB_STATUS0A, &statusa)){
                        if(statusa & FUSB_STATUS0A_RETRYFAIL){
                            printf("Error: Retry count exceeded\r\n");
                        }
                        if(statusa & FUSB_STATUS0A_SOFTRST){
                            printf("Error: Soft reset detected\r\n");
                        }
                        if(statusa & FUSB_STATUS0A_HARDRST){
                            printf("Error: Hard reset detected\r\n");
                        }
                    }

                    printf("\r\nAvailable Power Profiles:\r\n");
                    for (uint8_t i = 0; i < num_available_pdos; i++) {
                        fusb_print_pdo(i + 1, &available_pdos[i]);
                    }
                }
                #endif
            }
            break;
        }


        default:
            printf("Unknown action\r\n");
            break;
    }

fusb302_cleanup:
    // we manually control any FALA capture
    fala_stop_hook();
    fala_notify_hook();
}