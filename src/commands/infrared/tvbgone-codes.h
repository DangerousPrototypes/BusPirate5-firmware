/*
This is a simple player for TVBGONE power codes.
Our player is simple because PIC C18 has easy 
reads from program memory. 

This code was written based on the description 
of the data packing method published on the Adafruit 
website. It should be a clean, black-box rewrite, but 
we'll release it as CC 2.5 Attrib & Share Alike 
out of respect for the original authors.

PIC C18 Player (c) Ian Lesnet 2009
for use with IR Toy v1.0 hardware.
http://dangerousprototypes.com

With credits to:

TV-B-Gone Firmware version 1.2
for use with ATtiny85v and v1.2 hardware
(c) Mitch Altman + Limor Fried 2009
Last edits, August 16 2009

With some code from:
Kevin Timmerman & Damien Good 7-Dec-07

Distributed under Creative Commons 2.5 -- Attib & Share Alike

Ported to PIC (18F2550) by Ian Lesnet 2009
*/
//#include "HardwareProfile.h"

//Codes captured from Generation 3 TV-B-Gone by Limor Fried & Mitch Altman
// table of POWER codes
// ported to PIC C18 by Ian Lesnet
// see modified Perl script (parsegen3.pl)
#define NA_CODES
//#define EU_CODES //Incomplete EU power code files to generate EU power codes for PIC
					// see: http://forums.adafruit.com/viewtopic.php?f=23&t=12883&sid=e48df75eefc5975f4e2d7ce3d63e007a

#define uint8_t unsigned char
#define uint16_t unsigned int
//#define freq_to_timerval(x) ((3000000/(unsigned int)x)-1) //this calculates the PR2 PWM value for a 48MHZ pic with 4x prescaler
#define freq_to_timerval(x) (x)

struct IrCode {
  uint16_t timer_val;
  uint8_t numpairs;
  uint8_t bitcompression;
  uint16_t const *times;
  uint8_t const *samples;
};
#define NUM_NA_CODES 128

#ifdef NA_CODES

const uint16_t code_na000Times[] = {
	58, 60,
	58, 2687,
	118, 60,
	237, 60,
	238, 60,
};
const uint8_t code_na000Samples[] = {
		0x68,
		0x20,
		0x80,
		0x40,
		0x03,
		0x10,
		0x41,
		0x00,
		0x80,
		0x00,

};
const struct IrCode code_na000Code = {
	freq_to_timerval(38462),
	25,		// # of pairs
	3,		// # of bits per index
	code_na000Times,  
	code_na000Samples,  
};

const uint16_t code_na001Times[] = {
	50, 100,
	50, 200,
	50, 800,
	400, 400,
};
const uint8_t code_na001Samples[] = {
		0xD5,
		0x41,
		0x11,
		0x00,
		0x14,
		0x44,
		0x6D,
		0x54,
		0x11,
		0x10,
		0x01,
		0x44,
		0x44,

};
const struct IrCode code_na001Code = {
	freq_to_timerval(57143),
	51,		// # of pairs
	2,		// # of bits per index
	code_na001Times,  
	code_na001Samples,  
};

const uint16_t code_na002Times[] = {
	42, 46,
	42, 133,
	42, 7519,
	347, 176,
	347, 177,
};
const uint8_t code_na002Samples[] = {
		0x60,
		0x80,
		0x00,
		0x00,
		0x00,
		0x08,
		0x00,
		0x00,
		0x00,
		0x20,
		0x00,
		0x00,
		0x04,
		0x12,
		0x48,
		0x04,
		0x12,
		0x48,
		0x2A,
		0x02,
		0x00,
		0x00,
		0x00,
		0x00,
		0x20,
		0x00,
		0x00,
		0x00,
		0x80,
		0x00,
		0x00,
		0x10,
		0x49,
		0x20,
		0x10,
		0x49,
		0x20,
		0x80,

};
const struct IrCode code_na002Code = {
	freq_to_timerval(37037),
	99,		// # of pairs
	3,		// # of bits per index
	code_na002Times,  
	code_na002Samples,  
};

const uint16_t code_na003Times[] = {
	26, 185,
	27, 80,
	27, 185,
	27, 4549,
};
const uint8_t code_na003Samples[] = {
		0x15,
		0x5A,
		0x65,
		0x67,
		0x95,
		0x65,
		0x9A,
		0x9B,
		0x95,
		0x5A,
		0x65,
		0x67,
		0x95,
		0x65,
		0x9A,
		0x98,

};
const struct IrCode code_na003Code = {
	freq_to_timerval(38610),
	63,		// # of pairs
	2,		// # of bits per index
	code_na003Times,  
	code_na003Samples,  
};

const uint16_t code_na004Times[] = {
	55, 57,
	55, 170,
	55, 3949,
	55, 9623,
	898, 453,
	900, 226,
};
const uint8_t code_na004Samples[] = {
		0x80,
		0x00,
		0x01,
		0x04,
		0x92,
		0x48,
		0x20,
		0x80,
		0x40,
		0x04,
		0x12,
		0x09,
		0x2A,
		0xBA,

};
const struct IrCode code_na004Code = {
	freq_to_timerval(38610),
	37,		// # of pairs
	3,		// # of bits per index
	code_na004Times,  
	code_na004Samples,  
};

const uint16_t code_na005Times[] = {
	88, 90,
	88, 91,
	88, 181,
	88, 8976,
	177, 91,
};
const uint8_t code_na005Samples[] = {
		0x10,
		0x92,
		0x49,
		0x46,
		0x33,
		0x09,
		0x24,
		0x94,
		0x60,

};
const struct IrCode code_na005Code = {
	freq_to_timerval(35714),
	23,		// # of pairs
	3,		// # of bits per index
	code_na005Times,  
	code_na005Samples,  
};

const uint16_t code_na006Times[] = {
	50, 62,
	50, 172,
	50, 4541,
	448, 466,
	450, 465,
};
const uint8_t code_na006Samples[] = {
		0x64,
		0x90,
		0x00,
		0x04,
		0x90,
		0x00,
		0x00,
		0x80,
		0x00,
		0x04,
		0x12,
		0x49,
		0x2A,
		0x12,
		0x40,
		0x00,
		0x12,
		0x40,
		0x00,
		0x02,
		0x00,
		0x00,
		0x10,
		0x49,
		0x24,
		0x80,

};
const struct IrCode code_na006Code = {
	freq_to_timerval(38462),
	67,		// # of pairs
	3,		// # of bits per index
	code_na006Times,  
	code_na006Samples,  
};

const uint16_t code_na007Times[] = {
	49, 49,
	49, 50,
	49, 410,
	49, 510,
	49, 12107,
};
const uint8_t code_na007Samples[] = {
		0x09,
		0x94,
		0x53,
		0x29,
		0x94,
		0xD9,
		0x85,
		0x32,
		0x8A,
		0x65,
		0x32,
		0x9B,
		0x20,

};
const struct IrCode code_na007Code = {
	freq_to_timerval(39216),
	33,		// # of pairs
	3,		// # of bits per index
	code_na007Times,  
	code_na007Samples,  
};

const uint16_t code_na008Times[] = {
	56, 58,
	56, 170,
	56, 4011,
	898, 450,
	900, 449,
};
const uint8_t code_na008Samples[] = {
		0x64,
		0x00,
		0x49,
		0x00,
		0x92,
		0x00,
		0x20,
		0x82,
		0x01,
		0x04,
		0x10,
		0x48,
		0x2A,
		0x10,
		0x01,
		0x24,
		0x02,
		0x48,
		0x00,
		0x82,
		0x08,
		0x04,
		0x10,
		0x41,
		0x20,
		0x80,

};
const struct IrCode code_na008Code = {
	freq_to_timerval(38462),
	67,		// # of pairs
	3,		// # of bits per index
	code_na008Times,  
	code_na008Samples,  
};

const uint16_t code_na009Times[] = {
	53, 56,
	53, 171,
	53, 3950,
	53, 9599,
	898, 451,
	900, 226,
};
const uint8_t code_na009Samples[] = {
		0x84,
		0x90,
		0x00,
		0x20,
		0x80,
		0x08,
		0x00,
		0x00,
		0x09,
		0x24,
		0x92,
		0x40,
		0x0A,
		0xBA,

};
const struct IrCode code_na009Code = {
	freq_to_timerval(38462),
	37,		// # of pairs
	3,		// # of bits per index
	code_na009Times,  
	code_na009Samples,  
};

const uint16_t code_na010Times[] = {
	51, 55,
	51, 158,
	51, 2286,
	841, 419,
};
const uint8_t code_na010Samples[] = {
		0xD4,
		0x00,
		0x15,
		0x10,
		0x25,
		0x00,
		0x05,
		0x44,
		0x09,
		0x40,
		0x01,
		0x51,
		0x00,

};
const struct IrCode code_na010Code = {
	freq_to_timerval(38462),
	51,		// # of pairs
	2,		// # of bits per index
	code_na010Times,  
	code_na010Samples,  
};

const uint16_t code_na011Times[] = {
	55, 55,
	55, 172,
	55, 4039,
	55, 9348,
	884, 442,
	885, 225,
};
const uint8_t code_na011Samples[] = {
		0x80,
		0x00,
		0x41,
		0x04,
		0x92,
		0x08,
		0x24,
		0x90,
		0x40,
		0x00,
		0x02,
		0x09,
		0x2A,
		0xBA,

};
const struct IrCode code_na011Code = {
	freq_to_timerval(38462),
	37,		// # of pairs
	3,		// # of bits per index
	code_na011Times,  
	code_na011Samples,  
};

const uint16_t code_na012Times[] = {
	81, 87,
	81, 254,
	81, 3280,
	331, 336,
	331, 337,
};
const uint8_t code_na012Samples[] = {
		0x64,
		0x12,
		0x08,
		0x24,
		0x00,
		0x08,
		0x20,
		0x10,
		0x09,
		0x2A,
		0x10,
		0x48,
		0x20,
		0x90,
		0x00,
		0x20,
		0x80,
		0x40,
		0x24,
		0x80,

};
const struct IrCode code_na012Code = {
	freq_to_timerval(38462),
	51,		// # of pairs
	3,		// # of bits per index
	code_na012Times,  
	code_na012Samples,  
};

const uint16_t code_na013Times[] = {
	53, 55,
	53, 167,
	53, 2304,
	53, 9369,
	893, 448,
	895, 447,
};
const uint8_t code_na013Samples[] = {
		0x80,
		0x12,
		0x40,
		0x04,
		0x00,
		0x09,
		0x00,
		0x12,
		0x41,
		0x24,
		0x82,
		0x01,
		0x00,
		0x10,
		0x48,
		0x24,
		0xAA,
		0xE8,

};
const struct IrCode code_na013Code = {
	freq_to_timerval(38462),
	47,		// # of pairs
	3,		// # of bits per index
	code_na013Times,  
	code_na013Samples,  
};


/* Duplicate timing table, same as na004 !
const uint16_t code_na014Times[] = {
	55, 57,
	55, 170,
	55, 3949,
	55, 9623,
	898, 453,
	900, 226,
};
*/
const uint8_t code_na014Samples[] = {
		0x80,
		0x00,
		0x09,
		0x04,
		0x92,
		0x40,
		0x24,
		0x80,
		0x00,
		0x00,
		0x12,
		0x49,
		0x2A,
		0xBA,

};
const struct IrCode code_na014Code = {
	freq_to_timerval(38462),
	37,		// # of pairs
	3,		// # of bits per index
	code_na004Times,  
	code_na014Samples,  
};


