/* SPDX-License-Identifier: Apache-2.0 */

#define _POSIX_C_SOURCE 200809L

#include "ap33772s.h"
#include "ap33772s_int.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
 * @file ap33772s.c
 * @brief Implementation of AP33772S I2C USB PD3.1 EPR SINK CONTROLLER driver
 *
 * This file contains the core implementation for communicating with and controlling
 * the AP33772S USB Power Delivery sink controller. It handles PDO parsing, PD negotiation,
 * protection threshold configuration, and system monitoring.
 */

static inline int ap33772s_call_read(ap33772s_ref dev, uint8_t reg, uint8_t *data, size_t len)
{
    return dev->bus.read(dev->bus.ctx, reg, data, len);
}

static inline int ap33772s_call_write(ap33772s_ref dev, uint8_t reg, const uint8_t *data, size_t len)
{
    return dev->bus.write(dev->bus.ctx, reg, data, len);
}

int ap33772s_read_bytes(ap33772s_ref dev, uint8_t reg, uint8_t *data, size_t len)
{
    if (!dev || !data || len == 0) {
        return -EINVAL;
    }
    return ap33772s_call_read(dev, reg, data, len);
}

int ap33772s_write_bytes(ap33772s_ref dev, uint8_t reg, const uint8_t *data, size_t len)
{
    if (!dev || len > AP33772S_WRITE_BUFFER_LENGTH || (len > 0 && !data)) {
        return -EINVAL;
    }
    return ap33772s_call_write(dev, reg, data, len);
}

/**
 * @brief Check if PDO is programmable (PPS/AVS)
 * @param raw Raw PDO value
 * @return true if programmable, false if fixed
 *
 * Based on bit 14 of PDO (assumed programmable flag).
 */
static inline bool ap33772s_pdo_is_programmable(uint16_t raw)
{
    return ((raw >> 14) & 0x1) == 1;
}

/**
 * @brief Check if PDO is valid (non-zero)
 * @param raw Raw PDO value
 * @return true if valid, false otherwise
 */
static inline bool ap33772s_pdo_is_valid(uint16_t raw)
{
    return raw != 0;
}

/**
 * @brief Extract current capability code from PDO
 * @param raw Raw PDO value
 * @return Current code (4 bits, 0-15)
 */
static inline uint8_t ap33772s_pdo_current_code(uint16_t raw)
{
    return (raw >> 10) & 0xF;
}

static inline uint8_t ap33772s_pdo_voltage_field(uint16_t raw)
{
    return (raw >> 8) & 0x3;
}

/**
 * @brief Decode maximum voltage from PDO
 * @param raw Raw PDO value
 * @param is_epr true for EPR mode (200mV units), false for SPR (100mV units)
 * @return Maximum voltage in mV
 */
static inline int ap33772s_decode_voltage_max_mv(uint16_t raw, bool is_epr)
{
    int multiplier = is_epr ? 200 : 100;
    return (raw & 0xFF) * multiplier;
}

/**
 * @brief Get minimum voltage for PPS PDO
 * @param raw Raw PPS PDO value
 * @return Minimum voltage in mV (3300 or 0)
 */
static inline int ap33772s_pps_voltage_min_mv(uint16_t raw)
{
    return ((raw >> 8) & 0x3) > 0 ? 3300 : 0;
}

/**
 * @brief Get minimum voltage for AVS PDO
 * @param raw Raw AVS PDO value
 * @return Minimum voltage in mV (15000 or 0)
 */
static inline int ap33772s_avs_voltage_min_mv(uint16_t raw)
{
    return ((raw >> 8) & 0x3) > 0 ? 15000 : 0;
}

/**
 * @brief Convert current in mA to PDO current code for PD requests
 * @param current_ma Current in milliamps
 * @return Current code (0-15), or negative errno on error
 *
 * According to datasheet Table 2: CURRENT_SEL [0000] to [1111] = 1.00A to 5.00A
 * Maps 1000mA to 5000mA linearly across codes 0-15.
 */
static int ap33772s_current_code_from_ma(int current_ma)
{
    if (current_ma < 0 || current_ma > 5000) {
        return -EINVAL;
    }
    
    // Match the proven C++ Arduino implementation logic
    if (current_ma < 1250) {
        return 0;
    }
    
    // Calculate using the same formula as C++ version
    int code = ((current_ma - 1250) / 250) + 1;
    
    // Clamp to maximum value
    if (code > 15) {
        code = 15;
    }
    
    return code;
}

/**
 * @brief Convert PDO current code to current range
 * @param code Current code (0-15)
 * @param min_ma Pointer to store minimum current in mA
 * @param max_ma Pointer to store maximum current in mA
 *
 * Calculates the current range represented by the code.
 */
