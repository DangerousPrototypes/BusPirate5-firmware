/**
 * @file bp_expr_test.c
 * @brief Unit tests for bp_expr expression evaluator.
 * @details Standalone test - compile with:
 *          gcc -I../.. -I../../lib bp_expr_test.c bp_expr.c ../bp_number/bp_number.c -o bp_expr_test
 *          Or run: make -f bp_expr_test.mk
 */

#include <stdio.h>
#include <string.h>
#include "bp_expr.h"

#define TEST(name) static int test_##name(void)
#define RUN(name) do { \
    printf("  %-40s ", #name); \
    if (test_##name()) { printf("PASS\n"); pass++; } \
    else { printf("FAIL\n"); fail++; } \
} while(0)

#define EXPECT_EQ(expr, expected) do { \
    uint32_t result = 0; \
    bp_expr_err_t err; \
    if (!bp_expr_eval(expr, &result, &err)) { \
        printf("\n    ERROR: \"%s\" failed with %s", expr, bp_expr_strerror(err)); \
        return 0; \
    } \
    if (result != (expected)) { \
        printf("\n    MISMATCH: \"%s\" = 0x%X, expected 0x%X", expr, result, (expected)); \
        return 0; \
    } \
} while(0)

#define EXPECT_ERR(expr, expected_err) do { \
    uint32_t result = 0; \
    bp_expr_err_t err; \
    if (bp_expr_eval(expr, &result, &err)) { \
        printf("\n    UNEXPECTED SUCCESS: \"%s\" = 0x%X", expr, result); \
        return 0; \
    } \
    if (err != (expected_err)) { \
        printf("\n    WRONG ERROR: \"%s\" got %s, expected %s", expr, \
               bp_expr_strerror(err), bp_expr_strerror(expected_err)); \
        return 0; \
    } \
} while(0)

/*
 * =============================================================================
 * Test cases
 * =============================================================================
 */

TEST(decimal_literals) {
    EXPECT_EQ("0", 0);
    EXPECT_EQ("42", 42);
    EXPECT_EQ("255", 255);
    EXPECT_EQ("65535", 65535);
    EXPECT_EQ("4294967295", 0xFFFFFFFF);
    return 1;
}

TEST(hex_literals) {
    EXPECT_EQ("0x0", 0);
    EXPECT_EQ("0xFF", 255);
    EXPECT_EQ("0xff", 255);
    EXPECT_EQ("0xDEADBEEF", 0xDEADBEEF);
    EXPECT_EQ("0x12345678", 0x12345678);
    return 1;
}

TEST(binary_literals) {
    EXPECT_EQ("0b0", 0);
    EXPECT_EQ("0b1", 1);
    EXPECT_EQ("0b1010", 10);
    EXPECT_EQ("0b11111111", 255);
    EXPECT_EQ("0B1010", 10);  // uppercase B
    return 1;
}

TEST(addition) {
    EXPECT_EQ("1 + 2", 3);
    EXPECT_EQ("0 + 0", 0);
    EXPECT_EQ("100 + 200", 300);
    EXPECT_EQ("0xFFFFFFFF + 1", 0);  // overflow wraps
    return 1;
}

TEST(subtraction) {
    EXPECT_EQ("10 - 3", 7);
    EXPECT_EQ("0 - 1", 0xFFFFFFFF);  // underflow wraps
    EXPECT_EQ("100 - 100", 0);
    return 1;
}

TEST(multiplication) {
    EXPECT_EQ("6 * 7", 42);
    EXPECT_EQ("0 * 100", 0);
    EXPECT_EQ("256 * 256", 65536);
    return 1;
}

TEST(division) {
    EXPECT_EQ("100 / 4", 25);
    EXPECT_EQ("7 / 2", 3);  // integer division
    EXPECT_EQ("0 / 5", 0);
    return 1;
}

TEST(modulo) {
    EXPECT_EQ("17 % 5", 2);
    EXPECT_EQ("10 % 10", 0);
    EXPECT_EQ("7 % 3", 1);
    return 1;
}

TEST(bitwise_and) {
    EXPECT_EQ("0xFF & 0x0F", 0x0F);
    EXPECT_EQ("0xAA & 0x55", 0x00);
    EXPECT_EQ("0xFF & 0xFF", 0xFF);
    return 1;
}

TEST(bitwise_or) {
    EXPECT_EQ("0xF0 | 0x0F", 0xFF);
    EXPECT_EQ("0x00 | 0x00", 0x00);
    EXPECT_EQ("0xAA | 0x55", 0xFF);
    return 1;
}

TEST(bitwise_xor) {
    EXPECT_EQ("0xFF ^ 0x0F", 0xF0);
    EXPECT_EQ("0xFF ^ 0xFF", 0x00);
    EXPECT_EQ("0xAA ^ 0x55", 0xFF);
    return 1;
}

TEST(left_shift) {
    EXPECT_EQ("1 << 0", 1);
    EXPECT_EQ("1 << 4", 16);
    EXPECT_EQ("1 << 31", 0x80000000);
    EXPECT_EQ("0xFF << 8", 0xFF00);
    return 1;
}

