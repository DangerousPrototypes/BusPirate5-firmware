/**
 * @file hwspi.h
 * @brief Hardware SPI peripheral interface.
 * @details Uses RP2040/RP2350 hardware SPI peripheral (not PIO) for high-speed
 *          SPI communication. Supports various frame formats, data widths, and
 *          clock polarity/phase configurations.
 */

/**
 * @brief Initialize hardware SPI peripheral.
 * @param data_bits  Data width in bits (4-16)
 * @param cpol       Clock polarity (0=idle low, 1=idle high)
 * @param cpha       Clock phase (0=sample on first edge, 1=sample on second edge)
 */
void hwspi_init(uint8_t data_bits, uint8_t cpol, uint8_t cpha);

/**
 * @brief Deinitialize hardware SPI and reset pins to safe state.
 */
void hwspi_deinit(void);

/**
 * @brief Assert chip select (CS low).
 */
void hwspi_select(void);

/**
 * @brief Deassert chip select (CS high).
 */
void hwspi_deselect(void);

/**
 * @brief Write single word to SPI (discard read data).
 * @param data  Data to transmit
 * @note Blocks until transmission complete.
 */
void hwspi_write(uint32_t data);

/**
 * @brief Write array of bytes to SPI.
 * @param data   Pointer to data buffer
 * @param count  Number of bytes to write
 */
void hwspi_write_n(uint8_t* data, uint8_t count);

/**
 * @brief Write multiple bytes from a 32-bit word.
 * @param data   32-bit data word
 * @param count  Number of bytes to write (1-4)
 */
void hwspi_write_32(const uint32_t data, uint8_t count);

/**
 * @brief Read single word from SPI.
 * @return Received data
 */
uint32_t hwspi_read(void);

/**
 * @brief Read array of bytes from SPI.
 * @param data   Pointer to receive buffer
 * @param count  Number of bytes to read
 */
void hwspi_read_n(uint8_t* data, uint32_t count);

/**
 * @brief Full-duplex write and read single byte.
 * @param data  Byte to transmit
 * @return Received byte
 */
uint32_t hwspi_write_read(uint8_t data);

/**
 * @brief Full-duplex transaction with CS control.
 * @param write_data   Pointer to transmit buffer
 * @param write_count  Number of bytes to write
 * @param read_data    Pointer to receive buffer
 * @param read_count   Number of bytes to read
 * @note Asserts CS, writes, then reads, then deasserts CS.
 */
void hwspi_write_read_cs(uint8_t *write_data, uint32_t write_count, uint8_t *read_data, uint32_t read_count);

/**
 * @brief SPI frame format types.
 */
typedef enum {
    SPI_FRF_MOTOROLA = 0,   ///< Standard Motorola SPI
    SPI_FRF_TI = 1,         ///< Texas Instruments SSI
    SPI_FRF_MICROWIRE = 2   ///< National Semiconductor Microwire
} spi_frf_t;

/**
 * @brief Set SPI frame format.
 * @param frf  Frame format type
 */
void hwspi_set_frame_format(spi_frf_t frf);

/**
 * @brief Get current clock phase setting.
 * @return true if CPHA=1, false if CPHA=0
 */
bool hwspi_get_cphase(void);

/**
 * @brief Set clock phase dynamically.
 * @param cpha  Clock phase (0 or 1)
 */
void hwspi_set_cphase(bool cpha);