/* Duplicate timing table, same as na004 !
const uint16_t code_na015Times[] = {
	55, 57,
	55, 170,
	55, 3949,
	55, 9623,
	898, 453,
	900, 226,
};
*/
const uint8_t code_na015Samples[] = {
		0x80,
		0x80,
		0x01,
		0x04,
		0x12,
		0x48,
		0x24,
		0x00,
		0x00,
		0x00,
		0x92,
		0x49,
		0x2A,
		0xBA,

};
const struct IrCode code_na015Code = {
	freq_to_timerval(38462),
	37,		// # of pairs
	3,		// # of bits per index
	code_na004Times,  
	code_na015Samples,  
};

const uint16_t code_na016Times[] = {
	28, 90,
	28, 211,
	28, 2507,
};
const uint8_t code_na016Samples[] = {
		0x54,
		0x04,
		0x10,
		0x00,
		0x95,
		0x01,
		0x04,
		0x00,
		0x00,

};
const struct IrCode code_na016Code = {
	freq_to_timerval(34483),
	33,		// # of pairs
	2,		// # of bits per index
	code_na016Times,  
	code_na016Samples,  
};

const uint16_t code_na017Times[] = {
	56, 57,
	56, 175,
	56, 4150,
	56, 9499,
	898, 227,
	898, 449,
};
const uint8_t code_na017Samples[] = {
		0xA0,
		0x02,
		0x48,
		0x04,
		0x90,
		0x01,
		0x20,
		0x80,
		0x40,
		0x04,
		0x12,
		0x09,
		0x2A,
		0x38,

};
const struct IrCode code_na017Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na017Times,  
	code_na017Samples,  
};

const uint16_t code_na018Times[] = {
	51, 55,
	51, 161,
	51, 2566,
	849, 429,
	849, 430,
};
const uint8_t code_na018Samples[] = {
		0x60,
		0x82,
		0x08,
		0x24,
		0x10,
		0x41,
		0x00,
		0x12,
		0x40,
		0x04,
		0x80,
		0x09,
		0x2A,
		0x02,
		0x08,
		0x20,
		0x90,
		0x41,
		0x04,
		0x00,
		0x49,
		0x00,
		0x12,
		0x00,
		0x24,
		0xA8,
		0x08,
		0x20,
		0x82,
		0x41,
		0x04,
		0x10,
		0x01,
		0x24,
		0x00,
		0x48,
		0x00,
		0x92,
		0xA0,
		0x20,
		0x82,
		0x09,
		0x04,
		0x10,
		0x40,
		0x04,
		0x90,
		0x01,
		0x20,
		0x02,
		0x48,

};
const struct IrCode code_na018Code = {
	freq_to_timerval(38462),
	135,		// # of pairs
	3,		// # of bits per index
	code_na018Times,  
	code_na018Samples,  
};

const uint16_t code_na019Times[] = {
	40, 42,
	40, 124,
	40, 4601,
	325, 163,
	326, 163,
};
const uint8_t code_na019Samples[] = {
		0x60,
		0x10,
		0x40,
		0x04,
		0x80,
		0x09,
		0x00,
		0x00,
		0x00,
		0x00,
		0x10,
		0x00,
		0x20,
		0x10,
		0x00,
		0x20,
		0x80,
		0x00,
		0x0A,
		0x00,
		0x41,
		0x00,
		0x12,
		0x00,
		0x24,
		0x00,
		0x00,
		0x00,
		0x00,
		0x40,
		0x00,
		0x80,
		0x40,
		0x00,
		0x82,
		0x00,
		0x00,
		0x00,

};
const struct IrCode code_na019Code = {
	freq_to_timerval(38462),
	99,		// # of pairs
	3,		// # of bits per index
	code_na019Times,  
	code_na019Samples,  
};

const uint16_t code_na020Times[] = {
	60, 55,
	60, 163,
	60, 4099,
	60, 9698,
	898, 461,
	900, 230,
};
const uint8_t code_na020Samples[] = {
		0x80,
		0x10,
		0x00,
		0x04,
		0x82,
		0x49,
		0x20,
		0x02,
		0x00,
		0x04,
		0x90,
		0x49,
		0x2A,
		0xBA,

};
const struct IrCode code_na020Code = {
	freq_to_timerval(38462),
	37,		// # of pairs
	3,		// # of bits per index
	code_na020Times,  
	code_na020Samples,  
};

const uint16_t code_na021Times[] = {
	48, 52,
	48, 160,
	48, 400,
	48, 2335,
	799, 400,
};
const uint8_t code_na021Samples[] = {
		0x80,
		0x10,
		0x40,
		0x08,
		0x82,
		0x08,
		0x01,
		0xC0,
		0x08,
		0x20,
		0x04,
		0x41,
		0x04,
		0x00,

};
const struct IrCode code_na021Code = {
	freq_to_timerval(38462),
	37,		// # of pairs
	3,		// # of bits per index
	code_na021Times,  
	code_na021Samples,  
};

const uint16_t code_na022Times[] = {
	53, 60,
	53, 175,
	53, 4463,
	53, 9453,
	892, 450,
	895, 225,
};
const uint8_t code_na022Samples[] = {
		0x80,
		0x02,
		0x40,
		0x00,
		0x02,
		0x40,
		0x00,
		0x00,
		0x01,
		0x24,
		0x92,
		0x48,
		0x0A,
		0xBA,

};
const struct IrCode code_na022Code = {
	freq_to_timerval(38462),
	37,		// # of pairs
	3,		// # of bits per index
	code_na022Times,  
	code_na022Samples,  
};

const uint16_t code_na023Times[] = {
	48, 52,
	48, 409,
	48, 504,
	48, 10461,
};
const uint8_t code_na023Samples[] = {
		0xA1,
		0x18,
		0x61,
		0xA1,
		0x18,
		0x7A,
		0x11,
		0x86,
		0x1A,
		0x11,
		0x84,

};
const struct IrCode code_na023Code = {
	freq_to_timerval(40000),
	43,		// # of pairs
	2,		// # of bits per index
	code_na023Times,  
	code_na023Samples,  
};

const uint16_t code_na024Times[] = {
	58, 60,
	58, 2569,
	118, 60,
	237, 60,
	238, 60,
};
const uint8_t code_na024Samples[] = {
		0x69,
		0x24,
		0x10,
		0x40,
		0x03,
		0x12,
		0x48,
		0x20,
		0x80,
		0x00,

};
const struct IrCode code_na024Code = {
	freq_to_timerval(38462),
	25,		// # of pairs
	3,		// # of bits per index
	code_na024Times,  
	code_na024Samples,  
};

const uint16_t code_na025Times[] = {
	84, 90,
	84, 264,
	84, 3470,
	346, 350,
	347, 350,
};
const uint8_t code_na025Samples[] = {
		0x64,
		0x92,
		0x49,
		0x00,
		0x00,
		0x00,
		0x00,
		0x02,
		0x49,
		0x2A,
		0x12,
		0x49,
		0x24,
		0x00,
		0x00,
		0x00,
		0x00,
		0x09,
		0x24,
		0x80,

};
const struct IrCode code_na025Code = {
	freq_to_timerval(38462),
	51,		// # of pairs
	3,		// # of bits per index
	code_na025Times,  
	code_na025Samples,  
};

const uint16_t code_na026Times[] = {
	49, 49,
	49, 50,
	49, 410,
	49, 510,
	49, 12582,
};
const uint8_t code_na026Samples[] = {
		0x09,
		0x94,
		0x53,
		0x65,
		0x32,
		0x99,
		0x85,
		0x32,
		0x8A,
		0x6C,
		0xA6,
		0x53,
		0x20,

};
const struct IrCode code_na026Code = {
	freq_to_timerval(39216),
	33,		// # of pairs
	3,		// # of bits per index
	code_na026Times,  
	code_na026Samples,  
};


/* Duplicate timing table, same as na001 !
const uint16_t code_na027Times[] = {
	50, 100,
	50, 200,
	50, 800,
	400, 400,
};
*/
const uint8_t code_na027Samples[] = {
		0xC5,
		0x41,
		0x11,
		0x10,
		0x14,
		0x44,
		0x6C,
		0x54,
		0x11,
		0x11,
		0x01,
		0x44,
		0x44,

};
const struct IrCode code_na027Code = {
	freq_to_timerval(57143),
	51,		// # of pairs
	2,		// # of bits per index
	code_na001Times,  
	code_na027Samples,  
};

const uint16_t code_na028Times[] = {
	118, 121,
	118, 271,
	118, 4750,
	258, 271,
};
const uint8_t code_na028Samples[] = {
		0xC4,
		0x45,
		0x14,
		0x04,
		0x6C,
		0x44,
		0x51,
		0x40,
		0x44,

};
const struct IrCode code_na028Code = {
	freq_to_timerval(38610),
	35,		// # of pairs
	2,		// # of bits per index
	code_na028Times,  
	code_na028Samples,  
};

const uint16_t code_na029Times[] = {
	88, 90,
	88, 91,
	88, 181,
	177, 91,
	177, 8976,
};
const uint8_t code_na029Samples[] = {
		0x0C,
		0x92,
		0x53,
		0x46,
		0x16,
		0x49,
		0x29,
		0xA2,

};
const struct IrCode code_na029Code = {
	freq_to_timerval(35842),
	21,		// # of pairs
	3,		// # of bits per index
	code_na029Times,  
	code_na029Samples,  
};


/* Duplicate timing table, same as na009 !
const uint16_t code_na030Times[] = {
	53, 56,
	53, 171,
	53, 3950,
	53, 9599,
	898, 451,
	900, 226,
};
*/
const uint8_t code_na030Samples[] = {
		0x80,
		0x00,
		0x41,
		0x04,
		0x12,
		0x08,
		0x20,
		0x00,
		0x00,
		0x04,
		0x92,
		0x49,
		0x2A,
		0xBA,

};
const struct IrCode code_na030Code = {
	freq_to_timerval(38462),
	37,		// # of pairs
	3,		// # of bits per index
	code_na009Times,  
	code_na030Samples,  
};

const uint16_t code_na031Times[] = {
	88, 89,
	88, 90,
	88, 179,
	88, 8977,
	177, 90,
};
const uint8_t code_na031Samples[] = {
		0x06,
		0x12,
		0x49,
		0x46,
		0x32,
		0x61,
		0x24,
		0x94,
		0x60,

};
const struct IrCode code_na031Code = {
	freq_to_timerval(35842),
	23,		// # of pairs
	3,		// # of bits per index
	code_na031Times,  
	code_na031Samples,  
};


/* Duplicate timing table, same as na009 !
const uint16_t code_na032Times[] = {
	53, 56,
	53, 171,
	53, 3950,
	53, 9599,
	898, 451,
	900, 226,
};
*/
const uint8_t code_na032Samples[] = {
		0x80,
		0x00,
		0x41,
		0x04,
		0x12,
		0x08,
		0x20,
		0x80,
		0x00,
		0x04,
		0x12,
		0x49,
		0x2A,
		0xBA,

};
const struct IrCode code_na032Code = {
	freq_to_timerval(38462),
	37,		// # of pairs
	3,		// # of bits per index
	code_na009Times,  
	code_na032Samples,  
};

