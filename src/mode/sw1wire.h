/**
 * @file sw1wire.h
 * @brief Software 1-Wire mode interface.
 * @details Provides bit-banged 1-Wire/Dallas protocol mode.
 */

void ONEWIRE_start(void);
void ONEWIRE_startr(void);
void ONEWIRE_stop(void);
void ONEWIRE_stopr(void);
uint32_t ONEWIRE_send(uint32_t d);
uint32_t ONEWIRE_read(void);
void ONEWIRE_clkh(void);
void ONEWIRE_clkl(void);
void ONEWIRE_dath(void);
void ONEWIRE_datl(void);
uint32_t ONEWIRE_dats(void);
void ONEWIRE_clk(void);
uint32_t ONEWIRE_bitr(void);
uint32_t ONEWIRE_period(void);
void ONEWIRE_macro(uint32_t macro);
void ONEWIRE_setup(void);
void ONEWIRE_setup_exc(void);
void ONEWIRE_cleanup(void);
void ONEWIRE_pins(void);
void ONEWIRE_settings(void);
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
