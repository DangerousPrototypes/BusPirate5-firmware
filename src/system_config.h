/**
 * @file system_config.h
 * @brief System-wide configuration structure and initialization
 * 
 * This file defines the global system configuration structure that holds
 * all runtime settings for the Bus Pirate including terminal settings,
 * LED effects, storage configuration, mode settings, and pin assignments.
 * 
 * @author Bus Pirate Project
 * @date 2024-2026
 */

#pragma once

#include "./pirate/rgb.h" // for `led_effect_t` enumeration

/**
 * @brief PWM (Pulse Width Modulation) configuration structure
 */
typedef struct _pwm_config {
    float period;      /**< PWM period in seconds */
    float dutycycle;   /**< Duty cycle as a fraction (0.0 to 1.0) */
} _pwm_config;

/**
 * @brief Main system configuration structure
 * 
 * This structure contains all global system settings including terminal configuration,
 * LED effects, hardware settings, protocol mode configuration, and pin assignments.
 * 
 * @note Most settings are uint32_t to standardize JSON parsing
 * @todo Separate persisted settings into a dedicated struct
 */
typedef struct _system_config {
    /** @name Configuration Loading */
    /**@{*/
    bool config_loaded_from_file;                /**< True if configuration was loaded from storage */
    uint32_t disable_unique_usb_serial_number;   /**< Disable unique USB serial number (for manufacturing) */
    /**@}*/

    /** @name Hardware Information */
    /**@{*/
    uint8_t hardware_revision;                   /**< Hardware revision number */
    /**@}*/

    /** @name Terminal Settings */
    /**@{*/
    uint32_t terminal_language;                  /**< UI language selection */
    uint32_t terminal_usb_enable;                /**< Enable USB CDC terminal */
    uint32_t terminal_uart_enable;               /**< Enable UART terminal on IO pins */
    uint32_t terminal_uart_number;               /**< UART number to use for terminal (0 or 1) */
    /**@}*/

    /** @name Debug Settings */
    /**@{*/
    uint32_t debug_uart_enable;                  /**< Initialize UART for developer debugging */
    uint32_t debug_uart_number;                  /**< UART number for debugging (0 or 1) */
    /**@}*/

    /** @name LCD Settings */
    /**@{*/
    uint32_t lcd_screensaver_active;             /**< LCD screensaver currently active */
    uint32_t lcd_timeout;                        /**< LCD screensaver timeout in seconds */
    /**@}*/

    /** @name LED Settings */
    /**@{*/
    uint32_t led_color;                          /**< RGB color value (0xRRGGBB) */
    uint32_t led_brightness_divisor;             /**< Brightness divisor (10=10%, 5=20%, etc) */
    union {
        uint32_t led_effect_as_uint32;           /**< LED effect as uint32 for JSON parsing */
        led_effect_t led_effect;                 /**< LED effect enumeration */
    };
    /**@}*/

    /** @name Terminal Display Settings */
    /**@{*/
    uint8_t terminal_ansi_rows;                  /**< Terminal rows for ANSI display */
    uint8_t terminal_ansi_columns;               /**< Terminal columns for ANSI display */
    uint32_t terminal_ansi_color;                /**< ANSI color scheme */
    uint32_t terminal_ansi_statusbar;            /**< Status bar display mode */
    bool terminal_ansi_statusbar_update;         /**< Status bar needs update */
    bool terminal_hide_cursor;                   /**< Hide terminal cursor */
    bool terminal_ansi_statusbar_pause;          /**< Status bar updates paused */
    uint8_t terminal_update;                     /**< Terminal update flags */
    /**@}*/

    /** @name Storage Settings */
    /**@{*/
    uint8_t storage_available;                   /**< Storage device available */
    uint8_t storage_mount_error;                 /**< Storage mount error code */
    uint8_t storage_fat_type;                    /**< FAT filesystem type (12/16/32) */
    float storage_size;

    uint32_t display_format; // display format (dec, hex, oct, bin)

    uint8_t hiz;                  // is hiz pin mode?
    uint8_t mode;                 // which mode we are in?
    uint8_t display;              // which display mode we are in?
    const char* subprotocol_name; // can be set if there is a sub protocol

    uint8_t num_bits;        // number of used bits
    uint8_t bit_order;       // bitorder (0=msb, 1=lsb)
    uint8_t write_with_read; // write with read
    uint8_t open_drain;      // open drain pin mode (1=true)
    uint8_t pullup_enabled; // pullup enabled? (0=off, 1=on)

    const char* pin_labels[HW_PINS]; // pointer to labels for each pin on the bus pirate header
    enum bp_pin_func pin_func[HW_PINS];
    uint32_t pin_changed;
    bool info_bar_changed;

    uint8_t mode_active;
    uint8_t pwm_active;                    // pwm active, one bit per PWN channel/pin
    _pwm_config freq_config[BIO_MAX_PINS]; // holds PWM or FREQ settings for easier display later
    uint8_t freq_active;                   // freq measure active, one bit per channel/pin
    uint8_t aux_active;                    // user controlled aux pins are outputs, resets when used as inputs
    #if 0
    uint8_t psu; // psu (0=off, 1=on)
    uint32_t psu_voltage;   // psu voltage output setting in decimal * 10000
    bool psu_current_limit_en;
    uint32_t psu_current_limit; // psu current limit in decimal * 10000
    bool psu_current_error;     // psu over current limit fuse tripped
    bool psu_undervoltage_error;
    bool psu_error;             // error, usually with the dac
    bool psu_irq_en;
    #endif

    uint8_t error; // error occurred

    uint32_t big_buffer_owner;

    bool rts;

    bool binmode_usb_rx_queue_enable; // enable the binmode RX queue, disable to handle USB directly with tinyusb
                                      // functions
    bool binmode_usb_tx_queue_enable; // enable the binmode TX queue, disable to handle USB directly with tinyusb
                                      // functions
    uint8_t binmode_select;           // index of currently active binary mode
    bool binmode_lock_terminal;       // disable terminal while in binmode
    uint32_t bpio_debug_enable;           // enable debug output for BPIO

} _system_config;

extern struct _system_config system_config;

void system_init(void);

// TODO: Refactor to type-safe parameters
//       system_pin_update_purpose_and_label() is only called directly to update the BP_VOUT pin label
void system_pin_update_purpose_and_label(bool enable, uint8_t pin, enum bp_pin_func func, const char* label);
void system_bio_update_purpose_and_label(bool enable, uint8_t bio_pin, enum bp_pin_func func, const char* label);
void system_set_active(bool active, uint8_t bio_pin, uint8_t* function_register);
