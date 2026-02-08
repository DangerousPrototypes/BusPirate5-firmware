/**
 * @file hwspi.h
 * @brief Hardware SPI mode interface.
 * @details Implements SPI master mode using RP2040/RP2350 hardware peripheral.
 *          Supports configurable speed, data bits, clock polarity/phase, and
 *          chip select idle state. Includes flash programming and EEPROM support.
 */

/**
 * @brief Assert SPI chip select (CS low).
 * @param result  Pointer to bytecode result structure
 * @param next    Pointer to next bytecode instruction
 */
void spi_start(struct _bytecode* result, struct _bytecode* next);

/**
 * @brief Assert CS and read status.
 * @param result  Pointer to bytecode result structure
 * @param next    Pointer to next bytecode instruction
 */
void spi_startr(struct _bytecode* result, struct _bytecode* next);

/**
 * @brief Deassert SPI chip select (CS high).
 * @param result  Pointer to bytecode result structure
 * @param next    Pointer to next bytecode instruction
 */
void spi_stop(struct _bytecode* result, struct _bytecode* next);

/**
 * @brief Deassert CS and read status.
 * @param result  Pointer to bytecode result structure
 * @param next    Pointer to next bytecode instruction
 */
void spi_stopr(struct _bytecode* result, struct _bytecode* next);

/**
 * @brief Write data to SPI bus.
 * @param result  Pointer to bytecode result structure
 * @param next    Pointer to next bytecode instruction
 */
void spi_write(struct _bytecode* result, struct _bytecode* next);

/**
 * @brief Read data from SPI bus.
 * @param result  Pointer to bytecode result structure
 * @param next    Pointer to next bytecode instruction
 */
void spi_read(struct _bytecode* result, struct _bytecode* next);

/**
 * @brief Execute SPI macro command.
 * @param macro  Macro number to execute
 */
void spi_macro(uint32_t macro);

/**
 * @brief Interactive SPI mode setup.
 * @return 1 on success, 0 on failure
 */
uint32_t spi_setup(void);

/**
 * @brief Get binary mode configuration length.
 * @return Configuration size in bytes
 */
uint32_t spi_binmode_get_config_length(void);

/**
 * @brief Configure SPI mode from binary data.
 * @param config  Pointer to configuration data
 * @return 1 on success, 0 on failure
 */
uint32_t spi_binmode_setup(uint8_t* config);

/**
 * @brief Execute SPI mode setup.
 * @return 1 on success, 0 on failure
 */
uint32_t spi_setup_exc(void);

/**
 * @brief Cleanup and disable SPI mode.
 */
void spi_cleanup(void);

/**
 * @brief Display SPI pin assignments.
 */
void spi_pins(void);

/**
 * @brief Display current SPI settings.
 */
void spi_settings(void);

/**
 * @brief Display SPI configuration flags.
 */
void spi_printSPIflags(void);

/**
 * @brief Display SPI mode help.
 */
void spi_help(void);

/**
 * @brief Get current SPI speed.
 * @return Speed in Hz
 */
uint32_t spi_get_speed(void);

/**
 * @brief Perform SPI mode sanity checks.
 * @return true if all checks pass, false otherwise
 */
bool spi_preflight_sanity_check(void);

/**
 * @name SPI Configuration Functions
 * @details Low-level functions for SPI parameter control.
 * @{
 */

/**
 * @brief Set clock polarity.
 * @param val  0=idle low, 1=idle high
 */
void spi_setcpol(uint32_t val);

/**
 * @brief Set clock phase.
 * @param val  0=sample on first edge, 1=sample on second edge
 */
void spi_setcpha(uint32_t val);

/**
 * @brief Set baud rate.
 * @param val  Baud rate in Hz
 */
void spi_setbr(uint32_t val);

/**
 * @brief Set data frame format (data width).
 * @param val  Data bits (4-16)
 */
void spi_setdff(uint32_t val);

/**
 * @brief Set bit order.
 * @param val  0=MSB first, 1=LSB first
 */
void spi_setlsbfirst(uint32_t val);

/**
 * @brief Set CS idle state.
 * @param val  0=low, 1=high
 */
void spi_set_cs_idle(uint32_t val);

/**
 * @brief Manually control CS pin.
 * @param cs  CS state (0=low, 1=high)
 */
void spi_set_cs(uint8_t cs);

/**
 * @brief Full-duplex SPI transfer.
 * @param out  Byte to transmit
 * @return Received byte
 */
uint8_t spi_xfer(const uint8_t out);

/** @} */

/**
 * @brief Simple read without protocol overhead.
 * @return Received data
 */
uint32_t spi_read_simple(void);
void spi_write_simple(uint32_t data);
bool bpio_hwspi_configure(bpio_mode_configuration_t *bpio_mode_config);

typedef struct _spi_mode_config {
    uint32_t baudrate;
    uint32_t baudrate_actual;
    uint32_t data_bits;
    uint32_t clock_polarity;
    uint32_t clock_phase;
    uint32_t cs_idle;
    uint32_t dff;
    bool read_with_write;
    bool binmode;
} _spi_mode_config;

extern const struct _mode_command_struct hwspi_commands[];
extern const uint32_t hwspi_commands_count;
