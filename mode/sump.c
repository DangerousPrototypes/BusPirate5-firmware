/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Jaroslav Kysela <perex@perex.cz>
 * Modified 2024 by Ian Lesnet for Bus Pirate 5
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 *
 * Protocol link: https://www.sump.org/projects/analyzer/protocol
 *
 */
#include <pico/stdlib.h>
#include <string.h>
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/structs/bus_ctrl.h"
#include "sump.h"

#include "pirate.h"
#include "queue.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "pirate/bio.h"
#include "pirate/button.h"
#include "system_config.h"
#include "bytecode.h" //needed because modes.h has some functions that use it TODO: move all the opt args and bytecode stuff to a single helper file
#include "opt_args.h" //needed for same reason as bytecode and needs same fix
#include "modes.h"
#include "mode/binio.h"
#include "pirate/psu.h"
#include "binio_helpers.h"
#include "mode/logicanalyzer.h"

#include "tusb.h"

#define CDC_INTF		1

#define SAMPLING_DIVIDER	2	// minimal sysclk sampling divider. For Bus Pirate with PIO max speed is /2
#define SAMPLING_BITS		8
#define SAMPLING_BYTES		((SAMPLING_BITS+7)/8)
#define SUMP_MEMORY_SIZE	32768 * 4	// 100kB

#define SUMP_STATE_CONFIG	0
#define SUMP_STATE_INIT		1
#define SUMP_STATE_TRIGGER	2
#define SUMP_STATE_SAMPLING	3
#define SUMP_STATE_DUMP		4
#define SUMP_STATE_ERROR	5

#define ONE_MHZ			1000000u

struct _trigger {
    uint32_t mask;
    uint32_t value;
    uint16_t delay;
    uint8_t  channel;
    uint8_t  level;
    bool     serial;
    bool     start;
};

static struct _sump {

    /* internal states */
    bool     cdc_connected;
    uint8_t  cmd[5];		// command
    uint8_t  cmd_pos;		// command buffer position
    uint8_t  state;		// SUMP_STATE_*
    uint8_t  width;		// in bytes, 1 = 8 bits, 2 = 16 bits
    uint8_t  trigger_index;
    uint32_t pio_prog_offset;
    uint32_t read_start;
    uint64_t timestamp_start;

    /* protocol config */
    uint32_t divider;		// clock divider
    uint32_t read_count;
    uint32_t delay_count;
    uint32_t flags;
    struct _trigger trigger[4];

    /* DMA buffer */
    /*uint32_t chunk_size;	// in bytes
    uint32_t dma_start;
    uint32_t dma_count;
    uint32_t dma_curr_idx;	// current DMA channel (index)
    uint32_t dma_pos;
    uint32_t next_count;*/
    //uint8_t  buffer[SUMP_MEMORY_SIZE];

} sump;

static uint8_t * sump_add_metas(uint8_t * buf, uint8_t tag, const char *str)
{
    *buf++ = tag;
    while (*str)
	*buf++ = (uint8_t)(*str++);
    *buf++ = '\0';
    return buf;
}

static uint8_t * sump_add_meta1(uint8_t * buf, uint8_t tag, uint8_t val)
{
    buf[0] = tag;
    buf[1] = val;
    return buf + 2;
}

static uint8_t * sump_add_meta4(uint8_t * buf, uint8_t tag, uint32_t val)
{
    buf[0] = tag;
    // this is a bit weird, but libsigrok decodes Big-Endian words here
    // the commands use Little-Endian
    buf[1] = val >> 24;
    buf[2] = val >> 16;
    buf[3] = val >> 8;
    buf[4] = val;
    return buf + 5;
}

static void sump_do_meta(void)
{
    char cpu[32];
    uint8_t buf[128], *ptr = buf, *wptr = buf;
    uint32_t sysclk;

    sysclk = clock_get_hz(clk_sys) / SAMPLING_DIVIDER;
    sprintf(cpu, "RP2040 %uMhz", sysclk / ONE_MHZ);
    ptr = sump_add_metas(ptr, SUMP_META_NAME, "Bus Pirate 5");
    ptr = sump_add_metas(ptr, SUMP_META_FPGA_VERSION, "PIO+DMA!");
    ptr = sump_add_metas(ptr, SUMP_META_CPU_VERSION, cpu);
    ptr = sump_add_meta4(ptr, SUMP_META_SAMPLE_RATE, sysclk);
    ptr = sump_add_meta4(ptr, SUMP_META_SAMPLE_RAM, SUMP_MEMORY_SIZE);
    ptr = sump_add_meta1(ptr, SUMP_META_PROBES_B, SAMPLING_BITS);
    ptr = sump_add_meta1(ptr, SUMP_META_PROTOCOL_B, 2);
    *ptr++ = SUMP_META_END;
    while (wptr != ptr)
        wptr += tud_cdc_n_write(CDC_INTF, wptr, ptr - wptr);
    tud_cdc_n_write_flush(CDC_INTF);
}

