/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file ap33772s.h
 * @brief Header file for AP33772S I2C USB PD3.1 EPR SINK CONTROLLER driver
 *
 * This driver provides an interface to control and monitor the AP33772S USB Power Delivery
 * sink controller via I2C. It supports PD3.1 with EPR/AVS up to 28V and SPR/PPS up to 21V.
 *
 * Reference: AP33772S Datasheet DS 46176 Rev. 7 - 2
 */

#ifndef AP33772S_H
#define AP33772S_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** I2C slave address of the AP33772S controller */
#define AP33772S_ADDRESS 0x52

/** Maximum number of Power Data Objects (PDOs) supported (7 SPR + 6 EPR) */
#define AP33772S_MAX_PDO_ENTRIES 13

/** Buffer length for I2C read operations */
#define AP33772S_READ_BUFFER_LENGTH 128

/** Buffer length for I2C write operations */
#define AP33772S_WRITE_BUFFER_LENGTH 6

/** Byte length of all source PDO data (13 PDOs * 2 bytes each) */
#define AP33772S_SRCPDO_BYTE_LEN (AP33772S_MAX_PDO_ENTRIES * 2)

/** CONFIG register bit definitions */
#define AP33772S_CONFIG_DR_EN   (1u << 7)
#define AP33772S_CONFIG_OTP_EN  (1u << 6)
#define AP33772S_CONFIG_OCP_EN  (1u << 5)
#define AP33772S_CONFIG_OVP_EN  (1u << 4)
#define AP33772S_CONFIG_UVP_EN  (1u << 3)

/** PDO supply types reported by the AP33772S */
enum ap33772s_pdo_supply {
    AP33772S_PDO_SUPPLY_FIXED = 0,
    AP33772S_PDO_SUPPLY_PPS,
    AP33772S_PDO_SUPPLY_AVS,
};

/** Parsed representation of a single PDO entry */
struct ap33772s_pdo_info {
    bool detected;              /**< PDO register populated */
    bool is_epr;                /**< Source belongs to EPR domain */
    enum ap33772s_pdo_supply supply; /**< Reported supply type */
    uint16_t raw;               /**< Raw register value */
    int voltage_max_mv;         /**< Maximum voltage in millivolts */
    int voltage_min_mv;         /**< Minimum voltage in millivolts (-1 if not applicable) */
    uint8_t voltage_min_code;   /**< Encoded minimum voltage field */
    uint8_t peak_current_code;  /**< Encoded peak current field (fixed supplies only) */
    uint8_t current_code;       /**< Encoded current field */
    int current_min_ma;         /**< Minimum current supported (mA) */
    int current_max_ma;         /**< Maximum current supported (mA) */
};

/**
 * @enum ap33772s_mask
 * @brief Bit masks for STATUS register flags (Table 12)
 *
 * These correspond to the interrupt mask bits in the MASK register.
 * When set, the corresponding interrupt is enabled.
 */
enum ap33772s_mask {
    AP33772S_MASK_STARTED = 1 << 0,  /**< System started, ready for configuration */
    AP33772S_MASK_READY   = 1 << 1,  /**< Ready to receive I2C requests/commands */
    AP33772S_MASK_NEWPDO  = 1 << 2,  /**< New source PDOs received */
    AP33772S_MASK_UVP     = 1 << 3,  /**< Undervoltage protection triggered */
    AP33772S_MASK_OVP     = 1 << 4,  /**< Overvoltage protection triggered */
    AP33772S_MASK_OCP     = 1 << 5,  /**< Overcurrent protection triggered */
    AP33772S_MASK_OTP     = 1 << 6   /**< Overtemperature protection triggered */
};

struct ap33772s;

/** Opaque reference to an AP33772S device instance */
typedef struct ap33772s *ap33772s_ref;

