/* SPDX-License-Identifier: Apache-2.0 */

#ifndef AP33772S_INT_H
#define AP33772S_INT_H

#include "ap33772s.h"

/* Internal register addresses */
#define AP33772S_CMD_STATUS   0x01
#define AP33772S_CMD_MASK     0x02
#define AP33772S_CMD_OPMODE   0x03
#define AP33772S_CMD_CONFIG   0x04
#define AP33772S_CMD_PDCONFIG 0x05
#define AP33772S_CMD_SYSTEM   0x06
#define AP33772S_CMD_TR25     0x0C
#define AP33772S_CMD_TR50     0x0D
#define AP33772S_CMD_TR75     0x0E
#define AP33772S_CMD_TR100    0x0F
#define AP33772S_CMD_VOLTAGE  0x11
#define AP33772S_CMD_CURRENT  0x12
#define AP33772S_CMD_TEMP     0x13
#define AP33772S_CMD_VREQ     0x14
#define AP33772S_CMD_IREQ     0x15
#define AP33772S_CMD_VSELMIN  0x16
#define AP33772S_CMD_UVPTHR   0x17
#define AP33772S_CMD_OVPTHR   0x18
#define AP33772S_CMD_OCPTHR   0x19
#define AP33772S_CMD_OTPTHR   0x1A
#define AP33772S_CMD_DRTHR    0x1B
#define AP33772S_CMD_SRCPDO   0x20
#define AP33772S_CMD_PD_REQMSG 0x31
#define AP33772S_CMD_PD_CMDMSG 0x32
#define AP33772S_CMD_PD_MSGRLT 0x33
#define AP33772S_CMD_GPIO      0x52
#define AP33772S_CMD_SRC_SPR_PDO1  0x21
#define AP33772S_CMD_SRC_SPR_PDO7  0x27
#define AP33772S_CMD_SRC_EPR_PDO8  0x28
#define AP33772S_CMD_SRC_EPR_PDO13 0x2D

/* SYSTEM register values controlling the NMOS output switch */
#define AP33772S_SYSTEM_OUTPUT_ENABLE   0x12
#define AP33772S_SYSTEM_OUTPUT_DISABLE  0x11

/* GPIO register bit masks (AP33772S_CMD_GPIO) */
#define AP33772S_GPIO_PU_EN   (1u << 7)
#define AP33772S_GPIO_PD_EN   (1u << 6)
#define AP33772S_GPIO_DI      (1u << 3)
#define AP33772S_GPIO_IE      (1u << 2)
#define AP33772S_GPIO_DO      (1u << 1)
#define AP33772S_GPIO_OE      (1u << 0)

/* OPMODE and PD configuration bit helpers */
#define AP33772S_OPMODE_DATARL      (1u << 5)
#define AP33772S_PDCONFIG_DRSWP_EN (1u << 2)

/* PD command message bits (AP33772S_CMD_PD_CMDMSG) */
#define AP33772S_PD_CMDMSG_DRSWP   (1u << 1)
#define AP33772S_PD_CMDMSG_HRST    (1u << 0)

/* Internal defaults */
#define AP33772S_DEFAULT_TR25_OHM 10000
#define AP33772S_DEFAULT_TR50_OHM 4161
#define AP33772S_DEFAULT_TR75_OHM 1928
#define AP33772S_DEFAULT_TR100_OHM 974

struct ap33772s {
    uint16_t src_pdo[AP33772S_MAX_PDO_ENTRIES];
    int16_t num_pdo;
    int pps_index;
    int avs_index;
    int voltage_avs_byte;
    int current_avs_byte;
    int index_avs;
    struct ap33772s_bus_delegate bus;
};

#endif /* AP33772S_INT_H */
