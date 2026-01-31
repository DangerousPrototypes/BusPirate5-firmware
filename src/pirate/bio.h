/**
 * @file bio.h
 * @brief Buffered I/O (BIO) pin control interface
 * 
 * Declares functions for controlling the buffered I/O system on Bus Pirate.
 * Each BIO pin has bidirectional level-shifted buffers for safe interfacing
 * with external devices at different voltage levels.
 * 
 * @author Bus Pirate Project
 * @date 2024-2026
 */

/**
 * @brief Initialize all buffered I/O pins and buffers
 */
void bio_init(void);

/**
 * @brief Initialize buffer direction pin
 * @param bio BIO pin number (0-7)
 */
void bio_buf_pin_init(uint8_t bio);

/**
 * @brief Set buffer direction to output
 * @param bio BIO pin number (0-7)
 */
void bio_buf_output(uint8_t bio);

/**
 * @brief Set buffer direction to input
 * @param bio BIO pin number (0-7)
 */
void bio_buf_input(uint8_t bio);

/**
 * @brief Configure pin and buffer for output
 * @param bio BIO pin number (0-7)
 */
void bio_output(uint8_t bio);

/**
 * @brief Configure pin and buffer for input
 * @param bio BIO pin number (0-7)
 */
void bio_input(uint8_t bio);

/**
 * @brief Set output value of BIO pin
 * @param bio BIO pin number (0-7)
 * @param value true for high, false for low
 */
void bio_put(uint8_t bio, bool value);

/**
 * @brief Read current value of BIO pin
 * @param bio BIO pin number (0-7)
 * @return true if high, false if low
 */
bool bio_get(uint8_t bio);

/**
 * @brief Set GPIO function for BIO pin
 * @param bio BIO pin number (0-7)
 * @param function GPIO function code
 */
void bio_set_function(uint8_t bio, uint8_t function);

/**
 * @brief Test buffer functionality
 * @param bufio Buffer I/O pin
 * @param bufdir Buffer direction pin
 */
void bio_buf_test(uint8_t bufio, uint8_t bufdir);

/** Buffer direction: input (high-Z) */
#define BUFDIR_INPUT 0
/** Buffer direction: output (drive) */
#define BUFDIR_OUTPUT 1
