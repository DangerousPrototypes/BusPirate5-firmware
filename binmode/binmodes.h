void binmode_setup(void);
void binmode_service(void);
void binmode_cleanup(void);

enum {
	BINMODE_USE_SUMPLA=0,
	BINMODE_USE_DIRTYPROTO,
	BINMODE_USE_ARDUINO_CH32V003_SWIO,
	BINMODE_USE_LEGACY2THIRD,
	BINMODE_MAXPROTO
};

typedef struct _binmode{
	void (*binmode_setup)(void);
	void (*binmode_service)(void);
	void (*binmode_cleanup)(void);
	const char *binmode_name;
} binmode_t; // Add a typedef name for the struct.

extern const binmode_t binmodes[BINMODE_MAXPROTO]; // Use the typedef name for the struct.