const uint16_t code_na033Times[] = {
	40, 43,
	40, 122,
	40, 5297,
	334, 156,
	336, 155,
};
const uint8_t code_na033Samples[] = {
		0x60,
		0x10,
		0x40,
		0x04,
		0x80,
		0x09,
		0x00,
		0x00,
		0x00,
		0x00,
		0x10,
		0x00,
		0x20,
		0x82,
		0x00,
		0x20,
		0x00,
		0x00,
		0x0A,
		0x00,
		0x41,
		0x00,
		0x12,
		0x00,
		0x24,
		0x00,
		0x00,
		0x00,
		0x00,
		0x40,
		0x00,
		0x82,
		0x08,
		0x00,
		0x80,
		0x00,
		0x00,
		0x00,

};
const struct IrCode code_na033Code = {
	freq_to_timerval(38462),
	99,		// # of pairs
	3,		// # of bits per index
	code_na033Times,  
	code_na033Samples,  
};


/* Duplicate timing table, same as na004 !
const uint16_t code_na034Times[] = {
	55, 57,
	55, 170,
	55, 3949,
	55, 9623,
	898, 453,
	900, 226,
};
*/
const uint8_t code_na034Samples[] = {
		0x80,
		0x00,
		0x41,
		0x04,
		0x92,
		0x08,
		0x24,
		0x92,
		0x48,
		0x00,
		0x00,
		0x01,
		0x2A,
		0xBA,

};
const struct IrCode code_na034Code = {
	freq_to_timerval(38462),
	37,		// # of pairs
	3,		// # of bits per index
	code_na004Times,  
	code_na034Samples,  
};

const uint16_t code_na035Times[] = {
	96, 93,
	97, 93,
	97, 287,
	97, 3431,
};
const uint8_t code_na035Samples[] = {
		0x16,
		0x66,
		0x5D,
		0x59,
		0x99,
		0x40,

};
const struct IrCode code_na035Code = {
	freq_to_timerval(41667),
	21,		// # of pairs
	2,		// # of bits per index
	code_na035Times,  
	code_na035Samples,  
};

const uint16_t code_na036Times[] = {
	82, 581,
	84, 250,
	84, 580,
};
const uint8_t code_na036Samples[] = {
		0x15,
		0x9A,
		0x90,

};
const struct IrCode code_na036Code = {
	freq_to_timerval(37037),
	10,		// # of pairs
	2,		// # of bits per index
	code_na036Times,  
	code_na036Samples,  
};

const uint16_t code_na037Times[] = {
	39, 263,
	164, 163,
	514, 164,
};
const uint8_t code_na037Samples[] = {
		0x80,
		0x45,
		0x00,

};
const struct IrCode code_na037Code = {
	freq_to_timerval(41667),
	10,		// # of pairs
	2,		// # of bits per index
	code_na037Times,  
	code_na037Samples,  
};


/* Duplicate timing table, same as na017 !
const uint16_t code_na038Times[] = {
	56, 57,
	56, 175,
	56, 4150,
	56, 9499,
	898, 227,
	898, 449,
};
*/
const uint8_t code_na038Samples[] = {
		0xA4,
		0x10,
		0x40,
		0x00,
		0x82,
		0x09,
		0x20,
		0x80,
		0x40,
		0x04,
		0x12,
		0x09,
		0x2A,
		0x38,

};
const struct IrCode code_na038Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na017Times,  
	code_na038Samples,  
};

const uint16_t code_na039Times[] = {
	113, 101,
	688, 2707,
};
const uint8_t code_na039Samples[] = {
		0x10,

};
const struct IrCode code_na039Code = {
	freq_to_timerval(40000),
	3,		// # of pairs
	2,		// # of bits per index
	code_na039Times,  
	code_na039Samples,  
};

const uint16_t code_na040Times[] = {
	113, 101,
	113, 201,
	113, 2707,
};
const uint8_t code_na040Samples[] = {
		0x06,
		0x04,

};
const struct IrCode code_na040Code = {
	freq_to_timerval(40000),
	7,		// # of pairs
	2,		// # of bits per index
	code_na040Times,  
	code_na040Samples,  
};

const uint16_t code_na041Times[] = {
	58, 62,
	58, 2746,
	117, 62,
	242, 62,
};
const uint8_t code_na041Samples[] = {
		0xE2,
		0x20,
		0x80,
		0x78,
		0x88,
		0x20,
		0x00,

};
const struct IrCode code_na041Code = {
	freq_to_timerval(76923),
	25,		// # of pairs
	2,		// # of bits per index
	code_na041Times,  
	code_na041Samples,  
};

const uint16_t code_na042Times[] = {
	54, 65,
	54, 170,
	54, 4099,
	54, 8668,
	899, 226,
	899, 421,
};
const uint8_t code_na042Samples[] = {
		0xA4,
		0x80,
		0x00,
		0x20,
		0x82,
		0x49,
		0x00,
		0x02,
		0x00,
		0x04,
		0x90,
		0x49,
		0x2A,
		0x38,

};
const struct IrCode code_na042Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na042Times,  
	code_na042Samples,  
};

const uint16_t code_na043Times[] = {
	43, 120,
	43, 121,
	43, 3491,
	131, 45,
};
const uint8_t code_na043Samples[] = {
		0x15,
		0x75,
		0x56,
		0x55,
		0x75,
		0x54,

};
const struct IrCode code_na043Code = {
	freq_to_timerval(40000),
	23,		// # of pairs
	2,		// # of bits per index
	code_na043Times,  
	code_na043Samples,  
};

const uint16_t code_na044Times[] = {
	51, 51,
	51, 160,
	51, 4096,
	51, 9513,
	431, 436,
	883, 219,
};
const uint8_t code_na044Samples[] = {
		0x84,
		0x90,
		0x00,
		0x00,
		0x02,
		0x49,
		0x20,
		0x80,
		0x00,
		0x04,
		0x12,
		0x49,
		0x2A,
		0xBA,

};
const struct IrCode code_na044Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na044Times,  
	code_na044Samples,  
};

const uint16_t code_na045Times[] = {
	58, 53,
	58, 167,
	58, 4494,
	58, 9679,
	455, 449,
	456, 449,
};
const uint8_t code_na045Samples[] = {
		0x80,
		0x90,
		0x00,
		0x00,
		0x90,
		0x00,
		0x04,
		0x92,
		0x00,
		0x00,
		0x00,
		0x49,
		0x2A,
		0x97,
		0x48,

};
const struct IrCode code_na045Code = {
	freq_to_timerval(38462),
	39,		// # of pairs
	3,		// # of bits per index
	code_na045Times,  
	code_na045Samples,  
};

const uint16_t code_na046Times[] = {
	51, 277,
	52, 53,
	52, 105,
	52, 277,
	52, 2527,
	52, 12809,
	103, 54,
};
const uint8_t code_na046Samples[] = {
		0x0B,
		0x12,
		0x63,
		0x44,
		0x92,
		0x6B,
		0x44,
		0x92,
		0x40,

};
const struct IrCode code_na046Code = {
	freq_to_timerval(29412),
	22,		// # of pairs
	3,		// # of bits per index
	code_na046Times,  
	code_na046Samples,  
};


/* Duplicate timing table, same as na017 !
const uint16_t code_na047Times[] = {
	56, 57,
	56, 175,
	56, 4150,
	56, 9499,
	898, 227,
	898, 449,
};
*/
const uint8_t code_na047Samples[] = {
		0xA0,
		0x00,
		0x40,
		0x04,
		0x92,
		0x09,
		0x24,
		0x92,
		0x09,
		0x20,
		0x00,
		0x40,
		0x0A,
		0x38,

};
const struct IrCode code_na047Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na017Times,  
	code_na047Samples,  
};


/* Duplicate timing table, same as na044 !
const uint16_t code_na048Times[] = {
	51, 51,
	51, 160,
	51, 4096,
	51, 9513,
	431, 436,
	883, 219,
};
*/
const uint8_t code_na048Samples[] = {
		0x80,
		0x00,
		0x00,
		0x04,
		0x92,
		0x49,
		0x24,
		0x92,
		0x00,
		0x00,
		0x00,
		0x49,
		0x2A,
		0xBA,

};
const struct IrCode code_na048Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na044Times,  
	code_na048Samples,  
};

const uint16_t code_na049Times[] = {
	274, 854,
	274, 1986,
};
const uint8_t code_na049Samples[] = {
		0x14,
		0x11,
		0x40,

};
const struct IrCode code_na049Code = {
	freq_to_timerval(45455),
	10,		// # of pairs
	2,		// # of bits per index
	code_na049Times,  
	code_na049Samples,  
};

const uint16_t code_na050Times[] = {
	80, 88,
	80, 254,
	80, 3750,
	359, 331,
};
const uint8_t code_na050Samples[] = {
		0xC0,
		0x00,
		0x01,
		0x55,
		0x55,
		0x52,
		0xC0,
		0x00,
		0x01,
		0x55,
		0x55,
		0x50,

};
const struct IrCode code_na050Code = {
	freq_to_timerval(55556),
	47,		// # of pairs
	2,		// # of bits per index
	code_na050Times,  
	code_na050Samples,  
};


/* Duplicate timing table, same as na017 !
const uint16_t code_na051Times[] = {
	56, 57,
	56, 175,
	56, 4150,
	56, 9499,
	898, 227,
	898, 449,
};
*/
const uint8_t code_na051Samples[] = {
		0xA0,
		0x10,
		0x01,
		0x24,
		0x82,
		0x48,
		0x00,
		0x02,
		0x40,
		0x04,
		0x90,
		0x09,
		0x2A,
		0x38,

};
const struct IrCode code_na051Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na017Times,  
	code_na051Samples,  
};


/* Duplicate timing table, same as na017 !
const uint16_t code_na052Times[] = {
	56, 57,
	56, 175,
	56, 4150,
	56, 9499,
	898, 227,
	898, 449,
};
*/
const uint8_t code_na052Samples[] = {
		0xA4,
		0x90,
		0x48,
		0x00,
		0x02,
		0x01,
		0x20,
		0x80,
		0x40,
		0x04,
		0x12,
		0x09,
		0x2A,
		0x38,

};
const struct IrCode code_na052Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na017Times,  
	code_na052Samples,  
};

const uint16_t code_na053Times[] = {
	51, 232,
	51, 512,
	51, 792,
	51, 2883,
};
const uint8_t code_na053Samples[] = {
		0x22,
		0x21,
		0x40,
		0x1C,
		0x88,
		0x85,
		0x00,
		0x40,

};
const struct IrCode code_na053Code = {
	freq_to_timerval(55556),
	29,		// # of pairs
	2,		// # of bits per index
	code_na053Times,  
	code_na053Samples,  
};


/* Duplicate timing table, same as na053 !
const uint16_t code_na054Times[] = {
	51, 232,
	51, 512,
	51, 792,
	51, 2883,
};
*/
const uint8_t code_na054Samples[] = {
		0x22,
		0x20,
		0x15,
		0x72,
		0x22,
		0x01,
		0x54,

};
const struct IrCode code_na054Code = {
	freq_to_timerval(55556),
	27,		// # of pairs
	2,		// # of bits per index
	code_na053Times,  
	code_na054Samples,  
};

