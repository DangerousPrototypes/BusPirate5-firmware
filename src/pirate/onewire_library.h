/**
 * @file onewire_library.h
 * @brief 1-Wire protocol library interface.
 * @details Provides low-level 1-Wire protocol operations using PIO.
 */

typedef struct {
    PIO pio;
    uint sm;
    uint jmp_reset;
    int offset;
    int gpio;
} OW;

/**
 * @brief Initialize 1-Wire PIO interface.
 * @param bits_per_word  Bits per word
 * @param bufdir         Buffer direction pin
 * @param inpin          Input pin
 * @return               true on success
 */
bool ow_init(uint8_t bits_per_word, uint bufdir, uint inpin);

/**
 * @brief Send data over 1-Wire.
 * @param data  Data to send
 */
void ow_send(uint data);

/**
 * @brief Read data from 1-Wire.
 * @return  Received data byte
 */
uint8_t ow_read(void);

/**
 * @brief Perform 1-Wire bus reset.
 * @return  true if device present
 */
bool ow_reset(void);

/**
 * @brief Search for ROM codes on 1-Wire bus.
 * @param romcodes  Array to store ROM codes
 * @param maxdevs   Maximum number of devices
 * @param command   Search command
 * @return          Number of devices found
 */
int ow_romsearch(uint64_t* romcodes, int maxdevs, uint command);

/**
 * @brief Cleanup 1-Wire interface.
 */
void ow_cleanup(void);