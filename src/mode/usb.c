#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/spi.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"
#include "pirate/bio.h"
#include "ui/ui_prompt.h"
#include "ui/ui_term.h"
#include "pirate/storage.h"
#include "ui/ui_help.h"
#include "pirate/hwspi.h"
#include "usb_rx.h"
#include "pirate/psu.h"
#include "commands/global/p_pullups.h"
#include "pirate/mem.h"
#include "lib/bp_args/bp_cmd.h"

/*
pico usb sniffer lite is the Dreg's fork from:

https://github.com/ataradov/usb-sniffer-lite

SPDX-License-Identifier: BSD-3-Clause
Copyright (c) 2022, Alex Taradov <alex@taradov.com>. All rights reserved.

https://github.com/therealdreg/pico-usb-sniffer-lite

Copyright (c) [2025] by David Reguera Garcia aka Dreg
https://www.rootkit.es
X @therealdreg
dreg@rootkit.es
---------------------------------------------------------------------------
WARNING: DREG'S BULLSHIT CODE X-)
---------------------------------------------------------------------------
*/

#include <stdio.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/pll.h"
#include "hardware/structs/clocks.h"
#include "hardware/structs/pll.h"
#include "hardware/timer.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico_usb_sniffer_lite.pio.h"
#include "tusb.h"

#define BP() __asm("bkpt #1");

#define FVER 5

#define BIO_TRIGGER_INDEX BIO0
#define BIO_DP_INDEX BIO1
#define BIO_DM_INDEX BIO2
#define BIO_START_INDEX BIO3

#define TRIGGER_INDEX (BIO_TRIGGER_INDEX + 8)
#define DP_INDEX (BIO_DP_INDEX + 8)
#define DM_INDEX (BIO_DM_INDEX + 8)
#define START_INDEX (BIO_START_INDEX + 8)

#define LIMIT(a, b) (((int)(a) > (int)(b)) ? (int)(b) : (int)(a))

#define CAPTURE_ERROR_STUFF (1 << 31)
#define CAPTURE_ERROR_CRC (1 << 30)
#define CAPTURE_ERROR_PID (1 << 29)
#define CAPTURE_ERROR_SYNC (1 << 28)
#define CAPTURE_ERROR_NBIT (1 << 27)
#define CAPTURE_ERROR_SIZE (1 << 26)
#define CAPTURE_RESET (1 << 25)
#define CAPTURE_LS_SOF (1 << 24)
#define CAPTURE_MAY_FOLD (1 << 23)

#define CAPTURE_ERROR_MASK                                                                                             \
    (CAPTURE_ERROR_STUFF | CAPTURE_ERROR_CRC | CAPTURE_ERROR_PID | CAPTURE_ERROR_SYNC | CAPTURE_ERROR_NBIT |           \
     CAPTURE_ERROR_SIZE)

#define CAPTURE_SIZE_MASK 0xffff

#define ERROR_DATA_SIZE_LIMIT 16
#define MAX_PACKET_DELTA 10000

#define BUFFER_SIZE (128 * 1024)

typedef enum
{
    PIN_STATE_LOW = 0,
    PIN_STATE_HIGH,
    PIN_STATE_FLOATING
} pin_state_t;

enum
{
    Pid_Reserved = 0,

    Pid_Out = 1,
    Pid_In = 9,
    Pid_Sof = 5,
    Pid_Setup = 13,

    Pid_Data0 = 3,
    Pid_Data1 = 11,
    Pid_Data2 = 7,
    Pid_MData = 15,

    Pid_Ack = 2,
    Pid_Nak = 10,
    Pid_Stall = 14,
    Pid_Nyet = 6,

    Pid_PreErr = 12,
    Pid_Split = 8,
    Pid_Ping = 4,
};

enum
{
    CaptureSpeed_Low,
    CaptureSpeed_Full,
    CaptureSpeedCount,
};

enum
{
    CaptureTrigger_Enabled,
    CaptureTrigger_Disabled,
    CaptureTriggerCount,
};

enum
{
    CaptureLimit_100,
    CaptureLimit_200,
    CaptureLimit_500,
    CaptureLimit_1000,
    CaptureLimit_2000,
    CaptureLimit_5000,
    CaptureLimitCount,
};

enum
{
    DisplayTime_First,
    DisplayTime_Previous,
    DisplayTime_SOF,
    DisplayTime_Reset,
    DisplayTimeCount,
};

enum
{
    DisplayData_None,
    DisplayData_Limit16,
    DisplayData_Limit64,
    DisplayData_Full,
    DisplayDataCount,
};

enum
{
    DisplayFold_Enabled,
    DisplayFold_Disabled,
    DisplayFoldCount,
};

typedef struct
{
    bool fs;
    bool trigger;
    int limit;
    int count;
    int errors;
    int resets;
    int frames;
    int folded;
} buffer_info_t;

static volatile uint32_t* g_buffer;
static volatile buffer_info_t g_buffer_info;

extern char __flash_binary_end;

static volatile int g_capture_speed = CaptureSpeed_Low;
static volatile int g_capture_trigger = CaptureTrigger_Disabled;
static volatile int g_capture_limit = CaptureLimit_5000;
static volatile int g_display_time = DisplayTime_SOF;
static volatile int g_display_data = DisplayData_Full;
static volatile int g_display_fold = DisplayFold_Enabled;

static volatile int g_rd_ptr = 0;
static volatile int g_wr_ptr = 0;
static volatile int g_sof_index = 0;
static volatile bool g_may_fold = false;

static volatile uint32_t g_ref_time;
static volatile uint32_t g_prev_time;
static volatile bool g_check_delta;
static volatile bool g_folding;
static volatile int g_fold_count;
static volatile int g_display_ptr;