const uint16_t code_na055Times[] = {
	3, 10,
	3, 20,
	35, 10,
	35, 12778,
};
const uint8_t code_na055Samples[] = {
		0x85,
		0x44,
		0x53,
		0x85,
		0x44,
		0x50,

};
const struct IrCode code_na055Code = {
	freq_to_timerval(3068),
	23,		// # of pairs
	2,		// # of bits per index
	code_na055Times,  
	code_na055Samples,  
};

const uint16_t code_na056Times[] = {
	55, 193,
	57, 192,
	57, 384,
};
const uint8_t code_na056Samples[] = {
		0x2A,
		0x54,

};
const struct IrCode code_na056Code = {
	freq_to_timerval(37175),
	7,		// # of pairs
	2,		// # of bits per index
	code_na056Times,  
	code_na056Samples,  
};

const uint16_t code_na057Times[] = {
	45, 148,
	46, 148,
	46, 351,
	46, 2781,
};
const uint8_t code_na057Samples[] = {
		0x2A,
		0x5D,
		0xA9,
		0x40,

};
const struct IrCode code_na057Code = {
	freq_to_timerval(40000),
	13,		// # of pairs
	2,		// # of bits per index
	code_na057Times,  
	code_na057Samples,  
};

const uint16_t code_na058Times[] = {
	22, 101,
	22, 219,
	23, 101,
	23, 219,
	31, 218,
};
const uint8_t code_na058Samples[] = {
		0x8D,
		0xA4,
		0x08,
		0x04,
		0x04,
		0x92,
		0x40,

};
const struct IrCode code_na058Code = {
	freq_to_timerval(33333),
	17,		// # of pairs
	3,		// # of bits per index
	code_na058Times,  
	code_na058Samples,  
};


/* Duplicate timing table, same as na017 !
const uint16_t code_na059Times[] = {
	56, 57,
	56, 175,
	56, 4150,
	56, 9499,
	898, 227,
	898, 449,
};
*/
const uint8_t code_na059Samples[] = {
		0xA4,
		0x12,
		0x09,
		0x00,
		0x80,
		0x40,
		0x20,
		0x10,
		0x40,
		0x04,
		0x82,
		0x09,
		0x2A,
		0x38,

};
const struct IrCode code_na059Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na017Times,  
	code_na059Samples,  
};


/* Duplicate timing table, same as na017 !
const uint16_t code_na060Times[] = {
	56, 57,
	56, 175,
	56, 4150,
	56, 9499,
	898, 227,
	898, 449,
};
*/
const uint8_t code_na060Samples[] = {
		0xA0,
		0x00,
		0x08,
		0x04,
		0x92,
		0x41,
		0x24,
		0x00,
		0x40,
		0x00,
		0x92,
		0x09,
		0x2A,
		0x38,

};
const struct IrCode code_na060Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na017Times,  
	code_na060Samples,  
};


/* Duplicate timing table, same as na017 !
const uint16_t code_na061Times[] = {
	56, 57,
	56, 175,
	56, 4150,
	56, 9499,
	898, 227,
	898, 449,
};
*/
const uint8_t code_na061Samples[] = {
		0xA0,
		0x00,
		0x08,
		0x24,
		0x92,
		0x41,
		0x04,
		0x82,
		0x00,
		0x00,
		0x10,
		0x49,
		0x2A,
		0x38,

};
const struct IrCode code_na061Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na017Times,  
	code_na061Samples,  
};


/* Duplicate timing table, same as na017 !
const uint16_t code_na062Times[] = {
	56, 57,
	56, 175,
	56, 4150,
	56, 9499,
	898, 227,
	898, 449,
};
*/
const uint8_t code_na062Samples[] = {
		0xA0,
		0x02,
		0x08,
		0x04,
		0x90,
		0x41,
		0x24,
		0x82,
		0x00,
		0x00,
		0x10,
		0x49,
		0x2A,
		0x38,

};
const struct IrCode code_na062Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na017Times,  
	code_na062Samples,  
};


/* Duplicate timing table, same as na017 !
const uint16_t code_na063Times[] = {
	56, 57,
	56, 175,
	56, 4150,
	56, 9499,
	898, 227,
	898, 449,
};
*/
const uint8_t code_na063Samples[] = {
		0xA4,
		0x92,
		0x49,
		0x20,
		0x00,
		0x00,
		0x04,
		0x92,
		0x48,
		0x00,
		0x00,
		0x01,
		0x2A,
		0x38,

};
const struct IrCode code_na063Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na017Times,  
	code_na063Samples,  
};


/* Duplicate timing table, same as na001 !
const uint16_t code_na064Times[] = {
	50, 100,
	50, 200,
	50, 800,
	400, 400,
};
*/
const uint8_t code_na064Samples[] = {
		0xC0,
		0x01,
		0x51,
		0x55,
		0x54,
		0x04,
		0x2C,
		0x00,
		0x15,
		0x15,
		0x55,
		0x40,
		0x40,

};
const struct IrCode code_na064Code = {
	freq_to_timerval(57143),
	51,		// # of pairs
	2,		// # of bits per index
	code_na001Times,  
	code_na064Samples,  
};

const uint16_t code_na065Times[] = {
	48, 98,
	48, 197,
	98, 846,
	395, 392,
	1953, 392,
};
const uint8_t code_na065Samples[] = {
		0x84,
		0x92,
		0x01,
		0x24,
		0x12,
		0x00,
		0x04,
		0x80,
		0x08,
		0x09,
		0x92,
		0x48,
		0x04,
		0x90,
		0x48,
		0x00,
		0x12,
		0x00,
		0x20,
		0x26,
		0x49,
		0x20,
		0x12,
		0x41,
		0x20,
		0x00,
		0x48,
		0x00,
		0x80,

};
const struct IrCode code_na065Code = {
	freq_to_timerval(59172),
	77,		// # of pairs
	3,		// # of bits per index
	code_na065Times,  
	code_na065Samples,  
};

const uint16_t code_na066Times[] = {
	38, 276,
	165, 154,
	415, 155,
	742, 154,
};
const uint8_t code_na066Samples[] = {
		0xC0,
		0x45,
		0x02,
		0x01,
		0x14,
		0x08,
		0x04,
		0x50,

};
const struct IrCode code_na066Code = {
	freq_to_timerval(38462),
	32,		// # of pairs
	2,		// # of bits per index
	code_na066Times,  
	code_na066Samples,  
};


/* Duplicate timing table, same as na044 !
const uint16_t code_na067Times[] = {
	51, 51,
	51, 160,
	51, 4096,
	51, 9513,
	431, 436,
	883, 219,
};
*/
const uint8_t code_na067Samples[] = {
		0x80,
		0x02,
		0x49,
		0x24,
		0x90,
		0x00,
		0x00,
		0x80,
		0x00,
		0x04,
		0x12,
		0x49,
		0x2A,
		0xBA,

};
const struct IrCode code_na067Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na044Times,  
	code_na067Samples,  
};

const uint16_t code_na068Times[] = {
	43, 121,
	43, 9437,
	130, 45,
	131, 45,
};
const uint8_t code_na068Samples[] = {
		0x8C,
		0x30,
		0x0D,
		0xCC,
		0x30,
		0x0C,

};
const struct IrCode code_na068Code = {
	freq_to_timerval(40000),
	23,		// # of pairs
	2,		// # of bits per index
	code_na068Times,  
	code_na068Samples,  
};


/* Duplicate timing table, same as na017 !
const uint16_t code_na069Times[] = {
	56, 57,
	56, 175,
	56, 4150,
	56, 9499,
	898, 227,
	898, 449,
};
*/
const uint8_t code_na069Samples[] = {
		0xA0,
		0x00,
		0x00,
		0x04,
		0x92,
		0x49,
		0x24,
		0x82,
		0x00,
		0x00,
		0x10,
		0x49,
		0x2A,
		0x38,

};
const struct IrCode code_na069Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na017Times,  
	code_na069Samples,  
};

const uint16_t code_na070Times[] = {
	27, 76,
	27, 182,
	27, 183,
	27, 3199,
};
const uint8_t code_na070Samples[] = {
		0x40,
		0x02,
		0x08,
		0xA2,
		0xE0,
		0x00,
		0x82,
		0x28,

};
const struct IrCode code_na070Code = {
	freq_to_timerval(38462),
	32,		// # of pairs
	2,		// # of bits per index
	code_na070Times,  
	code_na070Samples,  
};

const uint16_t code_na071Times[] = {
	37, 181,
	37, 272,
};
const uint8_t code_na071Samples[] = {
		0x11,
		0x40,

};
const struct IrCode code_na071Code = {
	freq_to_timerval(55556),
	7,		// # of pairs
	2,		// # of bits per index
	code_na071Times,  
	code_na071Samples,  
};


/* Duplicate timing table, same as na042 !
const uint16_t code_na072Times[] = {
	54, 65,
	54, 170,
	54, 4099,
	54, 8668,
	899, 226,
	899, 421,
};
*/
const uint8_t code_na072Samples[] = {
		0xA0,
		0x90,
		0x00,
		0x00,
		0x90,
		0x00,
		0x00,
		0x10,
		0x40,
		0x04,
		0x82,
		0x09,
		0x2A,
		0x38,

};
const struct IrCode code_na072Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na042Times,  
	code_na072Samples,  
};


/* Duplicate timing table, same as na017 !
const uint16_t code_na073Times[] = {
	56, 57,
	56, 175,
	56, 4150,
	56, 9499,
	898, 227,
	898, 449,
};
*/
const uint8_t code_na073Samples[] = {
		0xA0,
		0x82,
		0x08,
		0x24,
		0x10,
		0x41,
		0x00,
		0x00,
		0x00,
		0x24,
		0x92,
		0x49,
		0x0A,
		0x38,

};
const struct IrCode code_na073Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na017Times,  
	code_na073Samples,  
};


/* Duplicate timing table, same as na017 !
const uint16_t code_na074Times[] = {
	56, 57,
	56, 175,
	56, 4150,
	56, 9499,
	898, 227,
	898, 449,
};
*/
const uint8_t code_na074Samples[] = {
		0xA4,
		0x00,
		0x41,
		0x00,
		0x92,
		0x08,
		0x20,
		0x02,
		0x00,
		0x04,
		0x90,
		0x49,
		0x2A,
		0x38,

};
const struct IrCode code_na074Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na017Times,  
	code_na074Samples,  
};

const uint16_t code_na075Times[] = {
	51, 98,
	51, 194,
	102, 931,
	390, 390,
	390, 391,
};
const uint8_t code_na075Samples[] = {
		0x60,
		0x00,
		0x01,
		0x04,
		0x10,
		0x49,
		0x24,
		0x82,
		0x08,
		0x2A,
		0x00,
		0x00,
		0x04,
		0x10,
		0x41,
		0x24,
		0x92,
		0x08,
		0x20,
		0x80,

};
const struct IrCode code_na075Code = {
	freq_to_timerval(41667),
	51,		// # of pairs
	3,		// # of bits per index
	code_na075Times,  
	code_na075Samples,  
};


