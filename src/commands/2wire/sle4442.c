#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "opt_args.h"
#include "pirate/hw2wire_pio.h"
#include "pirate/bio.h"
#include "ui/ui_cmdln.h"
#include "ui/ui_help.h"
#include "ui/ui_command.h"
#include "ui/ui_format.h"
#include "bytecode.h"
#include "mode/hw2wire.h"
#include "pirate/psu.h"
#include "usb_rx.h"
#include "fatfs/ff.h"
#include "pirate/storage.h"


#define SLE_CMD_READ_MEM 0x30
#define SLE_CMD_WRITE_MEM 0x38
#define SLE_CMD_READ_SECMEM 0x31
#define SLE_CMD_WRITE_SECMEM 0x39
#define SLE_CMD_READ_PRTMEM 0x34
#define SLE_CMD_WRITE_PRTMEM 0x3C
#define SLE_CMD_COMPARE_VERIFICATION_DATA 0x33

typedef struct __attribute__((packed)) sle44xx_atr_struct {
    uint8_t structure_identifier : 3;
    bool rfu1 : 1;
    uint8_t protocol_type : 4;
    uint8_t data_units_bits : 3;
    uint8_t data_units : 4;
    bool read_with_defined_length : 1;
    uint16_t rfu2 : 16;
} sle44xx_atr_t;

static const char* const usage[] = { "sle4442 [init|dump|unlock|write|erase|psc]\r\n\t[-a <address>] [-v <value>] [-p "
                                     "<current psc>] [-n <new psc>] [-h(elp)]",
                                     "Initialize and probe: sle4442 init",
                                     "Dump contents: sle4442 dump",
                                     "Unlock card: sle4442 unlock -p 0xffffff",
                                     "Write a value: sle4442 write -a 0xff -v 0x55",
                                     "Erase memory: sle4442 erase",
                                     "Update PSC: sle4442 psc -p 0xffffff -n 0x000000",
                                    "Dump contents to file: sle4442 dump -f dump.bin", 
                                    "Dump format: DATA[0:255],SECMEM[256:259],PRTMEM[260:263]"
                                    }; 

static const struct ui_help_options options[] = {
    { 1, "", T_HELP_SLE4442 }, // command help
    { 0, "init", T_HELP_SLE4442_INIT },
    { 0, "dump", T_HELP_SLE4442_DUMP },
    { 0, "unlock", T_HELP_SLE4442_UNLOCK },
    { 0, "write", T_HELP_SLE4442_WRITE },
    { 0, "erase", T_HELP_SLE4442_ERASE },
    { 0, "psc", T_HELP_SLE4442_PSC },
    { 0, "-a", T_HELP_SLE4442_ADDRESS_FLAG },
    { 0, "-v", T_HELP_SLE4442_VALUE_FLAG },
    { 0, "-p", T_HELP_SLE4442_CURRENT_PSC_FLAG },
    { 0, "-n", T_HELP_SLE4442_NEW_PSC_FLAG },
    { 0, "-f", T_HELP_SLE4442_FILE_FLAG },
};

uint32_t sle4442_ticks(void) {
    for (uint32_t i = 0; i < 0x100; i++) {
        pio_hw2wire_clock_tick();
    }
    return 0;
}

#if 0
uint32_t sle4442_ticks(void){
	uint32_t i=0;
	uint32_t timeout =0xffff;
	while(bio_get(M_2WIRE_SDA)==0){
		pio_hw2wire_clock_tick();
		i++;
		timeout--;
		if(timeout==0){
			printf("Timeout\r\n");
			break;
		}
	}
	return i;
}
#endif
void sle4442_write(uint32_t command, uint32_t address, uint32_t data) {
    pio_hw2wire_start();
    pio_hw2wire_put16((ui_format_lsb(command, 8)));
    pio_hw2wire_put16((ui_format_lsb(address, 8)));
    pio_hw2wire_put16((ui_format_lsb(data, 8)));
    pio_hw2wire_stop();
}

