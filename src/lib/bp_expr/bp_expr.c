/**
 * @file bp_expr.c
 * @brief Lightweight integer expression evaluator implementation.
 * @details Uses recursive descent parsing with operator precedence.
 */

#include "bp_expr.h"
#include "lib/bp_number/bp_number.h"

/*
 * =============================================================================
 * Internal types and constants
 * =============================================================================
 */

#define EXPR_STACK_SIZE 16

typedef struct {
    const char *p;        // Current position
    const char *end;      // End of input
    bp_expr_err_t err;    // Error code
} expr_ctx_t;

/*
 * =============================================================================
 * Tokenizer helpers
 * =============================================================================
 */

static void skip_ws(expr_ctx_t *ctx) {
    while (ctx->p < ctx->end && (*ctx->p == ' ' || *ctx->p == '\t')) {
        ctx->p++;
    }
}

static char peek(expr_ctx_t *ctx) {
    skip_ws(ctx);
    return (ctx->p < ctx->end) ? *ctx->p : '\0';
}

static bool match(expr_ctx_t *ctx, char c) {
    if (peek(ctx) == c) {
        ctx->p++;
        return true;
    }
    return false;
}

static bool match2(expr_ctx_t *ctx, char c1, char c2) {
    skip_ws(ctx);
    if (ctx->p + 1 < ctx->end && ctx->p[0] == c1 && ctx->p[1] == c2) {
        ctx->p += 2;
        return true;
    }
    return false;
}

/*
 * =============================================================================
 * Recursive descent parser
 * Precedence (low to high): | ^ & << >> + - * / % ~ (unary)
 * =============================================================================
 */

// Forward declarations
static bool parse_expr_or(expr_ctx_t *ctx, uint32_t *result);

/**
 * @brief Parse a number literal (0x, 0b, or decimal).
 */
static bool parse_number(expr_ctx_t *ctx, uint32_t *result) {
    skip_ws(ctx);
    const char *start = ctx->p;
    
    if (!bp_num_u32(&ctx->p, result, NULL)) {
        ctx->p = start;
        return false;
    }
    return true;
}

/**
 * @brief Parse primary: number, (expr), or unary operator.
 */
static bool parse_primary(expr_ctx_t *ctx, uint32_t *result) {
    skip_ws(ctx);
    
    // Unary minus
    if (match(ctx, '-')) {
        if (!parse_primary(ctx, result)) return false;
        *result = (uint32_t)(-(int32_t)*result);
        return true;
    }
    
    // Unary bitwise NOT
    if (match(ctx, '~')) {
        if (!parse_primary(ctx, result)) return false;
        *result = ~(*result);
        return true;
    }
    
    // Parenthesized expression
    if (match(ctx, '(')) {
        if (!parse_expr_or(ctx, result)) return false;
        if (!match(ctx, ')')) {
            ctx->err = BP_EXPR_ERR_PAREN;
            return false;
        }
        return true;
    }
    
    // Number literal
    if (parse_number(ctx, result)) {
        return true;
    }
    
    ctx->err = BP_EXPR_ERR_SYNTAX;
    return false;
}

/**
 * @brief Parse multiplicative: * / %
 */
static bool parse_mul(expr_ctx_t *ctx, uint32_t *result) {
    if (!parse_primary(ctx, result)) return false;
    
    for (;;) {
        if (match(ctx, '*')) {
            uint32_t rhs;
            if (!parse_primary(ctx, &rhs)) return false;
            *result *= rhs;
        } else if (match(ctx, '/')) {
            uint32_t rhs;
            if (!parse_primary(ctx, &rhs)) return false;
            if (rhs == 0) {
                ctx->err = BP_EXPR_ERR_DIV_ZERO;
                return false;
            }
            *result /= rhs;
        } else if (match(ctx, '%')) {
            uint32_t rhs;
            if (!parse_primary(ctx, &rhs)) return false;
            if (rhs == 0) {
                ctx->err = BP_EXPR_ERR_DIV_ZERO;
                return false;
            }
            *result %= rhs;
        } else {
            break;
        }
    }
    return true;
}

/**
 * @brief Parse additive: + -
 */
static bool parse_add(expr_ctx_t *ctx, uint32_t *result) {
    if (!parse_mul(ctx, result)) return false;
    
    for (;;) {
        if (match(ctx, '+')) {
            uint32_t rhs;
            if (!parse_mul(ctx, &rhs)) return false;
            *result += rhs;
        } else if (match(ctx, '-')) {
            uint32_t rhs;
            if (!parse_mul(ctx, &rhs)) return false;
            *result -= rhs;
        } else {
            break;
        }
    }
    return true;
}