/* Duplicate timing table, same as na017 !
const uint16_t code_na076Times[] = {
	56, 57,
	56, 175,
	56, 4150,
	56, 9499,
	898, 227,
	898, 449,
};
*/
const uint8_t code_na076Samples[] = {
		0xA0,
		0x92,
		0x09,
		0x04,
		0x00,
		0x40,
		0x20,
		0x10,
		0x40,
		0x04,
		0x82,
		0x09,
		0x2A,
		0x38,

};
const struct IrCode code_na076Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na017Times,  
	code_na076Samples,  
};


/* Duplicate timing table, same as na031 !
const uint16_t code_na077Times[] = {
	88, 89,
	88, 90,
	88, 179,
	88, 8977,
	177, 90,
};
*/
const uint8_t code_na077Samples[] = {
		0x10,
		0xA2,
		0x62,
		0x31,
		0x98,
		0x51,
		0x31,
		0x18,

};
const struct IrCode code_na077Code = {
	freq_to_timerval(35714),
	21,		// # of pairs
	3,		// # of bits per index
	code_na031Times,  
	code_na077Samples,  
};

const uint16_t code_na078Times[] = {
	40, 275,
	160, 154,
	480, 155,
};
const uint8_t code_na078Samples[] = {
		0x80,
		0x45,
		0x04,
		0x01,
		0x14,
		0x10,
		0x04,
		0x50,
		0x40,

};
const struct IrCode code_na078Code = {
	freq_to_timerval(38462),
	33,		// # of pairs
	2,		// # of bits per index
	code_na078Times,  
	code_na078Samples,  
};


/* Duplicate timing table, same as na017 !
const uint16_t code_na079Times[] = {
	56, 57,
	56, 175,
	56, 4150,
	56, 9499,
	898, 227,
	898, 449,
};
*/
const uint8_t code_na079Samples[] = {
		0xA0,
		0x82,
		0x08,
		0x24,
		0x10,
		0x41,
		0x04,
		0x90,
		0x08,
		0x20,
		0x02,
		0x41,
		0x0A,
		0x38,

};
const struct IrCode code_na079Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na017Times,  
	code_na079Samples,  
};


/* Duplicate timing table, same as na055 !
const uint16_t code_na080Times[] = {
	3, 10,
	3, 20,
	35, 10,
	35, 12778,
};
*/
const uint8_t code_na080Samples[] = {
		0x85,
		0x41,
		0x03,
		0x85,
		0x41,
		0x00,

};
const struct IrCode code_na080Code = {
	freq_to_timerval(3068),
	23,		// # of pairs
	2,		// # of bits per index
	code_na055Times,  
	code_na080Samples,  
};

const uint16_t code_na081Times[] = {
	48, 52,
	48, 409,
	48, 504,
	48, 9978,
};
const uint8_t code_na081Samples[] = {
		0x18,
		0x46,
		0x18,
		0x68,
		0x47,
		0x18,
		0x46,
		0x18,
		0x68,
		0x44,

};
const struct IrCode code_na081Code = {
	freq_to_timerval(40000),
	39,		// # of pairs
	2,		// # of bits per index
	code_na081Times,  
	code_na081Samples,  
};

const uint16_t code_na082Times[] = {
	88, 89,
	88, 90,
	88, 179,
	88, 8888,
	177, 90,
	177, 179,
};
const uint8_t code_na082Samples[] = {
		0x0A,
		0x12,
		0x49,
		0x2A,
		0xB2,
		0xA1,
		0x24,
		0x92,
		0xA8,

};
const struct IrCode code_na082Code = {
	freq_to_timerval(35714),
	23,		// # of pairs
	3,		// # of bits per index
	code_na082Times,  
	code_na082Samples,  
};


/* Duplicate timing table, same as na031 !
const uint16_t code_na083Times[] = {
	88, 89,
	88, 90,
	88, 179,
	88, 8977,
	177, 90,
};
*/
const uint8_t code_na083Samples[] = {
		0x10,
		0x92,
		0x49,
		0x46,
		0x33,
		0x09,
		0x24,
		0x94,
		0x60,

};
const struct IrCode code_na083Code = {
	freq_to_timerval(35714),
	23,		// # of pairs
	3,		// # of bits per index
	code_na031Times,  
	code_na083Samples,  
};
// Duplicate IR Code???

const uint16_t code_na084Times[] = {
	41, 43,
	41, 128,
	41, 7476,
	336, 171,
	338, 169,
};
const uint8_t code_na084Samples[] = {
		0x60,
		0x80,
		0x00,
		0x00,
		0x00,
		0x08,
		0x00,
		0x00,
		0x40,
		0x20,
		0x00,
		0x00,
		0x04,
		0x12,
		0x48,
		0x04,
		0x12,
		0x08,
		0x2A,
		0x02,
		0x00,
		0x00,
		0x00,
		0x00,
		0x20,
		0x00,
		0x01,
		0x00,
		0x80,
		0x00,
		0x00,
		0x10,
		0x49,
		0x20,
		0x10,
		0x48,
		0x20,
		0x80,

};
const struct IrCode code_na084Code = {
	freq_to_timerval(37037),
	99,		// # of pairs
	3,		// # of bits per index
	code_na084Times,  
	code_na084Samples,  
};

const uint16_t code_na085Times[] = {
	55, 60,
	55, 165,
	55, 2284,
	445, 437,
	448, 436,
};
const uint8_t code_na085Samples[] = {
		0x64,
		0x00,
		0x00,
		0x00,
		0x00,
		0x40,
		0x00,
		0x80,
		0xA1,
		0x00,
		0x00,
		0x00,
		0x00,
		0x10,
		0x00,
		0x20,
		0x00,

};
const struct IrCode code_na085Code = {
	freq_to_timerval(38462),
	43,		// # of pairs
	3,		// # of bits per index
	code_na085Times,  
	code_na085Samples,  
};

const uint16_t code_na086Times[] = {
	42, 46,
	42, 126,
	42, 6989,
	347, 176,
	347, 177,
};
const uint8_t code_na086Samples[] = {
		0x60,
		0x82,
		0x08,
		0x20,
		0x82,
		0x41,
		0x04,
		0x92,
		0x00,
		0x20,
		0x80,
		0x40,
		0x00,
		0x90,
		0x40,
		0x04,
		0x00,
		0x41,
		0x2A,
		0x02,
		0x08,
		0x20,
		0x82,
		0x09,
		0x04,
		0x12,
		0x48,
		0x00,
		0x82,
		0x01,
		0x00,
		0x02,
		0x41,
		0x00,
		0x10,
		0x01,
		0x04,
		0x80,

};
const struct IrCode code_na086Code = {
	freq_to_timerval(37175),
	99,		// # of pairs
	3,		// # of bits per index
	code_na086Times,  
	code_na086Samples,  
};

const uint16_t code_na087Times[] = {
	56, 69,
	56, 174,
	56, 4165,
	56, 9585,
	880, 222,
	880, 435,
};
const uint8_t code_na087Samples[] = {
		0xA0,
		0x02,
		0x40,
		0x04,
		0x90,
		0x09,
		0x20,
		0x02,
		0x00,
		0x04,
		0x90,
		0x49,
		0x2A,
		0x38,

};
const struct IrCode code_na087Code = {
	freq_to_timerval(38462),
	37,		// # of pairs
	3,		// # of bits per index
	code_na087Times,  
	code_na087Samples,  
};


/* Duplicate timing table, same as na009 !
const uint16_t code_na088Times[] = {
	53, 56,
	53, 171,
	53, 3950,
	53, 9599,
	898, 451,
	900, 226,
};
*/
const uint8_t code_na088Samples[] = {
		0x80,
		0x00,
		0x40,
		0x04,
		0x12,
		0x08,
		0x04,
		0x92,
		0x40,
		0x00,
		0x00,
		0x09,
		0x2A,
		0xBA,

};
const struct IrCode code_na088Code = {
	freq_to_timerval(38610),
	37,		// # of pairs
	3,		// # of bits per index
	code_na009Times,  
	code_na088Samples,  
};


/* Duplicate timing table, same as na004 !
const uint16_t code_na089Times[] = {
	55, 57,
	55, 170,
	55, 3949,
	55, 9623,
	898, 453,
	900, 226,
};
*/
const uint8_t code_na089Samples[] = {
		0x80,
		0x02,
		0x00,
		0x04,
		0x90,
		0x49,
		0x20,
		0x80,
		0x40,
		0x04,
		0x12,
		0x09,
		0x2A,
		0xBA,

};
const struct IrCode code_na089Code = {
	freq_to_timerval(38462),
	37,		// # of pairs
	3,		// # of bits per index
	code_na004Times,  
	code_na089Samples,  
};

const uint16_t code_na090Times[] = {
	88, 90,
	88, 91,
	88, 181,
	88, 8976,
	177, 91,
	177, 181,
};
const uint8_t code_na090Samples[] = {
		0x10,
		0xAB,
		0x11,
		0x8C,
		0xC2,
		0xAC,
		0x46,
		0x00,

};
const struct IrCode code_na090Code = {
	freq_to_timerval(35714),
	19,		// # of pairs
	3,		// # of bits per index
	code_na090Times,  
	code_na090Samples,  
};

const uint16_t code_na091Times[] = {
	48, 100,
	48, 200,
	48, 1050,
	400, 400,
};
const uint8_t code_na091Samples[] = {
		0xD5,
		0x41,
		0x51,
		0x40,
		0x14,
		0x04,
		0x2D,
		0x54,
		0x15,
		0x14,
		0x01,
		0x40,
		0x40,

};
const struct IrCode code_na091Code = {
	freq_to_timerval(58824),
	51,		// # of pairs
	2,		// # of bits per index
	code_na091Times,  
	code_na091Samples,  
};

const uint16_t code_na092Times[] = {
	54, 56,
	54, 170,
	54, 4927,
	451, 447,
};
const uint8_t code_na092Samples[] = {
		0xD1,
		0x00,
		0x11,
		0x00,
		0x04,
		0x00,
		0x11,
		0x55,
		0x6D,
		0x10,
		0x01,
		0x10,
		0x00,
		0x40,
		0x01,
		0x15,
		0x54,

};
const struct IrCode code_na092Code = {
	freq_to_timerval(38462),
	67,		// # of pairs
	2,		// # of bits per index
	code_na092Times,  
	code_na092Samples,  
};

const uint16_t code_na093Times[] = {
	55, 57,
	55, 167,
	55, 4400,
	895, 448,
	897, 447,
};
const uint8_t code_na093Samples[] = {
		0x60,
		0x90,
		0x00,
		0x20,
		0x80,
		0x00,
		0x04,
		0x02,
		0x01,
		0x00,
		0x90,
		0x48,
		0x2A,
		0x02,
		0x40,
		0x00,
		0x82,
		0x00,
		0x00,
		0x10,
		0x08,
		0x04,
		0x02,
		0x41,
		0x20,
		0x80,

};
const struct IrCode code_na093Code = {
	freq_to_timerval(38462),
	67,		// # of pairs
	3,		// # of bits per index
	code_na093Times,  
	code_na093Samples,  
};


/* Duplicate timing table, same as na005 !
const uint16_t code_na094Times[] = {
	88, 90,
	88, 91,
	88, 181,
	88, 8976,
	177, 91,
};
*/
const uint8_t code_na094Samples[] = {
		0x10,
		0x94,
		0x62,
		0x31,
		0x98,
		0x4A,
		0x31,
		0x18,

};
const struct IrCode code_na094Code = {
	freq_to_timerval(35714),
	21,		// # of pairs
	3,		// # of bits per index
	code_na005Times,  
	code_na094Samples,  
};

