#include <stdio.h>
//#include "pico/stdlib.h"
#include <stdint.h>

// include file from openocd/src/helper
static const char * const jep106[][126] = {
    #include "jep106.inc"
};

const char *jep106_table_manufacturer(uint8_t bank, uint8_t id){
	if (id < 1 || id > 126) {
		return "Unknown";
	}
	/* index is zero based */
	id--;
	if (bank >= sizeof(jep106) || jep106[bank][id] == 0)
		return "Unknown";
	return jep106[bank][id];
}