static const uint16_t crc16_usb_tab[256] = {
    0x0000, 0xc0c1, 0xc181, 0x0140, 0xc301, 0x03c0, 0x0280, 0xc241, 0xc601, 0x06c0, 0x0780, 0xc741, 0x0500, 0xc5c1,
    0xc481, 0x0440, 0xcc01, 0x0cc0, 0x0d80, 0xcd41, 0x0f00, 0xcfc1, 0xce81, 0x0e40, 0x0a00, 0xcac1, 0xcb81, 0x0b40,
    0xc901, 0x09c0, 0x0880, 0xc841, 0xd801, 0x18c0, 0x1980, 0xd941, 0x1b00, 0xdbc1, 0xda81, 0x1a40, 0x1e00, 0xdec1,
    0xdf81, 0x1f40, 0xdd01, 0x1dc0, 0x1c80, 0xdc41, 0x1400, 0xd4c1, 0xd581, 0x1540, 0xd701, 0x17c0, 0x1680, 0xd641,
    0xd201, 0x12c0, 0x1380, 0xd341, 0x1100, 0xd1c1, 0xd081, 0x1040, 0xf001, 0x30c0, 0x3180, 0xf141, 0x3300, 0xf3c1,
    0xf281, 0x3240, 0x3600, 0xf6c1, 0xf781, 0x3740, 0xf501, 0x35c0, 0x3480, 0xf441, 0x3c00, 0xfcc1, 0xfd81, 0x3d40,
    0xff01, 0x3fc0, 0x3e80, 0xfe41, 0xfa01, 0x3ac0, 0x3b80, 0xfb41, 0x3900, 0xf9c1, 0xf881, 0x3840, 0x2800, 0xe8c1,
    0xe981, 0x2940, 0xeb01, 0x2bc0, 0x2a80, 0xea41, 0xee01, 0x2ec0, 0x2f80, 0xef41, 0x2d00, 0xedc1, 0xec81, 0x2c40,
    0xe401, 0x24c0, 0x2580, 0xe541, 0x2700, 0xe7c1, 0xe681, 0x2640, 0x2200, 0xe2c1, 0xe381, 0x2340, 0xe101, 0x21c0,
    0x2080, 0xe041, 0xa001, 0x60c0, 0x6180, 0xa141, 0x6300, 0xa3c1, 0xa281, 0x6240, 0x6600, 0xa6c1, 0xa781, 0x6740,
    0xa501, 0x65c0, 0x6480, 0xa441, 0x6c00, 0xacc1, 0xad81, 0x6d40, 0xaf01, 0x6fc0, 0x6e80, 0xae41, 0xaa01, 0x6ac0,
    0x6b80, 0xab41, 0x6900, 0xa9c1, 0xa881, 0x6840, 0x7800, 0xb8c1, 0xb981, 0x7940, 0xbb01, 0x7bc0, 0x7a80, 0xba41,
    0xbe01, 0x7ec0, 0x7f80, 0xbf41, 0x7d00, 0xbdc1, 0xbc81, 0x7c40, 0xb401, 0x74c0, 0x7580, 0xb541, 0x7700, 0xb7c1,
    0xb681, 0x7640, 0x7200, 0xb2c1, 0xb381, 0x7340, 0xb101, 0x71c0, 0x7080, 0xb041, 0x5000, 0x90c1, 0x9181, 0x5140,
    0x9301, 0x53c0, 0x5280, 0x9241, 0x9601, 0x56c0, 0x5780, 0x9741, 0x5500, 0x95c1, 0x9481, 0x5440, 0x9c01, 0x5cc0,
    0x5d80, 0x9d41, 0x5f00, 0x9fc1, 0x9e81, 0x5e40, 0x5a00, 0x9ac1, 0x9b81, 0x5b40, 0x9901, 0x59c0, 0x5880, 0x9841,
    0x8801, 0x48c0, 0x4980, 0x8941, 0x4b00, 0x8bc1, 0x8a81, 0x4a40, 0x4e00, 0x8ec1, 0x8f81, 0x4f40, 0x8d01, 0x4dc0,
    0x4c80, 0x8c41, 0x4400, 0x84c1, 0x8581, 0x4540, 0x8701, 0x47c0, 0x4680, 0x8641, 0x8201, 0x42c0, 0x4380, 0x8341,
    0x4100, 0x81c1, 0x8081, 0x4040,
};

static const uint8_t crc5_usb_tab[256] = {
    0x00, 0x0e, 0x1c, 0x12, 0x11, 0x1f, 0x0d, 0x03, 0x0b, 0x05, 0x17, 0x19, 0x1a, 0x14, 0x06, 0x08, 0x16, 0x18, 0x0a,
    0x04, 0x07, 0x09, 0x1b, 0x15, 0x1d, 0x13, 0x01, 0x0f, 0x0c, 0x02, 0x10, 0x1e, 0x05, 0x0b, 0x19, 0x17, 0x14, 0x1a,
    0x08, 0x06, 0x0e, 0x00, 0x12, 0x1c, 0x1f, 0x11, 0x03, 0x0d, 0x13, 0x1d, 0x0f, 0x01, 0x02, 0x0c, 0x1e, 0x10, 0x18,
    0x16, 0x04, 0x0a, 0x09, 0x07, 0x15, 0x1b, 0x0a, 0x04, 0x16, 0x18, 0x1b, 0x15, 0x07, 0x09, 0x01, 0x0f, 0x1d, 0x13,
    0x10, 0x1e, 0x0c, 0x02, 0x1c, 0x12, 0x00, 0x0e, 0x0d, 0x03, 0x11, 0x1f, 0x17, 0x19, 0x0b, 0x05, 0x06, 0x08, 0x1a,
    0x14, 0x0f, 0x01, 0x13, 0x1d, 0x1e, 0x10, 0x02, 0x0c, 0x04, 0x0a, 0x18, 0x16, 0x15, 0x1b, 0x09, 0x07, 0x19, 0x17,
    0x05, 0x0b, 0x08, 0x06, 0x14, 0x1a, 0x12, 0x1c, 0x0e, 0x00, 0x03, 0x0d, 0x1f, 0x11, 0x14, 0x1a, 0x08, 0x06, 0x05,
    0x0b, 0x19, 0x17, 0x1f, 0x11, 0x03, 0x0d, 0x0e, 0x00, 0x12, 0x1c, 0x02, 0x0c, 0x1e, 0x10, 0x13, 0x1d, 0x0f, 0x01,
    0x09, 0x07, 0x15, 0x1b, 0x18, 0x16, 0x04, 0x0a, 0x11, 0x1f, 0x0d, 0x03, 0x00, 0x0e, 0x1c, 0x12, 0x1a, 0x14, 0x06,
    0x08, 0x0b, 0x05, 0x17, 0x19, 0x07, 0x09, 0x1b, 0x15, 0x16, 0x18, 0x0a, 0x04, 0x0c, 0x02, 0x10, 0x1e, 0x1d, 0x13,
    0x01, 0x0f, 0x1e, 0x10, 0x02, 0x0c, 0x0f, 0x01, 0x13, 0x1d, 0x15, 0x1b, 0x09, 0x07, 0x04, 0x0a, 0x18, 0x16, 0x08,
    0x06, 0x14, 0x1a, 0x19, 0x17, 0x05, 0x0b, 0x03, 0x0d, 0x1f, 0x11, 0x12, 0x1c, 0x0e, 0x00, 0x1b, 0x15, 0x07, 0x09,
    0x0a, 0x04, 0x16, 0x18, 0x10, 0x1e, 0x0c, 0x02, 0x01, 0x0f, 0x1d, 0x13, 0x0d, 0x03, 0x11, 0x1f, 0x1c, 0x12, 0x00,
    0x0e, 0x06, 0x08, 0x1a, 0x14, 0x17, 0x19, 0x0b, 0x05,
};

static const char *capture_speed_str[CaptureSpeedCount] = {
    [CaptureSpeed_Low] = "Low",
    [CaptureSpeed_Full] = "Full",
};

static const char *capture_trigger_str[CaptureTriggerCount] = {
    [CaptureTrigger_Enabled] = "Enabled",
    [CaptureTrigger_Disabled] = "Disabled",
};

static const char *capture_limit_str[CaptureLimitCount] = {
    [CaptureLimit_100] = "100 packets",     [CaptureLimit_200] = "200 packets",
    [CaptureLimit_500] = "500 packets",     [CaptureLimit_1000] = "1000 packets",
    [CaptureLimit_2000] = "2000 packets",   [CaptureLimit_5000] = "5000 packets",
};

static const char *display_time_str[DisplayTimeCount] = {
    [DisplayTime_First] = "Relative to the first packet",
    [DisplayTime_Previous] = "Relative to the previous packet",
    [DisplayTime_SOF] = "Relative to the SOF",
    [DisplayTime_Reset] = "Relative to the bus reset",
};

static const char *display_data_str[DisplayDataCount] = {
    [DisplayData_Full] = "Full",
    [DisplayData_Limit16] = "Limit to 16 bytes",
    [DisplayData_Limit64] = "Limit to 64 bytes",
    [DisplayData_None] = "Do not display data",
};

static const char *display_fold_str[DisplayFoldCount] = {
    [DisplayFold_Enabled] = "Enabled",
    [DisplayFold_Disabled] = "Disabled",
};