// ISO7816-3 Answer To Reset
bool sle4442_reset(char* atr) {
    bio_output(2); // IO2/RST low
    busy_wait_ms(1);
    bio_input(2); // IO2 high
    pio_hw2wire_clock_tick();
    busy_wait_us(50);
    bio_output(2);
    // read 4 bytes (32 bits)
    for (uint i = 0; i < 4; i++) {
        pio_hw2wire_get16(&atr[i]);
        atr[i] = ui_format_lsb(atr[i], 8);
    }
    if (atr[0] == 0x00 || atr[0] == 0xFF) {
        return false;
    }
    return true;
}

bool sle4442_atr_decode(char* atr) {
    sle44xx_atr_t* atr_head;
    atr_head = (sle44xx_atr_t*)atr;
    printf("--SLE44xx decoder--\r\n");
    printf("ATR: 0x%02x 0x%02x 0x%02x 0x%02x\r\n", atr[0], atr[1], atr[2], atr[3]);
    printf("Protocol Type: %s %d\r\n", (atr_head->protocol_type == 0b1010 ? "S" : "unknown"), atr_head->protocol_type);
    printf("Structure Identifier: %s\r\n",
           (atr_head->structure_identifier & 0b11 == 0b000 ? "ISO Reserved"
            : (atr_head->structure_identifier == 0b010)    ? "General Purpose (Structure 1)"
            : (atr_head->structure_identifier == 0b110)    ? "Proprietary"
                                                           : "Special Application"));
    printf("Read: %s\r\n", (atr_head->read_with_defined_length ? "Defined Length" : "Read to end"));
    printf("Data Units: ");
    if (atr_head->data_units == 0b0000) {
        printf("Undefined\r\n");
    } else {
        printf("%.0f\r\n", pow(2, atr_head->data_units + 6));
    }
    printf("Data Units Bits: %.0f\r\n", pow(2, atr_head->data_units_bits));
    if (atr_head->protocol_type == 0b1010) {
        return true;
    }
    return false;
}

bool sle4442_read_secmem(char* secmem) {
    // I2C start, 0x31, 0x00, 0x00, I2C stop
    sle4442_write(SLE_CMD_READ_SECMEM, 0, 0);
    for (uint i = 0; i < 4; i++) {
        pio_hw2wire_get16(&secmem[i]);
        secmem[i] = ui_format_lsb(secmem[i], 8);
    }
    //
    if (secmem[0] <= 7) {
        return true;
    }
    printf("Security memory: 0x%02x 0x%02x 0x%02x 0x%02x\r\n", secmem[0], secmem[1], secmem[2], secmem[3]);
    return false;
}

void sle4442_decode_secmem(char* secmem) {
    printf("Security memory: 0x%02x 0x%02x 0x%02x 0x%02x\r\n", secmem[0], secmem[1], secmem[2], secmem[3]);
    printf("Remaining attempts: %d (0x%1X)\r\n",
           (secmem[0] & 0b100 ? 1 : 0) + (secmem[0] & 0b010 ? 1 : 0) + (secmem[0] & 0b001 ? 1 : 0),
           secmem[0]);
}

bool sle4442_read_prtmem(char* prtmem) {
    // I2C start, 0x31, 0x00, 0x00, I2C stop
    sle4442_write(SLE_CMD_READ_PRTMEM, 0, 0);
    for (uint i = 0; i < 4; i++) {
        pio_hw2wire_get16(&prtmem[i]);
        prtmem[i] = ui_format_lsb(prtmem[i], 8);
    }
    return true;
}

