#define MAX_COMMAND_LENGTH 9

/*typedef struct opt_args {
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
*/
typedef struct command_result {
	uint8_t number_format;
    bool success;
	bool exit;
	bool no_value;
	bool default_value;
	bool error;
    bool help_flag;
} command_result;

struct _command_struct
{
    char command[MAX_COMMAND_LENGTH];
    bool allow_hiz;
    void (*func)(struct command_result *res);
    uint32_t help_text;
};