static void ap33772s_current_range_from_code(uint8_t code, int *min_ma, int *max_ma)
{
    if (!min_ma || !max_ma) {
        return;
    }
    if (code == 0) {
        *min_ma = 0;
        *max_ma = 1240;
        return;
    }
    int start = 1250 + (code - 1) * 250;
    *min_ma = start;
    if (code == 15) {
        *max_ma = 5000;
    } else {
        *max_ma = start + 249;
    }
}

/**
 * @brief Read 8-bit value from register
 * @param dev Device context
 * @param reg Register address
 * @param value Pointer to store value
 * @return 0 on success, negative errno on failure
 */
static int ap33772s_read_u8(struct ap33772s *dev, uint8_t reg, uint8_t *value)
{
    uint8_t tmp = 0;
    int ret = ap33772s_call_read(dev, reg, &tmp, 1);
    if (ret < 0) {
        return ret;
    }
    *value = tmp;
    return 0;
}

/**
 * @brief Read 16-bit value from register (little-endian)
 * @param dev Device context
 * @param reg Register address
 * @param value Pointer to store value
 * @return 0 on success, negative errno on failure
 */
static int ap33772s_read_u16(struct ap33772s *dev, uint8_t reg, uint16_t *value)
{
    uint8_t buf[2] = {0};
    int ret = ap33772s_call_read(dev, reg, buf, sizeof(buf));
    if (ret < 0) {
        return ret;
    }
    *value = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
    return 0;
}

/**
 * @brief Write 8-bit value to register
 * @param dev Device context
 * @param reg Register address
 * @param value Value to write
 * @return 0 on success, negative errno on failure
 */
static int ap33772s_write_u8(struct ap33772s *dev, uint8_t reg, uint8_t value)
{
    return ap33772s_call_write(dev, reg, &value, 1);
}

/**
 * @brief Write 16-bit value to register (little-endian)
 * @param dev Device context
 * @param reg Register address
 * @param value Value to write
 * @return 0 on success, negative errno on failure
 */
static int ap33772s_write_u16(struct ap33772s *dev, uint8_t reg, uint16_t value)
{
    uint8_t buf[2] = { (uint8_t)(value & 0xFF), (uint8_t)(value >> 8) };
    return ap33772s_call_write(dev, reg, buf, sizeof(buf));
}

int ap33772s_get_power_capabilities(ap33772s_ref dev)
{
    if (!dev) {
        return -EINVAL;
    }

    dev->num_pdo = 0;
    dev->pps_index = -1;
    dev->avs_index = -1;
    dev->index_avs = -1;
    dev->voltage_avs_byte = 0;
    dev->current_avs_byte = 0;
    memset(dev->src_pdo, 0, sizeof(dev->src_pdo));

    uint8_t buf[AP33772S_SRCPDO_BYTE_LEN] = {0};
    int ret = ap33772s_call_read(dev, AP33772S_CMD_SRCPDO, buf, sizeof(buf));
    if (ret < 0) {
        return ret;
    }

    for (int i = 0; i < AP33772S_MAX_PDO_ENTRIES; ++i) {
        uint16_t raw = (uint16_t)buf[i * 2] | ((uint16_t)buf[i * 2 + 1] << 8);
        dev->src_pdo[i] = raw;
        if (ap33772s_pdo_is_valid(raw)) {
            dev->num_pdo = i + 1;
            if (ap33772s_pdo_is_programmable(raw)) {
                if (i < 7) {
                    if (dev->pps_index == -1) {
                        dev->pps_index = i + 1;  // First PPS PDO (SPR)
                    }
                } else if (dev->avs_index == -1) {
                    dev->avs_index = i + 1;  // First AVS PDO (EPR)
                }
            }
        }
    }

    return 0;
}

ap33772s_ref ap33772s_init(const struct ap33772s_bus_delegate *delegate)
{
    if (!delegate || !delegate->read || !delegate->write || !delegate->delay_us) {
        errno = EINVAL;
        return NULL;
    }

    struct ap33772s *dev = calloc(1, sizeof(*dev));
    if (!dev) {
        return NULL;
    }

    dev->pps_index = -1;
    dev->avs_index = -1;
    dev->index_avs = -1;
    dev->bus = *delegate;

    return dev;
}

void ap33772s_destroy(ap33772s_ref dev)
{
    free(dev);
}

