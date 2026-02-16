#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"
#include "hwi2c.h"
#include "ui/ui_help.h"
#include "ui/ui_term.h"
#include "system_config.h"
#include "commands/global/w_psu.h"

#include "usbpd.h"

#define I2C_ADDR_FUSB302_WRITE 0x44
#define I2C_ADDR_FUSB302_READ 0x45

#define I2C_REG_ID 1


static void usbpd_sniff(struct command_result* res) {
  // STUB
}

const struct _mode_command_struct usbpd_commands[] = {
  {
    .command="sniff",
    .func=&usbpd_sniff,
    .supress_fala_capture=true
  },    
};
const uint32_t usbpd_commands_count = count_of(usbpd_commands);

uint32_t usbpd_setup(void) {
  hwi2c_set_speed(400000);
  hwi2c_set_databits(8);

  return 1;
}

uint32_t usbpd_setup_exc(void) {
  uint32_t retval = hwi2c_setup_exc();
  if (retval != 1) {
    return retval;
  }

  // 5V power on
  psucmd_enable(5.0, 20.0, false, 100);

  sleep_ms(500);

  struct _bytecode result;
  hwi2c_start(&result, NULL);
  result.out_data = I2C_ADDR_FUSB302_WRITE;
  hwi2c_write(&result, NULL);
  result.out_data = I2C_REG_ID;
  hwi2c_write(&result, NULL);
  hwi2c_start(&result, NULL);
  result.out_data = I2C_ADDR_FUSB302_READ;
  hwi2c_write(&result, NULL);
  struct _bytecode next;
  next.command = SYN_STOP;
  hwi2c_read(&result, &next);
  hwi2c_stop(&result, NULL);

  return result.error;
}

void usbpd_cleanup(void) {
  hwi2c_cleanup();
}

void usbpd_help(void) {
  printf("USBPD Plank\r\n");

  //ui_help_mode_commands(usbpd_commands, usbpd_commands_count);
}

void usbpd_settings(void) {
  // STUB
}
