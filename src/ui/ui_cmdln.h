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
 * @brief Action verb descriptor for command dispatch.
 * @details Used for finding subcommands/actions in command strings
 *          (e.g., "config", "show", "read", "write").
 */
struct cmdln_action_t {
    uint32_t action;  ///< Action identifier
    const char verb[9]; ///< Action verb string
};

/**
 * @brief Find action verb in command and return action ID.
 * @param action_list          Array of action descriptors
 * @param count_of_action_list Number of actions in list
 * @param[out] action          Matched action ID
 * @return false if action found, true if no match or error
 */
bool cmdln_args_get_action(const struct cmdln_action_t* action_list, size_t count_of_action_list, uint32_t *action);

/**
 * @brief Check if flag exists in command arguments.
 * @param flag  Flag character (e.g., 'v' for -v)
 * @return true if flag exists
 */
bool cmdln_args_find_flag(char flag);

/**
 * @brief Get uint32 value from flag argument.
 * @param flag       Flag character
 * @param[out] arg   Argument information
 * @param[out] value Parsed value
 * @return true on success
 */
bool cmdln_args_find_flag_uint32(char flag, command_var_t* arg, uint32_t* value);

/**
 * @brief Get string value from flag argument.
 * @param flag       Flag character
 * @param[out] arg   Argument information
 * @param max_len    Maximum string length
 * @param[out] str   Output string buffer
 * @return true on success
 */
bool cmdln_args_find_flag_string(char flag, command_var_t* arg, uint32_t max_len, char* str);

/**
 * @brief Get float argument by position.
 * @param pos         Argument position (0-based)
 * @param[out] value  Parsed float value
 * @return true on success
 */
bool cmdln_args_float_by_position(uint32_t pos, float* value);

/**
 * @brief Get uint32 argument by position.
 * @param pos         Argument position (0-based)
 * @param[out] value  Parsed uint32 value
 * @return true on success
 */
bool cmdln_args_uint32_by_position(uint32_t pos, uint32_t* value);

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