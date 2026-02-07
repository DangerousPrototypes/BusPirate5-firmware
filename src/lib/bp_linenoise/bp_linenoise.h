/**
 * @file bp_linenoise.h
 * @brief Linenoise line editing adapted for Bus Pirate (RP2040/RP2350).
 * @details Static allocation, no POSIX I/O, works with USB CDC.
 *          
 *          Based on linenoise by Salvatore Sanfilippo <antirez@gmail.com>
 *          Original license: BSD-2-Clause (see bp_linenoise.c)
 *          
 *          Changes for Bus Pirate:
 *          - Removed malloc/free, uses static buffers
 *          - Removed POSIX termios/read/write, uses callbacks
 *          - Removed file I/O for history (embedded system)
 *          - Simplified UTF-8 (ASCII focus, basic UTF-8 passthrough)
 *          - Added linear buffer reader for command/syntax parsing
 */

#ifndef BP_LINENOISE_H
#define BP_LINENOISE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Configuration
#define BP_LINENOISE_MAX_LINE    256   // Maximum line length
#define BP_LINENOISE_HISTORY_MAX 8     // Number of history entries

/*
 * =============================================================================
 * Linear Buffer Reader
 * =============================================================================
 * Simple interface for reading from the linear edit buffer.
 * Replaces the circular buffer access pattern used by syntax.c and ui_parse.c
 */

/**
 * @brief Linear buffer reader state.
 * @details Points to the current line being processed (from linenoise).
 *          Provides cmdln_try_peek/discard compatible interface.
 */
typedef struct {
    const char *buf;    ///< Pointer to line buffer
    size_t len;         ///< Total length of content
    size_t pos;         ///< Current read position
} bp_cmdln_reader_t;

/**
 * @brief Global command line reader (set when line is ready for processing).
 */
extern bp_cmdln_reader_t bp_cmdln;

/**
 * @brief Initialize reader with a line buffer.
 * @param buf   Line buffer (null-terminated)
 * @param len   Length of content
 */
void bp_cmdln_init_reader(const char *buf, size_t len);

/**
 * @brief Reset reader position to start.
 */
void bp_cmdln_reset(void);

/**
 * @brief Try to peek at character at offset from current position.
 * @param offset  Offset from current position
 * @param[out] c  Character at position
 * @return true if character available, false if past end
 */
bool bp_cmdln_try_peek(size_t offset, char *c);

/**
 * @brief Discard n characters by advancing read position.
 * @param n  Number of characters to discard
 * @return true (always succeeds, may advance past end)
 */
bool bp_cmdln_try_discard(size_t n);

/**
 * @brief Get pointer to current position in buffer.
 * @return Pointer to buf[pos]
 */
const char *bp_cmdln_current(void);

/**
 * @brief Get remaining length from current position.
 * @return Remaining characters
 */
size_t bp_cmdln_remaining(void);

/*
 * =============================================================================
 * Compatibility Layer
 * =============================================================================
 * When BP_USE_LINEAR_CMDLN is defined, the old cmdln_try_* functions
 * redirect to the new linear buffer reader. This allows incremental migration.
 * 
 * The semantics match the old circular buffer:
 * - cmdln_try_peek(i, c): peek at position pos+i, return char in c
 * - cmdln_try_discard(i): advance pos by i
 * - cmdln_try_remove(c): peek at pos, advance by 1, return char in c
 */
#ifdef BP_USE_LINEAR_CMDLN
#define cmdln_try_peek(i, c)    bp_cmdln_try_peek((i), (c))
#define cmdln_try_discard(i)    bp_cmdln_try_discard((i))
#define cmdln_try_remove(c)     (bp_cmdln_try_peek(0, (c)) ? (bp_cmdln_try_discard(1), true) : false)
#endif

// Result codes for bp_linenoise_feed()
typedef enum {
    BP_LN_CONTINUE = 0,    // Still editing, need more input
    BP_LN_ENTER,           // User pressed Enter, line complete
    BP_LN_CTRL_C,          // User pressed Ctrl+C
    BP_LN_CTRL_D,          // User pressed Ctrl+D (EOF on empty line)
    BP_LN_REFRESH,         // Screen refresh requested (Ctrl+B)
} bp_linenoise_result_t;

// I/O callback types
typedef bool (*bp_ln_read_char_fn)(char *c);           // Non-blocking char read, returns true if char available
typedef void (*bp_ln_read_char_blocking_fn)(char *c);  // Blocking char read
typedef void (*bp_ln_write_fn)(const char *s, size_t len);  // Write string

// Completion callback (optional)
typedef void (*bp_ln_completion_fn)(const char *buf, size_t len);

/**
 * @brief Line editing state.
 */
