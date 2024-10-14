void binloopback_open(struct _bytecode* result, struct _bytecode* next);      // start
void binloopback_open_read(struct _bytecode* result, struct _bytecode* next); // start with read
void binloopback_close(struct _bytecode* result, struct _bytecode* next);     // stop
void binloopback_write(struct _bytecode* result, struct _bytecode* next);
void binloopback_read(struct _bytecode* result, struct _bytecode* next);
void binloopback_macro(uint32_t macro);
uint32_t binloopback_setup(void);
uint32_t binloopback_setup_exc(void);
void binloopback_cleanup(void);
void binloopback_pins(void);
void binloopback_settings(void);
void binloopback_printerror(void);
void binloopback_help(void);
void binloopback_periodic(void);

typedef struct _binloopback_mode_config {

    uint32_t blocking;
    bool async_print;
} _binloopback_mode_config;

extern const struct _command_struct binloopback_commands[];
extern const uint32_t binloopback_commands_count;