struct ap33772s_bus_delegate {
    int (*read)(void *ctx, uint8_t reg, uint8_t *data, size_t len);
    int (*write)(void *ctx, uint8_t reg, const uint8_t *data, size_t len);
    void (*delay_us)(void *ctx, unsigned int usec);
    void *ctx;
};


/**
 * @brief Initialize an AP33772S device context
 * @param delegate Bus delegate providing I2C callbacks, delay hook, and context
 * @return Device reference on success, NULL on failure (errno set)
 */
ap33772s_ref ap33772s_init(const struct ap33772s_bus_delegate *delegate);

/**
 * @brief Destroy an AP33772S device context
 * @param dev Device context to destroy (NULL safe)
 */
void ap33772s_destroy(ap33772s_ref dev);

/**
 * @brief Perform a raw register read via the configured bus delegate
 * @param dev Device context
 * @param reg Register address
 * @param data Buffer to store read data
 * @param len Number of bytes to read (must be >0)
 * @return 0 on success, negative errno on failure
 */
int ap33772s_read_bytes(ap33772s_ref dev, uint8_t reg, uint8_t *data, size_t len);

/**
 * @brief Perform a raw register write via the configured bus delegate
 * @param dev Device context
 * @param reg Register address
 * @param data Data to write (may be NULL when len == 0)
 * @param len Number of bytes to write (max AP33772S_WRITE_BUFFER_LENGTH)
 * @return 0 on success, negative errno on failure
 */
int ap33772s_write_bytes(ap33772s_ref dev, uint8_t reg, const uint8_t *data, size_t len);

/**
 * @brief Reset device to default state and configure NTC
 * @param dev Device context
 * @return 0 on success, negative errno on failure
 */
int ap33772s_reset_device(ap33772s_ref dev);

/**
 * @brief Refresh source PDO capabilities from device
 * @param dev Device context
 * @return 0 on success, negative errno on failure
 */
int ap33772s_get_power_capabilities(ap33772s_ref dev);

/**
 * @brief Enable or disable VBUS output via NMOS switch
 * @param dev Device context
 * @param enable true to enable output, false to disable
 * @return 0 on success, negative errno on failure
 */
int ap33772s_set_output(ap33772s_ref dev, bool enable);

/**
 * @brief Request a fixed PDO with specified current
 * @param dev Device context
 * @param pdo_index PDO index (1-13)
 * @param current_ma Requested current in mA
 * @return 0 on success, negative errno on failure
 */
int ap33772s_request_fixed_pdo(ap33772s_ref dev, uint8_t pdo_index, int current_ma);

/**
 * @brief Request a PPS PDO with specified voltage and current
 * @param dev Device context
 * @param pdo_index PDO index (1-7 for SPR)
 * @param voltage_mv Requested voltage in mV
 * @param current_ma Requested current in mA
 * @return 0 on success, negative errno on failure
 */
int ap33772s_request_pps(ap33772s_ref dev, uint8_t pdo_index, int voltage_mv, int current_ma);

/**
 * @brief Request an AVS PDO with specified voltage and current
 * @param dev Device context
 * @param pdo_index PDO index (8-13 for EPR)
 * @param voltage_mv Requested voltage in mV
 * @param current_ma Requested current in mA
 * @return 0 on success, negative errno on failure
 */
int ap33772s_request_avs(ap33772s_ref dev, uint8_t pdo_index, int voltage_mv, int current_ma);

/**
 * @brief Read device status register
 * @param dev Device context
 * @param status Pointer to store status byte
 * @return 0 on success, negative errno on failure
 */
int ap33772s_read_status(ap33772s_ref dev, uint8_t *status);

/**
 * @brief Read operation mode register
 * @param dev Device context
 * @param opmode Pointer to store raw OPMODE byte
 * @return 0 on success, negative errno on failure
 */
int ap33772s_read_opmode(ap33772s_ref dev, uint8_t *opmode);

/**
 * @brief Request maximum current and voltage for the selected PDO
 * @param dev Device context
 * @param pdo_index PDO index (1-13)
 * @return 0 on success, negative errno on failure
 */
