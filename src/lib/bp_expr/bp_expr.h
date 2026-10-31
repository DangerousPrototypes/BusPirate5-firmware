/**
 * @file bp_expr.h
 * @brief Lightweight integer expression evaluator.
 * @details Supports bitwise and arithmetic operations on uint32_t values.
 *          No dynamic memory allocation - uses fixed-size stacks.
 * 
 * Supported operators (by precedence, low to high):
 *   1: |        (bitwise OR)
 *   2: ^        (bitwise XOR)
 *   3: &        (bitwise AND)
 *   4: << >>    (shifts)
 *   5: + -      (add/sub)
 *   6: * / %    (mul/div/mod)
 *   7: ~ -      (unary NOT/negate)
 * 
 * Supports: parentheses, 0x hex, 0b binary, decimal literals
 * 
 * Example: bp_expr_eval("0xFF & (1 << 4)", &result, &err)
 */

#ifndef BP_EXPR_H
#define BP_EXPR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Expression evaluation error codes.
 */
typedef enum {
    BP_EXPR_OK = 0,           /**< Success */
    BP_EXPR_ERR_SYNTAX,       /**< Syntax error */
    BP_EXPR_ERR_PAREN,        /**< Mismatched parentheses */
    BP_EXPR_ERR_OVERFLOW,     /**< Stack overflow (expression too complex) */
    BP_EXPR_ERR_DIV_ZERO,     /**< Division by zero */
    BP_EXPR_ERR_EMPTY,        /**< Empty expression */
} bp_expr_err_t;

/**
 * @brief Evaluate an integer expression.
 * 
 * @param expr   Input expression string (null-terminated)
 * @param result Output: evaluated result
 * @param err    Output: error code (can be NULL)
 * @return true on success, false on error
 * 
 * @note Expression is limited to ~16 nested operations.
 */
bool bp_expr_eval(const char *expr, uint32_t *result, bp_expr_err_t *err);

/**
 * @brief Evaluate expression with explicit length.
 * 
 * @param expr   Input expression string
 * @param len    Length of expression
 * @param result Output: evaluated result
 * @param err    Output: error code (can be NULL)
 * @return true on success, false on error
 */
bool bp_expr_eval_n(const char *expr, size_t len, uint32_t *result, bp_expr_err_t *err);

/**
 * @brief Get error message for error code.
 */
const char *bp_expr_strerror(bp_expr_err_t err);

#endif /* BP_EXPR_H */
