/*
 * test_vt100_keys.c — Host-side unit tests for vt100_keys decoder
 *
 * Build and run:
 *   gcc -I../src -o test_vt100_keys test_vt100_keys.c ../src/lib/vt100_keys/vt100_keys.c && ./test_vt100_keys
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "lib/vt100_keys/vt100_keys.h"

/* ── Fake input stream ──────────────────────────────────────────── */

static const char* fake_input;
static int         fake_pos;
static int         fake_len;

static void fake_set(const char* data, int len) {
    fake_input = data;
    fake_pos   = 0;
    fake_len   = len;
}

static int fake_blocking(char* c) {
    if (fake_pos >= fake_len) {
        return -1; /* should not happen in well-formed tests */
    }
    *c = fake_input[fake_pos++];
    return 1;
}

static int fake_try(char* c) {
    if (fake_pos >= fake_len) {
        return 0;
    }
    *c = fake_input[fake_pos++];
    return 1;
}

/* ── Helpers ────────────────────────────────────────────────────── */

static vt100_key_state_t state;

static void init(void) {
    vt100_key_init(&state, fake_blocking, fake_try);
}

static int read_key(const char* seq, int len) {
    init();
    fake_set(seq, len);
    return vt100_key_read(&state);
}

#define READ_KEY_STR(s) read_key((s), sizeof(s) - 1)

static int tests_run = 0;
static int tests_pass = 0;

#define CHECK(expr, msg)                                             \
    do {                                                             \
        tests_run++;                                                 \
        if (!(expr)) {                                               \
            printf("FAIL: %s  (%s:%d)\n", msg, __FILE__, __LINE__); \
        } else {                                                     \
            tests_pass++;                                            \
        }                                                            \
    } while (0)

/* ── Tests ──────────────────────────────────────────────────────── */

static void test_printable(void) {
    CHECK(READ_KEY_STR("a") == 'a', "lowercase a");
    CHECK(READ_KEY_STR("Z") == 'Z', "uppercase Z");
    CHECK(READ_KEY_STR(" ") == ' ', "space");
    CHECK(READ_KEY_STR("~") == '~', "tilde");
}

static void test_ctrl_keys(void) {
    CHECK(read_key("\x01", 1) == VT100_KEY_CTRL_A, "Ctrl-A");
    CHECK(read_key("\x03", 1) == VT100_KEY_CTRL_C, "Ctrl-C");
    CHECK(read_key("\x11", 1) == VT100_KEY_CTRL_Q, "Ctrl-Q");
    CHECK(read_key("\x0d", 1) == VT100_KEY_ENTER,  "Enter");
    CHECK(read_key("\x7f", 1) == VT100_KEY_BACKSPACE, "Backspace");
    CHECK(read_key("\x09", 1) == VT100_KEY_TAB,    "Tab");
}

static void test_arrow_keys(void) {
    CHECK(READ_KEY_STR("\x1b[A") == VT100_KEY_UP,    "Up arrow");
    CHECK(READ_KEY_STR("\x1b[B") == VT100_KEY_DOWN,  "Down arrow");
    CHECK(READ_KEY_STR("\x1b[C") == VT100_KEY_RIGHT, "Right arrow");
    CHECK(READ_KEY_STR("\x1b[D") == VT100_KEY_LEFT,  "Left arrow");
}

static void test_home_end_csi(void) {
    CHECK(READ_KEY_STR("\x1b[H") == VT100_KEY_HOME, "Home CSI letter");
    CHECK(READ_KEY_STR("\x1b[F") == VT100_KEY_END,  "End CSI letter");
}

static void test_single_digit_tilde(void) {
    CHECK(READ_KEY_STR("\x1b[1~") == VT100_KEY_HOME,     "Home ~");
    CHECK(READ_KEY_STR("\x1b[2~") == VT100_KEY_INSERT,   "Insert ~");
    CHECK(READ_KEY_STR("\x1b[3~") == VT100_KEY_DELETE,    "Delete ~");
    CHECK(READ_KEY_STR("\x1b[4~") == VT100_KEY_END,      "End ~");
    CHECK(READ_KEY_STR("\x1b[5~") == VT100_KEY_PAGEUP,   "PageUp ~");
    CHECK(READ_KEY_STR("\x1b[6~") == VT100_KEY_PAGEDOWN, "PageDown ~");
    CHECK(READ_KEY_STR("\x1b[7~") == VT100_KEY_HOME,     "Home rxvt ~");
    CHECK(READ_KEY_STR("\x1b[8~") == VT100_KEY_END,      "End rxvt ~");
}

