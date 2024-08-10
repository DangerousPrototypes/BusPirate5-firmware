#ifndef SWIO_H
#define SWIO_H

// SWIO on PB0
//#define SWIO_DDR  DDRB
//#define SWIO_PORT PORTB
//#define SWIO_PIN  PINB
//#define SWIO_BIT  0

void swio_init();
void swio_write_reg(uint8_t addr, uint32_t val);
uint32_t swio_read_reg(uint8_t addr);

#endif
