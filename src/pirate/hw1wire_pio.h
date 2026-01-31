/**
 * @file hw1wire_pio.h
 * @brief 1-Wire (Dallas/Maxim) protocol implementation using PIO.
 * @details PIO-based 1-Wire master interface with device search and ROM operations.
 *          Based on Stefan Althöfer's RP2040-PIO-1-Wire-Master.
 * @copyright Copyright (c) 2021 Stefan Althöfer (BSD-3-Clause)
 * @note Modified 2023 by Ian Lesnet for Bus Pirate 5 buffered I/O.
 * @warning OWxxxx functions may fall under Maxim copyright.
 */

/*
 * Copyright (c) 2021 Stefan Althöfer
 *
 * OWxxxx functions might fall under Maxxim copyriht.
 *
 * Demo code for PIO 1-Wire interface
 */

#include "hardware/pio.h"

/**
 * @brief 1-Wire object state and device search tracking.
 */
struct owobj {
    PIO pio;                       ///< PIO instance (pio0/pio1)
    uint sm;                       ///< State machine number
    uint offset;                   ///< PIO program offset in instruction memory
    uint pin;                      ///< GPIO pin for 1-Wire data signal
    uint dir;                      ///< GPIO pin for buffer direction control

    unsigned char ROM_NO[8];       ///< Last device ROM code found during search
    int LastDiscrepancy;           ///< Last discrepancy bit position in search
    int LastFamilyDiscrepancy;     ///< Last family code discrepancy
    int LastDeviceFlag;            ///< Flag indicating last device found
    unsigned char crc8;            ///< CRC8 accumulator
};
/**
 * @brief Initialize 1-Wire PIO and state machine.
 * @param pin  GPIO pin for 1-Wire data
 * @param dir  GPIO pin for buffer direction control
 */
void onewire_init(uint pin, uint dir);

/**
 * @brief Clean up and remove 1-Wire PIO program.
 */
void onewire_cleanup(void);

/**
 * @brief Debug: trace PIO state machine execution.
 * @param pio     PIO instance
 * @param sm      State machine number
 * @param usleep  Microseconds to sleep between traces
 */
void pio_sm_trace(PIO pio, uint sm, uint usleep);

/**
 * @brief Set TX/RX FIFO threshold for the state machine.
 * @param thresh  FIFO threshold (0-31, 0 disables threshold)
 */
void onewire_set_fifo_thresh(uint thresh);

/**
 * @brief Perform 1-Wire reset and presence detect.
 * @return 1 if device presence detected, 0 if no device
 */
int onewire_reset(void);

/**
 * @brief Wait for PIO to reach idle/waiting state.
 */
void onewire_wait_for_idle(void);

/**
 * @brief Transmit one byte on 1-Wire bus.
 * @param byte  Byte to transmit
 */
void onewire_tx_byte(uint byte);

/**
 * @brief Transmit one byte with strong pullup enabled.
 * @param byte  Byte to transmit
 * @note Strong pullup remains active after transmission.
 */
void onewire_tx_byte_spu(uint byte);

/**
 * @brief Disable strong pullup.
 */
void onewire_end_spu(void);

/**
 * @brief Receive one byte from 1-Wire bus.
 * @return Received byte
 */
uint onewire_rx_byte(void);

/**
 * @brief Perform search triplet operation.
 * @param[out] id_bit          First bit read from bus
 * @param[out] cmp_id_bit      Complement bit read from bus
 * @param[out] search_direction Direction bit written to bus
 */
void onewire_triplet(int* id_bit, int* cmp_id_bit, unsigned char* search_direction);

/**
 * @brief Calculate Dallas/Maxim 1-Wire CRC8 for buffer.
 * @param data  Pointer to data buffer
 * @param len   Length of data
 * @return CRC8 value
 */
unsigned char calc_crc8_buf(unsigned char* data, int len);

/**
 * @brief Select specific 1-Wire device by ROM ID.
 * @param romid  8-byte ROM ID
 * @return 1 on success, 0 on failure
 */
int onewire_select(unsigned char* romid);

/**
 * @brief Calculate Dallas/Maxim 1-Wire CRC8 for single byte.
 * @param data  Input byte
 * @return Updated CRC8 value
 */
unsigned char calc_crc8(unsigned char data);

/**
 * @brief Search for next 1-Wire device on bus.
 * @param search_owobj  Pointer to owobj with search state
 * @return 1 if device found, 0 if search complete
 */
int OWSearch(struct owobj* search_owobj);

/**
 * @brief Reset device search algorithm.
 * @param search_owobj  Pointer to owobj to reset
 * @return Always returns 0
 */
int OWSearchReset(struct owobj* search_owobj);

/**
 * @brief Find first 1-Wire device on bus.
 * @param search_owobj  Pointer to owobj for search
 * @return 1 if device found, 0 otherwise
 */
int OWFirst(struct owobj* search_owobj);

/**
 * @brief Find next 1-Wire device on bus.
 * @param search_owobj  Pointer to owobj with search state
 * @return 1 if device found, 0 if search complete
 */
int OWNext(struct owobj* search_owobj);

/**
 * @brief Test function: read DS18B20 scratchpad.
 */
void onewire_test_ds18b20_scratchpad(void);

/**
 * @brief Test function: strong pullup operations.
 */
void onewire_test_spu(void);

/**
 * @brief Test function: variable word length transfers.
 */
void onewire_test_wordlength(void);