bool sle4442_unlock(uint32_t psc) {
    char data[5];

    sle4442_read_secmem(data);

    printf("Unlocking with PSC: 0x%06X\r\n", psc);
    /*if(data[0]==0){ //should still have secmem from init
        printf("Card cannot be unlocked. Remaining attempts: 0\r\n");
        return false;
    }*/

    uint32_t security_bit = 0b100;
    if (data[0] & 0b100) {
        security_bit = 0b011;
    } else if (data[0] & 0b010) {
        security_bit = 0b101;
    } else if (data[0] & 0b001) {
        security_bit = 0b110;
    } else {
        printf("Card cannot be unlocked. Remaining attempts: 0\r\n");
        return false;
    }
    printf("Using free security bit: 0x%02X\r\n", security_bit);

    uint32_t ticks[5];
    // update security memory
    sle4442_write(SLE_CMD_WRITE_SECMEM, 0, security_bit);
    ticks[0] = sle4442_ticks();
    // passcode
    sle4442_write(SLE_CMD_COMPARE_VERIFICATION_DATA, 1, (psc >> 16) & 0xff);
    ticks[1] = sle4442_ticks();
    sle4442_write(SLE_CMD_COMPARE_VERIFICATION_DATA, 2, (psc >> 8) & 0xff);
    ticks[2] = sle4442_ticks();
    sle4442_write(SLE_CMD_COMPARE_VERIFICATION_DATA, 3, psc & 0xff);
    ticks[3] = sle4442_ticks();
    // reset passcode attempts
    sle4442_write(SLE_CMD_WRITE_SECMEM, 0, 0xff);
    ticks[4] = sle4442_ticks();
    // printf("DEBUG Ticks: %d %d %d %d %d\r\n", ticks[0], ticks[1], ticks[2], ticks[3], ticks[4]);

    sle4442_read_secmem(data);
    if (data[0] != 7) {
        printf("Failed to unlock card\r\n");
        sle4442_decode_secmem(data);
        return false;
    }
    printf("Card unlocked, security bits reset\r\n");
    sle4442_decode_secmem(data);
    return true;
}

bool sle4442_update_psc(uint32_t new_psc, char* data) {
    // update security memory //TODO: use ticks to determine success or error...
    sle4442_write(SLE_CMD_WRITE_SECMEM, 1, new_psc >> 16);
    uint32_t ticks = sle4442_ticks();
    sle4442_write(SLE_CMD_WRITE_SECMEM, 2, new_psc >> 8);
    ticks = sle4442_ticks();
    sle4442_write(SLE_CMD_WRITE_SECMEM, 3, new_psc);
    ticks = sle4442_ticks();
    // verify security memory
    sle4442_read_secmem(data);
    if (data[1] == (uint8_t)(new_psc >> 16) && data[2] == (uint8_t)(new_psc >> 8) && data[3] == (uint8_t)new_psc) {
        return true;
    } else {
        return false;
    }
}

