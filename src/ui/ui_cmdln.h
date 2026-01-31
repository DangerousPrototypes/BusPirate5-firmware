/**
 * @file ui_cmdln.h
 * @brief Command line buffer and argument parsing interface.
 * @details Provides circular buffer command line with history,
 *          command parsing, and argument extraction.
 */

/**
 * @brief Circular command line buffer with history.
 */
struct _command_line {
    uint32_t wptr;     ///< Write pointer
    uint32_t rptr;     ///< Read pointer  
    uint32_t histptr;  ///< History scroll pointer
    uint32_t cursptr;  ///< Cursor position pointer
    char buf[UI_CMDBUFFSIZE]; ///< Circular buffer
};

/**
 * @brief Command buffer pointer snapshot.
 */
struct _command_pointer {
    uint32_t wptr;  ///< Write pointer snapshot
    uint32_t rptr;  ///< Read pointer snapshot
};

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
 * @brief Get command info (for debugging).
 * @return true on success
 */
bool cmdln_info(void);

/**
 * @brief Get command info with uint32 parsing (for debugging).
 * @return true on success
 */
bool cmdln_info_uint32(void);

/**
 * @name Buffer management functions
 * @{
 */

/**
 * @brief Update pointer with circular buffer rollover.
 * @param i  Pointer value
 * @return Wrapped pointer value
 */
uint32_t cmdln_pu(uint32_t i);

/**
 * @brief Try to add byte to command buffer.
 * @param c  Character to add
 * @return false if buffer full
 */
bool cmdln_try_add(char* c);

/**
 * @brief Try to remove byte from command buffer.
 * @param[out] c  Removed character
 * @return false if buffer empty
 */
bool cmdln_try_remove(char* c);

/**
 * @brief Try to peek at byte without advancing pointer.
 * @param i      Offset from read pointer
 * @param[out] c Peeked character
 * @return false if end of buffer
 * @note Use sequentially (peek(0) before peek(1)) to avoid buffer overrun
 */
bool cmdln_try_peek(uint32_t i, char* c);

/**
 * @brief Try to discard n bytes by advancing read pointer.
 * @param i  Number of bytes to discard
 * @return false if end of buffer
 * @note Should be used with try_peek to confirm before discarding
 */
bool cmdln_try_discard(uint32_t i);

/**
 * @brief Move read pointer to write pointer for next command.
 * @details Advances to next command position, enabling history scrolling.
 * @return Always true
 */
bool cmdln_next_buf_pos(void);

/**
 * @brief Initialize command line buffer.
 */
void cmdln_init(void);

/**
 * @brief Try to peek using custom pointer.
 * @param cp     Custom pointer structure
 * @param i      Offset from read pointer
 * @param[out] c Peeked character
 * @return false if end of buffer
 */
bool cmdln_try_peek_pointer(struct _command_pointer* cp, uint32_t i, char* c);

/**
 * @brief Get snapshot of current buffer pointers.
 * @param[out] cp  Pointer snapshot
 */
void cmdln_get_command_pointer(struct _command_pointer* cp);

/**
 * @brief Copy current command from circular to linear buffer.
 * @details Sets up bp_cmdln reader for command processing.
 */
void cmdln_copy_to_linear(void);

/**
 * @brief Enable linear buffer mode (for linenoise integration).
 * @details Called when linenoise has set up bp_cmdln directly.
 */
void cmdln_enable_linear_mode(void);

/**
 * @brief Reset to circular buffer mode after command processing.
 */
void cmdln_end_linear(void);

/**
 * @brief Get the linear buffer for direct access.
 * @return Pointer to null-terminated linear command buffer
 */
const char* cmdln_get_linear_buf(void);

/**
 * @brief Get length of linear buffer content.
 * @return Length in bytes
 */
size_t cmdln_get_linear_len(void);

/** @} */

/**
 * @brief Global command line buffer.
 */
extern struct _command_line cmdln;