static bool wait_for_trigger(void)
{
    if (!g_buffer_info.trigger)
    {
        return true;
    }

    printf("Waiting for a trigger (waiting for a LOW)\r\n");

    while (1)
    {
        if (gpio_get(TRIGGER_INDEX) == 0)
        {
            printf("Triggered!\r\n");
            return true;
        }
    }

    return false;
}

static int capture_limit_value(void)
{
    if (g_capture_limit == CaptureLimit_100)
    {
        return 100;
    }
    else if (g_capture_limit == CaptureLimit_200)
    {
        return 200;
    }
    else if (g_capture_limit == CaptureLimit_500)
    {
        return 500;
    }
    else if (g_capture_limit == CaptureLimit_1000)
    {
        return 1000;
    }
    else if (g_capture_limit == CaptureLimit_2000)
    {
        return 2000;
    }
    return 5000;
}

static void print_g_fold_count(int count)
{
    printf("   ... : Folded ");

    if (count == 1)
    {
        printf("1 frame");
    }
    else
    {
        printf("%d frames", count);
    }

    printf("\r\n");
}

static void print_time(int time)
{
    printf("%d : ", time);
}

static void print_reset(void)
{
    printf("--- RESET ---\r\n");
}

static void print_ls_sof(void)
{
    printf("LS SOF\r\n");
}

static void print_errors(uint32_t flags, uint8_t *data, int size)
{
    flags &= CAPTURE_ERROR_MASK;

    printf("ERROR [");

    while (flags)
    {
        int bit = (flags & ~(flags - 1));

        if (bit == CAPTURE_ERROR_STUFF)
        {
            printf("STUFF");
        }
        else if (bit == CAPTURE_ERROR_CRC)
        {
            printf("CRC");
        }
        else if (bit == CAPTURE_ERROR_PID)
        {
            printf("PID");
        }
        else if (bit == CAPTURE_ERROR_SYNC)
        {
            printf("SYNC");
        }
        else if (bit == CAPTURE_ERROR_NBIT)
        {
            printf("NBIT");
        }
        else if (bit == CAPTURE_ERROR_SIZE)
        {
            printf("SIZE");
        }

        flags &= ~bit;

        if (flags)
        {
            printf(", ");
        }
    }

    printf("]: ");

    if (size > 0)
    {
        printf("SYNC = 0x%02X, ", data[0]);
    }

    if (size > 1)
    {
        printf("PID = 0x%02X, ", data[1]);
    }

    if (size > 2)
    {
        bool limited = false;

        printf("DATA: ");

        if (size > ERROR_DATA_SIZE_LIMIT)
        {
            size = ERROR_DATA_SIZE_LIMIT;
            limited = true;
        }

        for (int i = 2; i < size; i++)
        {
            printf("0x%02X ", data[i]);
        }

        if (limited)
        {
            printf("...");
        }
    }

    printf("\r\n");
}

static void print_sof(uint8_t *data)
{
    int frame = ((data[3] << 8) | data[2]) & 0x7ff;
    printf("SOF #%d\r\n", frame);
}

static void print_handshake(char *pid)
{
    printf("%s\r\n", pid);
}

static void print_in_out_setup(char *pid, uint8_t *data)
{
    int v = (data[3] << 8) | data[2];
    int addr = v & 0x7f;
    int ep = (v >> 7) & 0xf;

    printf("%s: 0x%02X/%X\r\n", pid, addr, ep);
}

static void print_split(uint8_t *data)
{
    int addr = data[2] & 0x7f;
    int sc = (data[2] >> 7) & 1;
    int port = data[3] & 0x7f;
    int s = (data[3] >> 7) & 1;
    int e = data[4] & 1;
    int et = (data[4] >> 1) & 3;

    printf("SPLIT: HubAddr=0x%02X, SC=%X, Port=%02X, S=%X, E=%X, ET=%X\r\n", addr, sc, port, s, e, et);
}

static void print_simple(char *text)
{
    printf("%s\r\n", text);
}

static void print_data(char *pid, uint8_t *data, int size)
{
    size -= 4;

    printf(pid);

    if (size == 0)
    {
        printf(": ZLP\r\n");
    }
    else
    {
        int limited = size;

        if (g_display_data == DisplayData_None)
        {
            limited = 0;
        }
        else if (g_display_data == DisplayData_Limit16)
        {
            limited = LIMIT(size, 16);
        }
        else if (g_display_data == DisplayData_Limit64)
        {
            limited = LIMIT(size, 64);
        }

        printf(" (%d): ", size);

        for (int j = 0; j < limited; j++)
        {
            printf("%02X ", data[j + 2]);
        }

        if (limited < size)
        {
            printf("...");
        }

        printf("\r\n");
    }
}

static bool print_packet(void)
{
    int flags = g_buffer[g_display_ptr];
    int time = g_buffer[g_display_ptr + 1];
    int ftime = time - g_ref_time;
    int delta = time - g_prev_time;
    int size = flags & CAPTURE_SIZE_MASK;
    uint8_t *payload = (uint8_t *)&g_buffer[g_display_ptr + 2];
    int pid = payload[1] & 0x0f;

    if (g_check_delta && delta > MAX_PACKET_DELTA)
    {
        printf("Time delta between packets is too large, possible buffer corruption.\r\n");
        return false;
    }

    g_display_ptr += (((size + 3) / 4) + 2);

    g_prev_time = time;
    g_check_delta = true;

    if (flags & CAPTURE_LS_SOF)
    {
        pid = Pid_Sof;
    }

    if ((g_display_time == DisplayTime_SOF && pid == Pid_Sof) || (g_display_time == DisplayTime_Previous))
    {
        g_ref_time = time;
    }

    if (g_folding)
    {
        if (pid != Pid_Sof)
        {
            return true;
        }

        if (flags & CAPTURE_MAY_FOLD)
        {
            g_fold_count++;
            return true;
        }

        print_g_fold_count(g_fold_count);
        g_folding = false;
    }

    if (flags & CAPTURE_MAY_FOLD && g_display_fold == DisplayFold_Enabled)
    {
        g_folding = true;
        g_fold_count = 1;
        return true;
    }

    print_time(ftime);

    if (flags & CAPTURE_RESET)
    {
        print_reset();

        if (g_display_time == DisplayTime_Reset)
        {
            g_ref_time = time;
        }

        g_check_delta = false;

        return true;
    }

    if (flags & CAPTURE_LS_SOF)
    {
        print_ls_sof();
        return true;
    }

    if (flags & CAPTURE_ERROR_MASK)
    {
        print_errors(flags, payload, size);
        return true;
    }

    if (pid == Pid_Sof)
    {
        print_sof(payload);
    }
    else if (pid == Pid_In)
    {
        print_in_out_setup("IN", payload);
    }
    else if (pid == Pid_Out)
    {
        print_in_out_setup("OUT", payload);
    }
    else if (pid == Pid_Setup)
    {
        print_in_out_setup("SETUP", payload);
    }

    else if (pid == Pid_Ack)
    {
        print_handshake("ACK");
    }
    else if (pid == Pid_Nak)
    {
        print_handshake("NAK");
    }
    else if (pid == Pid_Stall)
    {
        print_handshake("STALL");
    }
    else if (pid == Pid_Nyet)
    {
        print_handshake("NYET");
    }

    else if (pid == Pid_Data0)
    {
        print_data("DATA0", payload, size);
    }
    else if (pid == Pid_Data1)
    {
        print_data("DATA1", payload, size);
    }
    else if (pid == Pid_Data2)
    {
        print_data("DATA2", payload, size);
    }
    else if (pid == Pid_MData)
    {
        print_data("MDATA", payload, size);
    }

    else if (pid == Pid_Ping)
    {
        print_simple("PING");
    }
    else if (pid == Pid_PreErr)
    {
        print_simple("PRE/ERR");
    }
    else if (pid == Pid_Split)
    {
        print_split(payload);
    }
    else if (pid == Pid_Reserved)
    {
        print_simple("RESERVED");
    }

    return true;
}

