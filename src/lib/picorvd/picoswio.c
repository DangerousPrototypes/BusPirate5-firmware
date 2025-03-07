/*
Adapted from https://github.com/aappleby/picorvd
License: MIT
Changed by: Ian Lesnet, 2024 Where Labs LLC for Bus Pirate 5
*/

//#include "PicoSWIO.h"
#include "pico/stdlib.h"
#include "pirate.h"
#include "ch32vswio.pio.h"
#include "debug_defines.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "picoswio.h"
#include "pio_config.h"

static struct _pio_config pio_config;

#define DUMP_COMMANDS

// WCH-specific debug interface config registers
static const int WCH_DM_CPBR     = 0x7C;
static const int WCH_DM_CFGR     = 0x7D;
static const int WCH_DM_SHDWCFGR = 0x7E;
static const int WCH_DM_PART     = 0x7F; // not in doc but appears to be part info

const char* ch32swio_addr_to_regname(uint8_t addr);

//------------------------------------------------------------------------------
void ch32vswio_cleanup(void){
  pio_remove_program_and_unclaim_sm(&singlewire_program, pio_config.pio, pio_config.sm, pio_config.offset);
  pio_config.offset=0;
}

void ch32vswio_reset(int pin, int dirpin) {
  //CHECK(pin != -1);
    int pio_sm = 0;
  // Configure GPIO
  gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_2MA);
  gpio_set_slew_rate     (pin, GPIO_SLEW_RATE_SLOW);
  gpio_set_function      (pin, GPIO_FUNC_PIO0);

  gpio_set_drive_strength(dirpin, GPIO_DRIVE_STRENGTH_2MA);
  gpio_set_slew_rate     (dirpin, GPIO_SLEW_RATE_SLOW);
  gpio_set_function      (dirpin, GPIO_FUNC_PIO0);

  if(pio_config.offset) ch32vswio_cleanup(); //TODO: fix this hack...

  bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&singlewire_program, &pio_config.pio, &pio_config.sm, &pio_config.offset, dirpin, 9, true);
  hard_assert(success);
  printf("PIO: pio=%d, sm=%d, offset=%d\r\n", PIO_NUM(pio_config.pio), pio_config.sm, pio_config.offset);

  // Reset PIO module
//  pio_config.pio->ctrl = 0b000100010001;
  pio_sm_set_enabled(pio_config.pio, pio_config.sm, false);

  // Configure PIO module
  //TODO: side set pin is buffer direction, probably needs to be inverted
  //in pin should be buffer IO
  pio_sm_config c = pio_get_default_sm_config();
  sm_config_set_wrap        (&c, pio_config.offset + singlewire_wrap_target, pio_config.offset + singlewire_wrap);
  //sm_config_set_sideset     (&c, 1, /*optional*/ false, /*pindirs*/ true);
  sm_config_set_sideset     (&c, 1, /*optional*/ false, /*pindirs*/ false); //direct control of the buffer direction
  sm_config_set_out_pins    (&c, dirpin, 1);
  sm_config_set_in_pins     (&c, pin);
  sm_config_set_set_pins    (&c, dirpin, 1);
  sm_config_set_sideset_pins(&c, dirpin);
  sm_config_set_out_shift   (&c, /*shift_right*/ false, /*autopull*/ false, /*pull_threshold*/ 32);
  sm_config_set_in_shift    (&c, /*shift_right*/ false, /*autopush*/ true,  /*push_threshold*/ 32);

   sm_config_set_clkdiv(&c, clock_get_hz(clk_sys)/10000000);

#if RPI_PLATFORM == RP2040
// RP2350 has defective pull-downs (bug E9) that latch up
// RP2350 baords have extra large external pull-downs to compensate
// RP2040 has working pull-downs
// Don't enable pin pull-downs on RP2350
 gpio_pull_down(pin);
#endif
  pio_sm_set_pindirs_with_mask(pio_config.pio, pio_config.sm, 0, (1u<<pin)); //read pins to input (0, mask)  
  pio_sm_set_pindirs_with_mask(pio_config.pio, pio_config.sm, (1u<<dirpin), (1u<<dirpin)); //buf pins to output (pins, mask)    
  pio_sm_set_pins_with_mask(pio_config.pio, pio_config.sm, 0, (1u<<dirpin)); //buf dir to 0, buffer input/HiZ on the bus
  pio_gpio_init(pio_config.pio, dirpin);

  pio_sm_init       (pio_config.pio, pio_config.sm, pio_config.offset, &c);
  //pio_sm_set_pins   (pio0, pio_sm, 1);
  pio_sm_set_enabled(pio_config.pio, pio_config.sm, true);

  // Grab pin and send an 8 usec low pulse to reset debug module
  // If we use the sdk functions to do this we get jitter :/
  /*sio_hw->gpio_clr    = (1 << pin);
  sio_hw->gpio_oe_set = (1 << pin);
  iobank0_hw->io[pin].ctrl = GPIO_FUNC_SIO << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
  busy_wait_us(100); // ~8 usec
  sio_hw->gpio_oe_clr = (1 << pin);
  iobank0_hw->io[pin].ctrl = GPIO_FUNC_PIO0 << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
*/
  // Enable debug output pin on target
  ch32vswio_put(WCH_DM_SHDWCFGR, 0x5AA50400);
  ch32vswio_put(WCH_DM_CFGR,     0x5AA50400);

  // Reset debug module on target
  ch32vswio_put(DM_DMCONTROL, 0x00000000);
  ch32vswio_put(DM_DMCONTROL, 0x00000001);
}

