void binmode_setup(void);
void binmode_service(void);
void binmode_cleanup(void);

enum {
	BINMODE_USE_SUMPLA=0,
	BINMODE_USE_DIRTYPROTO,
	BINMODE_USE_ARDUINO_CH32V003_SWIO,
	BINMODE_USE_FALA,
	BINMODE_MAXPROTO
};

typedef struct _binmode{
	const char *binmode_name;
	void (*binmode_setup)(void);
	void (*binmode_service)(void);
	void (*binmode_cleanup)(void);
	void (*binmode_hook_mode_exc)(uint32_t frequency);
	void (*binmode_hook_syntax_pre_run)(void);
	void (*binmode_hook_syntax_post_run)(void);
	void (*binmode_hook_syntax_post_output)(void);
} binmode_t; // Add a typedef name for the struct.

extern const binmode_t binmodes[BINMODE_MAXPROTO]; // Use the typedef name for the struct.