static void display_buffer(void)
{
    if (g_buffer_info.count == 0)
    {
        printf("\r\nCapture buffer is empty\r\n");
        return;
    }

    printf("\r\nCapture buffer:\r\n");

    g_ref_time = g_buffer[1];
    g_prev_time = g_buffer[1];
    g_folding = false;
    g_check_delta = true;
    g_fold_count = 0;
    g_display_ptr = 0;

    for (int i = 0; i < g_buffer_info.count; i++)
    {
        if (!print_packet())
        {
            break;
        }
    }

    if (g_folding && g_fold_count)
    {
        print_g_fold_count(g_fold_count);
    }

    printf("\r\nTotal: error: %d, bus reset: %d, %s packet: %d, frame: %d, empty frame: %d\r\n\r\n",
           g_buffer_info.errors, g_buffer_info.resets, g_buffer_info.fs ? "FS" : "LS", g_buffer_info.count,
           g_buffer_info.frames, g_buffer_info.folded);
}

static uint16_t crc16_usb(uint8_t *data, int size)
{
    uint16_t crc = 0xffff;

    for (int i = 0; i < size; i++)
    {
        crc = crc16_usb_tab[(crc ^ data[i]) & 0xff] ^ (crc >> 8);
    }

    return crc;
}

static uint8_t crc5_usb(uint8_t *data, int size)
{
    uint8_t crc = 0xff;

    for (int i = 0; i < size; i++)
    {
        crc = crc5_usb_tab[(crc ^ data[i]) & 0xff] ^ (crc >> 8);
    }

    return crc;
}

static void set_error(bool error)
{
    return;
}

static void handle_folding(int pid, uint32_t error)
{
    if (error)
    {
        g_buffer_info.errors++;
        set_error(true);
    }

    if (pid == Pid_Sof)
    {
        g_buffer_info.frames++;

        if (g_may_fold)
        {
            g_buffer[g_sof_index] |= CAPTURE_MAY_FOLD;
            g_buffer_info.folded++;
        }

        g_sof_index = g_wr_ptr - 2;
        g_may_fold = true;
    }
    else if (pid != Pid_In && pid != Pid_Nak)
    {
        g_may_fold = false;
    }

    if (error)
    {
        g_may_fold = false;
    }
}

static void process_packet(int size)
{
    uint8_t *out_data = (uint8_t *)&g_buffer[g_wr_ptr];
    uint32_t v = 0x80000000;
    uint32_t error = 0;
    int out_size = 0;
    int out_bit = 0;
    int out_byte = 0;
    int stuff_count = 0;
    int pid, npid;

    while (size)
    {
        uint32_t w = g_buffer[g_rd_ptr++];
        int bit_count;

        if (size < 31)
        {
            w <<= (30 - size);
            bit_count = size;
        }
        else
        {
            bit_count = 31;
        }

        v ^= (w ^ (w << 1));

        for (int i = 0; i < bit_count; i++)
        {
            int bit = (v & 0x80000000) ? 0 : 1;

            v <<= 1;

            if (stuff_count == 6)
            {
                if (bit)
                {
                    error |= CAPTURE_ERROR_STUFF;
                }

                stuff_count = 0;
                continue;
            }
            else if (bit)
            {
                stuff_count++;
            }
            else
            {
                stuff_count = 0;
            }

            out_byte |= (bit << out_bit);
            out_bit++;

            if (out_bit == 8)
            {
                out_data[out_size++] = out_byte;
                out_byte = 0;
                out_bit = 0;
            }
        }

        size -= bit_count;
    }

    if (out_bit)
    {
        error |= CAPTURE_ERROR_NBIT;
    }

    if (out_size < 1)
    {
        error |= CAPTURE_ERROR_SIZE;
        return;
    }

    if (out_data[0] != (g_buffer_info.fs ? 0x80 : 0x81))
    {
        error |= CAPTURE_ERROR_SYNC;
    }

    if (out_size < 2)
    {
        error |= CAPTURE_ERROR_SIZE;
        return;
    }

    pid = out_data[1] & 0x0f;
    npid = (~out_data[1] >> 4) & 0x0f;

    if ((pid != npid) || (pid == Pid_Reserved))
    {
        error |= CAPTURE_ERROR_PID;
    }

    if (pid == Pid_Sof || pid == Pid_In || pid == Pid_Out || pid == Pid_Setup || pid == Pid_Ping || pid == Pid_Split)
    {
        if (((pid == Pid_Split) && (out_size != 5)) || ((pid != Pid_Split) && (out_size != 4)))
        {
            error |= CAPTURE_ERROR_SIZE;
        }
        else if (crc5_usb(&out_data[2], out_size - 2) != 0x09)
        {
            error |= CAPTURE_ERROR_CRC;
        }
    }
    else if (pid == Pid_Data0 || pid == Pid_Data1 || pid == Pid_Data2 || pid == Pid_MData)
    {
        if (out_size < 4)
        {
            error |= CAPTURE_ERROR_SIZE;
        }
        else if (crc16_usb(&out_data[2], out_size - 2) != 0xb001)
        {
            error |= CAPTURE_ERROR_CRC;
        }
    }

    handle_folding(pid, error);

    g_buffer[g_wr_ptr - 2] = error | out_size;
    g_wr_ptr += (out_size + 3) / 4;
}

static uint32_t start_time(uint32_t end_time, uint32_t size)
{
    if (g_buffer_info.fs)
    {
        return end_time - ((size * 5461) >> 16);
    }
    else
    {
        return end_time - ((size * 43691) >> 16);
    }
}

static void process_buffer(void)
{
    uint32_t time_offset = start_time(g_buffer[1], g_buffer[0]);
    int out_count = 0;

    g_rd_ptr = 0;
    g_wr_ptr = 0;
    g_sof_index = 0;
    g_may_fold = false;

    g_buffer_info.errors = 0;
    g_buffer_info.resets = 0;
    g_buffer_info.frames = 0;
    g_buffer_info.folded = 0;

    for (int i = 0; i < g_buffer_info.count; i++)
    {
        uint32_t size = g_buffer[g_rd_ptr];
        uint32_t time = start_time(g_buffer[g_rd_ptr + 1], size);

        if (size > 0xffff)
        {
            printf("Synchronization error. Check your speed setting.\r\n");
            out_count = 0;
            break;
        }

        g_buffer[g_wr_ptr + 1] = time - time_offset;
        g_rd_ptr += 2;
        g_wr_ptr += 2;
        out_count++;

        if (size == 0)
        {
            g_buffer[g_wr_ptr - 2] = CAPTURE_RESET;
            handle_folding(-1, 0);
            g_buffer_info.resets++;
        }
        else if (size == 1)
        {
            if (g_buffer_info.fs)
            {
                out_count--;
                g_wr_ptr -= 2;
            }
            else
            {
                g_buffer[g_wr_ptr - 2] = CAPTURE_LS_SOF;
                handle_folding(Pid_Sof, 0);
            }

            g_rd_ptr++;
        }
        else
        {
            process_packet(size - 1);

            if (size % 31 <= 1)
            {
                g_rd_ptr++;
            }
        }
    }

    g_buffer_info.count = out_count;
}

