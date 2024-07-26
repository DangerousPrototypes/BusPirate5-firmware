/*
 * This file is part of the Bus Pirate project (http://code.google.com/p/the-bus-pirate/).
 *
 * Written and maintained by the Bus Pirate project.
 *
 * To the extent possible under law, the project has
 * waived all copyright and related or neighboring rights to Bus Pirate. This
 * work is published from United States.
 *
 * For details see: http://creativecommons.org/publicdomain/zero/1.0/.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */
#define BP_ADC_PROBE  BIO2
#define BP_AUX0 BIO3
#define BP_MOSI BIO4
#define BP_CLK  BIO5
#define BP_MISO BIO6
#define BP_CS   BIO7
void script_mode(void);
unsigned char binBBpindirectionset(unsigned char inByte);
unsigned char binBBpinset(unsigned char inByte);

typedef struct _binmode{
	void (*binmode_service)(void);
} binmode_t;

extern struct _binmode binmodes[];

