
void SW3W_start(void);
void SW3W_startr(void);
void SW3W_stop(void);
void SW3W_stopr(void);
uint32_t SW3W_send(uint32_t d);
uint32_t SW3W_read(void);
void SW3W_clkh(void);
void SW3W_clkl(void);
void SW3W_dath(void);
void SW3W_datl(void);
uint32_t SW3W_dats(void);
void SW3W_clk(void);
uint32_t SW3W_bitr(void);
uint32_t SW3W_period(void);
void SW3W_macro(uint32_t macro);
void SW3W_setup(void);
void SW3W_setup_exc(void);
void SW3W_cleanup(void);
void SW3W_pins(void);
void SW3W_settings(void);
void SW3W_help(void);

#define SW3WPERIODMENU	"\r\nPeriodd in us (>20)\r\nperiod> "
#define SW3WCSMENU	"\r\nCS mode\r\n 1. CS\r\n 2. !CS*\r\ncs> "
#define SW3WODMENU	"\r\nSelect output type:\r\n 1. Normal (H=3.3V, L=GND)*\r\n 2. Open drain (H=Hi-Z, L=GND)\r\noutput> "
#define SW3WCPOLMENU	"\r\nClock polarity\r\n 1. idle low\r\n 2. idle high*\r\ncpol> "
#define SW3WCPHAMENU	"\r\nClock phase\r\n 1. leading edge\r\n 2. trailing edge*\r\ncpha> "