static void free_all_pio_state_machines(PIO pio)
{
    for (int sm = 0; sm < 4; sm++)
    {
        if (pio_sm_is_claimed(pio, sm))
        {
            pio_sm_unclaim(pio, sm);
        }
    }
}

static void pio_destroy(void)
{
    free_all_pio_state_machines(pio0);
    free_all_pio_state_machines(pio1);
    pio_clear_instruction_memory(pio0);
    pio_clear_instruction_memory(pio1);
}

static void change_setting(char *name, int *value, int count, const char *str[])
{
    (*value)++;

    if (*value == count)
    {
        *value = 0;
    }

    printf("%s changed to %s\r\n", name, str[*value]);
}

static void print_help(void)
{
    printf("\r\n-------------------------------------------------------------------\r\n"
           "pico-usb-sniffer-lite for Bus Pirate\r\n"
           "https://github.com/therealdreg/pico-usb-sniffer-lite\r\n"
           "BSD-3-Clause Alex Taradov & David Reguera Garcia aka Dreg\r\n"
           "-------------------------------------------------------------------\r\n"
           "Settings:\r\n"
           "  e - Capture speed       : %s\r\n"
           "  g - Capture trigger     : %s\r\n"
           "  l - Capture limit       : %s\r\n"
           "  t - Time display format : %s\r\n"
           "  a - Data display format : %s\r\n"
           "  f - Fold empty frames   : %s\r\n"
           "\r\n"
           "Commands:\r\n"
           "  h - Print this help message\r\n"
           "  b - Display buffer\r\n"
           "  s - Start capture\r\n"
           "  p - Stop capture\r\n"
           "  k - Keylogger low speed infinite ('q' to exit)\r\n"
           "  x - Exit\r\n"
           "\r\n",
           capture_speed_str[g_capture_speed],
           capture_trigger_str[g_capture_trigger],
           capture_limit_str[g_capture_limit],
           display_time_str[g_display_time],
           display_data_str[g_display_data],
           display_fold_str[g_display_fold]);
}

void usb_pio1_irq(void)
{
    bio_put(BIO_START_INDEX, true);
    printf("PIO1 IRQ!\r\n");
    if (pio1_hw->irq & 1)
    {
        printf("    IRQ 1 !\r\n");
        pio1_hw->irq = 1;
    }
    else if (pio1_hw->irq & 2)
    {
        printf("    IRQ 2 !\r\n");
        pio1_hw->irq = 2;
    }
}

static volatile bool keylog_active;
static void keylog_drain_fifo(void);

void usb_pio0_irq(void)
{
    if (keylog_active)
    {
        keylog_drain_fifo();
        return;
    }
    printf("PIO0 IRQ!\r\n");
    if (pio0_hw->irq & 1)
    {
        printf("    IRQ 1 !\r\n");
        pio0_hw->irq = 1;
    }
    else if (pio0_hw->irq & 2)
    {
        printf("    IRQ 1 !\r\n");
        pio0_hw->irq = 2;
    }
}

void cap()
{
    g_buffer_info.fs = (g_capture_speed == CaptureSpeed_Full);
    g_buffer_info.trigger = (g_capture_trigger == CaptureTrigger_Enabled);
    g_buffer_info.limit = capture_limit_value();

    uint16_t tar_pio0_program_instructions_current[32] = {0};
    struct pio_program tar_pio0_program_current = {0};

    memcpy(&tar_pio0_program_current, &tar_pio0_program, sizeof(tar_pio0_program_current));
    tar_pio0_program_current.instructions = tar_pio0_program_instructions_current,
    memcpy(tar_pio0_program_instructions_current, tar_pio0_program_instructions,
            tar_pio0_program.length * sizeof(uint16_t));

    pio_destroy();

    const uint16_t *instructions = g_buffer_info.fs ? usb_full_speed_template_program_instructions
                                                    : usb_low_speed_template_program_instructions;
    pio_add_program(pio0,
                    g_buffer_info.fs ? &usb_full_speed_template_program : &usb_low_speed_template_program);
    tar_pio0_program_instructions_current[1] = instructions[0];
    tar_pio0_program_instructions_current[2] = instructions[1];

    float target_frequency_hz = g_buffer_info.fs ? 120000000.0f : 15000000.0f;
    float sys_clk_hz = (float)clock_get_hz(clk_sys);
    float div = sys_clk_hz / target_frequency_hz;

    uint jmp_pin = g_buffer_info.fs ? DP_INDEX : DM_INDEX;

    pio_destroy();

    bio_input(BIO_DP_INDEX);
    bio_input(BIO_DM_INDEX);
    bio_input(BIO_TRIGGER_INDEX);
    bio_output(BIO_START_INDEX);
    bio_put(BIO_START_INDEX, false);

    pio_gpio_init(pio1, START_INDEX);
    pio_set_irq0_source_mask_enabled(pio1, 0x0F00, true);
    irq_set_exclusive_handler(PIO1_IRQ_0, usb_pio1_irq);
    irq_set_enabled(PIO1_IRQ_0, true);
    int pio1_sm = pio_claim_unused_sm(pio1, true);
    uint offset_pio1 = pio_add_program(pio1, &tar_pio1_program);
    pio_sm_set_consecutive_pindirs(pio1, pio1_sm, START_INDEX, 1, true);
    pio_sm_set_consecutive_pindirs(pio1, pio1_sm, DP_INDEX, 2, false);
    pio_sm_config c_pio1 = tar_pio1_program_get_default_config(offset_pio1);
    sm_config_set_set_pins(&c_pio1, START_INDEX, 1);
    sm_config_set_in_shift(&c_pio1, false, false, 0);
    sm_config_set_out_shift(&c_pio1, false, false, 0);
    sm_config_set_in_pins(&c_pio1, DP_INDEX);
    sm_config_set_clkdiv(&c_pio1, div);
    pio_sm_init(pio1, pio1_sm, offset_pio1, &c_pio1);
    pio_sm_set_enabled(pio1, pio1_sm, false);
    pio_sm_clear_fifos(pio1, pio1_sm);
    pio_sm_restart(pio1, pio1_sm);
    pio_sm_clkdiv_restart(pio1, pio1_sm);

    pio_set_irq0_source_mask_enabled(pio0, 0x0F00, true);
    irq_set_exclusive_handler(PIO0_IRQ_0, usb_pio0_irq);
    irq_set_enabled(PIO0_IRQ_0, true);
    int pio0_sm = pio_claim_unused_sm(pio0, true);
    uint offset_pio0 = pio_add_program(pio0, &tar_pio0_program_current);
    pio_sm_set_consecutive_pindirs(pio0, pio0_sm, DP_INDEX, 3, false);
    pio_sm_config c_pio0 = tar_pio0_program_get_default_config(offset_pio0);
    sm_config_set_in_pins(&c_pio0, DP_INDEX);
    sm_config_set_jmp_pin(&c_pio0, jmp_pin);
    sm_config_set_in_shift(&c_pio0, false, true, 31);
    sm_config_set_out_shift(&c_pio0, false, false, 32);
    sm_config_set_fifo_join(&c_pio0, PIO_FIFO_JOIN_RX);
    sm_config_set_clkdiv(&c_pio0, div);
    pio_sm_init(pio0, pio0_sm, offset_pio0, &c_pio0);
    pio_sm_set_enabled(pio0, pio0_sm, false);
    pio_sm_clear_fifos(pio0, pio0_sm);
    pio_sm_restart(pio0, pio0_sm);
    pio_sm_clkdiv_restart(pio0, pio0_sm);

    int index = 0;
    int packet = 0;

    index = 2;
    packet = 0;
    g_buffer_info.count = 0;
    memset((void *)g_buffer, 0, BUFFER_SIZE);

    set_error(false);

    wait_for_trigger();

    pio_sm_exec(pio0, pio0_sm, pio_encode_jmp(31));
    pio_sm_set_enabled(pio0, pio0_sm, true);
    pio_sm_exec(pio0, pio0_sm, pio_encode_jmp(31));
    pio_sm_clear_fifos(pio0, pio0_sm);
    pio_sm_set_enabled(pio1, pio1_sm, true);

    uint32_t base_time = 0;
    while (1)
    {
        uint32_t v = pio_sm_get_blocking(pio0, pio0_sm);

        if (v & 0x80000000)
        {
            g_buffer[packet + 0] = 0xffffffff - v;
            base_time = base_time ? base_time : timer_hw->timelr;
            g_buffer[packet + 1] = timer_hw->timelr - base_time;
            g_buffer_info.count++;
            packet = index;
            index += 2;

            if (g_buffer_info.count == g_buffer_info.limit)
            {
                break;
            }
        }
        else
        {
            if (index < (BUFFER_SIZE - 4))
            {
                g_buffer[index++] = v;
            }
            else
            {
                break;
            }
        }
    }
    process_buffer();
}