static void sump_do_id(void)
{
    tud_cdc_n_write_str(CDC_INTF, "1ALS");
    tud_cdc_n_write_flush(CDC_INTF);
}
/*
static uint32_t sump_calc_sysclk_divider()
{
    uint32_t divider = sump.divider, v;
    const uint32_t common_divisor = 4;

    if (divider > 65535)
        divider = 65535;
    // return the fractional part in lowest byte (8 bits)
    if (sump.flags & SUMP_FLAG1_DDR) {
        // 125Mhz support
        divider *= 128 / common_divisor;
    } else {
        divider *= 256 / common_divisor;
    }
    v = clock_get_hz(clk_sys);
    assert((v % ONE_MHZ) == 0);
    // conversion from 100Mhz to sysclk
    v = ((v / ONE_MHZ) * divider) / ((100 / common_divisor) * SAMPLING_DIVIDER);
    v *= sump.width;
    if (v > 65535 * 256)
        v = 65535 * 256;
    else if (v <= 255)
        v = 256;
    //printf("%s(): %u %u -> %u (%.4f)\n", __func__, clock_get_hz(clk_sys), sump.divider, v, (float)v / 256.0);
    return v;
}*/


static void sump_do_run(void)
{
    uint8_t state;
    uint32_t i, tmask = 0;
    bool tstart = false;

    if (sump.width == 0) 
    {
        // invalid config, dump something nice
        sump.state = SUMP_STATE_DUMP;
	    return;
    }

    for (i = 0; i < count_of(sump.trigger); i++) 
    {
        tstart |= sump.trigger[i].start; //is one group of trigger channels enabled
        tmask |= sump.trigger[i].mask; //is a value actually masked?
    }

    float freq = (100 * ONE_MHZ)/(sump.divider); //already added +1 when we rx the value...

    if (tstart && tmask) 
    {
	    sump.state = SUMP_STATE_TRIGGER;
	    //sump.trigger_index = 0;    
    } 
    else 
    {
        sump.state = SUMP_STATE_SAMPLING;
    }

    logic_analyzer_arm(freq, sump.delay_count, sump.trigger[0].mask, sump.trigger[0].value);   

    return;
}

static void sump_do_finish(void)
{
    if (sump.state == SUMP_STATE_TRIGGER || sump.state == SUMP_STATE_SAMPLING) {
        sump.state = SUMP_STATE_DUMP;
        //sump_dma_done();
        return;
    }
}

static void sump_do_stop(void)
{
    uint32_t i;

    if (sump.state == SUMP_STATE_INIT)
        return;
    // protocol state
    sump.state = SUMP_STATE_INIT;
    logicanalyzer_reset_led();
}

static void sump_do_reset(void)
{
    uint32_t i;

    sump_do_stop();
    memset(&sump.trigger, 0, sizeof(sump.trigger));
}

static void sump_set_flags(uint32_t flags)
{
    uint8_t width;

    sump.flags = flags;
    width = 2;
    if (flags & SUMP_FLAG1_GR0_DISABLE)
	width--;
    if (flags & SUMP_FLAG1_GR1_DISABLE)
	width--;
    // we don't support 24-bit or 32-bit capture - sorry
    if ((flags & SUMP_FLAG1_GR2_DISABLE) == 0)
	width = 0;
    if ((flags & SUMP_FLAG1_GR3_DISABLE) == 0)
	width = 0;
    //printf("%s(): sample %u bytes\n", __func__, width);
    sump.width = width;
}

static void sump_update_counts(uint32_t val)
{
    /*
     * This just sets up how many samples there should be before
     * and after the trigger fires. The read_count is total samples
     * to return and delay_count number of samples after
     * the trigger.
     *
     * This sets the buffer splits like 0/100, 25/75, 50/50
     * for example if read_count == delay_count then we should
     * return all samples starting from the trigger point.
     * If delay_count < read_count we return
     * (read_count - delay_count) of samples from before
     * the trigger fired.
     */
    uint32_t read_count = ((val & 0xffff) + 1) * 4;
    uint32_t delay_count = ((val >> 16) + 1) * 4;
    if (delay_count > read_count)
        read_count = delay_count;
    sump.read_count = read_count;
    sump.delay_count = delay_count;
}