int ap33772s_reset_device(ap33772s_ref dev)
{
    if (!dev) {
        return -EINVAL;
    }

    // Set configuration registers to documented power-on defaults (Table 19)
    static const struct {
        uint8_t reg;
        uint8_t value;
    } writes[] = {
        {AP33772S_CMD_MASK, 0x03},      // Interrupt enable mask (STARTED | READY)
        {AP33772S_CMD_OPMODE, 0x00},    // Operation mode
        {AP33772S_CMD_CONFIG, 0xF8},    // System configuration (protections enabled)
        {AP33772S_CMD_PDCONFIG, 0x03},  // PD mode configuration
        {AP33772S_CMD_VSELMIN, 0x19},   // Minimum selection voltage (5000mV)
        {AP33772S_CMD_UVPTHR, 0x01},    // UVP threshold (80%)
        {AP33772S_CMD_OVPTHR, 0x19},    // OVP threshold (2000mV)
        {AP33772S_CMD_OCPTHR, 0x00},    // OCP threshold
        {AP33772S_CMD_OTPTHR, 0x78},    // OTP threshold (120°C)
        {AP33772S_CMD_DRTHR, 0x78},     // De-rating threshold (120°C)
    };

    for (size_t i = 0; i < sizeof(writes) / sizeof(writes[0]); ++i) {
        int ret = ap33772s_write_u8(dev, writes[i].reg, writes[i].value);
        if (ret < 0) {
            return ret;
        }
    }

    // Disable output
    int ret = ap33772s_set_output(dev, false);
    if (ret < 0) {
        return ret;
    }

    // Set default NTC resistance values (Murata NCP03XH103)
    ret = ap33772s_set_ntc(dev, AP33772S_DEFAULT_TR25_OHM, AP33772S_DEFAULT_TR50_OHM,
                             AP33772S_DEFAULT_TR75_OHM, AP33772S_DEFAULT_TR100_OHM);
    if (ret < 0) {
        return ret;
    }

    // Wait up to 1 second for PDOs to be available
    for (int i = 0; i < 20; ++i) {
        uint8_t status = 0;
        int status_ret = ap33772s_read_status(dev, &status);
        if (status_ret == 0 && (status & AP33772S_MASK_NEWPDO)) {
            break;
        }
        dev->bus.delay_us(dev->bus.ctx, 50000);
    }

    return 0;
}



int ap33772s_set_output(ap33772s_ref dev, bool enable)
{
    if (!dev) {
        return -EINVAL;
    }
    // Use defined constants for SYSTEM register values
    uint8_t value = enable ? AP33772S_SYSTEM_OUTPUT_ENABLE : AP33772S_SYSTEM_OUTPUT_DISABLE;
    return ap33772s_write_u8(dev, AP33772S_CMD_SYSTEM, value);
}

static int ap33772s_send_request(struct ap33772s *dev, uint8_t pdo_index, uint8_t voltage_sel, uint8_t current_sel)
{
    // PD_REQMSG format: [PDO_INDEX:4][CURRENT_SEL:4][VOLTAGE_SEL:8]
    uint16_t payload = ((uint16_t)(pdo_index & 0x0F) << 12) |
                      ((uint16_t)(current_sel & 0x0F) << 8) |
                      voltage_sel;
    uint8_t bytes[2] = { (uint8_t)(payload & 0xFF), (uint8_t)(payload >> 8) };
    return dev->bus.write(dev->bus.ctx, AP33772S_CMD_PD_REQMSG, bytes, sizeof(bytes));
}

int ap33772s_request_fixed_pdo(ap33772s_ref dev, uint8_t pdo_index, int current_ma)
{
    if (!dev || pdo_index == 0 || pdo_index > AP33772S_MAX_PDO_ENTRIES) {
        return -EINVAL;
    }

    uint16_t raw = dev->src_pdo[pdo_index - 1];
    if (!ap33772s_pdo_is_valid(raw) || ap33772s_pdo_is_programmable(raw)) {
        return -EINVAL;  // Must be valid fixed PDO
    }

    int current_code = ap33772s_current_code_from_ma(current_ma);
    if (current_code < 0) {
        return current_code;
    }

    if (current_code > ap33772s_pdo_current_code(raw)) {
        return -ERANGE;  // Requested current exceeds PDO capability
    }

    // For fixed PDOs, voltage_sel = 0
    return ap33772s_send_request(dev, pdo_index, 0, (uint8_t)current_code);
}

int ap33772s_request_pps(ap33772s_ref dev, uint8_t pdo_index, int voltage_mv, int current_ma)
{
    if (!dev || pdo_index == 0 || pdo_index > AP33772S_MAX_PDO_ENTRIES) {
        return -EINVAL;
    }

    uint16_t raw = dev->src_pdo[pdo_index - 1];
    if (!ap33772s_pdo_is_valid(raw) || !ap33772s_pdo_is_programmable(raw) || pdo_index >= 8) {
        return -EINVAL;  // Must be valid PPS PDO (SPR, indices 1-7)
    }

    int current_code = ap33772s_current_code_from_ma(current_ma);
    if (current_code < 0) {
        return current_code;
    }
    if (current_code > ap33772s_pdo_current_code(raw)) {
        return -ERANGE;  // Current exceeds capability
    }

    int voltage_min = ap33772s_pps_voltage_min_mv(raw);
    int voltage_max = ap33772s_decode_voltage_max_mv(raw, false);  // SPR mode

    if (voltage_mv < voltage_min || voltage_mv > voltage_max) {
        return -ERANGE;  // Voltage out of range
    }

    // PPS voltage selection: voltage_mv / 100 (100mV units)
    uint8_t voltage_sel = (uint8_t)(voltage_mv / 100);
    return ap33772s_send_request(dev, pdo_index, voltage_sel, (uint8_t)current_code);
}