#define KEYLOG_BATCH_PACKETS 256
#define KEYLOG_TOTAL_WORDS (BUFFER_SIZE / (int)sizeof(uint32_t))
#define KEYLOG_HALF_WORDS (KEYLOG_TOTAL_WORDS / 2)
#define KEYLOG_HID_MAX 0x68
#define KEYLOG_TRACKER_SIZE 320

static const char keylog_hid_lower[KEYLOG_HID_MAX] = {
    [0x04] = 'a', [0x05] = 'b', [0x06] = 'c', [0x07] = 'd', [0x08] = 'e', [0x09] = 'f',
    [0x0A] = 'g', [0x0B] = 'h', [0x0C] = 'i', [0x0D] = 'j', [0x0E] = 'k', [0x0F] = 'l',
    [0x10] = 'm', [0x11] = 'n', [0x12] = 'o', [0x13] = 'p', [0x14] = 'q', [0x15] = 'r',
    [0x16] = 's', [0x17] = 't', [0x18] = 'u', [0x19] = 'v', [0x1A] = 'w', [0x1B] = 'x',
    [0x1C] = 'y', [0x1D] = 'z',
    [0x1E] = '1', [0x1F] = '2', [0x20] = '3', [0x21] = '4', [0x22] = '5', [0x23] = '6',
    [0x24] = '7', [0x25] = '8', [0x26] = '9', [0x27] = '0',
    [0x2C] = ' ', [0x2D] = '-', [0x2E] = '=', [0x2F] = '[', [0x30] = ']', [0x31] = '\\',
    [0x32] = '#', [0x33] = ';', [0x34] = '\'', [0x35] = '`', [0x36] = ',', [0x37] = '.',
    [0x38] = '/',
    [0x54] = '/', [0x55] = '*', [0x56] = '-', [0x57] = '+',
    [0x59] = '1', [0x5A] = '2', [0x5B] = '3', [0x5C] = '4', [0x5D] = '5', [0x5E] = '6',
    [0x5F] = '7', [0x60] = '8', [0x61] = '9', [0x62] = '0', [0x63] = '.', [0x64] = '\\',
};

static const char keylog_hid_upper[KEYLOG_HID_MAX] = {
    [0x1E] = '!', [0x1F] = '@', [0x20] = '#', [0x21] = '$', [0x22] = '%', [0x23] = '^',
    [0x24] = '&', [0x25] = '*', [0x26] = '(', [0x27] = ')',
    [0x2D] = '_', [0x2E] = '+', [0x2F] = '{', [0x30] = '}', [0x31] = '|',
    [0x32] = '~', [0x33] = ':', [0x34] = '"', [0x35] = '~', [0x36] = '<', [0x37] = '>',
    [0x38] = '?', [0x64] = '|',
};

static const char *const keylog_hid_names[KEYLOG_HID_MAX] = {
    [0x28] = "<ENTER>", [0x29] = "<ESC>", [0x2A] = "<BKSP>", [0x2B] = "<TAB>",
    [0x39] = "<CAPS>",
    [0x3A] = "<F1>", [0x3B] = "<F2>", [0x3C] = "<F3>", [0x3D] = "<F4>", [0x3E] = "<F5>",
    [0x3F] = "<F6>", [0x40] = "<F7>", [0x41] = "<F8>", [0x42] = "<F9>", [0x43] = "<F10>",
    [0x44] = "<F11>", [0x45] = "<F12>",
    [0x46] = "<PRTSC>", [0x47] = "<SCRLK>", [0x48] = "<PAUSE>", [0x49] = "<INS>",
    [0x4A] = "<HOME>", [0x4B] = "<PGUP>", [0x4C] = "<DEL>", [0x4D] = "<END>",
    [0x4E] = "<PGDN>", [0x4F] = "<RIGHT>", [0x50] = "<LEFT>", [0x51] = "<DOWN>",
    [0x52] = "<UP>", [0x53] = "<NUM>", [0x58] = "<KPENTER>", [0x65] = "<APPS>",
};

static volatile uint32_t *keylog_fill = NULL;
static volatile uint32_t *keylog_half0 = NULL;
static volatile uint32_t *keylog_half1 = NULL;
static volatile int keylog_index = 0;
static volatile int keylog_packet = 0;
static volatile int keylog_count = 0;
static volatile bool keylog_ready = false;
static volatile int keylog_sm = 0;
static volatile uint32_t keylog_base_time = 0;
static volatile unsigned int keylog_dropped = 0;

static uint8_t keylog_prev_report[8];
static unsigned char keylog_tracker[KEYLOG_TRACKER_SIZE];
static unsigned int keylog_tracker_index = 0;
static unsigned int keylog_reports = 0;
static bool keylog_caps = false;

static void keylog_drain_fifo(void)
{
    while (!pio_sm_is_rx_fifo_empty(pio0, keylog_sm))
    {
        uint32_t v = pio_sm_get(pio0, keylog_sm);

        if (keylog_ready)
        {
            keylog_dropped++;
            continue;
        }

        if (v & 0x80000000)
        {
            keylog_fill[keylog_packet + 0] = 0xffffffff - v;
            keylog_base_time = keylog_base_time ? keylog_base_time : timer_hw->timelr;
            keylog_fill[keylog_packet + 1] = timer_hw->timelr - keylog_base_time;
            keylog_count++;
            keylog_packet = keylog_index;
            keylog_index += 2;

            if (keylog_count >= KEYLOG_BATCH_PACKETS)
            {
                keylog_ready = true;
            }
        }
        else if (keylog_index < (KEYLOG_HALF_WORDS - 4))
        {
            keylog_fill[keylog_index++] = v;
        }
        else
        {
            keylog_ready = true;
        }
    }
}

static void keylog_print_tracker(void)
{
    printf("\r\n**********\r\nusb key tracker (limit 0x%02X bytes): %s\r\n**********\r\n",
           (unsigned int)(KEYLOG_TRACKER_SIZE - 1), keylog_tracker);
}

