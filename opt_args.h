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

struct _command_struct {
    char command[MAX_COMMAND_LENGTH];
    bool allow_hiz;
    void (*func)(struct command_result* res);
    uint32_t help_text;
};