const uint16_t code_na095Times[] = {
	56, 58,
	56, 174,
	56, 4549,
	56, 9448,
	440, 446,
};
const uint8_t code_na095Samples[] = {
		0x80,
		0x02,
		0x00,
		0x00,
		0x02,
		0x00,
		0x04,
		0x82,
		0x00,
		0x00,
		0x10,
		0x49,
		0x2A,
		0x17,
		0x08,

};
const struct IrCode code_na095Code = {
	freq_to_timerval(38462),
	39,		// # of pairs
	3,		// # of bits per index
	code_na095Times,  
	code_na095Samples,  
};


/* Duplicate timing table, same as na009 !
const uint16_t code_na096Times[] = {
	53, 56,
	53, 171,
	53, 3950,
	53, 9599,
	898, 451,
	900, 226,
};
*/
const uint8_t code_na096Samples[] = {
		0x80,
		0x80,
		0x40,
		0x04,
		0x92,
		0x49,
		0x20,
		0x92,
		0x00,
		0x04,
		0x00,
		0x49,
		0x2A,
		0xBA,

};
const struct IrCode code_na096Code = {
	freq_to_timerval(38462),
	37,		// # of pairs
	3,		// # of bits per index
	code_na009Times,  
	code_na096Samples,  
};


/* Duplicate timing table, same as na009 !
const uint16_t code_na097Times[] = {
	53, 56,
	53, 171,
	53, 3950,
	53, 9599,
	898, 451,
	900, 226,
};
*/
const uint8_t code_na097Samples[] = {
		0x84,
		0x80,
		0x00,
		0x24,
		0x10,
		0x41,
		0x00,
		0x80,
		0x01,
		0x24,
		0x12,
		0x48,
		0x0A,
		0xBA,

};
const struct IrCode code_na097Code = {
	freq_to_timerval(38462),
	37,		// # of pairs
	3,		// # of bits per index
	code_na009Times,  
	code_na097Samples,  
};


/* Duplicate timing table, same as na004 !
const uint16_t code_na098Times[] = {
	55, 57,
	55, 170,
	55, 3949,
	55, 9623,
	898, 453,
	900, 226,
};
*/
const uint8_t code_na098Samples[] = {
		0x80,
		0x00,
		0x00,
		0x04,
		0x92,
		0x49,
		0x24,
		0x00,
		0x41,
		0x00,
		0x92,
		0x08,
		0x2A,
		0xBA,

};
const struct IrCode code_na098Code = {
	freq_to_timerval(38462),
	37,		// # of pairs
	3,		// # of bits per index
	code_na004Times,  
	code_na098Samples,  
};


/* Duplicate timing table, same as na009 !
const uint16_t code_na099Times[] = {
	53, 56,
	53, 171,
	53, 3950,
	53, 9599,
	898, 451,
	900, 226,
};
*/
const uint8_t code_na099Samples[] = {
		0x80,
		0x00,
		0x00,
		0x04,
		0x12,
		0x48,
		0x24,
		0x00,
		0x00,
		0x00,
		0x92,
		0x49,
		0x2A,
		0xBA,

};
const struct IrCode code_na099Code = {
	freq_to_timerval(38462),
	37,		// # of pairs
	3,		// # of bits per index
	code_na009Times,  
	code_na099Samples,  
};

const uint16_t code_na100Times[] = {
	43, 171,
	45, 60,
	45, 170,
	54, 2301,
};
const uint8_t code_na100Samples[] = {
		0x29,
		0x59,
		0x65,
		0x55,
		0xEA,
		0x56,
		0x59,
		0x55,
		0x40,

};
const struct IrCode code_na100Code = {
	freq_to_timerval(35842),
	33,		// # of pairs
	2,		// # of bits per index
	code_na100Times,  
	code_na100Samples,  
};


/* Duplicate timing table, same as na004 !
const uint16_t code_na101Times[] = {
	55, 57,
	55, 170,
	55, 3949,
	55, 9623,
	898, 453,
	900, 226,
};
*/
const uint8_t code_na101Samples[] = {
		0x80,
		0x00,
		0x09,
		0x04,
		0x92,
		0x40,
		0x20,
		0x00,
		0x00,
		0x04,
		0x92,
		0x49,
		0x2A,
		0xBA,

};
const struct IrCode code_na101Code = {
	freq_to_timerval(38462),
	37,		// # of pairs
	3,		// # of bits per index
	code_na004Times,  
	code_na101Samples,  
};

const uint16_t code_na102Times[] = {
	86, 87,
	86, 258,
	86, 3338,
	346, 348,
	348, 347,
};
const uint8_t code_na102Samples[] = {
		0x64,
		0x02,
		0x08,
		0x00,
		0x02,
		0x09,
		0x04,
		0x12,
		0x49,
		0x0A,
		0x10,
		0x08,
		0x20,
		0x00,
		0x08,
		0x24,
		0x10,
		0x49,
		0x24,
		0x00,

};
const struct IrCode code_na102Code = {
	freq_to_timerval(40000),
	51,		// # of pairs
	3,		// # of bits per index
	code_na102Times,  
	code_na102Samples,  
};


/* Duplicate timing table, same as na045 !
const uint16_t code_na103Times[] = {
	58, 53,
	58, 167,
	58, 4494,
	58, 9679,
	455, 449,
	456, 449,
};
*/
const uint8_t code_na103Samples[] = {
		0x80,
		0x02,
		0x00,
		0x00,
		0x02,
		0x00,
		0x04,
		0x92,
		0x00,
		0x00,
		0x00,
		0x49,
		0x2A,
		0x97,
		0x48,

};
const struct IrCode code_na103Code = {
	freq_to_timerval(38462),
	39,		// # of pairs
	3,		// # of bits per index
	code_na045Times,  
	code_na103Samples,  
};


/* Duplicate timing table, same as na017 !
const uint16_t code_na104Times[] = {
	56, 57,
	56, 175,
	56, 4150,
	56, 9499,
	898, 227,
	898, 449,
};
*/
const uint8_t code_na104Samples[] = {
		0xA4,
		0x00,
		0x49,
		0x00,
		0x92,
		0x00,
		0x20,
		0x02,
		0x00,
		0x04,
		0x90,
		0x49,
		0x2A,
		0x38,

};
const struct IrCode code_na104Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na017Times,  
	code_na104Samples,  
};


/* Duplicate timing table, same as na017 !
const uint16_t code_na105Times[] = {
	56, 57,
	56, 175,
	56, 4150,
	56, 9499,
	898, 227,
	898, 449,
};
*/
const uint8_t code_na105Samples[] = {
		0xA4,
		0x80,
		0x00,
		0x20,
		0x12,
		0x49,
		0x04,
		0x92,
		0x49,
		0x20,
		0x00,
		0x00,
		0x0A,
		0x38,

};
const struct IrCode code_na105Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na017Times,  
	code_na105Samples,  
};


/* Duplicate timing table, same as na044 !
const uint16_t code_na106Times[] = {
	51, 51,
	51, 160,
	51, 4096,
	51, 9513,
	431, 436,
	883, 219,
};
*/
const uint8_t code_na106Samples[] = {
		0x80,
		0x02,
		0x00,
		0x04,
		0x90,
		0x49,
		0x24,
		0x92,
		0x00,
		0x00,
		0x00,
		0x49,
		0x2A,
		0xBA,

};
const struct IrCode code_na106Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na044Times,  
	code_na106Samples,  
};


/* Duplicate timing table, same as na045 !
const uint16_t code_na107Times[] = {
	58, 53,
	58, 167,
	58, 4494,
	58, 9679,
	455, 449,
	456, 449,
};
*/
const uint8_t code_na107Samples[] = {
		0x80,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0x04,
		0x92,
		0x00,
		0x00,
		0x00,
		0x49,
		0x2A,
		0x97,
		0x48,

};
const struct IrCode code_na107Code = {
	freq_to_timerval(38462),
	39,		// # of pairs
	3,		// # of bits per index
	code_na045Times,  
	code_na107Samples,  
};


/* Duplicate timing table, same as na045 !
const uint16_t code_na108Times[] = {
	58, 53,
	58, 167,
	58, 4494,
	58, 9679,
	455, 449,
	456, 449,
};
*/
const uint8_t code_na108Samples[] = {
		0x80,
		0x90,
		0x40,
		0x00,
		0x90,
		0x40,
		0x04,
		0x92,
		0x00,
		0x00,
		0x00,
		0x49,
		0x2A,
		0x97,
		0x48,

};
const struct IrCode code_na108Code = {
	freq_to_timerval(38462),
	39,		// # of pairs
	3,		// # of bits per index
	code_na045Times,  
	code_na108Samples,  
};

const uint16_t code_na109Times[] = {
	58, 61,
	58, 211,
	58, 9582,
	73, 4164,
	883, 211,
	1050, 494,
};
const uint8_t code_na109Samples[] = {
		0xA0,
		0x00,
		0x08,
		0x24,
		0x92,
		0x41,
		0x00,
		0x82,
		0x00,
		0x04,
		0x10,
		0x49,
		0x2E,
		0x28,

};
const struct IrCode code_na109Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na109Times,  
	code_na109Samples,  
};


/* Duplicate timing table, same as na017 !
const uint16_t code_na110Times[] = {
	56, 57,
	56, 175,
	56, 4150,
	56, 9499,
	898, 227,
	898, 449,
};
*/
const uint8_t code_na110Samples[] = {
		0xA4,
		0x80,
		0x00,
		0x20,
		0x12,
		0x49,
		0x00,
		0x02,
		0x00,
		0x04,
		0x90,
		0x49,
		0x2A,
		0x38,

};
const struct IrCode code_na110Code = {
	freq_to_timerval(40161),
	37,		// # of pairs
	3,		// # of bits per index
	code_na017Times,  
	code_na110Samples,  
};


/* Duplicate timing table, same as na044 !
const uint16_t code_na111Times[] = {
	51, 51,
	51, 160,
	51, 4096,
	51, 9513,
	431, 436,
	883, 219,
};
*/
const uint8_t code_na111Samples[] = {
		0x84,
		0x92,
		0x49,
		0x20,
		0x00,
		0x00,
		0x04,
		0x92,
		0x00,
		0x00,
		0x00,
		0x49,
		0x2A,
		0xBA,

};
const struct IrCode code_na111Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na044Times,  
	code_na111Samples,  
};


/* Duplicate timing table, same as na004 !
const uint16_t code_na112Times[] = {
	55, 57,
	55, 170,
	55, 3949,
	55, 9623,
	898, 453,
	900, 226,
};
*/
const uint8_t code_na112Samples[] = {
		0x84,
		0x00,
		0x00,
		0x00,
		0x92,
		0x49,
		0x24,
		0x00,
		0x00,
		0x00,
		0x92,
		0x49,
		0x2A,
		0xBA,

};
const struct IrCode code_na112Code = {
	freq_to_timerval(38462),
	37,		// # of pairs
	3,		// # of bits per index
	code_na004Times,  
	code_na112Samples,  
};