void sle4442(struct command_result* res) {
    // check help
    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
        return;
    }

    // parse command line
    char action[9];
    cmdln_args_string_by_position(1, sizeof(action), action);

    bool dump = (strcmp(action, "dump") == 0);
    bool unlock = (strcmp(action, "unlock") == 0);
    bool write = (strcmp(action, "write") == 0);
    bool erase = (strcmp(action, "erase") == 0);
    bool protect = (strcmp(action, "protect") == 0);
    bool update_psc = (strcmp(action, "psc") == 0);

    bool glitch = (strcmp(action, "glitch") == 0);

    if (hw2wire_mode_config.baudrate > (51)) {
        printf("Whoa there! %dkHz is probably too fast. Try 50kHz\r\n", hw2wire_mode_config.baudrate);
        return;
    }

    uint8_t data[4];
    if (!sle4442_reset(data)) {
        printf("Card not found\r\n");
        return;
    }

    if (!sle4442_atr_decode(data)) {
        printf("Card not supported\r\n");
        return;
    }

    if (!sle4442_read_secmem(data)) {
        printf("Error reading security memory\r\n");
        return;
    }
    sle4442_decode_secmem(data);

    if (glitch) {
        // unlock card, reset password attempts
        uint32_t psc;
        command_var_t arg;
        if (!cmdln_args_find_flag_uint32('p', &arg, &psc)) {
            printf("Specify a 24 bit PSC with the -p flag (-p 0xffffff)\r\n");
            return;
        }

        if (!sle4442_unlock(psc)) {
            return;
        }
        char c;
        // disable any current system control
        psu_vreg_enable(false); // actually controls the current limit system interaction with PSU
        while (true) {
            // power cycle
            printf("Power cycle\r\n");
            psu_current_limit_override(false);
            busy_wait_ms(1000);
            psu_current_limit_override(true);
            busy_wait_ms(1000);
            printf("Any key for bad attempt, x to exit\r\n");
            while (!rx_fifo_try_get(&c))
                ; // pause for keypress
            if (c == 'x') {
                break;
            }
            // wrong passcode
            sle4442_unlock(psc - 1);
            // pause
            printf("Any key to clear attempts, x to exit\r\n");
            while (!rx_fifo_try_get(&c))
                ; // pause for keypress
            if (c == 'x') {
                break;
            }
            // correct passcode
            if (!sle4442_unlock(psc)) {
                printf("Glitch failed to unlock card\r\n");
                return;
            }
        }
    }

    if (dump) {
        sle4442_read_prtmem(data);
        printf("Protection memory: 0x%02x 0x%02x 0x%02x 0x%02x\r\n", data[0], data[1], data[2], data[3]);

        printf("Memory:\r\n");
        sle4442_write(SLE_CMD_READ_MEM, 0, 0);
        for (uint i = 0; i < 256; i++) {
            uint8_t temp;
            pio_hw2wire_get16(&temp);
            printf("0x%02x ", (uint8_t)ui_format_lsb(temp, 8));
        }
        printf("\r\n");

        //file to read/write/verify
        char file[13];
        command_var_t arg;
        bool file_flag = cmdln_args_find_flag_string('f'|0x20, &arg, sizeof(file), file);

        if(file_flag){
            //new file
            FIL fil;		/* File object needed for each open file */
            FRESULT fr;     /* FatFs return code */
            UINT bw;

            printf("Dumping to %s...\r\n", file);
            //open file
            fr = f_open(&fil, file, FA_WRITE | FA_CREATE_ALWAYS);	
            if (fr != FR_OK) {
                storage_file_error(fr);
                res->error=true;
                return;
            }
            char buf[256+4+4];
            sle4442_write(SLE_CMD_READ_MEM, 0, 0);
            for(uint i =0; i<256; i++){
                uint8_t temp;
                pio_hw2wire_get16(&temp);
                buf[i]=(uint8_t)ui_format_lsb(temp, 8);
            }
            sle4442_read_secmem(&buf[256]);
            sle4442_read_prtmem(&buf[256+4]);
            //write to file
            fr = f_write(&fil, buf, sizeof(buf), &bw);
            if (fr != FR_OK|| bw != sizeof(buf)) {
                storage_file_error(fr);
                res->error=true;
                return;
            }
            //close file
            fr = f_close(&fil);
            if (fr != FR_OK) {
                storage_file_error(fr);
                res->error=true;
                return;
            }
            printf("Dump complete\r\n");
        }
    }

    if (unlock || update_psc) {
        // get cmdln flag -p uint32
        uint32_t psc;
        command_var_t arg;
        if (!cmdln_args_find_flag_uint32('p', &arg, &psc)) {
            printf("Specify a 24 bit PSC with the -p flag (-p 0xffffff)\r\n");
            return;
        }
        printf("Unlocking with PSC: 0x%06X\r\n", psc);
        // if(data[0]==0){ //should still have secmem from init
        // printf("Card cannot be unlocked. Remaining attempts: 0\r\n");
        // return;
        //}

        uint32_t security_bit = 0b100;
        if (data[0] & 0b100) {
            security_bit = 0b011;
        } else if (data[0] & 0b010) {
            security_bit = 0b101;
        } else if (data[0] & 0b001) {
            security_bit = 0b110;
        } else {
            printf("Card cannot be unlocked. Remaining attempts: 0\r\n");
            return;
        }
        printf("Using free security bit: 0x%02X\r\n", security_bit);

        uint32_t ticks[5];
        // update security memory
        sle4442_write(SLE_CMD_WRITE_SECMEM, 0, security_bit);
        ticks[0] = sle4442_ticks();
        // passcode
        sle4442_write(SLE_CMD_COMPARE_VERIFICATION_DATA, 1, (psc >> 16) & 0xff);
        ticks[1] = sle4442_ticks();
        sle4442_write(SLE_CMD_COMPARE_VERIFICATION_DATA, 2, (psc >> 8) & 0xff);
        ticks[2] = sle4442_ticks();
        sle4442_write(SLE_CMD_COMPARE_VERIFICATION_DATA, 3, psc & 0xff);
        ticks[3] = sle4442_ticks();
        // reset passcode attempts
        sle4442_write(SLE_CMD_WRITE_SECMEM, 0, 0xff);
        ticks[4] = sle4442_ticks();
        // printf("DEBUG Ticks: %d %d %d %d %d\r\n", ticks[0], ticks[1], ticks[2], ticks[3], ticks[4]);

        sle4442_read_secmem(data);
        if (data[0] == 7) {
            printf("Card unlocked, security bits reset\r\n");
            sle4442_decode_secmem(data);
        } else {
            printf("Failed to unlock card\r\n");
            sle4442_decode_secmem(data);
            return;
        }
    }

    if (update_psc) {
        uint32_t new_psc;
        command_var_t arg;
        if (!cmdln_args_find_flag_uint32('n', &arg, &new_psc)) {
            printf("Specify a new 24 bit PSC with the -n flag (-n 0xffffff)\r\n");
            return;
        }
        printf("Updating with PSC: 0x%06X\r\n", new_psc);
        // update security memory
        if (!sle4442_update_psc(new_psc, data)) {
            sle4442_decode_secmem(data);
            printf("Failed to update PSC\r\n");
            return;
        }
        sle4442_write(SLE_CMD_WRITE_SECMEM, 0, 0xff);
        sle4442_ticks();
        printf("PSC updated to: 0x%06X\r\n", new_psc);
        sle4442_decode_secmem(data);
    }

    if (erase) {
        printf("Erasing memory\r\n");
        for (uint i = 32; i < (0xff + 1); i++) {
            sle4442_write(SLE_CMD_WRITE_MEM, i, 0xff);
            sle4442_ticks();
        }
        printf("Memory erased\r\n");
    }

    if (write) {
        command_var_t arg;
        uint32_t val = 0, addr = 0;
        if (!cmdln_args_find_flag_uint32('v', &arg, &val)) {
            printf("Specify 8bit write value -v flag (-v 0xffffffff)\r\n");
            return;
        }
        if (!cmdln_args_find_flag_uint32('a', &arg, &addr)) {
            printf("Specify 8bit address -a flag (-a 0x32)\r\n");
            return;
        }
        printf("Writing 0x%02x to 0x%02x\r\n", val, addr);
        sle4442_write(SLE_CMD_WRITE_MEM, addr, val);
        sle4442_ticks();
    }

    if (protect) {
        command_var_t arg;
        uint32_t prtmem = 0;
        if (!cmdln_args_find_flag_uint32('v', &arg, &prtmem)) {
            printf("Specify 32bit protection value -v flag (-v 0xffffffff)\r\n");
            return;
        }
        printf("Writing Protection Memory\r\n");
        sle4442_write(SLE_CMD_WRITE_PRTMEM, 0, prtmem);
        sle4442_ticks();
        sle4442_write(SLE_CMD_WRITE_PRTMEM, 1, prtmem >> 8);
        sle4442_ticks();
        sle4442_write(SLE_CMD_WRITE_PRTMEM, 2, prtmem >> 16);
        sle4442_ticks();
        sle4442_write(SLE_CMD_WRITE_PRTMEM, 3, prtmem >> 24);
        sle4442_ticks();
        sle4442_read_prtmem(data);
        printf("Protection memory: 0x%02x 0x%02x 0x%02x 0x%02x\r\n", data[0], data[1], data[2], data[3]);
    }
}