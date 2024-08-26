#ifndef FALA_H
#define FALA_H

typedef struct {
    uint32_t base_frequency;
    uint32_t oversample;
    uint8_t debug_level;
} FalaConfig;

extern FalaConfig fala_config;

// Function declarations
void fala_set_freq(uint32_t freq);
void fala_set_oversample(uint32_t oversample_rate);
void fala_set_triggers(uint8_t trigger_pin, uint8_t trigger_level);
void fala_start(void);
void fala_stop(void);
void fala_print_result(void);

void fala_start_hook(void);
void fala_stop_hook(void);
void fala_notify_hook(void);
bool fala_notify_register(void (*hook)());
void fala_notify_unregister(void (*hook)());
void fala_mode_change_hook(void);

#endif // FALA_H