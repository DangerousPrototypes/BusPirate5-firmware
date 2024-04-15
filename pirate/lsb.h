// format a number of num_bits into LSB format
uint32_t lsb_convert(uint32_t d, uint8_t num_bits);
// format a number of num_bits into MSB or LSB format
// bit_order: 0=MSB, 1=LSB
void lsb_get(uint32_t *d, uint8_t num_bits, bool bit_order);