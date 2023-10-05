//struct __attribute__((packed, aligned(sizeof(uint64_t)))) _bytecode_output{
struct _bytecode_output{    
	uint8_t number_format;
    uint8_t command; //255 command options, write/write_return
	uint32_t bits; //0-32 bits?
	uint32_t repeat; //0-0xffff repeat
	uint32_t data; //32 data bits
    bool has_repeat;
    bool has_bits;
};

//need a way to generate multiple results from a single repeated command
//track by command ID? sequence number?
struct _bytecode_result{
    struct _bytecode_output output;
	uint8_t error; //mode flags errors. One bit to halt execution? Other bits for warnings? ccan override the halt from configuration menu?
	uint32_t result; //up to 32bits results? BUT: how to deal with repeated reads????
};

enum SYNTAX{
    SYN_WRITE=0,
    SYN_WRITE_READ,
    SYN_READ,
    SYN_START,
    SYN_STOP,
    SYN_DELAY_US,
    SYN_DELAY_MS,
    SYN_AUX_OUTPUT,
    SYN_AUX_INPUT,
    SYN_ADC,
    SYN_FREQ
};

bool syntax_compile(struct opt_args *args);
bool syntax_run(void);
bool syntax_post(void);