const uint16_t code_na113Times[] = {
	56, 54,
	56, 166,
	56, 3945,
	896, 442,
	896, 443,
};
const uint8_t code_na113Samples[] = {
		0x60,
		0x00,
		0x00,
		0x20,
		0x02,
		0x09,
		0x04,
		0x02,
		0x01,
		0x00,
		0x90,
		0x48,
		0x2A,
		0x00,
		0x00,
		0x00,
		0x80,
		0x08,
		0x24,
		0x10,
		0x08,
		0x04,
		0x02,
		0x41,
		0x20,
		0x80,

};
const struct IrCode code_na113Code = {
	freq_to_timerval(40000),
	67,		// # of pairs
	3,		// # of bits per index
	code_na113Times,  
	code_na113Samples,  
};

const uint16_t code_na114Times[] = {
	44, 50,
	44, 147,
	44, 447,
	44, 2236,
	791, 398,
	793, 397,
};
const uint8_t code_na114Samples[] = {
		0x84,
		0x10,
		0x40,
		0x08,
		0x82,
		0x08,
		0x01,
		0xD2,
		0x08,
		0x20,
		0x04,
		0x41,
		0x04,
		0x00,

};
const struct IrCode code_na114Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na114Times,  
	code_na114Samples,  
};

const uint16_t code_na115Times[] = {
	81, 86,
	81, 296,
	81, 3349,
	328, 331,
	329, 331,
};
const uint8_t code_na115Samples[] = {
		0x60,
		0x82,
		0x00,
		0x20,
		0x80,
		0x41,
		0x04,
		0x90,
		0x41,
		0x2A,
		0x02,
		0x08,
		0x00,
		0x82,
		0x01,
		0x04,
		0x12,
		0x41,
		0x04,
		0x80,

};
const struct IrCode code_na115Code = {
	freq_to_timerval(40000),
	51,		// # of pairs
	3,		// # of bits per index
	code_na115Times,  
	code_na115Samples,  
};


/* Duplicate timing table, same as na017 !
const uint16_t code_na116Times[] = {
	56, 57,
	56, 175,
	56, 4150,
	56, 9499,
	898, 227,
	898, 449,
};
*/
const uint8_t code_na116Samples[] = {
		0xA0,
		0x00,
		0x40,
		0x04,
		0x92,
		0x09,
		0x24,
		0x00,
		0x40,
		0x00,
		0x92,
		0x09,
		0x2A,
		0x38,

};
const struct IrCode code_na116Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na017Times,  
	code_na116Samples,  
};

const uint16_t code_na117Times[] = {
	49, 54,
	49, 158,
	49, 420,
	49, 2446,
	819, 420,
	821, 419,
};
const uint8_t code_na117Samples[] = {
		0x84,
		0x00,
		0x00,
		0x08,
		0x12,
		0x40,
		0x01,
		0xD2,
		0x00,
		0x00,
		0x04,
		0x09,
		0x20,
		0x00,

};
const struct IrCode code_na117Code = {
	freq_to_timerval(41667),
	37,		// # of pairs
	3,		// # of bits per index
	code_na117Times,  
	code_na117Samples,  
};


/* Duplicate timing table, same as na044 !
const uint16_t code_na118Times[] = {
	51, 51,
	51, 160,
	51, 4096,
	51, 9513,
	431, 436,
	883, 219,
};
*/
const uint8_t code_na118Samples[] = {
		0x84,
		0x90,
		0x49,
		0x20,
		0x02,
		0x00,
		0x04,
		0x92,
		0x00,
		0x00,
		0x00,
		0x49,
		0x2A,
		0xBA,

};
const struct IrCode code_na118Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na044Times,  
	code_na118Samples,  
};

const uint16_t code_na119Times[] = {
	55, 63,
	55, 171,
	55, 4094,
	55, 9508,
	881, 219,
	881, 438,
};
const uint8_t code_na119Samples[] = {
		0xA0,
		0x10,
		0x00,
		0x04,
		0x82,
		0x49,
		0x20,
		0x02,
		0x00,
		0x04,
		0x90,
		0x49,
		0x2A,
		0x38,

};
const struct IrCode code_na119Code = {
	freq_to_timerval(55556),
	37,		// # of pairs
	3,		// # of bits per index
	code_na119Times,  
	code_na119Samples,  
};


/* Duplicate timing table, same as na017 !
const uint16_t code_na120Times[] = {
	56, 57,
	56, 175,
	56, 4150,
	56, 9499,
	898, 227,
	898, 449,
};
*/
const uint8_t code_na120Samples[] = {
		0xA0,
		0x12,
		0x00,
		0x04,
		0x80,
		0x49,
		0x24,
		0x92,
		0x40,
		0x00,
		0x00,
		0x09,
		0x2A,
		0x38,

};
const struct IrCode code_na120Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na017Times,  
	code_na120Samples,  
};


/* Duplicate timing table, same as na017 !
const uint16_t code_na121Times[] = {
	56, 57,
	56, 175,
	56, 4150,
	56, 9499,
	898, 227,
	898, 449,
};
*/
const uint8_t code_na121Samples[] = {
		0xA0,
		0x00,
		0x40,
		0x04,
		0x92,
		0x09,
		0x20,
		0x02,
		0x00,
		0x04,
		0x90,
		0x49,
		0x2A,
		0x38,

};
const struct IrCode code_na121Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na017Times,  
	code_na121Samples,  
};

const uint16_t code_na122Times[] = {
	80, 95,
	80, 249,
	80, 3867,
	329, 322,
};
const uint8_t code_na122Samples[] = {
		0xC0,
		0x00,
		0x01,
		0x55,
		0x55,
		0x06,
		0xC0,
		0x00,
		0x01,
		0x55,
		0x55,
		0x04,

};
const struct IrCode code_na122Code = {
	freq_to_timerval(52632),
	47,		// # of pairs
	2,		// # of bits per index
	code_na122Times,  
	code_na122Samples,  
};


/* Duplicate timing table, same as na017 !
const uint16_t code_na123Times[] = {
	56, 57,
	56, 175,
	56, 4150,
	56, 9499,
	898, 227,
	898, 449,
};
*/
const uint8_t code_na123Samples[] = {
		0xA0,
		0x02,
		0x48,
		0x04,
		0x90,
		0x01,
		0x20,
		0x12,
		0x40,
		0x04,
		0x80,
		0x09,
		0x2A,
		0x38,

};
const struct IrCode code_na123Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na017Times,  
	code_na123Samples,  
};

const uint16_t code_na124Times[] = {
	54, 56,
	54, 151,
	54, 4092,
	54, 8677,
	900, 421,
	901, 226,
};
const uint8_t code_na124Samples[] = {
		0x80,
		0x00,
		0x48,
		0x04,
		0x92,
		0x01,
		0x20,
		0x00,
		0x00,
		0x04,
		0x92,
		0x49,
		0x2A,
		0xBA,

};
const struct IrCode code_na124Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na124Times,  
	code_na124Samples,  
};


/* Duplicate timing table, same as na119 !
const uint16_t code_na125Times[] = {
	55, 63,
	55, 171,
	55, 4094,
	55, 9508,
	881, 219,
	881, 438,
};
*/
const uint8_t code_na125Samples[] = {
		0xA0,
		0x02,
		0x48,
		0x04,
		0x90,
		0x01,
		0x20,
		0x80,
		0x40,
		0x04,
		0x12,
		0x09,
		0x2A,
		0x38,

};
const struct IrCode code_na125Code = {
	freq_to_timerval(55556),
	37,		// # of pairs
	3,		// # of bits per index
	code_na119Times,  
	code_na125Samples,  
};
// Duplicate IR Code???


/* Duplicate timing table, same as na017 !
const uint16_t code_na126Times[] = {
	56, 57,
	56, 175,
	56, 4150,
	56, 9499,
	898, 227,
	898, 449,
};
*/
const uint8_t code_na126Samples[] = {
		0xA4,
		0x10,
		0x00,
		0x20,
		0x82,
		0x49,
		0x00,
		0x02,
		0x00,
		0x04,
		0x90,
		0x49,
		0x2A,
		0x38,

};
const struct IrCode code_na126Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na017Times,  
	code_na126Samples,  
};

const uint16_t code_na127Times[] = {
	114, 100,
	115, 100,
	115, 200,
	115, 2706,
};
const uint8_t code_na127Samples[] = {
		0x1B,
		0x58,

};
const struct IrCode code_na127Code = {
	freq_to_timerval(25641),
	7,		// # of pairs
	2,		// # of bits per index
	code_na127Times,  
	code_na127Samples,  
};


/* Duplicate timing table, same as na102 !
const uint16_t code_na128Times[] = {
	86, 87,
	86, 258,
	86, 3338,
	346, 348,
	348, 347,
};
*/
const uint8_t code_na128Samples[] = {
		0x60,
		0x02,
		0x08,
		0x00,
		0x02,
		0x49,
		0x04,
		0x12,
		0x49,
		0x0A,
		0x00,
		0x08,
		0x20,
		0x00,
		0x09,
		0x24,
		0x10,
		0x49,
		0x24,
		0x00,

};
const struct IrCode code_na128Code = {
	freq_to_timerval(40000),
	51,		// # of pairs
	3,		// # of bits per index
	code_na102Times,  
	code_na128Samples,  
};


/* Duplicate timing table, same as na017 !
const uint16_t code_na129Times[] = {
	56, 57,
	56, 175,
	56, 4150,
	56, 9499,
	898, 227,
	898, 449,
};
*/
const uint8_t code_na129Samples[] = {
		0xA4,
		0x92,
		0x49,
		0x20,
		0x00,
		0x00,
		0x00,
		0x02,
		0x00,
		0x04,
		0x90,
		0x49,
		0x2A,
		0x38,

};
const struct IrCode code_na129Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na017Times,  
	code_na129Samples,  
};

const uint16_t code_na130Times[] = {
	88, 90,
	88, 258,
	88, 2247,
	358, 349,
	358, 350,
};
const uint8_t code_na130Samples[] = {
		0x64,
		0x00,
		0x08,
		0x24,
		0x82,
		0x09,
		0x24,
		0x10,
		0x01,
		0x0A,
		0x10,
		0x00,
		0x20,
		0x92,
		0x08,
		0x24,
		0x90,
		0x40,
		0x04,
		0x00,

};
const struct IrCode code_na130Code = {
	freq_to_timerval(37037),
	51,		// # of pairs
	3,		// # of bits per index
	code_na130Times,  
	code_na130Samples,  
};


/* Duplicate timing table, same as na042 !
const uint16_t code_na131Times[] = {
	54, 65,
	54, 170,
	54, 4099,
	54, 8668,
	899, 226,
	899, 421,
};
*/
const uint8_t code_na131Samples[] = {
		0xA0,
		0x10,
		0x40,
		0x04,
		0x82,
		0x09,
		0x24,
		0x82,
		0x40,
		0x00,
		0x10,
		0x09,
		0x2A,
		0x38,

};
const struct IrCode code_na131Code = {
	freq_to_timerval(40000),
	37,		// # of pairs
	3,		// # of bits per index
	code_na042Times,  
	code_na131Samples,  
};

