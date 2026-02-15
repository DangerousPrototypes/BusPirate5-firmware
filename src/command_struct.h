/**
 * @file command_struct.h
 * @brief Command handler structure definitions.
 * @details Defines structures for global and mode-specific command handling.
 */

#define MAX_COMMAND_LENGTH 10

/* Forward declare bp_command_def_t so we don't force-include bp_cmd.h everywhere */
struct bp_command_def;

/**
 * @brief Command execution result structure.
 */
typedef struct command_result {
    uint8_t number_format;  /**< Number display format */
    bool success;           /**< Command succeeded */
    bool exit;              /**< Exit command loop */
    bool no_value;          /**< No value provided */
    bool default_value;     /**< Using default value */
    bool error;             /**< Error occurred */
    bool help_flag;         /**< Help requested */
} command_result;

/**
 * @brief Global command structure definition.
 */
struct _global_command_struct {
    char command[MAX_COMMAND_LENGTH]; /**< Command line string */
    bool allow_hiz;                   /**< Allow execution in HiZ mode */
    void (*func)(struct command_result* res); /**< Function to execute */
    uint32_t help_text;               /**< Translation string for help, 0x00 = self-managed */
    const struct bp_command_def *def;  /**< Unified command definition (NULL = legacy) */
};

/**
 * @brief Mode-specific command structure definition.
 */
struct _mode_command_struct {
    char command[MAX_COMMAND_LENGTH]; /**< Command line string */
    void (*func)(struct command_result* res); /**< Function to execute */
    uint32_t description_text;        /**< Help and command list description */
    bool supress_fala_capture;        /**< Disable follow-along logic analyzer */
    const struct bp_command_def *def;  /**< Unified command definition (NULL = legacy) */
};

/**
 * @brief Command response structure.
 */
struct command_response {
    bool error;         /**< Error occurred */
    uint32_t data;      /**< Response data */
};

/**
 * @brief Command attribute structure.
 */
struct command_attributes {
    bool has_value;         /**< Value present */
    bool has_dot;           /**< Dot notation present */
    bool has_colon;         /**< Colon notation present */
    bool has_string;        /**< String argument present */
    uint8_t command;        /**< Command identifier */
    uint8_t number_format;  /**< DEC/HEX/BIN */
    uint32_t value;         /**< Integer value */
    uint32_t dot;           /**< Value after . */
    uint32_t colon;         /**< Value after : */
};
