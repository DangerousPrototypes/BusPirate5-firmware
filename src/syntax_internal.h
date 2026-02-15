/**
 * @file syntax_internal.h
 * @brief Internal types and declarations for syntax processing.
 * @details Shared between syntax_compile.c, syntax_run.c, and syntax_post.c.
 *          This header is NOT part of the public API.
 */

#ifndef SYNTAX_INTERNAL_H
#define SYNTAX_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include "bytecode.h"

/// Maximum number of bytecode instructions
#define SYN_MAX_LENGTH 1024

/**
 * @brief Syntax I/O buffers for bytecode processing.
 * @details Contains output bytecode (compiled) and input bytecode (results).
 */
struct _syntax_io {
    struct _bytecode out[SYN_MAX_LENGTH];  ///< Compiled bytecode output
    struct _bytecode in[SYN_MAX_LENGTH];   ///< Execution results input
    uint32_t out_cnt;                       ///< Number of output bytecodes
    uint32_t in_cnt;                        ///< Number of input bytecodes
};

/**
 * @brief Output formatting state for post-processing.
 */
struct _output_info {
    uint8_t previous_command;       ///< Last processed command type
    uint8_t previous_number_format; ///< Last number format used
    uint8_t row_length;             ///< Numbers per row
    uint8_t row_counter;            ///< Current position in row
};

/// Global syntax I/O state
extern struct _syntax_io syntax_io;

/// Empty bytecode for quick initialization
extern const struct _bytecode bytecode_empty;

/*
 * =============================================================================
 * Compile phase (syntax_compile.c)
 * =============================================================================
 */

/**
 * @brief Command symbol to bytecode mapping.
 */
struct _syntax_compile_commands_t {
    char symbol;    ///< Command character
    uint8_t code;   ///< Bytecode instruction
};

/// Command symbol lookup table
extern const struct _syntax_compile_commands_t syntax_compile_commands[];

/// Number of compile commands
extern const size_t syntax_compile_commands_count;

/*
 * =============================================================================
 * Run phase (syntax_run.c)
 * =============================================================================
 */

/// Function pointer type for run phase handlers
typedef void (*syntax_run_func_ptr_t)(struct _syntax_io *io, uint32_t pos);

/// Run function dispatch table
extern syntax_run_func_ptr_t syntax_run_func[];

/*
 * =============================================================================
 * Post-process phase (syntax_post.c)
 * =============================================================================
 */

/// Function pointer type for post-process handlers
typedef void (*syntax_post_func_ptr_t)(struct _bytecode *in, struct _output_info *info);

/// Post-process function dispatch table
extern syntax_post_func_ptr_t syntax_post_func[];

/**
 * @brief Format and print write operation result.
 * @param in    Bytecode with result data
 * @param info  Output state
 */
void postprocess_mode_write(struct _bytecode *in, struct _output_info *info);

#endif // SYNTAX_INTERNAL_H
