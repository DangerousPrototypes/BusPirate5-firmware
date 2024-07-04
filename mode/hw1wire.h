/*/
void hw1wire_start(void);
void hw1wire_startr(void);
void hw1wire_stop(void);
void hw1wire_stopr(void);
uint32_t hw1wire_send(uint32_t d);
uint32_t hw1wire_read(void);
void hw1wire_clkh(void);
void hw1wire_clkl(void);
void hw1wire_dath(void);
void hw1wire_datl(void);
uint32_t hw1wire_dats(void);
void hw1wire_clk(void);
uint32_t hw1wire_bitr(void);
uint32_t hw1wire_period(void);
void hw1wire_macro(uint32_t macro);
*/
uint32_t hw1wire_setup(void);
uint32_t hw1wire_setup_exc(void);
void hw1wire_cleanup(void);
void hw1wire_pins(void);
void hw1wire_settings(void);
void hw1wire_start(bytecode_t *result, bytecode_t *next);
void hw1wire_write(bytecode_t *result, bytecode_t *next);
void hw1wire_read(bytecode_t *result, bytecode_t *next);
void hw1wire_macro(uint32_t macro);
void hw1wire_help(void);

extern const struct _command_struct hw1wire_commands[];
extern const uint32_t hw1wire_commands_count;
/*/
unsigned char OWReset(void);
unsigned char OWBit(unsigned char c);
unsigned char OWByte(unsigned char OWbyte);
void DS1wireReset(void);
void DS1wireID(unsigned char famID);
unsigned char OWFirst(void);
unsigned char OWNext(void);
unsigned char OWSearch(void);
unsigned char OWVerify(void);
unsigned char docrc8(unsigned char value);

#define OWWriteByte(d) OWByte(d)
#define OWReadByte() OWByte(0xFF)
#define OWReadBit() OWBit(1)
#define OWWriteBit(b) OWBit(b)
*/