//------------------------------------------------------------------------------

uint32_t ch32vswio_get(uint32_t addr) {
  //cmd_count++;
  pio_sm_put_blocking(pio_config.pio, pio_config.sm, ((~addr) << 1) | 1);
  uint32_t data = pio_sm_get_blocking(pio_config.pio, pio_config.sm);
#ifdef DUMP_COMMANDS
  printf("get_dbg %15s 0x%08x\r\n", ch32swio_addr_to_regname(addr), data);
#endif
  return data;
}

//------------------------------------------------------------------------------

void ch32vswio_put(uint32_t addr, uint32_t data) {
  //cmd_count++;
#ifdef DUMP_COMMANDS
  printf("set_dbg %15s 0x%08x\r\n", ch32swio_addr_to_regname(addr), data);
#endif
  pio_sm_put_blocking(pio_config.pio, pio_config.sm, ((~addr) << 1) | 0);
  pio_sm_put_blocking(pio_config.pio, pio_config.sm, ~data);
}
/*
//------------------------------------------------------------------------------

Reg_CPBR PicoSWIO::get_cpbr() {
  return get(WCH_DM_CPBR);
}

//------------------------------------------------------------------------------

Reg_CFGR PicoSWIO::get_cfgr() {
  return get(WCH_DM_CFGR);
}

//------------------------------------------------------------------------------

Reg_SHDWCFGR PicoSWIO::get_shdwcfgr() {
  return get(WCH_DM_SHDWCFGR);
}

//------------------------------------------------------------------------------

uint32_t PicoSWIO::get_partid() {
  return get(WCH_DM_PART);
}

//------------------------------------------------------------------------------

void PicoSWIO::dump() {
  get_cpbr().dump();
  get_cfgr().dump();
  get_shdwcfgr().dump();
  printf("DM_PARTID = 0x%08x\n", get_partid());
}

//------------------------------------------------------------------------------
*/
const char* ch32swio_addr_to_regname(uint8_t addr) {
  switch(addr) {
    case WCH_DM_CPBR:     return "WCH_DM_CPBR";
    case WCH_DM_CFGR:     return "WCH_DM_CFGR";
    case WCH_DM_SHDWCFGR: return "WCH_DM_SHDWCFGR";
    case WCH_DM_PART:     return "WCH_DM_PART";

    case DM_DATA0:        return "DM_DATA0";
    case DM_DATA1:        return "DM_DATA1";
    case DM_DMCONTROL:    return "DM_DMCONTROL";
    case DM_DMSTATUS:     return "DM_DMSTATUS";
    case DM_HARTINFO:     return "DM_HARTINFO";
    case DM_ABSTRACTCS:   return "DM_ABSTRACTCS";
    case DM_COMMAND:      return "DM_COMMAND";
    case DM_ABSTRACTAUTO: return "DM_ABSTRACTAUTO";
    case DM_PROGBUF0:     return "DM_PROGBUF0";
    case DM_PROGBUF1:     return "DM_PROGBUF1";
    case DM_PROGBUF2:     return "DM_PROGBUF2";
    case DM_PROGBUF3:     return "DM_PROGBUF3";
    case DM_PROGBUF4:     return "DM_PROGBUF4";
    case DM_PROGBUF5:     return "DM_PROGBUF5";
    case DM_PROGBUF6:     return "DM_PROGBUF6";
    case DM_PROGBUF7:     return "DM_PROGBUF7";
    case DM_HALTSUM0:     return "DM_HALTSUM0";
    default:              return "???";
  }
}
/*
//------------------------------------------------------------------------------

void Reg_CPBR::dump() {
  printf("DM_CPBR = 0x%08x\n", raw);
  printf("  TDIV:%d  SOPN:%d  CHECKSTA:%d  CMDEXTENSTA:%d  OUTSTA:%d  IOMODE:%d  VERSION:%d\n",
    TDIV, SOPN, CHECKSTA, CMDEXTENSTA, OUTSTA, IOMODE, VERSION);
}

void Reg_CFGR::dump() {
  printf("DM_CFGR = 0x%08x\n", raw);
  printf("  TDIVCFG:%d  SOPNCFG:%d  CHECKEN:%d  CMDEXTEN:%d  OUTEN:%d  IOMODECFG:%d  KEY:0x%04x\n",
    TDIVCFG, SOPNCFG, CHECKEN, CMDEXTEN, OUTEN, IOMODECFG, KEY);
}

void Reg_SHDWCFGR::dump() {
  printf("DM_SHDWCFGR = 0x%08x\n", raw);
  printf("  TDIVCFG:%d  SOPNCFG:%d  CHECKEN:%d  CMDEXTEN:%d  OUTEN:%d  IOMODECFG:%d  KEY:0x%04x\n",
    TDIVCFG, SOPNCFG, CHECKEN, CMDEXTEN, OUTEN, IOMODECFG, KEY);
}
*/