struct _command_line {
    uint32_t wptr;
    uint32_t rptr;
    uint32_t histptr;
    uint32_t cursptr;
    char buf[UI_CMDBUFFSIZE];
};

struct _command_pointer {
    uint32_t wptr;
    uint32_t rptr;
};

struct _command_info_t {
    uint32_t rptr;
    uint32_t wptr;
    uint32_t startptr;
    uint32_t endptr;
    uint32_t nextptr;
    char delimiter;
    char command[9];
};

typedef struct command_var_struct {
    bool has_arg;
    bool has_value;
    uint32_t value_pos;
    bool error;
    uint8_t number_format;
} command_var_t;

bool cmdln_args_find_flag(char flag);
bool cmdln_args_find_flag_uint32(char flag, command_var_t *arg, uint32_t *value);
bool cmdln_args_find_flag_string(char flag, command_var_t *arg, uint32_t max_len, char *str);
bool cmdln_args_float_by_position(uint32_t pos, float *value);
bool cmdln_args_uint32_by_position(uint32_t pos, uint32_t *value);
bool cmdln_args_string_by_position(uint32_t pos, uint32_t max_len, char *str);
bool cmdln_find_next_command(struct _command_info_t *cp);
bool cmdln_info(void);
bool cmdln_info_uint32(void);

// update a command line buffer pointer with rollover
uint32_t cmdln_pu(uint32_t i); 
// try to add a byte to the command line buffer, return false if buffer full
bool cmdln_try_add(char *c);
// try to get a byte, return false if buffer empty
bool cmdln_try_remove(char *c);
// try to peek 0+n bytes (no pointer update), return false if end of buffer
// this should always be used on sequency (eg if(peek(0)){peek(1)}) 
// to avoid missing the end of the buffer
bool cmdln_try_peek(uint32_t i, char *c);
// try to discard n bytes (advance the pointer), return false if end of buffer 
//(should be used with try_peek to confirm before discarding...)
bool cmdln_try_discard(uint32_t i);
// this moves the read pointer to the write pointer, 
// allowing the next command line to be entered after the previous. 
// this allows the history scroll through the circular buffer
bool cmdln_next_buf_pos(void);

void cmdln_init(void);

bool cmdln_try_peek_pointer(struct _command_pointer *cp, uint32_t i, char *c);
void cmdln_get_command_pointer(struct _command_pointer *cp);

extern struct _command_line cmdln;