int ap33772s_request_max_power(ap33772s_ref dev, uint8_t pdo_index);

/**
 * @brief Refresh a single PDO entry from device registers
 * @param dev Device context
 * @param index PDO index (1-13)
 * @return 0 on success, negative errno on failure
 */
int ap33772s_refresh_pdo(ap33772s_ref dev, int index);

/**
 * @brief Configure NTC thermistor resistance values for temperature estimation
 * @param dev Device context
 * @param tr25 Resistance at 25°C in ohms
 * @param tr50 Resistance at 50°C in ohms
 * @param tr75 Resistance at 75°C in ohms
 * @param tr100 Resistance at 100°C in ohms
 * @return 0 on success, negative errno on failure
 */
int ap33772s_set_ntc(ap33772s_ref dev, int tr25, int tr50, int tr75, int tr100);

/**
 * @brief Read current temperature from NTC
 * @param dev Device context
 * @param temperature_c Pointer to store temperature in °C
 * @return 0 on success, negative errno on failure
 */
int ap33772s_read_temperature(ap33772s_ref dev, int *temperature_c);

/**
 * @brief Read current VBUS output voltage
 * @param dev Device context
 * @param voltage_mv Pointer to store voltage in mV
 * @return 0 on success, negative errno on failure
 */
int ap33772s_read_voltage(ap33772s_ref dev, int *voltage_mv);

/**
 * @brief Read current VBUS output current
 * @param dev Device context
 * @param current_ma Pointer to store current in mA
 * @return 0 on success, negative errno on failure
 */
int ap33772s_read_current(ap33772s_ref dev, int *current_ma);

/**
 * @brief Read last negotiated voltage
 * @param dev Device context
 * @param voltage_mv Pointer to store voltage in mV
 * @return 0 on success, negative errno on failure
 */
int ap33772s_read_vreq(ap33772s_ref dev, int *voltage_mv);

/**
 * @brief Read last negotiated current
 * @param dev Device context
 * @param current_ma Pointer to store current in mA
 * @return 0 on success, negative errno on failure
 */
int ap33772s_read_ireq(ap33772s_ref dev, int *current_ma);

/**
 * @brief Read minimum voltage selection threshold
 * @param dev Device context
 * @param voltage_mv Pointer to store voltage in mV
 * @return 0 on success, negative errno on failure
 */
int ap33772s_read_vselmin(ap33772s_ref dev, int *voltage_mv);

/**
 * @brief Set minimum voltage selection threshold
 * @param dev Device context
 * @param voltage_mv Voltage threshold in mV (must be multiple of 200)
 * @return 0 on success, negative errno on failure
 */
int ap33772s_set_vselmin(ap33772s_ref dev, int voltage_mv);

/**
 * @brief Read UVP threshold percentage
 * @param dev Device context
 * @param percent Pointer to store percentage (70, 75, or 80)
 * @return 0 on success, negative errno on failure
 */
int ap33772s_read_uvp_threshold(ap33772s_ref dev, int *percent);

/**
 * @brief Set UVP threshold percentage
 * @param dev Device context
 * @param percent Threshold percentage (70, 75, or 80)
 * @return 0 on success, negative errno on failure
 */
int ap33772s_set_uvp_threshold(ap33772s_ref dev, int percent);

/**
 * @brief Read OVP threshold offset
 * @param dev Device context
 * @param millivolts Pointer to store offset in mV
 * @return 0 on success, negative errno on failure
 */
int ap33772s_read_ovp_threshold(ap33772s_ref dev, int *millivolts);

/**
 * @brief Set OVP threshold offset
 * @param dev Device context
 * @param millivolts Offset in mV (must be multiple of 80)
 * @return 0 on success, negative errno on failure
 */
int ap33772s_set_ovp_threshold(ap33772s_ref dev, int millivolts);

