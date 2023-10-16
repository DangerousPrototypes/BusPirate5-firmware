#ifndef _UI_COMMAND_H
#define _UI_COMMAND_H
struct command_response
{
    bool error;
    uint32_t data;
};

struct command_attributes
{   bool has_value;
    bool has_dot;
    bool has_colon;
    bool has_string;
    uint8_t command;    //the actual command called
    uint8_t number_format; //DEC/HEX/BIN
    uint32_t value;     // integer value parsed from command line
    uint32_t dot;       // value after .
    uint32_t colon;     // value after :
};

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

extern const struct _command_parse exec_new[];
extern const char *cmd[];
extern const uint32_t count_of_cmd;
#endif