int ap33772s_request_avs(ap33772s_ref dev, uint8_t pdo_index, int voltage_mv, int current_ma)
{
    if (!dev || pdo_index == 0 || pdo_index > AP33772S_MAX_PDO_ENTRIES) {
        return -EINVAL;
    }

    uint16_t raw = dev->src_pdo[pdo_index - 1];
    if (!ap33772s_pdo_is_valid(raw) || !ap33772s_pdo_is_programmable(raw) || pdo_index < 8) {
        return -EINVAL;  // Must be valid AVS PDO (EPR, indices 8-13)
    }

    int current_code = ap33772s_current_code_from_ma(current_ma);
    if (current_code < 0) {
        return current_code;
    }
    if (current_code > ap33772s_pdo_current_code(raw)) {
        return -ERANGE;  // Current exceeds capability
    }

    int voltage_min = ap33772s_avs_voltage_min_mv(raw);
    int voltage_max = ap33772s_decode_voltage_max_mv(raw, true);  // EPR mode

    if (voltage_mv < voltage_min || voltage_mv > voltage_max) {
        return -ERANGE;  // Voltage out of range
    }

    // AVS voltage selection: voltage_mv / 200 (200mV units)
    uint8_t voltage_sel = (uint8_t)(voltage_mv / 200);
    int ret = ap33772s_send_request(dev, pdo_index, voltage_sel, (uint8_t)current_code);
    if (ret == 0) {
        // Cache the AVS request parameters for potential re-negotiation
        dev->index_avs = pdo_index;
        dev->voltage_avs_byte = voltage_sel;
        dev->current_avs_byte = current_code;
    }
    return ret;
}

int ap33772s_request_max_power(ap33772s_ref dev, uint8_t pdo_index)
{
    if (!dev || pdo_index == 0 || pdo_index > AP33772S_MAX_PDO_ENTRIES) {
        return -EINVAL;
    }

    uint16_t raw = dev->src_pdo[pdo_index - 1];
    if (!ap33772s_pdo_is_valid(raw)) {
        return -EINVAL;
    }

    return ap33772s_send_request(dev, pdo_index, 0xFF, 0x0F);
}

int ap33772s_read_status(ap33772s_ref dev, uint8_t *status)
{
    if (!dev || !status) {
        return -EINVAL;
    }
    uint8_t raw = 0;
    int ret = ap33772s_call_read(dev, AP33772S_CMD_STATUS, &raw, 1);
    if (ret < 0) {
        return ret;
    }
    *status = raw;
    return 0;
}

int ap33772s_read_opmode(ap33772s_ref dev, uint8_t *opmode)
{
    if (!dev || !opmode) {
        return -EINVAL;
    }
    uint8_t raw = 0;
    int ret = ap33772s_call_read(dev, AP33772S_CMD_OPMODE, &raw, 1);
    if (ret < 0) {
        return ret;
    }
    *opmode = raw;
    return 0;
}

int ap33772s_set_ntc(ap33772s_ref dev, int tr25, int tr50, int tr75, int tr100)
{
    if (!dev) {
        return -EINVAL;
    }

    // Device requires ~5 ms between successive NTC writes.
    int ret = ap33772s_write_u16(dev, AP33772S_CMD_TR25, (uint16_t)tr25);
    if (ret < 0) {
        return ret;
    }
    dev->bus.delay_us(dev->bus.ctx, 5000);

    ret = ap33772s_write_u16(dev, AP33772S_CMD_TR50, (uint16_t)tr50);
    if (ret < 0) {
        return ret;
    }
    dev->bus.delay_us(dev->bus.ctx, 5000);

    ret = ap33772s_write_u16(dev, AP33772S_CMD_TR75, (uint16_t)tr75);
    if (ret < 0) {
        return ret;
    }
    dev->bus.delay_us(dev->bus.ctx, 5000);

    ret = ap33772s_write_u16(dev, AP33772S_CMD_TR100, (uint16_t)tr100);
    if (ret < 0) {
        return ret;
    }
    return 0;
}

int ap33772s_read_temperature(ap33772s_ref dev, int *temperature_c)
{
    if (!dev || !temperature_c) {
        return -EINVAL;
    }
    uint8_t raw = 0;
    int ret = ap33772s_read_u8(dev, AP33772S_CMD_TEMP, &raw);
    if (ret < 0) {
        return ret;
    }
    *temperature_c = raw;  // Direct °C value
    return 0;
}

int ap33772s_read_voltage(ap33772s_ref dev, int *voltage_mv)
{
    if (!dev || !voltage_mv) {
        return -EINVAL;
    }
    uint16_t raw = 0;
    int ret = ap33772s_read_u16(dev, AP33772S_CMD_VOLTAGE, &raw);
    if (ret < 0) {
        return ret;
    }
    *voltage_mv = raw * 80;  // LSB = 80mV
    return 0;
}