TEST(right_shift) {
    EXPECT_EQ("256 >> 4", 16);
    EXPECT_EQ("0xFF >> 4", 0x0F);
    EXPECT_EQ("0x80000000 >> 31", 1);
    return 1;
}

TEST(unary_not) {
    EXPECT_EQ("~0", 0xFFFFFFFF);
    EXPECT_EQ("~0xFF", 0xFFFFFF00);
    EXPECT_EQ("~0xFFFFFFFF", 0);
    return 1;
}

TEST(unary_minus) {
    EXPECT_EQ("-1", 0xFFFFFFFF);
    EXPECT_EQ("-0", 0);
    EXPECT_EQ("--1", 1);  // double negative
    return 1;
}

TEST(parentheses) {
    EXPECT_EQ("(1 + 2) * 3", 9);
    EXPECT_EQ("1 + (2 * 3)", 7);
    EXPECT_EQ("((1 + 2))", 3);
    EXPECT_EQ("(((42)))", 42);
    return 1;
}

TEST(precedence) {
    // * before +
    EXPECT_EQ("1 + 2 * 3", 7);
    EXPECT_EQ("2 * 3 + 1", 7);
    
    // & before |
    EXPECT_EQ("0xF0 | 0x0F & 0x0F", 0xF0 | (0x0F & 0x0F));
    
    // << before &
    EXPECT_EQ("1 << 4 & 0xFF", (1 << 4) & 0xFF);
    
    // + before <<
    EXPECT_EQ("1 + 1 << 4", (1 + 1) << 4);
    return 1;
}

TEST(complex_expressions) {
    EXPECT_EQ("0xFF & (1 << 4)", 0x10);
    EXPECT_EQ("(0xF0 | 0x0F) ^ 0xFF", 0x00);
    EXPECT_EQ("(1 << 8) - 1", 255);
    EXPECT_EQ("~(1 << 8) & 0xFFFF", 0xFEFF);
    EXPECT_EQ("((0xAB << 8) | 0xCD)", 0xABCD);
    return 1;
}

TEST(whitespace) {
    EXPECT_EQ("  42  ", 42);
    EXPECT_EQ("1+2", 3);
    EXPECT_EQ("1 +  2", 3);
    EXPECT_EQ("  (  1  +  2  )  ", 3);
    return 1;
}

TEST(error_empty) {
    EXPECT_ERR("", BP_EXPR_ERR_EMPTY);
    // NULL test handled separately to avoid format warnings
    {
        uint32_t result = 0;
        bp_expr_err_t err;
        if (bp_expr_eval(NULL, &result, &err) || err != BP_EXPR_ERR_EMPTY) {
            printf("\n    NULL test failed");
            return 0;
        }
    }
    return 1;
}

TEST(error_syntax) {
    EXPECT_ERR("abc", BP_EXPR_ERR_SYNTAX);
    EXPECT_ERR("1 +", BP_EXPR_ERR_SYNTAX);
    EXPECT_ERR("+ 1", BP_EXPR_ERR_SYNTAX);
    EXPECT_ERR("1 2", BP_EXPR_ERR_SYNTAX);
    return 1;
}

TEST(error_paren) {
    EXPECT_ERR("(1 + 2", BP_EXPR_ERR_PAREN);
    EXPECT_ERR("((1)", BP_EXPR_ERR_PAREN);
    return 1;
}

TEST(error_div_zero) {
    EXPECT_ERR("1 / 0", BP_EXPR_ERR_DIV_ZERO);
    EXPECT_ERR("1 % 0", BP_EXPR_ERR_DIV_ZERO);
    return 1;
}

/*
 * =============================================================================
 * Test runner
 * =============================================================================
 */

int main(void) {
    int pass = 0, fail = 0;
    
    printf("bp_expr unit tests\n");
    printf("==================\n\n");
    
    printf("Literals:\n");
    RUN(decimal_literals);
    RUN(hex_literals);
    RUN(binary_literals);
    
    printf("\nArithmetic:\n");
    RUN(addition);
    RUN(subtraction);
    RUN(multiplication);
    RUN(division);
    RUN(modulo);
    
    printf("\nBitwise:\n");
    RUN(bitwise_and);
    RUN(bitwise_or);
    RUN(bitwise_xor);
    RUN(left_shift);
    RUN(right_shift);
    
    printf("\nUnary:\n");
    RUN(unary_not);
    RUN(unary_minus);
    
    printf("\nGrouping:\n");
    RUN(parentheses);
    RUN(precedence);
    RUN(complex_expressions);
    RUN(whitespace);
    
    printf("\nError handling:\n");
    RUN(error_empty);
    RUN(error_syntax);
    RUN(error_paren);
    RUN(error_div_zero);
    
    printf("\n==================\n");
    printf("Results: %d passed, %d failed\n", pass, fail);
    
    return fail > 0 ? 1 : 0;
}
