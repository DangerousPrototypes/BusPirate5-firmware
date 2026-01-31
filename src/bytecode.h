/**
 * @file bytecode.h
 * @brief Bytecode structures and definitions for protocol execution.
 * @details Defines the internal bytecode representation used by the syntax compiler and executor.
 *          The bytecode system allows user commands to be parsed once and executed multiple times.
 */

/**
 * @brief Main bytecode instruction structure.
 * @details Represents a single bytecode instruction for protocol operations.
 *          Size-critical structure - changes impact total RAM usage significantly.
 * @note Fields are overloaded per-protocol; meaning varies between modes.
 * @warning Current size target: <= 28 bytes
 */
struct _bytecode {
    uint8_t number_format;  /**< Display format (binary, hex, decimal, ASCII) */
    uint8_t command;        /**< Operation type (read, write, start, stop, etc) */
    uint8_t error;          /**< Error severity level (SERR_NONE to SERR_ERROR) */
    
    /** @name Bit Flags */
    /**@{*/
    uint8_t read_with_write : 1;  /**< Read during write operation */
    uint8_t has_bits : 1;         /**< Bit count explicitly specified */
    uint8_t has_repeat : 1;       /**< Repeat count specified */
    /**@}*/

    const char* error_message;  /**< Error description string */
    const char* data_message;   /**< Data display message */
    
    /** @name Protocol-Specific Fields
     * These fields are overloaded per protocol with no universal meaning.
     * 
     * Example (HWLED protocol):
     * - WS2812: out_data[23:0] = RGB value
     * - APA102: out_data[23:0] = RGB, out_data[31:24] = brightness
     */
    /**@{*/
    uint32_t bits;      /**< Bit count (0-32) or protocol-specific use */
    uint32_t repeat;    /**< Repeat count or protocol-specific use */
    uint32_t out_data;  /**< Data to transmit or protocol-specific use */
    uint32_t in_data;   /**< Data received or protocol-specific use */
    /**@}*/
};
/**
 * @brief Compile-time assertion to protect against RAM bloat
 * 
 * Ensures _bytecode structure doesn't exceed 28 bytes, preventing
 * unintentional RAM usage increases.
 */
static_assert(
    sizeof(struct _bytecode) <= 28,
    "sizeof(struct _bytecode) has increased.  This will impact RAM.  Review to ensure this is not avoidable.");

/**
 * @brief Bytecode output structure (alternative representation)
 * 
 * Simplified output-only bytecode structure without error tracking.
 */
struct _bytecode_output {
    uint8_t number_format;  /**< Display format */
    uint8_t command;        /**< Operation type */
    uint32_t bits;          /**< Bit count */
    uint32_t repeat;        /**< Repeat count */
    uint32_t data;          /**< Data value */
    bool has_repeat;        /**< Repeat explicitly specified */
    bool has_bits;          /**< Bit count explicitly specified */
};

/**
 * @brief Bytecode execution result structure
 * 
 * Contains both the command output and execution results/errors.
 * 
 * @note Handles multiple results from repeated commands
 */
struct _bytecode_result {
    struct _bytecode_output output;  /**< Original command */
    uint8_t error;                   /**< Error severity */
    const char* error_message;       /**< Error description */
    uint32_t data;                   /**< Result data */
    const char* message;             /**< Result message */
};

/**
 * @brief Syntax processing status codes
 */
typedef enum {
    SSTATUS_OK,     /**< Operation successful */
    SSTATUS_ERROR   /**< Operation failed */
} SYNTAX_STATUS;

/**
 * @brief Syntax error severity levels
 */
enum SYNTAX_ERRORS {
    SERR_NONE = 0,  /**< No error */
    SERR_DEBUG,     /**< Debug message */
    SERR_INFO,      /**< Informational message */
    SERR_WARN,      /**< Warning */
    SERR_ERROR      /**< Error - halts execution */
};

/**
 * @brief Syntax command opcodes
 * 
 * These opcodes represent the different operations that can be
 * encoded in the bytecode instruction stream.
 */
enum SYNTAX {
    SYN_WRITE = 0,      /**< Write data */
    SYN_READ,           /**< Read data */
    SYN_START,          /**< Start condition */
    SYN_STOP,           /**< Stop condition */
    SYN_START_ALT,      /**< Alternate start condition */
    SYN_STOP_ALT,       /**< Alternate stop condition */
    SYN_TICK_CLOCK,     /**< Pulse clock */
    SYN_SET_CLK_HIGH,   /**< Set clock pin high */
    SYN_SET_CLK_LOW,    /**< Set clock pin low */
    SYN_SET_DAT_HIGH,   /**< Set data pin high */
    SYN_SET_DAT_LOW,    /**< Set data pin low */
    SYN_READ_DAT,       /**< Read data pin */
    SYN_DELAY_US,       /**< Delay microseconds */
    SYN_DELAY_MS,
    SYN_AUX_OUTPUT_HIGH,
    SYN_AUX_OUTPUT_LOW,
    SYN_AUX_INPUT,
    SYN_ADC,
    // SYN_FREQ
};