static void keylog_emit(char c)
{
    printf(" --> %c", c);
    keylog_tracker[keylog_tracker_index++ % (KEYLOG_TRACKER_SIZE - 1)] = (unsigned char)c;
    keylog_print_tracker();
}

static void keylog_report(const uint8_t *report)
{
    bool shift = (report[0] & 0x22) != 0;

    printf("[0x%04X] IN: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\r\n", keylog_reports, report[0],
           report[1], report[2], report[3], report[4], report[5], report[6], report[7]);
    keylog_reports++;

    for (int i = 2; i < 8; i++)
    {
        uint8_t k = report[i];

        if (k < 4 || k >= KEYLOG_HID_MAX)
        {
            continue;
        }

        bool held = false;
        for (int j = 2; j < 8; j++)
        {
            if (keylog_prev_report[j] == k)
            {
                held = true;
                break;
            }
        }
        if (held)
        {
            continue;
        }

        if (k == 0x39)
        {
            keylog_caps = !keylog_caps;
            printf("<CAPS>");
            continue;
        }

        char c = keylog_hid_lower[k];

        if (c >= 'a' && c <= 'z')
        {
            if (shift != keylog_caps)
            {
                c = (char)(c - 32);
            }
        }
        else if (shift && keylog_hid_upper[k])
        {
            c = keylog_hid_upper[k];
        }

        if (c)
        {
            keylog_emit(c);
        }
        else if (keylog_hid_names[k])
        {
            printf("%s", keylog_hid_names[k]);
        }
        else
        {
            printf("<%02X>", k);
        }
    }

    printf("-----------\r\n");

    memcpy(keylog_prev_report, report, 8);
}

static void keylog_scan_buffer(void)
{
    int ptr = 0;
    bool last_in = false;

    for (int i = 0; i < g_buffer_info.count; i++)
    {
        uint32_t flags = g_buffer[ptr];
        int size = flags & CAPTURE_SIZE_MASK;
        const uint8_t *payload = (const uint8_t *)&g_buffer[ptr + 2];
        int pid = payload[1] & 0x0f;

        ptr += (((size + 3) / 4) + 2);

        if (flags & (CAPTURE_ERROR_MASK | CAPTURE_RESET | CAPTURE_LS_SOF))
        {
            continue;
        }

        if (pid == Pid_In)
        {
            last_in = true;
        }
        else
        {
            if (last_in && (pid == Pid_Data0 || pid == Pid_Data1) && (size - 4) == 8)
            {
                keylog_report(&payload[2]);
            }
            last_in = false;
        }
    }
}

static int keylog_pio_start(int *out_pio1_sm)
{
    uint16_t tar_pio0_program_instructions_current[32] = {0};
    struct pio_program tar_pio0_program_current = {0};

    memcpy(&tar_pio0_program_current, &tar_pio0_program, sizeof(tar_pio0_program_current));
    tar_pio0_program_current.instructions = tar_pio0_program_instructions_current,
    memcpy(tar_pio0_program_instructions_current, tar_pio0_program_instructions,
           tar_pio0_program.length * sizeof(uint16_t));

    pio_destroy();

    const uint16_t *instructions = usb_low_speed_template_program_instructions;
    pio_add_program(pio0, &usb_low_speed_template_program);
    tar_pio0_program_instructions_current[1] = instructions[0];
    tar_pio0_program_instructions_current[2] = instructions[1];

    float div = (float)clock_get_hz(clk_sys) / 15000000.0f;

    pio_destroy();

    bio_input(BIO_DP_INDEX);
    bio_input(BIO_DM_INDEX);
    bio_input(BIO_TRIGGER_INDEX);
    bio_output(BIO_START_INDEX);
    bio_put(BIO_START_INDEX, false);

    pio_gpio_init(pio1, START_INDEX);
    int pio1_sm = pio_claim_unused_sm(pio1, true);
    uint offset_pio1 = pio_add_program(pio1, &tar_pio1_program);
    pio_sm_set_consecutive_pindirs(pio1, pio1_sm, START_INDEX, 1, true);
    pio_sm_set_consecutive_pindirs(pio1, pio1_sm, DP_INDEX, 2, false);
    pio_sm_config c_pio1 = tar_pio1_program_get_default_config(offset_pio1);
    sm_config_set_set_pins(&c_pio1, START_INDEX, 1);
    sm_config_set_in_shift(&c_pio1, false, false, 0);
    sm_config_set_out_shift(&c_pio1, false, false, 0);
    sm_config_set_in_pins(&c_pio1, DP_INDEX);
    sm_config_set_clkdiv(&c_pio1, div);
    pio_sm_init(pio1, pio1_sm, offset_pio1, &c_pio1);
    pio_sm_set_enabled(pio1, pio1_sm, false);
    pio_sm_clear_fifos(pio1, pio1_sm);
    pio_sm_restart(pio1, pio1_sm);
    pio_sm_clkdiv_restart(pio1, pio1_sm);

    int pio0_sm = pio_claim_unused_sm(pio0, true);
    uint offset_pio0 = pio_add_program(pio0, &tar_pio0_program_current);
    pio_sm_set_consecutive_pindirs(pio0, pio0_sm, DP_INDEX, 3, false);
    pio_sm_config c_pio0 = tar_pio0_program_get_default_config(offset_pio0);
    sm_config_set_in_pins(&c_pio0, DP_INDEX);
    sm_config_set_jmp_pin(&c_pio0, DM_INDEX);
    sm_config_set_in_shift(&c_pio0, false, true, 31);
    sm_config_set_out_shift(&c_pio0, false, false, 32);
    sm_config_set_fifo_join(&c_pio0, PIO_FIFO_JOIN_RX);
    sm_config_set_clkdiv(&c_pio0, div);
    pio_sm_init(pio0, pio0_sm, offset_pio0, &c_pio0);
    pio_sm_set_enabled(pio0, pio0_sm, false);
    pio_sm_clear_fifos(pio0, pio0_sm);
    pio_sm_restart(pio0, pio0_sm);
    pio_sm_clkdiv_restart(pio0, pio0_sm);

    *out_pio1_sm = pio1_sm;
    return pio0_sm;
}

