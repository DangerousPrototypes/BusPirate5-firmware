#ifndef FALA_H
#define FALA_H

// Global variables
extern const char fala_name[];

// Function declarations
void fala_setup(void);
void fala_cleanup(void);
void fala_start(void);
void fala_stop(void);
void fala_reset(void);
void fala_set_freq(uint32_t freq);
void fala_set_oversample(uint32_t oversample_rate);
void fala_notify(void);
void fala_service(void);
void fala_print_result(void);

#endif // FALA_H