int ap33772s_read_current(ap33772s_ref dev, int *current_ma)
{
    if (!dev || !current_ma) {
        return -EINVAL;
    }
    uint8_t raw = 0;
    int ret = ap33772s_read_u8(dev, AP33772S_CMD_CURRENT, &raw);
    if (ret < 0) {
        return ret;
    }
    *current_ma = raw * 24;  // LSB = 24mA
    return 0;
}

int ap33772s_read_vreq(ap33772s_ref dev, int *voltage_mv)
{
    if (!dev || !voltage_mv) {
        return -EINVAL;
    }
    uint8_t raw = 0;
    int ret = ap33772s_read_u8(dev, AP33772S_CMD_VREQ, &raw);
    if (ret < 0) {
        return ret;
    }
    *voltage_mv = raw * 50;  // LSB = 50mV
    return 0;
}

int ap33772s_read_ireq(ap33772s_ref dev, int *current_ma)
{
    if (!dev || !current_ma) {
        return -EINVAL;
    }
    uint8_t raw = 0;
    int ret = ap33772s_read_u8(dev, AP33772S_CMD_IREQ, &raw);
    if (ret < 0) {
        return ret;
    }
    *current_ma = raw * 10;  // LSB = 10mA
    return 0;
}

int ap33772s_read_vselmin(ap33772s_ref dev, int *voltage_mv)
{
    if (!dev || !voltage_mv) {
        return -EINVAL;
    }
    uint8_t raw = 0;
    int ret = ap33772s_read_u8(dev, AP33772S_CMD_VSELMIN, &raw);
    if (ret < 0) {
        return ret;
    }
    *voltage_mv = raw * 200;  // LSB = 200mV
    return 0;
}

int ap33772s_set_vselmin(ap33772s_ref dev, int voltage_mv)
{
    if (!dev || voltage_mv < 0 || voltage_mv % 200 != 0) {
        return -EINVAL;
    }
    uint8_t raw = (uint8_t)(voltage_mv / 200);
    return ap33772s_write_u8(dev, AP33772S_CMD_VSELMIN, raw);
}

int ap33772s_read_uvp_threshold(ap33772s_ref dev, int *percent)
{
    if (!dev || !percent) {
        return -EINVAL;
    }
    uint8_t raw = 0;
    int ret = ap33772s_read_u8(dev, AP33772S_CMD_UVPTHR, &raw);
    if (ret < 0) {
        return ret;
    }
    // Map register value to percentage
    switch (raw) {
        case 1:
            *percent = 80;
            break;
        case 2:
            *percent = 75;
            break;
        case 3:
            *percent = 70;
            break;
        default:
            *percent = -1;  // Invalid value
            break;
    }
    return 0;
}

int ap33772s_set_uvp_threshold(ap33772s_ref dev, int percent)
{
    if (!dev) {
        return -EINVAL;
    }
    uint8_t code = 0;
    switch (percent) {
        case 80:
            code = 1;
            break;
        case 75:
            code = 2;
            break;
        case 70:
            code = 3;
            break;
        default:
            return -EINVAL;
    }
    return ap33772s_write_u8(dev, AP33772S_CMD_UVPTHR, code);
}

int ap33772s_read_ovp_threshold(ap33772s_ref dev, int *millivolts)
{
    if (!dev || !millivolts) {
        return -EINVAL;
    }
    uint8_t raw = 0;
    int ret = ap33772s_read_u8(dev, AP33772S_CMD_OVPTHR, &raw);
    if (ret < 0) {
        return ret;
    }
    *millivolts = raw * 80;  // LSB = 80mV
    return 0;
}

int ap33772s_set_ovp_threshold(ap33772s_ref dev, int millivolts)
{
    if (!dev || millivolts < 0 || millivolts % 80 != 0) {
        return -EINVAL;
    }
    uint8_t raw = (uint8_t)(millivolts / 80);
    return ap33772s_write_u8(dev, AP33772S_CMD_OVPTHR, raw);
}

int ap33772s_read_ocp_threshold(ap33772s_ref dev, int *milliamps)
{
    if (!dev || !milliamps) {
        return -EINVAL;
    }
    uint8_t raw = 0;
    int ret = ap33772s_read_u8(dev, AP33772S_CMD_OCPTHR, &raw);
    if (ret < 0) {
        return ret;
    }
    *milliamps = raw * 50;  // LSB = 50mA
    return 0;
}

int ap33772s_set_ocp_threshold(ap33772s_ref dev, int milliamps)
{
    if (!dev || milliamps < 0 || milliamps % 50 != 0) {
        return -EINVAL;
    }
    uint8_t raw = (uint8_t)(milliamps / 50);
    return ap33772s_write_u8(dev, AP33772S_CMD_OCPTHR, raw);
}

int ap33772s_read_otp_threshold(ap33772s_ref dev, int *temp_c)
{
    if (!dev || !temp_c) {
        return -EINVAL;
    }
    uint8_t raw = 0;
    int ret = ap33772s_read_u8(dev, AP33772S_CMD_OTPTHR, &raw);
    if (ret < 0) {
        return ret;
    }
    *temp_c = raw;  // Direct °C value
    return 0;
}

