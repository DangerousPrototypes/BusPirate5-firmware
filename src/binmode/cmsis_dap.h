/**
 * @file cmsis_dap.h
 * @brief CMSIS-DAP v2 debug probe binary mode.
 * @details Exposes SWD/JTAG on the IO header through a vendor bulk USB interface,
 *          usable with OpenOCD, pyOCD, probe-rs and other CMSIS-DAP hosts.
 */

#ifndef CMSIS_DAP_H
#define CMSIS_DAP_H

extern const char cmsis_dap_name[];

/**
 * @brief Claim the debug pins and start answering CMSIS-DAP requests.
 */
void cmsis_dap_setup(void);

/**
 * @brief Display the pinout and host configuration hints.
 */
void cmsis_dap_setup_message(void);

/**
 * @brief Execute pending CMSIS-DAP requests (core0).
 */
void cmsis_dap_service(void);

/**
 * @brief Release the debug pins.
 */
void cmsis_dap_cleanup(void);

/**
 * @brief Move CMSIS-DAP packets between USB and the request/response queues (core1).
 */
void cmsis_dap_usb_task(void);

#endif // CMSIS_DAP_H
