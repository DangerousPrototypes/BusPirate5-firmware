/**
 * @file hwuart.h
 * @brief Hardware UART mode interface.
 * @details Implements UART (serial) protocol using PIO-based implementation.
 *          Supports configurable baud rate, data bits, parity, stop bits,
 *          and hardware flow control. Includes GPS NMEA decoding, UART bridge,
 *          and glitch testing features.
 */

/**
 * @brief UART mode configuration structure.
 */
typedef struct _uart_mode_config {
    uint32_t baudrate;         ///< Baud rate in bits per second
    uint32_t baudrate_actual;  ///< Actual achieved baud rate
    uint32_t data_bits;        ///< Data bits (5-8)
    uint32_t stop_bits;        ///< Stop bits (1 or 2)
    uint32_t parity;           ///< Parity (UART_PARITY_NONE, EVEN, ODD)
    uint32_t blocking;         ///< Blocking mode (0=non-blocking, 1=blocking)
    bool async_print;          ///< Enable automatic printing of received data
    uint32_t flow_control;     ///< Hardware flow control (0=disabled, 1=RTS/CTS)
    uint32_t invert;           ///< Signal inversion (0=normal, 1=inverted)
} _uart_mode_config;

/**
 * @brief Open UART connection (start).
 * @param result  Pointer to bytecode result structure
 * @param next    Pointer to next bytecode instruction
 */
void hwuart_open(struct _bytecode* result, struct _bytecode* next);

/**
 * @brief Open UART and read status.
 * @param result  Pointer to bytecode result structure
 * @param next    Pointer to next bytecode instruction
 */
void hwuart_open_read(struct _bytecode* result, struct _bytecode* next);

/**
 * @brief Close UART connection (stop).
 * @param result  Pointer to bytecode result structure
 * @param next    Pointer to next bytecode instruction
 */
void hwuart_close(struct _bytecode* result, struct _bytecode* next);

/**
 * @brief Write data to UART.
 * @param result  Pointer to bytecode result structure
 * @param next    Pointer to next bytecode instruction
 */
void hwuart_write(struct _bytecode* result, struct _bytecode* next);

/**
 * @brief Read data from UART.
 * @param result  Pointer to bytecode result structure
 * @param next    Pointer to next bytecode instruction
 */
void hwuart_read(struct _bytecode* result, struct _bytecode* next);

/**
 * @brief Execute UART macro command.
 * @param macro  Macro number to execute
 */
void hwuart_macro(uint32_t macro);

/**
 * @brief Interactive UART mode setup.
 * @return 1 on success, 0 on failure
 */
uint32_t hwuart_setup(void);

/**
 * @brief Execute UART mode setup.
 * @return 1 on success, 0 on failure
 */
uint32_t hwuart_setup_exc(void);

/**
 * @brief Cleanup and disable UART mode.
 */
void hwuart_cleanup(void);

/**
 * @brief Display UART pin assignments.
 */
void hwuart_pins(void);

/**
 * @brief Display current UART settings.
 */
void hwuart_settings(void);

/**
 * @brief Display UART error information.
 */
void hwuart_printerror(void);

/**
 * @brief Display UART mode help.
 */
void hwuart_help(void);

/**
 * @brief Periodic UART handler for async data.
 */
void hwuart_periodic(void);

/**
 * @brief Wait for UART transmission to complete.
 */
void hwuart_wait_done(void);

/**
 * @brief Get current UART speed.
 * @return Baud rate in bits per second
 */
uint32_t hwuart_get_speed(void);

/**
 * @brief Perform UART mode sanity checks.
 * @return true if all checks pass, false otherwise
 */
bool hwuart_preflight_sanity_check(void);

extern const struct _mode_command_struct hwuart_commands[];
extern const uint32_t hwuart_commands_count;

bool bpio_hwuart_configure(bpio_mode_configuration_t *bpio_mode_config);