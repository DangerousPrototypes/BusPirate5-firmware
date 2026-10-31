/**
 * @file spiflash.h
 * @brief SPI flash memory operations library interface.
 * @details Provides low-level SPI flash operations using SFUD library:
 *          erase, write, verify, dump, load.
 */

/**
 * @brief Probe and identify SPI flash chip.
 */
void spiflash_probe(void);

/**
 * @brief Initialize SPI flash.
 * @param flash_info  Flash information structure
 * @return true on success
 */
bool spiflash_init(sfud_flash* flash_info);

/**
 * @brief Erase SPI flash.
 * @param flash_info  Flash information structure
 * @return true on success
 */
bool spiflash_erase(sfud_flash* flash_info);

/**
 * @brief Erase and verify SPI flash region.
 * @param start_address  Start address
 * @param end_address    End address
 * @param buf_size       Buffer size
 * @param buf            Buffer
 * @param flash_info     Flash information structure
 * @return true on success
 */
bool spiflash_erase_verify(
    uint32_t start_address, uint32_t end_address, uint32_t buf_size, uint8_t* buf, sfud_flash* flash_info);

/**
 * @brief Write test pattern to SPI flash.
 * @param start_address  Start address
 * @param end_address    End address
 * @param buf_size       Buffer size
 * @param buf            Buffer
 * @param flash_info     Flash information structure
 * @return true on success
 */
bool spiflash_write_test(
    uint32_t start_address, uint32_t end_address, uint32_t buf_size, uint8_t* buf, sfud_flash* flash_info);

/**
 * @brief Write and verify SPI flash.
 * @param start_address  Start address
 * @param end_address    End address
 * @param buf_size       Buffer size
 * @param buf            Buffer
 * @param flash_info     Flash information structure
 * @return true on success
 */
bool spiflash_write_verify(
    uint32_t start_address, uint32_t end_address, uint32_t buf_size, uint8_t* buf, sfud_flash* flash_info);

/**
 * @brief Dump SPI flash to file.
 * @param start_address  Start address
 * @param end_address    End address
 * @param buf_size       Buffer size
 * @param buf            Buffer
 * @param flash_info     Flash information structure
 * @param file_name      Output filename
 * @return true on success
 */
bool spiflash_dump(uint32_t start_address,
                   uint32_t end_address,
                   uint32_t buf_size,
                   uint8_t* buf,
                   sfud_flash* flash_info,
                   const char* file_name);

/**
 * @brief Load file to SPI flash.
 * @param start_address  Start address
 * @param end_address    End address
 * @param buf_size       Buffer size
 * @param buf            Buffer
 * @param flash_info     Flash information structure
 * @param file_name      Input filename
 * @return true on success
 */
bool spiflash_load(uint32_t start_address,
                   uint32_t end_address,
                   uint32_t buf_size,
                   uint8_t* buf,
                   sfud_flash* flash_info,
                   const char* file_name);

/**
 * @brief Verify SPI flash against file.
 * @param start_address  Start address
 * @param end_address    End address
 * @param buf_size       Buffer size
 * @param buf            Buffer 1
 * @param buf2           Buffer 2
 * @param flash_info     Flash information structure
 * @param file_name      Verification filename
 * @return true on success
 */
bool spiflash_verify(uint32_t start_address,
                     uint32_t end_address,
                     uint32_t buf_size,
                     uint8_t* buf,
                     uint8_t* buf2,
                     sfud_flash* flash_info,
                     const char* file_name);

/**
 * @brief Force dump of SPI flash (ignore errors).
 * @param start_address  Start address for dump
 * @param end_address    End address for dump
 * @param buf_size       Buffer size in bytes
 * @param buf            Buffer for flash data
 * @param flash_info     SFUD flash device information structure
 * @param file_name      Output filename for dump
 */
bool spiflash_force_dump(uint32_t start_address,
                         uint32_t end_address,
                         uint32_t buf_size,
                         uint8_t* buf,
                         sfud_flash* flash_info,
                         const char* file_name);
struct bp_command_def;
bool spiflash_show_hex(const struct bp_command_def *def,
                      uint32_t buf_size,
                      uint8_t* buf,
                      sfud_flash* flash_info); 