static void sump_set_trigger_mask(uint trig, uint32_t val)
{
    struct _trigger *t = &sump.trigger[trig];
    t->mask = val;
    //printf("%s(): idx=%u val=0x%08x\n", __func__, trig, val);
}

static void sump_set_trigger_value(uint trig, uint32_t val)
{
    struct _trigger *t = &sump.trigger[trig];
    t->value = val;
    //printf("%s(): idx=%u val=0x%08x\n", __func__, trig, val);
}

static void sump_set_trigger_config(uint trig, uint32_t val)
{
    struct _trigger *t = &sump.trigger[trig];
    t->start = (val & 0x08000000) != 0;
    t->serial = (val & 0x02000000) != 0;
    t->channel = ((val >> 20) & 0x0f) | ((val >> (24 - 4)) & 0x10);
    t->level = (val >> 16) & 3;
    t->delay = val & 0xffff;
    //printf("%s(): idx=%u val=0x%08x (start=%u serial=%u channel=%u level=%u delay=%u)\n",__func__, trig, val, t->start, t->serial, t->channel, t->level, t->delay);
}

static void sump_rx_short(uint8_t cmd)
{
    //printf("%s(): 0x%02x\n", __func__, cmd);
    switch (cmd) {
    case SUMP_CMD_RESET:
	sump_do_reset();
	break;
    case SUMP_CMD_ARM:
	sump_do_run();
	break;
    case SUMP_CMD_ID:
	sump_do_id();
	break;
    case SUMP_CMD_META:
	sump_do_meta();
	break;
    case SUMP_CMD_FINISH:
	sump_do_finish();
	break;
    case SUMP_CMD_QUERY_INPUT:
	break;
    case SUMP_CMD_ADVANCED_ARM:
	sump_do_run();
	break;
    default:
	break;
    }
}

static void sump_rx_long(uint8_t * cmd)
{
    uint32_t val;

    val = cmd[1] | (cmd[2] << 8) | (cmd[3] << 16) | (cmd[4] << 24);
    //printf("%s(): [0x%02x] 0x%08x\n", __func__, cmd[0], val);
    switch (cmd[0]) {
    case SUMP_CMD_SET_SAMPLE_RATE:
	sump_do_stop();
	sump.divider = val + 1;
	break;
    case SUMP_CMD_SET_COUNTS:
	sump_do_stop();  

	sump_update_counts(val);
	break;
    case SUMP_CMD_SET_FLAGS:
	sump_do_stop();
	sump_set_flags(val);
	break;
    case SUMP_CMD_SET_ADV_TRG_SELECT:
    case SUMP_CMD_SET_ADV_TRG_DATA:
	break;			/* not implemented */

    case SUMP_CMD_SET_BTRG0_MASK:
    case SUMP_CMD_SET_BTRG1_MASK:
    case SUMP_CMD_SET_BTRG2_MASK:
    case SUMP_CMD_SET_BTRG3_MASK:
	sump_set_trigger_mask((cmd[0] - SUMP_CMD_SET_BTRG0_MASK) / 3, val);
	break;

    case SUMP_CMD_SET_BTRG0_VALUE:
    case SUMP_CMD_SET_BTRG1_VALUE:
    case SUMP_CMD_SET_BTRG2_VALUE:
    case SUMP_CMD_SET_BTRG3_VALUE:
	sump_set_trigger_value((cmd[0] - SUMP_CMD_SET_BTRG0_VALUE) / 3, val);
	break;

    case SUMP_CMD_SET_BTRG0_CONFIG:
    case SUMP_CMD_SET_BTRG1_CONFIG:
    case SUMP_CMD_SET_BTRG2_CONFIG:
    case SUMP_CMD_SET_BTRG3_CONFIG:
	sump_set_trigger_config((cmd[0] - SUMP_CMD_SET_BTRG0_CONFIG) / 3, val);
	break;
    default:
	return;
    }
}

void sump_rx(uint8_t *buf, uint count)
{
    if (count == 0)	return;

    while (count-- > 0) 
    {
        sump.cmd[sump.cmd_pos++] = *buf++;
        if (SUMP_CMD_IS_SHORT(sump.cmd[0])) {
            sump_rx_short(sump.cmd[0]);
            sump.cmd_pos = 0;
        } else if (sump.cmd_pos >= 5) {
            sump_rx_long(sump.cmd);
            sump.cmd_pos = 0;
        }
    }
}

