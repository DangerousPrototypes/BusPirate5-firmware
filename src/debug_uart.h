/**
 * @file debug_uart.h
 * @brief Debug UART interface.
 * @details Provides hardware UART debug output and terminal interface.
 */

/**
 * @brief Debug UART configuration structure.
 */
struct _debug_uart {
    uart_inst_t(*const uart);  /**< UART instance */
    uint8_t rx_pin;            /**< RX pin */
    uint8_t tx_pin;            /**< TX pin */
    int irq;                   /**< IRQ number */
};

extern struct _debug_uart debug_uart[];
extern int debug_uart_number;

/**
 * @brief Initialize debug UART.
 * @param uart_number     UART instance number
 * @param dbrx            Enable RX
 * @param dbtx            Enable TX
 * @param terminal_label  Display terminal label
 */
void debug_uart_init(int uart_number, bool dbrx, bool dbtx, bool terminal_label);

/**
 * @brief Transmit character on debug UART.
 * @param c  Character to transmit
 */
void debug_tx(char c);

/**
 * @brief Receive character from debug UART.
 * @param c  Output character
 * @return   true if character available
 */
bool debug_rx(char* c);