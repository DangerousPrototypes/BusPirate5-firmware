#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>

#include "pirate.h"
#include "hardware/uart.h"
#include "bytecode.h"
#include "command_struct.h"
#include "modes.h"
#include "ui/ui_const.h"
#include "ui_cmdln.h"

// initializes the ui variables
void ui_init(void) {
    cmdln_init();
}