typedef struct {
    char buf[BP_LINENOISE_MAX_LINE + 1];  // Edit buffer
    size_t buflen;      // Buffer capacity
    size_t pos;         // Cursor position (byte offset)
    size_t len;         // Current content length (bytes)
    size_t cols;        // Terminal width
    size_t plen;        // Prompt length (display width)
    const char *prompt; // Prompt string
    
    // History navigation
    int history_index;  // Current position in history (0 = current line)
    char history_save[BP_LINENOISE_MAX_LINE + 1]; // Save current line when browsing history
    
    // I/O callbacks
    bp_ln_read_char_fn try_read;
    bp_ln_read_char_blocking_fn read_blocking;
    bp_ln_write_fn write;
    
    // Optional completion
    bp_ln_completion_fn completion_callback;
    
    // State flags
    bool initialized;
    bool multiline;     // Multi-line mode (not yet implemented)
} bp_linenoise_state_t;

/**
 * @brief Initialize linenoise with I/O callbacks.
 * @param state         State structure to initialize
 * @param try_read      Non-blocking read callback
 * @param read_blocking Blocking read callback
 * @param write         Write callback
 * @param cols          Terminal width (columns)
 */
void bp_linenoise_init(bp_linenoise_state_t *state,
                       bp_ln_read_char_fn try_read,
                       bp_ln_read_char_blocking_fn read_blocking,
                       bp_ln_write_fn write,
                       size_t cols);

/**
 * @brief Start line editing session.
 * @param state   State structure
 * @param prompt  Prompt string to display
 */
void bp_linenoise_start(bp_linenoise_state_t *state, const char *prompt);

/**
 * @brief Feed a character to the line editor (non-blocking).
 * @param state  State structure
 * @return Result code indicating edit status
 * 
 * Call this when a character is available. The function will:
 * - Process the character (insert, delete, move cursor, etc.)
 * - Update the display
 * - Return status indicating if line is complete
 */
bp_linenoise_result_t bp_linenoise_feed(bp_linenoise_state_t *state);

/**
 * @brief Stop line editing session.
 * @param state  State structure
 * 
 * Called after bp_linenoise_feed() returns BP_LN_ENTER.
 * Outputs newline and finalizes the edit buffer.
 */
void bp_linenoise_stop(bp_linenoise_state_t *state);

/**
 * @brief Get the edited line buffer.
 * @param state  State structure
 * @return Pointer to null-terminated line buffer
 */
const char *bp_linenoise_get_line(bp_linenoise_state_t *state);

/**
 * @brief Get length of edited line.
 * @param state  State structure
 * @return Length in bytes
 */
size_t bp_linenoise_get_len(bp_linenoise_state_t *state);

/**
 * @brief Add line to history.
 * @param state  State structure
 * @param line   Line to add (copied)
 * @return true on success
 */
bool bp_linenoise_history_add(bp_linenoise_state_t *state, const char *line);

/**
 * @brief Clear history.
 * @param state  State structure
 */
void bp_linenoise_history_clear(bp_linenoise_state_t *state);

/**
 * @brief Set terminal width.
 * @param state  State structure
 * @param cols   New column count
 */
void bp_linenoise_set_cols(bp_linenoise_state_t *state, size_t cols);

/**
 * @brief Clear screen (Ctrl+L).
 * @param state  State structure
 */
void bp_linenoise_clear_screen(bp_linenoise_state_t *state);

/**
 * @brief Set completion callback.
 * @param state     State structure  
 * @param callback  Completion callback function
 */
void bp_linenoise_set_completion(bp_linenoise_state_t *state, 
                                  bp_ln_completion_fn callback);

// Editing functions (can be called directly for programmatic control)
void bp_linenoise_edit_insert(bp_linenoise_state_t *state, char c);
void bp_linenoise_edit_backspace(bp_linenoise_state_t *state);
void bp_linenoise_edit_delete(bp_linenoise_state_t *state);
void bp_linenoise_edit_move_left(bp_linenoise_state_t *state);
void bp_linenoise_edit_move_right(bp_linenoise_state_t *state);
void bp_linenoise_edit_move_home(bp_linenoise_state_t *state);
void bp_linenoise_edit_move_end(bp_linenoise_state_t *state);
void bp_linenoise_edit_history_prev(bp_linenoise_state_t *state);
void bp_linenoise_edit_history_next(bp_linenoise_state_t *state);
void bp_linenoise_edit_delete_line(bp_linenoise_state_t *state);
void bp_linenoise_edit_delete_to_end(bp_linenoise_state_t *state);
void bp_linenoise_edit_delete_word(bp_linenoise_state_t *state);
void bp_linenoise_edit_transpose(bp_linenoise_state_t *state);

#endif // BP_LINENOISE_H
