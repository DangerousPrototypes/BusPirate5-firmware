

typedef struct {
    PIO pio;
    uint sm;
    uint jmp_reset;
    int offset;
    int gpio;
} OW;

bool ow_init (uint8_t bits_per_word, uint bufdir, uint inpin) ;
void ow_send (uint data);
uint8_t ow_read (void);
bool ow_reset (void);
int ow_romsearch (uint64_t *romcodes, int maxdevs, uint command);
void ow_cleanup (void);