int ap33772s_set_otp_threshold(ap33772s_ref dev, int temp_c)
{
    if (!dev || temp_c < 0 || temp_c > 255) {
        return -EINVAL;
    }
    return ap33772s_write_u8(dev, AP33772S_CMD_OTPTHR, (uint8_t)temp_c);
}

int ap33772s_read_dr_threshold(ap33772s_ref dev, int *temp_c)
{
    if (!dev || !temp_c) {
        return -EINVAL;
    }
    uint8_t raw = 0;
    int ret = ap33772s_read_u8(dev, AP33772S_CMD_DRTHR, &raw);
    if (ret < 0) {
        return ret;
    }
    *temp_c = raw;  // Direct °C value
    return 0;
}

int ap33772s_set_dr_threshold(ap33772s_ref dev, int temp_c)
{
    if (!dev || temp_c < 0 || temp_c > 255) {
        return -EINVAL;
    }
    return ap33772s_write_u8(dev, AP33772S_CMD_DRTHR, (uint8_t)temp_c);
}

int16_t ap33772s_get_num_pdo(ap33772s_ref dev)
{
    if (!dev) {
        return -EINVAL;
    }
    return dev->num_pdo;
}

int ap33772s_get_pps_index(ap33772s_ref dev)
{
    if (!dev) {
        return -EINVAL;
    }
    return dev->pps_index;
}

int ap33772s_get_avs_index(ap33772s_ref dev)
{
    if (!dev) {
        return -EINVAL;
    }
    return dev->avs_index;
}

static void ap33772s_print_profile(ap33772s_ref dev, int idx)
{
    struct ap33772s_pdo_info info;
    if (ap33772s_get_pdo_info(dev, idx + 1, &info) < 0 || !info.detected) {
        return;
    }

    const char *domain = info.is_epr ? "EPR" : "SPR";
    const char *type = NULL;
    switch (info.supply) {
    case AP33772S_PDO_SUPPLY_FIXED:
        type = "Fixed";
        break;
    case AP33772S_PDO_SUPPLY_PPS:
        type = "PPS";
        break;
    case AP33772S_PDO_SUPPLY_AVS:
        type = "AVS";
        break;
    default:
        type = "Unknown";
        break;
    }

    printf("PDO %d (%s %s): ", idx + 1, domain, type);

    if (info.supply == AP33772S_PDO_SUPPLY_FIXED) {
        printf("Voltage %d mV, Current %d-%d mA", info.voltage_max_mv, info.current_min_ma, info.current_max_ma);
        const char *peak = ap33772s_fixed_peak_current_desc(info.peak_current_code);
        if (peak && peak[0] != '\0') {
            printf(", Peak %s", peak);
        }
    } else {
        const char *min_desc = info.supply == AP33772S_PDO_SUPPLY_AVS ?
                               ap33772s_avs_voltage_min_desc(info.voltage_min_code) :
                               ap33772s_pps_voltage_min_desc(info.voltage_min_code);
        printf("Voltage %s to %d mV, Current %d-%d mA", min_desc, info.voltage_max_mv,
               info.current_min_ma, info.current_max_ma);
    }

    printf(" (raw=0x%04X)\n", info.raw);
}

void ap33772s_print_profiles(ap33772s_ref dev)
{
    if (!dev) {
        return;
    }
    for (int i = 0; i < dev->num_pdo && i < AP33772S_MAX_PDO_ENTRIES; ++i) {
        ap33772s_print_profile(dev, i);
    }
}

static int ap33772s_read_pdo_register(struct ap33772s *dev, int index, uint16_t *raw)
{
    if (!dev || !raw) {
        return -EINVAL;
    }
    if (index < 1 || index > AP33772S_MAX_PDO_ENTRIES) {
        return -ERANGE;
    }

    uint8_t reg = (index <= 7)
                    ? (uint8_t)(AP33772S_CMD_SRC_SPR_PDO1 + (index - 1))
                    : (uint8_t)(AP33772S_CMD_SRC_EPR_PDO8 + (index - 8));

    uint8_t buf[2] = {0};
    int ret = ap33772s_call_read(dev, reg, buf, sizeof(buf));
    if (ret < 0) {
        return ret;
    }

    *raw = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
    return 0;
}

