#include <avr/io.h>
#include "uart.h"

int _uart_putchar(char c, FILE *unused);
int _uart_getchar(FILE *unused);
FILE uart_file = FDEV_SETUP_STREAM(_uart_putchar, _uart_getchar, _FDEV_SETUP_READ | _FDEV_SETUP_WRITE);
FILE *uart = &uart_file;

void uart_init() {
    // Enable 2X clock.
    UCSR0A |= _BV(U2X0);

    // Set the baud rate divider.
    // 16M / 8 / (16 + 1) = 117647 bps (2% error)
    UBRR0L = 16;

    // Enable TX.
    UCSR0B |= _BV(TXEN0) | _BV(RXEN0);
}

int _uart_putchar(char c, FILE *unused) {
    (void)unused;

    // Wait for the UART to become ready.
    while (!(UCSR0A & _BV(UDRE0)))
        ;

    // Send.
    UDR0 = c;

    return 0;
}

int _uart_getchar(FILE *unused) {
    (void)*unused;

    // Wait for a character.
    while (!(UCSR0A & _BV(RXC0)))
        ;

    return UDR0;
}