static uint sump_tx_empty(uint8_t *buf, uint len)
{
    uint32_t i, count;
    uint8_t a, b;

    count = sump.read_count;
    //printf("%s: count=%u\n", __func__, count);
    a = 0x55;
    if (sump.width == 1) {
        for (i = 0; i < len && count > 0; count--, i++) {
            *buf++ = a;
            a ^= 0xff;
        }
        sump.read_count -= i;
    } else {
        return 0;
    }
   ////printf("%s: ret=%u\n", __func__, i);
    return i;
}

static uint sump_tx8(uint8_t *buf, uint len)
{
    uint32_t i, count;
    uint8_t *ptr;

    count = sump.read_count;
    //printf("%s: count=%u, start=%u\n", __func__, count);
    //ptr = sump.buffer + (sump.read_start + count) % SUMP_MEMORY_SIZE;
    for (i = 0; i < len && count > 0; i++, count--) {
        //if (ptr == sump.buffer)
            //ptr = sump.buffer + SUMP_MEMORY_SIZE;
        //*buf++ = *(--ptr);
        logicanalyzer_dump(&buf[i]);
    }
    sump.read_count -= i;
    //printf("%s: ret=%u\n", __func__, i);
    return i;
}


static uint sump_fill_tx(uint8_t *buf, uint len)
{
    uint ret;

    assert((len & 3) == 0);
    if (sump.read_count == 0) {
        sump.state = SUMP_STATE_CONFIG;
        //rgb_irq_enable(true);
        logicanalyzer_reset_led();
        return 0;
    }
    if (sump.state == SUMP_STATE_DUMP) {
        if (sump.width == 1) {
            ret = sump_tx8(buf, len);
        } else {
            // invalid
            ret = sump_tx_empty(buf, len);
        }
    } else {
        // invalid or error
        ret = sump_tx_empty(buf, len);
    }
    if (ret == 0){
        sump.state = SUMP_STATE_CONFIG;
        //rgb_irq_enable(true);
        logicanalyzer_reset_led();
    }
    return ret;
}

static void cdc_sump_init_connect(void)
{
    uint32_t pio_off;

    pio_off = sump.pio_prog_offset;
    memset(&sump, 0, sizeof(sump));
    sump.pio_prog_offset = pio_off;
    sump.width = 1;
    sump.divider = 1000;		// a safe value
    sump.read_count = 256;
    sump.delay_count = 256;

    ////printf("%s(): memory buffer %u bytes\n", __func__, SUMP_MEMORY_SIZE);
}

void cdc_sump_init(void)
{
    if(!logicanalyzer_setup())
        printf("Error with setup");
}

#define MAX_UART_PKT 64
void cdc_sump_task(void)
{
    uint8_t buf[MAX_UART_PKT];

    //if (tud_cdc_n_connected(CDC_INTF)) {
        //if (!sump.cdc_connected) {
            //cdc_sump_init_connect();
        //    sump.cdc_connected = true;
        //}
        if (sump.state == SUMP_STATE_DUMP || sump.state == SUMP_STATE_ERROR) {
            if (tud_cdc_n_write_available(CDC_INTF) >= sizeof(buf)) {
                uint tx_len = sump_fill_tx(buf, sizeof(buf));
                tud_cdc_n_write(CDC_INTF, buf, tx_len);
                tud_cdc_n_write_flush(CDC_INTF);
            }
            //tud_cdc_n_write_flush(CDC_INTF);
        }
		if (tud_cdc_n_available(CDC_INTF)) {
			uint cmd_len = tud_cdc_n_read(CDC_INTF, buf, sizeof(buf));
			sump_rx(buf, cmd_len);
		}
		if (sump.state == SUMP_STATE_TRIGGER || sump.state == SUMP_STATE_SAMPLING){
            if(logic_analyzer_is_done()) //get status from logic analyzer, move to cancel or dump
            {
                //rgb_set_all(0xff,0,0xff);
                sump.state=SUMP_STATE_DUMP;
            }
        } //else if (!sump.cdc_connected) { 
          //  sump.cdc_connected = false;
          //  sump_do_reset();
        //}  
    //}
}

void sump_logic_analyzer(void){
#if TURBO_200MHZ
    set_sys_clock_khz(200000, true);
#endif

    cdc_sump_init();
    cdc_sump_init_connect();
    psu_enable(3.3,100, true);

    while (1) {
        //tud_task(); // tinyusb device task
        cdc_sump_task();
        // exit out on button press
        if(button_get(0)) break;
    }
    logic_analyzer_cleanup();
    psu_disable();
}