/**
 * @brief Read OCP threshold
 * @param dev Device context
 * @param milliamps Pointer to store current in mA
 * @return 0 on success, negative errno on failure
 */
int ap33772s_read_ocp_threshold(ap33772s_ref dev, int *milliamps);

/**
 * @brief Set OCP threshold
 * @param dev Device context
 * @param milliamps Current threshold in mA (must be multiple of 50)
 * @return 0 on success, negative errno on failure
 */
int ap33772s_set_ocp_threshold(ap33772s_ref dev, int milliamps);

/**
 * @brief Read OTP threshold temperature
 * @param dev Device context
 * @param temp_c Pointer to store temperature in °C
 * @return 0 on success, negative errno on failure
 */
int ap33772s_read_otp_threshold(ap33772s_ref dev, int *temp_c);

/**
 * @brief Set OTP threshold temperature
 * @param dev Device context
 * @param temp_c Temperature threshold in °C
 * @return 0 on success, negative errno on failure
 */
int ap33772s_set_otp_threshold(ap33772s_ref dev, int temp_c);

/**
 * @brief Read de-rating threshold temperature
 * @param dev Device context
 * @param temp_c Pointer to store temperature in °C
 * @return 0 on success, negative errno on failure
 */
int ap33772s_read_dr_threshold(ap33772s_ref dev, int *temp_c);

/**
 * @brief Set de-rating threshold temperature
 * @param dev Device context
 * @param temp_c Temperature threshold in °C
 * @return 0 on success, negative errno on failure
 */
int ap33772s_set_dr_threshold(ap33772s_ref dev, int temp_c);

/**
 * @brief Get number of valid PDOs
 * @param dev Device context
 * @return Number of PDOs, or negative errno on error
 */
int16_t ap33772s_get_num_pdo(ap33772s_ref dev);

/**
 * @brief Get index of first PPS PDO
 * @param dev Device context
 * @return PDO index (1-based), -1 if none, or negative errno on error
 */
int ap33772s_get_pps_index(ap33772s_ref dev);

/**
 * @brief Get index of first AVS PDO
 * @param dev Device context
 * @return PDO index (1-based), -1 if none, or negative errno on error
 */
int ap33772s_get_avs_index(ap33772s_ref dev);

/**
 * @brief Print all available PDO profiles to stdout
 * @param dev Device context
 */
void ap33772s_print_profiles(ap33772s_ref dev);

/**
 * @brief Read GPIO register value
 * @param dev Device context
 * @param value Pointer to store GPIO register value
 * @return 0 on success, negative errno on failure
 *
 * Note: GPIO functionality is only available on AP33772SDKZ-13-FA02 variant
 */
int ap33772s_read_gpio(ap33772s_ref dev, uint8_t *value);

/**
 * @brief Write GPIO register value
 * @param dev Device context
 * @param value GPIO register value to write
 * @return 0 on success, negative errno on failure
 *
 * Note: GPIO functionality is only available on AP33772SDKZ-13-FA02 variant
 */
int ap33772s_write_gpio(ap33772s_ref dev, uint8_t value);

/**
 * @brief Configure GPIO pull-up resistor
 * @param dev Device context
 * @param enable true to enable pull-up, false to disable
 * @return 0 on success, negative errno on failure
 */
int ap33772s_set_gpio_pullup(ap33772s_ref dev, bool enable);

/**
 * @brief Configure GPIO pull-down resistor
 * @param dev Device context
 * @param enable true to enable pull-down, false to disable
 * @return 0 on success, negative errno on failure
 */
int ap33772s_set_gpio_pulldown(ap33772s_ref dev, bool enable);

/**
 * @brief Enable GPIO input mode
 * @param dev Device context
 * @param enable true to enable input, false to disable
 * @return 0 on success, negative errno on failure
 */
int ap33772s_set_gpio_input_enable(ap33772s_ref dev, bool enable);

