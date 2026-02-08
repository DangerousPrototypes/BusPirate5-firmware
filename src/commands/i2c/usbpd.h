/**
 * @file usbpd.h
 * @brief USB Power Delivery protocol command interface.
 * @details Provides command to interact with USB-PD controllers.
 */

/**
 * @brief Handler for USB Power Delivery command.
 * @param res  Command result structure
 */
void usbpd_handler(struct command_result* res);