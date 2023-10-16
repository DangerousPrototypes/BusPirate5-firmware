#ifndef _BYTECODE
#define _BYTECODE

/*//struct __attribute__((packed, aligned(sizeof(uint64_t)))) _bytecode_output{
struct _bytecode_output{    
	uint8_t number_format;
    uint8_t command; //255 command options, write/write_return
	uint32_t bits; //0-32 bits?
	uint32_t repeat; //0-0xffff repeat
	uint32_t data; //32 data bits
    bool has_repeat;
    bool has_bits;
} bytecode_output;

//need a way to generate multiple results from a single repeated command
//track by command ID? sequence number?
struct _bytecode_result{
    struct _bytecode_output output;
	uint8_t error; //mode flags errors. One bit to halt execution? Other bits for warnings? ccan override the halt from configuration menu?
	uint32_t result; //up to 32bits results? BUT: how to deal with repeated reads????
} bytecode_result;
*/

struct _errorbc{
	uint8_t error; //mode flags errors. One bit to halt execution? Other bits for warnings? ccan override the halt from configuration menu?
	uint32_t result; //up to 32bits results? BUT: how to deal with repeated reads????
}error_bytecode;

#endif