/**
 * @brief Set GPIO output value
 * @param dev Device context
 * @param high true for high output, false for low output
 * @return 0 on success, negative errno on failure
 */
int ap33772s_set_gpio_output(ap33772s_ref dev, bool high);

/**
 * @brief Enable GPIO output mode
 * @param dev Device context
 * @param enable true to enable output mode, false for input mode
 * @return 0 on success, negative errno on failure
 */
int ap33772s_set_gpio_output_enable(ap33772s_ref dev, bool enable);

/**
 * @brief Read GPIO input value
 * @param dev Device context
 * @param value Pointer to store input value (true for high, false for low)
 * @return 0 on success, negative errno on failure
 */
int ap33772s_read_gpio_input(ap33772s_ref dev, bool *value);

/**
 * @brief Read protection configuration register
 * @param dev Device context
 * @param config Pointer to store config byte
 * @return 0 on success, negative errno on failure
 */
int ap33772s_read_config(ap33772s_ref dev, uint8_t *config);

/**
 * @brief Write protection configuration register
 * @param dev Device context
 * @param config Config byte to write
 * @return 0 on success, negative errno on failure
 */
int ap33772s_write_config(ap33772s_ref dev, uint8_t config);

/**
 * @brief Read PD configuration register
 * @param dev Device context
 * @param config Pointer to store config byte
 * @return 0 on success, negative errno on failure
 */
int ap33772s_read_pdconfig(ap33772s_ref dev, uint8_t *config);

/**
 * @brief Write PD configuration register
 * @param dev Device context
 * @param config Config byte to write
 * @return 0 on success, negative errno on failure
 */
int ap33772s_write_pdconfig(ap33772s_ref dev, uint8_t config);

/**
 * @brief Decode PDO entry into structured information
 * @param dev Device context
 * @param index PDO index (1-13)
 * @param info Output structure
 * @return 0 on success, negative errno on failure
 */
int ap33772s_get_pdo_info(ap33772s_ref dev, int index, struct ap33772s_pdo_info *info);

/**
 * @brief Describe PPS voltage minimum code
 * @param code Encoded 2-bit field
 * @return Descriptive string, never NULL
 */
const char *ap33772s_pps_voltage_min_desc(uint8_t code);

/**
 * @brief Describe AVS voltage minimum code
 * @param code Encoded 2-bit field
 * @return Descriptive string, never NULL
 */
const char *ap33772s_avs_voltage_min_desc(uint8_t code);

/**
 * @brief Describe fixed supply peak current capability code
 * @param code Encoded 2-bit field
 * @return Descriptive string, never NULL
 */
const char *ap33772s_fixed_peak_current_desc(uint8_t code);

/**
 * @brief Read PD message result register
 * @param dev Device context
 * @param result Pointer to store result byte
 * @return 0 on success, negative errno on failure
 */
int ap33772s_read_pd_msg_result(ap33772s_ref dev, uint8_t *result);

/**
 * @brief Enable/disable Data Role Swap acceptance
 * @param dev Device context
 * @param accept true to accept DRS, false to reject
 * @return 0 on success, negative errno on failure
 */
int ap33772s_set_data_role_swap_accept(ap33772s_ref dev, bool accept);

/**
 * @brief Read current Data Role from OPMODE register
 * @param dev Device context
 * @param is_dfp Pointer to store true if DFP (host), false if UFP (device)
 * @return 0 on success, negative errno on failure
 */
int ap33772s_read_data_role(ap33772s_ref dev, bool *is_dfp);

/**
 * @brief Send Data Role Swap command
 * @param dev Device context
 * @return 0 on success, negative errno on failure
 */
int ap33772s_send_data_role_swap(ap33772s_ref dev);

/**
 * @brief Send Hard Reset command
 * @param dev Device context
 * @return 0 on success, negative errno on failure
 */
int ap33772s_send_hard_reset(ap33772s_ref dev);

#endif
