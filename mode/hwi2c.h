
void HWI2C_start(void);
void HWI2C_stop(void);
uint32_t HWI2C_send(uint32_t d);
uint32_t HWI2C_read(void);
void HWI2C_macro(uint32_t macro);
uint32_t HWI2C_setup(void);
uint32_t HWI2C_setup_exc(void);
void HWI2C_cleanup(void);
//void HWI2C_pins(void);
void HWI2C_settings(void);
void HWI2C_printI2Cflags(void);
void HWI2C_help(void);

typedef struct _i2c_mode_config{
	uint32_t baudrate;
	uint32_t baudrate_actual;
	uint32_t data_bits;
	bool ack_pending;
} _i2c_mode_config;

/*
//A. syntax begins with bus start [ or /
//B. some kind of final byte before stop flag? look ahead?
//1. Compile loop: process all commands to bytecode
//2. Run loop: run the bytecode all at once
//3. Post-process loop: process output into UI

//mode commands:
//start
//stop
//write
//read

//global commands
//delay
//aux pins
//adc
//pwm?
//freq?

struct __attribute__((packed, aligned(sizeof(uint64_t)))) _bytecode_output{
	uint8_t command; //255 command options
	uint8_t bits; //0-32 bits?
	uint16_t repeat; //0-0xffff repeat
	uint32_t data; //32 data bits
};


//need a way to generate multiple results from a single repeated command
//track by command ID? sequence number?
struct _bytecode_result{
	uint8_t error; //mode flags errors. One bit to halt execution? Other bits for warnings? ccan override the halt from configuration menu?
	uint8_t command; //copied from above for post-process
	uint8_t bits;  //copied from above for post-process
	uint16_t repeat; //copied from above for post-process
	uint32_t data; //copied from above for post-process
	uint32_t result; //up to 32bits results? BUT: how to deal with repeated reads????
}
*/