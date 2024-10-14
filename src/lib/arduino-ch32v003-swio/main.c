#include <stdio.h>
#include <avr/io.h>
#include <stdint.h>
#include "uart.h"
#include "swio.h"

#define TARGET_POWER_PORT  PORTB
#define TARGET_POWER_DDR   DDRB
#define TARGET_POWER_BIT   1

void target_power(int x) {
    if (x)
        TARGET_POWER_PORT |= _BV(TARGET_POWER_BIT);
    else
        TARGET_POWER_PORT &= ~_BV(TARGET_POWER_BIT);
}

#define PROTOCOL_START     '!'
#define PROTOCOL_ACK       '+'
#define PROTOCOL_TEST      '?'
#define PROTOCOL_POWER_ON  'p'
#define PROTOCOL_POWER_OFF 'P'
#define PROTOCOL_WRITE_REG 'w'
#define PROTOCOL_READ_REG  'r'

int main() {
    uint8_t reg;
    uint32_t val;

    // Make the target reset pin an output.
    TARGET_POWER_DDR |= _BV(TARGET_POWER_BIT);

    uart_init();
    swio_init();

    fputc(PROTOCOL_START, uart);
    while (1) {
        switch (fgetc(uart)) {
            case PROTOCOL_TEST:
                fputc(PROTOCOL_ACK, uart);
                break;
            case PROTOCOL_POWER_ON:
                target_power(1);
                fputc(PROTOCOL_ACK, uart);
                break;
            case PROTOCOL_POWER_OFF:
                target_power(0);
                fputc(PROTOCOL_ACK, uart);
                break;
            case PROTOCOL_WRITE_REG:
                fread(&reg, sizeof(uint8_t), 1, uart);
                fread(&val, sizeof(uint32_t), 1, uart);
                swio_write_reg(reg, val);
                fputc(PROTOCOL_ACK, uart);
                break;
            case PROTOCOL_READ_REG:
                fread(&reg, sizeof(uint8_t), 1, uart);
                val = swio_read_reg(reg);
                fwrite(&val, sizeof(uint32_t), 1, uart);
                break;
        }
    }
}
