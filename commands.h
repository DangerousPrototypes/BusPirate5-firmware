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
/*
struct _command
{
    bool allow_hiz;
    void (*command)( struct command_attributes *attributes, struct command_response *response);

};

extern struct _command commands[];
*/
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


struct _command_parse
{
    bool allow_hiz;
    void (*command)(struct opt_args *args, struct command_result *res);
    bool (*opt1_parser)(struct opt_args *args);
    //bool (*opt2_parser)(struct command_attributes *attributes, struct command_response *response);
    //bool (*opt3_parser)(struct command_attributes *attributes, struct command_response *response);
    //bool (*opt4_parser)(struct command_attributes *attributes, struct command_response *response);
    //bool (*opt5_parser)(struct command_attributes *attributes, struct command_response *response);
    char (*help_text);

};


struct _parsers
{
    bool (*opt_parser)(struct opt_args *args);
};

struct _command_parse_new
{
    bool allow_hiz;
    void (*command)(struct opt_args *args, struct command_result *res);
    struct _parsers (*parsers)[5];
    //bool (*opt2_parser)(struct command_attributes *attributes, struct command_response *response);
    //bool (*opt3_parser)(struct command_attributes *attributes, struct command_response *response);
    //bool (*opt4_parser)(struct command_attributes *attributes, struct command_response *response);
    //bool (*opt5_parser)(struct command_attributes *attributes, struct command_response *response);
    char (*help_text);

};




extern struct _command_parse exec_new[];
extern const char *cmd[];
extern const uint32_t count_of_cmd;
#endif