/**
 * @brief Parse shift: << >>
 */
static bool parse_shift(expr_ctx_t *ctx, uint32_t *result) {
    if (!parse_add(ctx, result)) return false;
    
    for (;;) {
        if (match2(ctx, '<', '<')) {
            uint32_t rhs;
            if (!parse_add(ctx, &rhs)) return false;
            *result <<= (rhs & 31);  // Mask to valid shift range
        } else if (match2(ctx, '>', '>')) {
            uint32_t rhs;
            if (!parse_add(ctx, &rhs)) return false;
            *result >>= (rhs & 31);
        } else {
            break;
        }
    }
    return true;
}

/**
 * @brief Parse bitwise AND: &
 */
static bool parse_and(expr_ctx_t *ctx, uint32_t *result) {
    if (!parse_shift(ctx, result)) return false;
    
    for (;;) {
        // Don't match && (logical and)
        skip_ws(ctx);
        if (ctx->p < ctx->end && ctx->p[0] == '&' && 
            (ctx->p + 1 >= ctx->end || ctx->p[1] != '&')) {
            ctx->p++;
            uint32_t rhs;
            if (!parse_shift(ctx, &rhs)) return false;
            *result &= rhs;
        } else {
            break;
        }
    }
    return true;
}

/**
 * @brief Parse bitwise XOR: ^
 */
static bool parse_xor(expr_ctx_t *ctx, uint32_t *result) {
    if (!parse_and(ctx, result)) return false;
    
    for (;;) {
        if (match(ctx, '^')) {
            uint32_t rhs;
            if (!parse_and(ctx, &rhs)) return false;
            *result ^= rhs;
        } else {
            break;
        }
    }
    return true;
}

/**
 * @brief Parse bitwise OR: |
 */
static bool parse_expr_or(expr_ctx_t *ctx, uint32_t *result) {
    if (!parse_xor(ctx, result)) return false;
    
    for (;;) {
        // Don't match || (logical or)
        skip_ws(ctx);
        if (ctx->p < ctx->end && ctx->p[0] == '|' && 
            (ctx->p + 1 >= ctx->end || ctx->p[1] != '|')) {
            ctx->p++;
            uint32_t rhs;
            if (!parse_xor(ctx, &rhs)) return false;
            *result |= rhs;
        } else {
            break;
        }
    }
    return true;
}

/*
 * =============================================================================
 * Public API
 * =============================================================================
 */

bool bp_expr_eval_n(const char *expr, size_t len, uint32_t *result, bp_expr_err_t *err) {
    if (!expr || len == 0) {
        if (err) *err = BP_EXPR_ERR_EMPTY;
        return false;
    }
    
    expr_ctx_t ctx = {
        .p = expr,
        .end = expr + len,
        .err = BP_EXPR_OK
    };
    
    *result = 0;
    
    if (!parse_expr_or(&ctx, result)) {
        if (err) *err = ctx.err;
        return false;
    }
    
    // Check for trailing garbage
    skip_ws(&ctx);
    if (ctx.p < ctx.end && *ctx.p != '\0') {
        if (err) *err = BP_EXPR_ERR_SYNTAX;
        return false;
    }
    
    if (err) *err = BP_EXPR_OK;
    return true;
}

bool bp_expr_eval(const char *expr, uint32_t *result, bp_expr_err_t *err) {
    if (!expr) {
        if (err) *err = BP_EXPR_ERR_EMPTY;
        return false;
    }
    
    size_t len = 0;
    while (expr[len]) len++;
    
    return bp_expr_eval_n(expr, len, result, err);
}

const char *bp_expr_strerror(bp_expr_err_t err) {
    switch (err) {
        case BP_EXPR_OK:           return "OK";
        case BP_EXPR_ERR_SYNTAX:   return "Syntax error";
        case BP_EXPR_ERR_PAREN:    return "Mismatched parentheses";
        case BP_EXPR_ERR_OVERFLOW: return "Expression too complex";
        case BP_EXPR_ERR_DIV_ZERO: return "Division by zero";
        case BP_EXPR_ERR_EMPTY:    return "Empty expression";
        default:                   return "Unknown error";
    }
}
