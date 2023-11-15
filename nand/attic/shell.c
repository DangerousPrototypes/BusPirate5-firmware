/**
 * @file		shell.c
 * @author		Andrew Loebs
 * @brief		Implementation file of the shell module
 *
 */

#include "shell.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#include "shell_cmd.h"

// defines
#define RECEIVE_BUFFER_LEN 128 // max command + args length supported

// private function prototypes
static void receive_buffer_reset(void);
static bool receive_buffer_is_full(void);
static size_t receive_buffer_len(void);
static void receive_buffer_push(char c);

static bool try_get_char(char *out);
static void echo(char c);
static void put_prompt(void);

// private variables
static char receive_buffer[RECEIVE_BUFFER_LEN];
static size_t receive_index;

// public function definitions
void shell_init(void)
{
    receive_buffer_reset();
    // start with welcome message and prompt
    shell_put_newline();
    shell_put_newline();
    shell_prints_line("==== shell started! ====");
    shell_put_newline();
    put_prompt();
}

void shell_tick(void)
{
    char c;
    if (try_get_char(&c)) {
        // ignore newline -- we only expect carriage return from the client
        if (c != '\n') {
            echo(c);
            receive_buffer_push(c);
            // if carriage return, process command
            if ('\r' == c) {
                shell_cmd_process(receive_buffer, receive_buffer_len());
                shell_put_newline(); // more readable with the extra newline
                put_prompt();
                receive_buffer_reset();
            }
            // else, check for rx full
            else if (receive_buffer_is_full()) {
                shell_printf_line("Receive buffer full (limit %d). Resetting receive buffer.",
                                  RECEIVE_BUFFER_LEN);
                receive_buffer_reset();
            }
        }
    }
}

void shell_print(const char *buff, size_t len)
{
    for (int i = 0; i < len; i++) {
        putchar(buff[i]);
    }
}

void shell_prints(const char *string)
{
    while (*string) {
        putchar(*string);
        string++;
    }
}

void shell_prints_line(const char *string)
{
    // this gives us control over the newline behavior (over puts())
    while (*string) {
        putchar(*string);
        string++;
    }
    shell_put_newline();
}

void shell_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void shell_printf_line(const char *format, ...)
{
    // handle the printf
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    // newline
    shell_put_newline();
}

void shell_put_newline(void)
{
    putchar('\r');
    putchar('\n');
}

// private function definitions
static void receive_buffer_reset(void)
{
    receive_buffer[0] = 0;
    receive_index = 0;
}

static bool receive_buffer_is_full(void)
{
    return receive_index >= (RECEIVE_BUFFER_LEN - 1);
}

static size_t receive_buffer_len(void)
{
    return receive_index;
}

static void receive_buffer_push(char c)
{
    if ('\b' == c) {
        if (receive_index != 0) {
            receive_index--;
            receive_buffer[receive_index] = 0;
        }
    }
    else {
        receive_buffer[receive_index] = c;
        receive_index++;
    }
}

static bool try_get_char(char *out)
{
    // To get around getchar() blocking, the _read() syscall returns
    // EOF for stdin whenever the UART rx is empty. Because of this,
    // we must check getchar() for EOF so that we know if we have a new
    // character, and rewind stdin when we do not.
    char c = getchar();
    if ((char)EOF == c) {
        rewind(stdin);
        return false;
    }
    else {
        *out = c;
        return true;
    }
}

static void echo(char c)
{
    // handle newline
    if ('\r' == c) {
        shell_put_newline();
    }
    // handle backspace
    else if ('\b' == c) {
        if (receive_index != 0) { // dont backspace prompt
            putchar('\b');
            putchar(' ');
            putchar('\b');
        }
    }
    // else, just echo
    else {
        putchar(c);
    }
}

static void put_prompt(void)
{
    putchar('>');
    putchar(' ');
}
