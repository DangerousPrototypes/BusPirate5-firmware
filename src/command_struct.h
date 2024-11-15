#define MAX_COMMAND_LENGTH 10

typedef struct command_result {
    uint8_t number_format;
    bool success;
    bool exit;
    bool no_value;
    bool default_value;
    bool error;
    bool help_flag;
} command_result;

struct _global_command_struct {
    char command[MAX_COMMAND_LENGTH];
    bool allow_hiz;
    void (*func)(struct command_result* res);
    uint32_t help_text;
};

struct _mode_command_struct {
    char command[MAX_COMMAND_LENGTH];
    void (*func)(struct command_result* res);
    uint32_t description_text;
    bool supress_fala_capture;
    //bool supress_system_help; //do any actually use this? maybe remove this option
};

struct command_response {
    bool error;
    uint32_t data;
};

struct command_attributes {
    bool has_value;
    bool has_dot;
    bool has_colon;
    bool has_string;
    uint8_t command;       // the actual command called
    uint8_t number_format; // DEC/HEX/BIN
    uint32_t value;        // integer value parsed from command line
    uint32_t dot;          // value after .
    uint32_t colon;        // value after :
};