int ap33772s_refresh_pdo(ap33772s_ref dev, int index)
{
    if (!dev) {
        return -EINVAL;
    }

    uint16_t raw = 0;
    int ret = ap33772s_read_pdo_register(dev, index, &raw);
    if (ret < 0) {
        return ret;
    }

    dev->src_pdo[index - 1] = raw;

    if (dev->index_avs == index && (!ap33772s_pdo_is_valid(raw) || !ap33772s_pdo_is_programmable(raw))) {
        dev->index_avs = -1;
        dev->voltage_avs_byte = 0;
        dev->current_avs_byte = 0;
    }

    int16_t num_pdo = 0;
    int pps_index = -1;
    int avs_index = -1;

    for (int i = 0; i < AP33772S_MAX_PDO_ENTRIES; ++i) {
        uint16_t value = dev->src_pdo[i];
        if (ap33772s_pdo_is_valid(value)) {
            num_pdo = (int16_t)(i + 1);
            if (ap33772s_pdo_is_programmable(value)) {
                if (i < 7) {
                    if (pps_index == -1) {
                        pps_index = i + 1;
                    }
                } else if (avs_index == -1) {
                    avs_index = i + 1;
                }
            }
        }
    }

    dev->num_pdo = num_pdo;
    dev->pps_index = pps_index;
    dev->avs_index = avs_index;

    return 0;
}

int ap33772s_get_pdo_info(ap33772s_ref dev, int index, struct ap33772s_pdo_info *info)
{
    if (!dev || !info) {
        return -EINVAL;
    }
    if (index < 1 || index > AP33772S_MAX_PDO_ENTRIES) {
        return -ERANGE;
    }

    memset(info, 0, sizeof(*info));

    uint16_t raw = dev->src_pdo[index - 1];
    info->raw = raw;
    info->detected = ap33772s_pdo_is_valid(raw);
    if (!info->detected) {
        return 0;
    }

    bool is_epr = index > 7;
    bool programmable = ap33772s_pdo_is_programmable(raw);

    info->is_epr = is_epr;
    info->supply = programmable ? (is_epr ? AP33772S_PDO_SUPPLY_AVS : AP33772S_PDO_SUPPLY_PPS)
                                : AP33772S_PDO_SUPPLY_FIXED;
    info->voltage_max_mv = ap33772s_decode_voltage_max_mv(raw, is_epr);
    info->current_code = ap33772s_pdo_current_code(raw);
    ap33772s_current_range_from_code(info->current_code, &info->current_min_ma, &info->current_max_ma);

    if (info->supply == AP33772S_PDO_SUPPLY_FIXED) {
        info->voltage_min_mv = info->voltage_max_mv;
        info->peak_current_code = ap33772s_pdo_voltage_field(raw);
        info->voltage_min_code = 0;
    } else {
        info->voltage_min_code = ap33772s_pdo_voltage_field(raw);
        info->peak_current_code = 0;
        info->voltage_min_mv = info->supply == AP33772S_PDO_SUPPLY_AVS ?
                               ap33772s_avs_voltage_min_mv(raw) :
                               ap33772s_pps_voltage_min_mv(raw);
        if (info->voltage_min_mv == 0) {
            info->voltage_min_mv = -1;
        }
    }

    return 0;
}

const char *ap33772s_pps_voltage_min_desc(uint8_t code)
{
    switch (code & 0x3) {
    case 0:
        return "reserved";
    case 1:
        return ">=3300 mV";
    case 2:
        return ">3300 mV and <=5000 mV";
    case 3:
        return "reserved";
    default:
        return "reserved";
    }
}

const char *ap33772s_avs_voltage_min_desc(uint8_t code)
{
    switch (code & 0x3) {
    case 0:
        return "reserved";
    case 1:
        return ">=15000 mV";
    case 2:
        return ">15000 mV and <=20000 mV";
    case 3:
        return "reserved";
    default:
        return "reserved";
    }
}

const char *ap33772s_fixed_peak_current_desc(uint8_t code)
{
    switch (code & 0x3) {
    case 0:
        return "not specified";
    case 1:
        return "150% peak";
    case 2:
        return "200% peak";
    case 3:
        return "300% peak";
    default:
        return "unknown";
    }
}

int ap33772s_read_gpio(ap33772s_ref dev, uint8_t *value)
{
    if (!dev || !value) {
        return -EINVAL;
        }
    return ap33772s_read_u8(dev, AP33772S_CMD_GPIO, value);
}

int ap33772s_write_gpio(ap33772s_ref dev, uint8_t value)
{
    if (!dev) {
        return -EINVAL;
    }
    return ap33772s_write_u8(dev, AP33772S_CMD_GPIO, value);
}

int ap33772s_set_gpio_pullup(ap33772s_ref dev, bool enable)
{
    if (!dev) {
        return -EINVAL;
    }
    uint8_t current = 0;
    int ret = ap33772s_read_gpio(dev, &current);
    if (ret < 0) {
        return ret;
    }
    if (enable) {
        current |= AP33772S_GPIO_PU_EN;
    } else {
        current &= (uint8_t)~AP33772S_GPIO_PU_EN;
    }
    return ap33772s_write_gpio(dev, current);
}

int ap33772s_set_gpio_pulldown(ap33772s_ref dev, bool enable)
{
    if (!dev) {
        return -EINVAL;
    }
    uint8_t current = 0;
    int ret = ap33772s_read_gpio(dev, &current);
    if (ret < 0) {
        return ret;
    }
    if (enable) {
        current |= AP33772S_GPIO_PD_EN;
    } else {
        current &= (uint8_t)~AP33772S_GPIO_PD_EN;
    }
    return ap33772s_write_gpio(dev, current);
}

