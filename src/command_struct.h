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
 * @brief Help menu category for global commands.
 */
enum cmd_category {
    CMD_CAT_IO = 0,        /**< Pin I/O, power, measurement */
    CMD_CAT_CONFIGURE,     /**< Terminal, display, mode config */
    CMD_CAT_SYSTEM,        /**< Info, reboot, selftest */
    CMD_CAT_FILES,         /**< Storage and file operations */
    CMD_CAT_SCRIPT,        /**< Scripting and macros */
    CMD_CAT_TOOLS,         /**< Utilities and converters */
    CMD_CAT_MODE,          /**< Mode selection */
    CMD_CAT_HIDDEN,        /**< Aliases/internal — not shown in help */
    CMD_CAT_COUNT          /**< Number of categories (sentinel) */
};

/**
 * @brief Global command structure definition.
 */
struct _global_command_struct {
    const char *command; /**< Command line string */
    void (*func)(struct command_result* res); /**< Function to execute */
    const struct bp_command_def *def;  /**< Unified command definition (NULL = legacy) */
    //deprecate after full migration to bp_command_def_t: use description in def instead
    uint32_t description_text;        /**< Help and command list description */
    bool allow_hiz;                   /**< Allow execution in HiZ mode */
    uint8_t category;                 /**< Help menu category (enum cmd_category) */
};

/**
 * @brief Mode-specific command structure definition.
 */
struct _mode_command_struct {
    void (*func)(struct command_result* res); /**< Function to execute */
    const struct bp_command_def *def;  /**< Unified command definition */
    bool supress_fala_capture;        /**< Disable follow-along logic analyzer */
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
