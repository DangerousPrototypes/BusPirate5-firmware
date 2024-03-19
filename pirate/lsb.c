#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"

// format a number of num_bits into LSB format
uint32_t lsb_convert(uint32_t d, uint8_t num_bits){
	uint32_t result=0, mask=0x80000000;
	for(uint32_t i=0; i<32; i++){
		if((d)&mask){
			result|=(1<<(i));	
		}
		mask>>=1;
	}
	return (result>>(32-num_bits));
}

// format a number of num_bits into MSB or LSB format
// bit_order: 0=MSB, 1=LSB
void lsb_get(uint32_t *d, uint8_t num_bits, bool bit_order){
	if(!bit_order) return; // 0=MSB
	(*d) = lsb_convert(*d, num_bits);
}