static void keylog_run(void)
{
    volatile uint32_t *g_buffer_base = g_buffer;
    bool saved_fs = g_buffer_info.fs;
    int pio1_sm = 0;

    printf("\r\nUSB low speed keylogger started, press 'q' to exit\r\n"
           "only USB 1.1 low speed boot protocol keyboards are decoded\r\n\r\n");

    keylog_half0 = g_buffer_base;
    keylog_half1 = g_buffer_base + KEYLOG_HALF_WORDS;

    memset(keylog_prev_report, 0, sizeof(keylog_prev_report));
    memset(keylog_tracker, 0, sizeof(keylog_tracker));
    keylog_tracker_index = 0;
    keylog_reports = 0;
    keylog_caps = false;
    keylog_dropped = 0;

    keylog_fill = keylog_half0;
    keylog_index = 2;
    keylog_packet = 0;
    keylog_count = 0;
    keylog_ready = false;
    keylog_base_time = 0;

    g_buffer_info.fs = false;

    int pio0_sm = keylog_pio_start(&pio1_sm);
    keylog_sm = pio0_sm;

    pio_set_irq0_source_mask_enabled(pio0, 0x0F00, false);
    pio_set_irq0_source_mask_enabled(pio1, 0x0F00, false);
    irq_set_enabled(PIO1_IRQ_0, false);
    pio0_hw->irq = 0x0f;
    pio1_hw->irq = 0x0f;

    irq_set_exclusive_handler(PIO0_IRQ_0, usb_pio0_irq);
    irq_set_enabled(PIO0_IRQ_0, true);

    keylog_active = true;

    pio_sm_exec(pio0, pio0_sm, pio_encode_jmp(31));
    pio_sm_set_enabled(pio0, pio0_sm, true);
    pio_sm_exec(pio0, pio0_sm, pio_encode_jmp(31));
    pio_sm_clear_fifos(pio0, pio0_sm);
    pio_set_irq0_source_enabled(pio0, (enum pio_interrupt_source)(pis_sm0_rx_fifo_not_empty + pio0_sm), true);
    pio_sm_set_enabled(pio1, pio1_sm, true);

    char cmd = '\0';
    while (1)
    {
        if (keylog_ready)
        {
            uint32_t irq_state = save_and_disable_interrupts();
            volatile uint32_t *done = keylog_fill;
            int count = keylog_count;

            keylog_fill = (done == keylog_half0) ? keylog_half1 : keylog_half0;
            keylog_index = 2;
            keylog_packet = 0;
            keylog_count = 0;
            keylog_base_time = 0;
            keylog_ready = false;
            restore_interrupts(irq_state);

            if (count > 0)
            {
                g_buffer = done;
                g_buffer_info.count = count;
                process_buffer();
                keylog_scan_buffer();
            }
        }

        if (rx_fifo_try_get(&cmd))
        {
            if (cmd == 'q' || cmd == 'Q')
            {
                break;
            }
        }
    }

    keylog_active = false;
    pio_set_irq0_source_enabled(pio0, (enum pio_interrupt_source)(pis_sm0_rx_fifo_not_empty + pio0_sm), false);
    irq_set_enabled(PIO0_IRQ_0, false);
    pio_sm_set_enabled(pio0, pio0_sm, false);
    pio_sm_set_enabled(pio1, pio1_sm, false);
    pio_destroy();

    g_buffer = g_buffer_base;
    g_buffer_info.count = 0;
    g_buffer_info.fs = saved_fs;

    printf("\r\n\r\nkeys captured: %s\r\n", keylog_tracker);
    if (keylog_dropped)
    {
        printf("words dropped while decoding: %u\r\n", keylog_dropped);
    }
    printf("keylogger stopped\r\n");
}

void usb_handler(struct command_result* res) {
    if (NULL == g_buffer)
    {
        printf("Buffer not allocated\r\n");
        return;
    }

    bool success = set_sys_clock_khz(120000, true);
    stdio_init_all();
    sleep_ms(500);

    uint32_t result = psu_enable(3.33f, 400.0f, false, 100);

    if (result) {
            printf("\r\nPSU ERROR CODE %d\r\n", result);
    } else {
            printf("\r\nPSU 3V3 Enabled\r\n");
        }

    if (success)
    {
        printf("Clock successfully set to 120 MHz\r\n");
    }
    else
    {
        printf("Failed to set the clock\r\n");
    }

    print_help();

    char cmd = '\0';
    do
    {

        while (rx_fifo_try_get(&cmd))
        {
            busy_wait_ms(10);
        }
        printf("\r\nCommand: ");
        cmd = '\0';
        while (!rx_fifo_try_get(&cmd))
        {
            busy_wait_ms(10);
        }

        printf(" %c\r\n", cmd);
        if (cmd >= 'A' && cmd <= 'Z')
        {
            cmd += 0x20;
        }

        if (cmd == 's')
        {
            printf("capture start!\r\n");
            cap();
            printf("capture end!, press 'b' to show the buffer\r\n");
        }
        else if (cmd == 'p')
        {
            printf("capture stop is not implemented yet!\r\n");
        }
        else if (cmd == 'b')
        {
            display_buffer();
        }
        else if (cmd == 'k')
        {
            keylog_run();
        }
        else if (cmd == 'h' || cmd == '?')
        {
            print_help();
        }
        else if (cmd == 'e')
        {
            change_setting("Capture speed", (int *)&g_capture_speed, CaptureSpeedCount, capture_speed_str);
        }
        else if (cmd == 'g')
        {
            change_setting("Capture trigger", (int *)&g_capture_trigger, CaptureTriggerCount, capture_trigger_str);
        }
        else if (cmd == 'l')
        {
            change_setting("Capture limit", (int *)&g_capture_limit, CaptureLimitCount, capture_limit_str);
        }
        else if (cmd == 't')
        {
            change_setting("Time display format", (int *)&g_display_time, DisplayTimeCount, display_time_str);
        }
        else if (cmd == 'a')
        {
            change_setting("Data display format", (int *)&g_display_data, DisplayDataCount, display_data_str);
        }
        else if (cmd == 'f')
        {
            change_setting("Fold empty frames", (int *)&g_display_fold, DisplayFoldCount, display_fold_str);
        }
        else if (cmd == 'x')
        {
            return;
        }
        else
        {
            print_help();
        }
    } while (1);
}

const bp_command_def_t usb_sniff_def = {
    .name        = "sniff",
    .description = T_USB_DESCRIPTION,
};

const struct _mode_command_struct usb_commands[] = {
    {   .func=&usb_handler,
        .def=&usb_sniff_def,
        .supress_fala_capture=true
    },

};
const uint32_t usb_commands_count = count_of(usb_commands);

static const char pin_labels[][5] = { "TRIG", "D+", "D-", "RESV", "DAT", "CLK", "RESV", "RESV" };

uint32_t usb_setup(void) {

    return 1;
}

uint32_t usb_setup_exc(void) {
    bio_init();
    system_bio_update_purpose_and_label(true, BIO0, BP_PIN_MODE, pin_labels[0]);
    system_bio_update_purpose_and_label(true, BIO1, BP_PIN_MODE, pin_labels[1]);
    system_bio_update_purpose_and_label(true, BIO2, BP_PIN_MODE, pin_labels[2]);
    system_bio_update_purpose_and_label(true, BIO3, BP_PIN_MODE, pin_labels[3]);
    system_bio_update_purpose_and_label(true, BIO4, BP_PIN_MODE, pin_labels[4]);
    system_bio_update_purpose_and_label(true, BIO5, BP_PIN_MODE, pin_labels[5]);
    system_bio_update_purpose_and_label(true, BIO6, BP_PIN_MODE, pin_labels[6]);
    system_bio_update_purpose_and_label(true, BIO7, BP_PIN_MODE, pin_labels[7]);

    if (NULL == g_buffer)
    {
        g_buffer = (volatile uint32_t *) mem_alloc(BUFFER_SIZE, 0);
    }

    return 10000000;
}

void usb_cleanup(void) {
    if (g_buffer)
    {
        mem_free((uint8_t*)g_buffer);
        g_buffer = NULL;
    }
    system_bio_update_purpose_and_label(false, BIO0, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO1, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO2, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO3, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_SPI_CLK, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_SPI_CDO, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_SPI_CDI, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_SPI_CS, BP_PIN_MODE, 0);
    bio_init();

    psu_disable();

    pio_destroy();
}

bool usb_preflight_sanity_check(void){
    ui_help_sanity_check(true, 0x00);
    return true;
}

void usb_pins(void){
    printf("TRST\tSRST\tCS\tMISO\tCLK\tMOSI");
}

void usb_settings(void) {
}

void usb_help(void) {
    printf("usb mode\r\n");
    ui_help_mode_commands(usb_commands, usb_commands_count);
}

uint32_t usb_get_speed(void) {
    return 10000000;
}
