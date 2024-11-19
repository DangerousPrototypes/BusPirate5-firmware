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
    char command[MAX_COMMAND_LENGTH]; //command line string to execute command
    bool allow_hiz; //allow execution in high impedance mode
    void (*func)(struct command_result* res); //function to execute
    uint32_t help_text; // translation string to show when -h is used, 0x00 = command can manage it's own extended help
};

struct _mode_command_struct {
    char command[MAX_COMMAND_LENGTH]; //command line string to execute command
    void (*func)(struct command_result* res); //function to execute
    uint32_t description_text; // shown in help and command lists
    bool supress_fala_capture; //global follow along logic analyzer is disabled, can be managed within the command
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
