/**
 * @file ui_cmdln.h
 * @brief Command line argument parsing interface.
 * @details Provides command parsing and argument extraction.
 *          Uses linenoise linear buffer via ln_cmdreader.
 */

/**
 * @brief Command parsing state.
 */
struct _command_info_t {
    uint32_t rptr;      ///< Read pointer
    uint32_t wptr;      ///< Write pointer
    uint32_t startptr;  ///< Command start position
    uint32_t endptr;    ///< Command end position
    uint32_t nextptr;   ///< Next command position
    char delimiter;     ///< Command delimiter (;, ||, &&)
    char command[9];    ///< Command string
};

/**
 * @brief Global command parsing state (set by cmdln_find_next_command).
 */
extern struct _command_info_t command_info;

/**
 * @brief Parsed command argument information.
 */
typedef struct command_var_struct {
    bool has_arg;          ///< Argument exists
    bool has_value;        ///< Argument has value
    uint32_t value_pos;    ///< Position of value in buffer
    bool error;            ///< Parse error occurred
    uint8_t number_format; ///< Number format (hex/dec/bin)
} command_var_t;

/**
 * @brief Check if flag exists in command arguments.
 * @param flag  Flag character (e.g., 'v' for -v)
 * @return true if flag exists
 */
bool cmdln_args_find_flag(char flag);

/**
 * @brief Get string argument by position.
 * @param pos      Argument position (0-based)
 * @param max_len  Maximum string length
 * @param[out] str Output string buffer
 * @return true on success
 */
bool cmdln_args_string_by_position(uint32_t pos, uint32_t max_len, char* str);

/**
 * @brief Find next command in buffer.
 * @param[in,out] cp  Command info structure
 * @return true if command found
 */
bool cmdln_find_next_command(struct _command_info_t* cp);

/**
 * @name Buffer access (provided by ln_cmdreader macros)
 * @details cmdln_try_peek, cmdln_try_discard, cmdln_try_remove
 *          are macros defined in ln_cmdreader.h mapping to ln_cmdln_*.
 * @{
 */
#include "lib/bp_linenoise/ln_cmdreader.h"
/** @} */