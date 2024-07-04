#pragma once

//struct __attribute__((packed, aligned(sizeof(uint64_t)))) _bytecode_output{
typedef struct _bytecode{    
	uint8_t number_format;
    uint8_t command; //255 command options, write/write_return
	uint32_t bits; //0-32 bits?
	uint32_t repeat; //0-0xffff repeat
	uint32_t out_data; //32 data bits
    bool has_repeat;
    bool has_bits; 
	uint8_t error; //mode flags errors. One bit to halt execution? Other bits for warnings? ccan override the halt from configuration menu?
    const char *error_message;
	uint32_t in_data; //up to 32bits results? BUT: how to deal with repeated reads????
    const char *data_message;    
    bool read_with_write;
} bytecode_t;

typedef struct _bytecode_output{    
	uint8_t number_format;
    uint8_t command; //255 command options, write/write_return
	uint32_t bits; //0-32 bits?
	uint32_t repeat; //0-0xffff repeat
	uint32_t data; //32 data bits
    bool has_repeat;
    bool has_bits; 
} bytecode_output_t;

//need a way to generate multiple results from a single repeated command
//track by command ID? sequence number?
typedef struct _bytecode_result {
    bytecode_output_t output;
	uint8_t error; //mode flags errors. One bit to halt execution? Other bits for warnings? ccan override the halt from configuration menu?
    const char *error_message;
	uint32_t data; //up to 32bits results? BUT: how to deal with repeated reads????
    const char *message;
} bytecode_result_t;

enum SYNTAX_RESULT{
    SRES_NONE=0,
    SRES_DEBUG,
    SRES_INFO,
    SRES_WARN,
    SRES_ERROR
};

enum SYNTAX{
    SYN_WRITE=0,
    SYN_READ,
    SYN_START,
    SYN_STOP,
    SYN_START_ALT,
    SYN_STOP_ALT,
    SYN_TICK_CLOCK,
    SYN_SET_CLK_HIGH, ///here
    SYN_SET_CLK_LOW,
    SYN_SET_DAT_HIGH,
    SYN_SET_DAT_LOW,
    SYN_READ_DAT,       //here
    SYN_DELAY_US,
    SYN_DELAY_MS,
    SYN_AUX_OUTPUT,
    SYN_AUX_INPUT,
    SYN_ADC,
    //SYN_FREQ
};
