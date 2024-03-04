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