static void test_function_keys_tilde(void) {
    CHECK(READ_KEY_STR("\x1b[11~") == VT100_KEY_F1,  "F1");
    CHECK(READ_KEY_STR("\x1b[12~") == VT100_KEY_F2,  "F2");
    CHECK(READ_KEY_STR("\x1b[13~") == VT100_KEY_F3,  "F3");
    CHECK(READ_KEY_STR("\x1b[14~") == VT100_KEY_F4,  "F4");
    CHECK(READ_KEY_STR("\x1b[15~") == VT100_KEY_F5,  "F5");
    CHECK(READ_KEY_STR("\x1b[17~") == VT100_KEY_F6,  "F6");
    CHECK(READ_KEY_STR("\x1b[18~") == VT100_KEY_F7,  "F7");
    CHECK(READ_KEY_STR("\x1b[19~") == VT100_KEY_F8,  "F8");
    CHECK(READ_KEY_STR("\x1b[20~") == VT100_KEY_F9,  "F9");
    CHECK(READ_KEY_STR("\x1b[21~") == VT100_KEY_F10, "F10");
    CHECK(READ_KEY_STR("\x1b[23~") == VT100_KEY_F11, "F11");
    CHECK(READ_KEY_STR("\x1b[24~") == VT100_KEY_F12, "F12");
}

static void test_ss3_sequences(void) {
    CHECK(READ_KEY_STR("\x1bOH") == VT100_KEY_HOME, "Home SS3");
    CHECK(READ_KEY_STR("\x1bOF") == VT100_KEY_END,  "End SS3");
    CHECK(READ_KEY_STR("\x1bOP") == VT100_KEY_F1,   "F1 SS3");
    CHECK(READ_KEY_STR("\x1bOQ") == VT100_KEY_F2,   "F2 SS3");
    CHECK(READ_KEY_STR("\x1bOR") == VT100_KEY_F3,   "F3 SS3");
    CHECK(READ_KEY_STR("\x1bOS") == VT100_KEY_F4,   "F4 SS3");
}

static void test_bare_esc(void) {
    /* Bare ESC: only one byte, read_try returns 0 */
    CHECK(read_key("\x1b", 1) == VT100_KEY_ESC, "Bare ESC");
}

static void test_pushback(void) {
    init();
    fake_set("ab", 2);

    int k = vt100_key_read(&state);
    CHECK(k == 'a', "pushback: first read");

    vt100_key_unget(&state, k);
    int k2 = vt100_key_read(&state);
    CHECK(k2 == 'a', "pushback: unget returns same key");

    int k3 = vt100_key_read(&state);
    CHECK(k3 == 'b', "pushback: next read advances");
}

static void test_sequential_keys(void) {
    /* Two normal keys in sequence */
    init();
    fake_set("xy", 2);
    CHECK(vt100_key_read(&state) == 'x', "seq: first");
    CHECK(vt100_key_read(&state) == 'y', "seq: second");
}

static void test_escape_then_normal(void) {
    /* ESC [ A followed by a normal 'z' */
    init();
    fake_set("\x1b[Az", 4);
    CHECK(vt100_key_read(&state) == VT100_KEY_UP, "esc+normal: Up");
    CHECK(vt100_key_read(&state) == 'z', "esc+normal: z after");
}

int main(void) {
    test_printable();
    test_ctrl_keys();
    test_arrow_keys();
    test_home_end_csi();
    test_single_digit_tilde();
    test_function_keys_tilde();
    test_ss3_sequences();
    test_bare_esc();
    test_pushback();
    test_sequential_keys();
    test_escape_then_normal();

    printf("\n%d/%d tests passed\n", tests_pass, tests_run);
    return (tests_pass == tests_run) ? 0 : 1;
}
