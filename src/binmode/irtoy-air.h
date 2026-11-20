/**
 * @file irtoy-air.h
 * @brief IRToy AIR infrared protocol interface.
 * @details Provides AIR binary mode for infrared communication.
 */

#ifndef IRTOY_AIR_H
#define IRTOY_AIR_H

#include <stdbool.h>
#include <stdint.h>

extern const char irtoy_air_name[];

enum {
    AIR_OK = 0,
    AIR_ERROR_START = 1,
    AIR_ERROR_LENGTH = 2,
    AIR_ERROR_MOD_FREQ,
    AIR_ERROR_DATA,
    AIR_ERROR_RESTART,
    AIR_PENDING,
};

enum {
    AIR_TX_IDLE,
    AIR_TX_MOD_FREQ,
    AIR_TX_DATA,
};

#define AIR_TX_MAX_PAIRS 128

// aIR packet parsed as it arrives: its length in characters doesn't matter
typedef struct _air_tx {
    uint8_t state;
    uint32_t mod_freq; //kHz
    uint32_t value;
    bool digits; //the current value has at least one digit
    bool mark;
    uint16_t pairs;
    uint32_t data[AIR_TX_MAX_PAIRS]; //upper 16 bits are the mark, lower 16 bits the space
} air_tx_t;

/**
 * @brief Parse aIR text one character at a time, used by AIR mode and irtx.
 * @param tx  Parser state, start with .state = AIR_TX_IDLE
 * @param c   Next character
 * @return    AIR_OK when a whole packet is in tx, AIR_PENDING until then, or the error that dropped it
 */
int air_rx_char(air_tx_t *tx, char c);

/**
 * @brief Print the message for an air_rx_char() error.
 * @param error  Error code from air_rx_char()
 */
void air_tx_error(int error);

/**
 * @brief Setup AIR mode.
 */
void irtoy_air_setup(void);

/**
 * @brief Cleanup AIR mode.
 */
void irtoy_air_cleanup(void);

/**
 * @brief Service AIR mode.
 */
void irtoy_air_service(void);

#endif // IRTOY_AIR_H