int ap33772s_set_gpio_input_enable(ap33772s_ref dev, bool enable)
{
    if (!dev) {
        return -EINVAL;
    }
    uint8_t current = 0;
    int ret = ap33772s_read_gpio(dev, &current);
    if (ret < 0) {
        return ret;
    }
    if (enable) {
        current |= AP33772S_GPIO_IE;
    } else {
        current &= (uint8_t)~AP33772S_GPIO_IE;
    }
    return ap33772s_write_gpio(dev, current);
}

int ap33772s_set_gpio_output(ap33772s_ref dev, bool high)
{
    if (!dev) {
        return -EINVAL;
    }
    uint8_t current = 0;
    int ret = ap33772s_read_gpio(dev, &current);
    if (ret < 0) {
        return ret;
    }
    if (high) {
        current |= AP33772S_GPIO_DO;
    } else {
        current &= (uint8_t)~AP33772S_GPIO_DO;
    }
    return ap33772s_write_gpio(dev, current);
}

int ap33772s_set_gpio_output_enable(ap33772s_ref dev, bool enable)
{
    if (!dev) {
        return -EINVAL;
    }
    uint8_t current = 0;
    int ret = ap33772s_read_gpio(dev, &current);
    if (ret < 0) {
        return ret;
    }
    if (enable) {
        current |= AP33772S_GPIO_OE;
    } else {
        current &= (uint8_t)~AP33772S_GPIO_OE;
    }
    return ap33772s_write_gpio(dev, current);
}

int ap33772s_read_gpio_input(ap33772s_ref dev, bool *value)
{
    if (!dev || !value) {
        return -EINVAL;
    }
    uint8_t current = 0;
    int ret = ap33772s_read_gpio(dev, &current);
    if (ret < 0) {
        return ret;
    }
    *value = (current & AP33772S_GPIO_DI) != 0;
    return 0;
}

int ap33772s_read_config(ap33772s_ref dev, uint8_t *config)
{
    if (!dev || !config) {
        return -EINVAL;
    }
    return ap33772s_read_u8(dev, AP33772S_CMD_CONFIG, config);
}

int ap33772s_write_config(ap33772s_ref dev, uint8_t config)
{
    if (!dev) {
        return -EINVAL;
    }
    return ap33772s_write_u8(dev, AP33772S_CMD_CONFIG, config);
}

int ap33772s_read_pdconfig(ap33772s_ref dev, uint8_t *config)
{
    if (!dev || !config) {
        return -EINVAL;
    }
    return ap33772s_read_u8(dev, AP33772S_CMD_PDCONFIG, config);
}

int ap33772s_write_pdconfig(ap33772s_ref dev, uint8_t config)
{
    if (!dev) {
        return -EINVAL;
    }
    return ap33772s_write_u8(dev, AP33772S_CMD_PDCONFIG, config);
}

int ap33772s_read_pd_msg_result(ap33772s_ref dev, uint8_t *result)
{
    if (!dev || !result) {
        return -EINVAL;
    }
    return ap33772s_read_u8(dev, AP33772S_CMD_PD_MSGRLT, result);
}

int ap33772s_set_data_role_swap_accept(ap33772s_ref dev, bool accept)
{
    if (!dev) {
        return -EINVAL;
    }
    uint8_t current = 0;
    int ret = ap33772s_read_pdconfig(dev, &current);
    if (ret < 0) {
        return ret;
    }
    if (accept) {
        current |= AP33772S_PDCONFIG_DRSWP_EN;
    } else {
        current &= (uint8_t)~AP33772S_PDCONFIG_DRSWP_EN;
    }
    return ap33772s_write_pdconfig(dev, current);
}

int ap33772s_read_data_role(ap33772s_ref dev, bool *is_dfp)
{
    if (!dev || !is_dfp) {
        return -EINVAL;
    }
    uint8_t opmode = 0;
    int ret = ap33772s_read_u8(dev, AP33772S_CMD_OPMODE, &opmode);
    if (ret < 0) {
        return ret;
    }
    *is_dfp = (opmode & AP33772S_OPMODE_DATARL) != 0;  // DATARL bit
    return 0;
}

int ap33772s_send_data_role_swap(ap33772s_ref dev)
{
    if (!dev) {
        return -EINVAL;
    }
    return ap33772s_write_u8(dev, AP33772S_CMD_PD_CMDMSG, AP33772S_PD_CMDMSG_DRSWP);
}

int ap33772s_send_hard_reset(ap33772s_ref dev)
{
    if (!dev) {
        return -EINVAL;
    }
    return ap33772s_write_u8(dev, AP33772S_CMD_PD_CMDMSG, AP33772S_PD_CMDMSG_HRST);
}
