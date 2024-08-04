typedef struct _binmode{
	void (*binmode_service)(void);
} binmode_t;

extern struct _binmode binmodes[];