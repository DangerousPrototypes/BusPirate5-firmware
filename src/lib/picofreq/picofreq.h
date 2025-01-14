#ifndef IRIO_PIO_H
#define IRIO_PIO_H

// Function declarations
void freq_counter_init(int pin);
void gate_timer_init(int pin);
void freq_counter_start(void);
void dma_handler(void);
void gpio_fall_irq_handler(uint gpio, uint32_t events);
bool freq_counter_value_ready(void);
int freq_counter_value(void);
int edge_counter_frequency(void);
#endif // IRIO_PIO_H