const uint16_t code_na132Times[] = {
	28, 106,
	28, 238,
	28, 370,
	28, 1173,
};
const uint8_t code_na132Samples[] = {
		0x22,
		0x20,
		0x00,
		0x17,
		0x22,
		0x20,
		0x00,
		0x14,

};
const struct IrCode code_na132Code = {
	freq_to_timerval(83333),
	31,		// # of pairs
	2,		// # of bits per index
	code_na132Times,  
	code_na132Samples,  
};

const uint16_t code_na133Times[] = {
	13, 741,
	15, 489,
	15, 740,
	17, 4641,
};
const uint8_t code_na133Samples[] = {
		0x2A,
		0x95,
		0xA7,
		0xAA,
		0x95,
		0xA4,

};
const struct IrCode code_na133Code = {
	freq_to_timerval(41667),
	23,		// # of pairs
	2,		// # of bits per index
	code_na133Times,  
	code_na133Samples,  
};


/* Duplicate timing table, same as na113 !
const uint16_t code_na134Times[] = {
	56, 54,
	56, 166,
	56, 3945,
	896, 442,
	896, 443,
};
*/
const uint8_t code_na134Samples[] = {
		0x60,
		0x90,
		0x00,
		0x24,
		0x10,
		0x00,
		0x04,
		0x92,
		0x00,
		0x00,
		0x00,
		0x49,
		0x2A,
		0x02,
		0x40,
		0x00,
		0x90,
		0x40,
		0x00,
		0x12,
		0x48,
		0x00,
		0x00,
		0x01,
		0x24,
		0x80,

};
const struct IrCode code_na134Code = {
	freq_to_timerval(40000),
	67,		// # of pairs
	3,		// # of bits per index
	code_na113Times,  
	code_na134Samples,  
};

const uint16_t code_na135Times[] = {
	53, 59,
	53, 171,
	53, 2301,
	892, 450,
	895, 448,
};
const uint8_t code_na135Samples[] = {
		0x60,
		0x12,
		0x49,
		0x00,
		0x00,
		0x09,
		0x00,
		0x00,
		0x49,
		0x24,
		0x80,
		0x00,
		0x00,
		0x12,
		0x49,
		0x24,
		0xA8,
		0x01,
		0x24,
		0x90,
		0x00,
		0x00,
		0x90,
		0x00,
		0x04,
		0x92,
		0x48,
		0x00,
		0x00,
		0x01,
		0x24,
		0x92,
		0x48,

};
const struct IrCode code_na135Code = {
	freq_to_timerval(38462),
	87,		// # of pairs
	3,		// # of bits per index
	code_na135Times,  
	code_na135Samples,  
};


/* Duplicate timing table, same as na135 !
const uint16_t code_na136Times[] = {
	53, 59,
	53, 171,
	53, 2301,
	892, 450,
	895, 448,
};
*/
const uint8_t code_na136Samples[] = {
		0x64,
		0x82,
		0x49,
		0x00,
		0x00,
		0x00,
		0x20,
		0x00,
		0x49,
		0x24,
		0x80,
		0x00,
		0x00,
		0x12,
		0x49,
		0x24,
		0xA8,
		0x48,
		0x24,
		0x90,
		0x00,
		0x00,
		0x02,
		0x00,
		0x04,
		0x92,
		0x48,
		0x00,
		0x00,
		0x01,
		0x24,
		0x92,
		0x48,

};
const struct IrCode code_na136Code = {
	freq_to_timerval(38610),
	87,		// # of pairs
	3,		// # of bits per index
	code_na135Times,  
	code_na136Samples,  
};
#endif
////////////////////////////////////////////////////////////////

const struct IrCode *NApowerCodes[]  = {
#ifdef NA_CODES
	&code_na000Code,
	&code_na001Code,
   	&code_na002Code,
	&code_na003Code,
	&code_na004Code,
	&code_na005Code,
	&code_na006Code,
	&code_na007Code,
	&code_na008Code,
	&code_na009Code,
	&code_na010Code,
	&code_na011Code,
	&code_na012Code,
	&code_na013Code,
	&code_na014Code,
	&code_na015Code,
	&code_na016Code,
	&code_na017Code,
	&code_na018Code,
	&code_na019Code,
	&code_na020Code,
	&code_na021Code,
	&code_na022Code,
	&code_na023Code,
	&code_na024Code,
	&code_na025Code,
	&code_na026Code,
	&code_na027Code,
	&code_na028Code,
	&code_na029Code,
	&code_na030Code,
	&code_na031Code,
	&code_na032Code,
	&code_na033Code,
	&code_na034Code,
	&code_na035Code,
	&code_na036Code,
	&code_na037Code,
	&code_na038Code,
	&code_na039Code,
	&code_na040Code,
	&code_na041Code,
	&code_na042Code,
	&code_na043Code,
	&code_na044Code,
	&code_na045Code,
	&code_na046Code,
	&code_na047Code,
	&code_na048Code,
	&code_na049Code,
	&code_na050Code,
	&code_na051Code,
	&code_na052Code,
	&code_na053Code,
	&code_na054Code,
	&code_na055Code,
	&code_na056Code,
	&code_na057Code,
	&code_na058Code,
	&code_na059Code,
	&code_na060Code,
	&code_na061Code,
	&code_na062Code,
	&code_na063Code,
	&code_na064Code,
	&code_na065Code,
	&code_na066Code,
	&code_na067Code,
	&code_na068Code,
	&code_na069Code,
	&code_na070Code,
	&code_na071Code,
	&code_na072Code,
	&code_na073Code,
	&code_na074Code,
	&code_na075Code,
	&code_na076Code,
	&code_na077Code,
	&code_na078Code,
	&code_na079Code,
	&code_na080Code,
	&code_na081Code,
	&code_na082Code,
	&code_na083Code,
	&code_na084Code,
	&code_na085Code,
	&code_na086Code,
	&code_na087Code,
	&code_na088Code,
	&code_na089Code,
	&code_na090Code,
	&code_na091Code,
	&code_na092Code,
	&code_na093Code,
	&code_na094Code,
	&code_na095Code,
	&code_na096Code,
	&code_na097Code,
	&code_na098Code,
	&code_na099Code,
	&code_na100Code,
	&code_na101Code,
	&code_na102Code,
	&code_na103Code,
	&code_na104Code,
	&code_na105Code,
	&code_na106Code,
	&code_na107Code,
	&code_na108Code,
	&code_na109Code,
	&code_na110Code,
	&code_na111Code,
	&code_na112Code,
	&code_na113Code,
	&code_na114Code,
//#ifndef EU_CODES
	&code_na115Code,
	&code_na116Code,
	&code_na117Code,
	&code_na118Code,
	&code_na119Code,
	&code_na120Code,
	&code_na121Code,	
	&code_na122Code,
	&code_na123Code,
	&code_na124Code,
	&code_na125Code,
	&code_na126Code,
	&code_na127Code,
/*	&code_na128Code,
	&code_na129Code,
	&code_na130Code,
	&code_na131Code,
	&code_na132Code,
	&code_na133Code,
	&code_na134Code,
	&code_na135Code,
	&code_na136Code,*/
//#endif
#endif
}; 

/* -- Incomplete EU power code files to generate EU power codes for PIC
const struct IrCode *EUpowerCodes[]  = {
#ifdef EU_CODES
    &code_eu000Code,
	&code_eu001Code,
	&code_eu002Code,
	&code_na000Code, // same as &code_eu003Code
	&code_eu004Code,
	&code_eu005Code,
	&code_eu006Code,
	&code_eu007Code,
	&code_eu008Code,
	&code_na005Code, // same as &code_eu009Code
	&code_na004Code, // same as &code_eu010Code
	&code_eu011Code,
	&code_eu012Code,
	&code_eu013Code,
	&code_na021Code, // same as &code_eu014Code
	&code_eu015Code,
	&code_eu016Code,
	&code_eu017Code,
	&code_eu018Code,
	&code_eu019Code,
	&code_eu020Code,
	&code_eu021Code,
	&code_eu022Code,
	&code_na022Code, // same as &code_eu023Code
	&code_eu024Code,
	&code_eu025Code,
	&code_eu026Code,
	&code_eu027Code,
	&code_eu028Code,
	&code_eu029Code,
	&code_eu030Code,
	&code_eu031Code,
	&code_eu032Code,
	&code_eu033Code,
	&code_eu034Code,
	//&code_eu035Code, same as eu009
	&code_eu036Code,
	&code_eu037Code,
	&code_eu038Code,
	&code_eu039Code,
	&code_eu040Code,
	&code_eu041Code,
	&code_eu042Code,
	&code_eu043Code,
	&code_eu044Code,
	&code_eu045Code,
	&code_eu046Code,
	&code_eu047Code,
	&code_eu048Code,
	&code_eu049Code,
	&code_eu050Code,
	&code_eu051Code,
	&code_eu052Code,
	&code_eu053Code,
	&code_eu054Code,
	&code_eu055Code,
	&code_eu056Code,
	//&code_eu057Code, same as eu008
	&code_eu058Code,
	&code_eu059Code,
	&code_eu060Code,
	&code_eu061Code,
	&code_eu062Code,
	&code_eu063Code,
	&code_eu064Code,
	&code_eu065Code,
	&code_eu066Code,
	&code_eu067Code,
	&code_eu068Code,
	&code_eu069Code,
	&code_eu070Code,
	&code_eu071Code,
	&code_eu072Code,
	&code_eu073Code,
	&code_eu074Code,
	&code_eu075Code,
	&code_eu076Code,
	&code_eu077Code,
	&code_eu078Code,
	&code_eu079Code,
	&code_eu080Code,
	&code_eu081Code,
	&code_eu082Code,
	&code_eu083Code,
	&code_eu084Code,
	&code_eu085Code,
	&code_eu086Code,
	&code_eu087Code,
	&code_eu088Code,
	&code_eu089Code,
	&code_eu090Code,
	&code_eu091Code,
	&code_eu092Code,
	&code_eu093Code,
	&code_eu094Code,
	&code_eu095Code,
	&code_eu096Code,
	&code_eu097Code,
	&code_eu098Code,
	&code_eu099Code,
	&code_eu100Code,
	&code_eu101Code,
	&code_eu102Code,
	&code_eu103Code,
	&code_eu104Code,
	&code_eu105Code,
	&code_eu106Code,
	&code_eu107Code,
	&code_eu108Code,
	&code_eu109Code,
	&code_eu110Code,
	&code_eu111Code,
	&code_eu112Code,
	&code_eu113Code,
	&code_eu114Code,
#ifndef NA_CODES
	&code_eu115Code,
	&code_eu116Code,
	&code_eu117Code,
	&code_eu118Code,
	&code_eu119Code,
	&code_eu120Code,
	&code_eu121Code,
	&code_eu122Code,
	&code_eu123Code,
	&code_eu124Code,
	&code_eu125Code,
	&code_eu126Code,
	&code_eu127Code,
	&code_eu128Code,
	&code_eu129Code,
	&code_eu130Code,
	&code_eu131Code,
	&code_eu132Code,
	&code_eu133Code,
	&code_eu134Code,
	&code_eu135Code,
	&code_eu136Code,
	&code_eu137Code,
	&code_eu138Code,
	&code_eu139Code,
#endif
#endif
};
*/
//const uint8_t num_NAcodes = 100; //NUM_ELEM(NApowerCodes);
//const uint8_t num_EUcodes = 100; //NUM_ELEM(EUpowerCodes);

