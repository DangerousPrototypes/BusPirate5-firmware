
void infrared_write(struct _bytecode *result, struct _bytecode *next);
void infrared_read(struct _bytecode *result, struct _bytecode *next);
void infrared_start(struct _bytecode *result, struct _bytecode *next);
void infrared_stop(struct _bytecode *result, struct _bytecode *next);

// full duplex commands not currently implemented
void infrared_startr(void);
void infrared_stopr(void);

// passes the number in (1) = 1 for mode based macros
void infrared_macro(uint32_t macro);

// a periodic service call for doing things async.
uint32_t infrared_periodic(void);

// setup functions
uint32_t infrared_setup(void);
uint32_t infrared_setup_exc(void); 
void infrared_cleanup(void);

// displayed in the 'i' command
void infrared_settings(void);

const char * infrared_pins(void);

void infrared_help(void);

uint32_t infrared_get_speed(void);

extern const struct _command_struct infrared_commands[];
extern const uint32_t infrared_commands_count;

typedef struct _infrared_mode_config{
	uint32_t baudrate;
}_infrared_mode_config;
