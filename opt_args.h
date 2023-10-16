typedef struct opt_args {
    bool no_value;
    bool error;
    bool type;
    bool success;
    uint8_t number_format;
    uint8_t units;
    uint32_t i;
    float f;
    uint16_t len;
    uint16_t max_len;    
	char c[OPTARG_STRING_LEN+1];
} opt_args;

typedef struct command_result {
	uint8_t number_format;
    bool success;
	bool exit;
	bool no_value;
	bool default_value;
	bool error;
} command_result;

struct _parsers
{
    bool (*opt_parser)(opt_args *args);
};

struct _command_parse
{
    bool allow_hiz;
    void (*command)(opt_args *args, struct command_result *res);
    const struct _parsers (*parsers);
    const char (*help_text);

};