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
#include "commands/global/w_psu.h"
#include "commands/global/p_pullups.h"
#include "pirate/mem.h"
#include "lib/bp_args/bp_cmd.h"

/*
pico usb sniffer lite is the Dreg's fork from:

https://github.com/ataradov/usb-sniffer-lite

SPDX-License-Identifier: BSD-3-Clause
Copyright (c) 2022, Alex Taradov <alex@taradov.com>. All rights reserved.

https://github.com/therealdreg/pico-usb-sniffer-lite

The keylogger's report handling comes from okhi, the Open Keylogger Hardware Implant:
https://github.com/therealdreg/okhi

Copyright (c) [2025] by David Reguera Garcia aka Dreg
https://www.rootkit.es
X @therealdreg
dreg@rootkit.es
---------------------------------------------------------------------------
WARNING: DREG'S BULLSHIT CODE X-)
---------------------------------------------------------------------------
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/structs/clocks.h"
#include "hardware/timer.h"
#include "pico/stdlib.h"
#include "pico_usb_sniffer_lite.pio.h"

#define BIO_TRIGGER_INDEX BIO0
#define BIO_DP_INDEX BIO1
#define BIO_DM_INDEX BIO2
#define BIO_START_INDEX BIO3

// The sampler reads D+, D- and START as consecutive pins from one IN base (PIN 0, 1 and 2).
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
// g_buffer is indexed in words: every bound below is in words, never in bytes
#define BUFFER_WORDS (BUFFER_SIZE / (int)sizeof(uint32_t))

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

// True when 'p' (stop capture) is waiting in the terminal.
static bool stop_requested(void)
{
    char c;

    while (rx_fifo_try_get(&c))
    {
        if (c == 'p' || c == 'P')
        {
            return true;
        }
    }

    return false;
}

static bool wait_for_trigger(void)
{
    if (!g_buffer_info.trigger)
    {
        return true;
    }

    printf("Waiting for a trigger (waiting for a LOW), 'p' to stop\r\n");

    while (gpio_get(TRIGGER_INDEX))
    {
        if (stop_requested())
        {
            printf("capture stopped\r\n");
            return false;
        }
    }

    printf("Triggered!\r\n");
    return true;
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

    printf("%s", pid);

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

    // The bus goes quiet while nothing is attached, during a detach or while the host suspends it,
    // and the timestamps are real, so a long gap is the bus, not a broken buffer: show it and go on.
    // It used to end the display there, right before the enumeration of a keyboard plugged in.
    if (g_check_delta && delta > MAX_PACKET_DELTA)
    {
        printf("   ... : %d us without traffic\r\n", delta);
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

static uint16_t crc16_usb(const uint8_t *data, int size)
{
    uint16_t crc = 0xffff;

    for (int i = 0; i < size; i++)
    {
        crc = crc16_usb_tab[(crc ^ data[i]) & 0xff] ^ (crc >> 8);
    }

    return crc;
}

static uint8_t crc5_usb(const uint8_t *data, int size)
{
    uint8_t crc = 0xff;

    for (int i = 0; i < size; i++)
    {
        crc = crc5_usb_tab[(crc ^ data[i]) & 0xff] ^ (crc >> 8);
    }

    return crc;
}

static void handle_folding(int pid, uint32_t error)
{
    if (error)
    {
        g_buffer_info.errors++;
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

// NRZI-decode and unstuff nbits line samples into bytes. The sampler packs 31 samples to a word, the
// first one in bit 30; at most in_words words are read and out_max bytes written. Returns the
// CAPTURE_ERROR_* flags with the byte count in the low 16 bits. The output may overlap the input
// as long as it stays behind it, which process_packet() relies on.
static uint32_t decode_samples(const volatile uint32_t *in, int in_words, int nbits, uint8_t *out, int out_max)
{
    uint32_t v = 0x80000000;
    uint32_t error = 0;
    int out_size = 0;
    int out_bit = 0;
    int out_byte = 0;
    int stuff_count = 0;
    int n = 0;

    while (nbits)
    {
        // A sample count that asks for more words than the packet has (a corrupt size word) stops
        // here, flagged, instead of reading into the next packet or past the buffer.
        if (n >= in_words)
        {
            error |= CAPTURE_ERROR_SIZE;
            break;
        }

        uint32_t w = in[n++];
        int bit_count;

        if (nbits < 31)
        {
            w <<= (30 - nbits);
            bit_count = nbits;
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
                if (out_size < out_max)
                {
                    out[out_size++] = out_byte;
                }
                else
                {
                    error |= CAPTURE_ERROR_SIZE;
                }

                out_byte = 0;
                out_bit = 0;
            }
        }

        nbits -= bit_count;
    }

    if (out_bit)
    {
        error |= CAPTURE_ERROR_NBIT;
    }

    return error | (uint32_t)out_size;
}

// SYNC, PID and CRC checks of a decoded packet, as CAPTURE_ERROR_* flags.
static uint32_t check_packet(const uint8_t *out, int out_size, bool fs)
{
    uint32_t error = 0;

    if (out_size < 1)
    {
        return CAPTURE_ERROR_SIZE;
    }

    if (out[0] != (fs ? 0x80 : 0x81))
    {
        error |= CAPTURE_ERROR_SYNC;
    }

    if (out_size < 2)
    {
        return error | CAPTURE_ERROR_SIZE;
    }

    int pid = out[1] & 0x0f;
    int npid = (~out[1] >> 4) & 0x0f;

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
        else if (crc5_usb(&out[2], out_size - 2) != 0x09)
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
        else if (crc16_usb(&out[2], out_size - 2) != 0xb001)
        {
            error |= CAPTURE_ERROR_CRC;
        }
    }

    return error;
}

// Decode the packet at g_rd_ptr in place. Every exit leaves a well formed record (flags | byte count)
// behind: an early return used to leave the header word stale, and the reader skipped over the rest
// of the buffer by whatever count was left in it.
static void process_packet(int size)
{
    uint8_t *out_data = (uint8_t *)&g_buffer[g_wr_ptr];
    uint32_t r = decode_samples(&g_buffer[g_rd_ptr], BUFFER_WORDS - g_rd_ptr, size, out_data, 0xffff);
    int out_size = r & CAPTURE_SIZE_MASK;
    uint32_t error = (r & ~CAPTURE_SIZE_MASK) | check_packet(out_data, out_size, g_buffer_info.fs);

    handle_folding(out_size >= 2 ? (out_data[1] & 0x0f) : -1, error);

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
        if (g_rd_ptr + 2 > BUFFER_WORDS)
        {
            printf("Synchronization error after %d packets, the buffer ended.\r\n", out_count);
            break;
        }

        uint32_t size = g_buffer[g_rd_ptr];
        uint32_t time = start_time(g_buffer[g_rd_ptr + 1], size);

        // Everything decoded so far is intact (it was compacted behind the read pointer): keep it
        // and lose only the rest of the buffer. Clearing the count threw away every packet. A size
        // that runs past the buffer is just as wrong.
        if (size > 0xffff || g_rd_ptr + 2 + (int)(size / 31) + 1 > BUFFER_WORDS)
        {
            printf("Synchronization error after %d packets, the rest of the buffer is lost. Check your speed "
                   "setting.\r\n",
                   out_count);
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
            // The sampler pushes a word every 31 samples and one more at the EOP: size / 31 + 1 words,
            // whatever the decoder managed to read.
            int next = g_rd_ptr + (int)(size / 31) + 1;

            process_packet(size - 1);
            g_rd_ptr = next;
        }
    }

    g_buffer_info.count = out_count;
}

// ---- Clock -------------------------------------------------------------------------------------
//
// The sampler runs 10 PIO cycles per bit: 15 MHz for low speed, 120 MHz for full speed, from an
// integer divider of clk_sys. The RP2350 default of 150 MHz already divides to 15 MHz exactly; the
// RP2040 default of 125 MHz does not, so clk_sys goes to 120 MHz for the capture and comes back
// after it. set_sys_clock_khz() also moves clk_peri to the 48 MHz USB PLL, while the display, the
// flash and any UART were set up for the clk_peri they had (clk_sys from boot, the USB PLL after an
// overclock), so that source is saved and put back after every clk_sys change.

static uint32_t usb_clock_restore_khz;
static uint32_t usb_peri_auxsrc;
static uint32_t usb_peri_hz;

static void usb_clock_set(uint32_t khz)
{
    set_sys_clock_khz(khz, true);
    clock_configure_undivided(clk_peri, 0, usb_peri_auxsrc,
                              usb_peri_auxsrc == CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS ? khz * 1000 : usb_peri_hz);
}

static void usb_clock_begin(bool fs)
{
    uint32_t sys_hz = clock_get_hz(clk_sys);
    uint32_t pio_hz = fs ? 120000000u : 15000000u;

    usb_clock_restore_khz = 0;

    if (sys_hz % pio_hz == 0)
    {
        return;
    }

    usb_clock_restore_khz = sys_hz / 1000;
    usb_peri_auxsrc = (clocks_hw->clk[clk_peri].ctrl & CLOCKS_CLK_PERI_CTRL_AUXSRC_BITS) >> CLOCKS_CLK_PERI_CTRL_AUXSRC_LSB;
    usb_peri_hz = clock_get_hz(clk_peri);
    usb_clock_set(120000);
}

static void usb_clock_end(void)
{
    if (usb_clock_restore_khz)
    {
        usb_clock_set(usb_clock_restore_khz);
        usb_clock_restore_khz = 0;
    }
}

// ---- What is attached --------------------------------------------------------------------------
//
// Read from the idle level of D+ and D- (USB 2.0 7.1.5): a low speed device pulls D- up, a full
// speed one D+; with nothing attached, or a high speed device past its chirp (400 mV, under the logic
// threshold), both read low, so those two cannot be told apart. Traffic leaves the idle level over
// 90% of the time, so a majority of 230 of 256 samples taken 1 ms apart, confirmed by two windows in
// a row, ignores packets, resets and plug-in bounces. Taken from okhi's usb_bus.h, to tell people
// when the keyboard they plugged in cannot be sniffed.
#define USB_BUS_SAMPLE_US 1000
#define USB_BUS_WINDOW 256
#define USB_BUS_MAJORITY 230

enum
{
    UsbBus_Unknown,
    UsbBus_Low,
    UsbBus_Full,
    UsbBus_None,
    UsbBus_Invalid,
};

static struct
{
    uint32_t sampled_at;
    uint16_t counts[4];
    uint16_t samples;
    uint8_t candidate;
    uint8_t state;
} usb_bus;

// Sample once per ms; true when a new state was confirmed.
static bool usb_bus_watch(void)
{
    static const uint8_t verdict_by_lines[4] = { UsbBus_None, UsbBus_Full, UsbBus_Low, UsbBus_Invalid };
    uint32_t now = time_us_32();

    if (now - usb_bus.sampled_at < USB_BUS_SAMPLE_US)
    {
        return false;
    }

    usb_bus.sampled_at = now;

    uint32_t pads = gpio_get_all();
    usb_bus.counts[((pads >> DP_INDEX) & 1u) | (((pads >> DM_INDEX) & 1u) << 1)]++;

    if (++usb_bus.samples < USB_BUS_WINDOW)
    {
        return false;
    }

    uint8_t verdict = UsbBus_Unknown;

    for (int lines = 0; lines < 4; lines++)
    {
        if (usb_bus.counts[lines] >= USB_BUS_MAJORITY)
        {
            verdict = verdict_by_lines[lines];
        }
    }

    memset(usb_bus.counts, 0, sizeof(usb_bus.counts));
    usb_bus.samples = 0;

    bool changed = verdict != UsbBus_Unknown && verdict == usb_bus.candidate && verdict != usb_bus.state;

    if (changed)
    {
        usb_bus.state = verdict;
    }

    usb_bus.candidate = verdict;
    return changed;
}

// The state, with a warning when it is not what a capture at this speed can decode.
static void usb_bus_print(bool fs)
{
    switch (usb_bus.state)
    {
        case UsbBus_Low:
            if (fs)
            {
                printf("%susb bus: low speed device, set the capture speed (e) to Low%s\r\n", ui_term_color_warning(),
                       ui_term_color_reset());
            }
            else
            {
                printf("usb bus: low speed device\r\n");
            }
            break;
        case UsbBus_Full:
            if (fs)
            {
                printf("usb bus: full speed device\r\n");
            }
            else
            {
                printf("%susb bus: full speed device, the keylogger reads only low speed (1.5 Mbit/s)\r\n"
                       "usb bus: set the capture speed (e) to Full to capture its packets with s%s\r\n",
                       ui_term_color_warning(), ui_term_color_reset());
            }
            break;
        case UsbBus_None:
            printf("%susb bus: nothing attached, or a high speed device, which cannot be sniffed%s\r\n",
                   ui_term_color_warning(), ui_term_color_reset());
            break;
        case UsbBus_Invalid:
            printf("%susb bus: invalid line state, D+ and D- both high: check the plank and the cables%s\r\n",
                   ui_term_color_warning(), ui_term_color_reset());
            break;
        default:
            printf("usb bus: unknown, the lines keep changing\r\n");
            break;
    }
}

// Two windows from scratch, about 0.5 s, then print what is attached.
static void usb_bus_check(bool fs)
{
    memset(&usb_bus, 0, sizeof(usb_bus));
    usb_bus.sampled_at = time_us_32() - USB_BUS_SAMPLE_US;

    for (uint32_t start = time_us_32(); time_us_32() - start < 3 * USB_BUS_WINDOW * USB_BUS_SAMPLE_US;)
    {
        if (usb_bus_watch())
        {
            break;
        }
    }

    usb_bus_print(fs);
}

// ---- PIO ---------------------------------------------------------------------------------------
//
// The sampler (tar_pio0) is exactly 32 instructions, entered at 31 and wrapping at 30, so it takes
// a whole instruction memory: the mode's PIO block. The START synchroniser (tar_pio1) goes into
// pio0 beside the logic analyzer and, on the Bus Pirate 5, the RGB LEDs (PIO_RGB_LED_PIO), on a state
// machine neither of them uses (they run on 0 and 1 without claiming them). Only these two programs
// are added and removed: clearing both blocks also wiped the LED program.

#define USB_SAMPLER_PIO PIO_MODE_PIO
#define USB_START_PIO pio0

static uint16_t usb_sampler_instructions[32];
static struct pio_program usb_sampler_program;
static int usb_sampler_sm = -1;
static int usb_start_sm = -1;
static uint usb_start_offset;

static void usb_pio_stop(void)
{
    if (usb_sampler_sm >= 0)
    {
        pio_sm_set_enabled(USB_SAMPLER_PIO, usb_sampler_sm, false);
        pio_sm_unclaim(USB_SAMPLER_PIO, usb_sampler_sm);
        pio_remove_program(USB_SAMPLER_PIO, &usb_sampler_program, 0);
        usb_sampler_sm = -1;
    }

    if (usb_start_sm >= 0)
    {
        pio_sm_set_enabled(USB_START_PIO, usb_start_sm, false);
        pio_sm_unclaim(USB_START_PIO, usb_start_sm);
        pio_remove_program(USB_START_PIO, &tar_pio1_program, usb_start_offset);
        usb_start_sm = -1;
        gpio_set_function(START_INDEX, GPIO_FUNC_SIO);
    }

    bio_input(BIO_START_INDEX);
}

// Load and configure both state machines, stopped. False (with the reason printed) if a PIO is busy.
static bool usb_pio_load(bool fs)
{
    float div = (float)clock_get_hz(clk_sys) / (fs ? 120000000.0f : 15000000.0f);
    const uint16_t *speed = fs ? usb_full_speed_template_program_instructions
                               : usb_low_speed_template_program_instructions;

    memcpy(usb_sampler_instructions, tar_pio0_program_instructions, sizeof(usb_sampler_instructions));
    usb_sampler_instructions[1] = speed[0];
    usb_sampler_instructions[2] = speed[1];
    usb_sampler_program = tar_pio0_program;
    usb_sampler_program.instructions = usb_sampler_instructions;

    // The mode's PIO belongs to the active mode: clear whatever an earlier one left in it.
    for (int sm = 0; sm < 4; sm++)
    {
        if (pio_sm_is_claimed(USB_SAMPLER_PIO, sm))
        {
            pio_sm_set_enabled(USB_SAMPLER_PIO, sm, false);
            pio_sm_unclaim(USB_SAMPLER_PIO, sm);
        }
    }
    pio_clear_instruction_memory(USB_SAMPLER_PIO);
#if PICO_PIO_USE_GPIO_BASE
    pio_set_gpio_base(USB_SAMPLER_PIO, 0);
#endif

    int start_sm = !pio_sm_is_claimed(USB_START_PIO, 3) ? 3 : !pio_sm_is_claimed(USB_START_PIO, 2) ? 2 : -1;

    if (start_sm < 0 || !pio_can_add_program(USB_START_PIO, &tar_pio1_program))
    {
        printf("pio0 has no free state machine or no room for the start program\r\n");
        return false;
    }

    pio_add_program_at_offset(USB_SAMPLER_PIO, &usb_sampler_program, 0);
    usb_sampler_sm = pio_claim_unused_sm(USB_SAMPLER_PIO, true);
    usb_start_offset = pio_add_program(USB_START_PIO, &tar_pio1_program);
    pio_sm_claim(USB_START_PIO, start_sm);
    usb_start_sm = start_sm;

    bio_input(BIO_DP_INDEX);
    bio_input(BIO_DM_INDEX);
    bio_input(BIO_TRIGGER_INDEX);
    bio_output(BIO_START_INDEX);
    bio_put(BIO_START_INDEX, false);

    // A PIO keeps driving the level it last wrote: START stays high from the previous capture and
    // would release the sampler before the synchroniser looks for an SE0.
    pio_sm_set_pins_with_mask(USB_START_PIO, usb_start_sm, 0, 1u << START_INDEX);
    pio_gpio_init(USB_START_PIO, START_INDEX);
    pio_sm_set_consecutive_pindirs(USB_START_PIO, usb_start_sm, START_INDEX, 1, true);
    pio_sm_set_consecutive_pindirs(USB_START_PIO, usb_start_sm, DP_INDEX, 2, false);
    pio_sm_config c1 = tar_pio1_program_get_default_config(usb_start_offset);
    sm_config_set_set_pins(&c1, START_INDEX, 1);
    sm_config_set_in_shift(&c1, false, false, 0);
    sm_config_set_out_shift(&c1, false, false, 0);
    sm_config_set_in_pins(&c1, DP_INDEX);
    sm_config_set_clkdiv(&c1, div);
    pio_sm_init(USB_START_PIO, usb_start_sm, usb_start_offset, &c1);
    pio_sm_set_enabled(USB_START_PIO, usb_start_sm, false);
    pio_sm_clear_fifos(USB_START_PIO, usb_start_sm);
    pio_sm_restart(USB_START_PIO, usb_start_sm);
    pio_sm_clkdiv_restart(USB_START_PIO, usb_start_sm);

    pio_sm_set_consecutive_pindirs(USB_SAMPLER_PIO, usb_sampler_sm, DP_INDEX, 3, false);
    pio_sm_config c0 = tar_pio0_program_get_default_config(0);
    sm_config_set_in_pins(&c0, DP_INDEX);
    sm_config_set_jmp_pin(&c0, fs ? DP_INDEX : DM_INDEX);
    sm_config_set_in_shift(&c0, false, true, 31);
    sm_config_set_out_shift(&c0, false, false, 32);
    sm_config_set_fifo_join(&c0, PIO_FIFO_JOIN_RX);
    sm_config_set_clkdiv(&c0, div);
    pio_sm_init(USB_SAMPLER_PIO, usb_sampler_sm, 0, &c0);
    pio_sm_set_enabled(USB_SAMPLER_PIO, usb_sampler_sm, false);
    pio_sm_clear_fifos(USB_SAMPLER_PIO, usb_sampler_sm);
    pio_sm_restart(USB_SAMPLER_PIO, usb_sampler_sm);
    pio_sm_clkdiv_restart(USB_SAMPLER_PIO, usb_sampler_sm);

    return true;
}

// The sampler waits at its entry point (31) for START, which the synchroniser raises on a clean SE0.
static void usb_pio_run(void)
{
    pio_sm_exec(USB_SAMPLER_PIO, usb_sampler_sm, pio_encode_jmp(31));
    pio_sm_set_enabled(USB_SAMPLER_PIO, usb_sampler_sm, true);
    pio_sm_exec(USB_SAMPLER_PIO, usb_sampler_sm, pio_encode_jmp(31));
    pio_sm_clear_fifos(USB_SAMPLER_PIO, usb_sampler_sm);
    pio_sm_set_enabled(USB_START_PIO, usb_start_sm, true);
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

static void cap(void)
{
    bool fs = (g_capture_speed == CaptureSpeed_Full);

    usb_clock_begin(fs);

    // a failure leaves the previous capture as it was, for 'b'
    if (!usb_pio_load(fs))
    {
        usb_pio_stop();
        usb_clock_end();
        return;
    }

    g_buffer_info.fs = fs;
    g_buffer_info.trigger = (g_capture_trigger == CaptureTrigger_Enabled);
    g_buffer_info.limit = capture_limit_value();
    g_buffer_info.count = 0;

    int index = 2;
    int packet = 0;
    uint8_t expected = fs ? UsbBus_Full : UsbBus_Low;
    uint8_t known = usb_bus.state;

    memset((void *)g_buffer, 0, BUFFER_SIZE);

    // Nothing to capture at this speed, as far as the last look at the bus says: a full speed bus
    // never gives the start synchroniser the long SE0 a low speed capture waits for, so the capture
    // stays empty until 'p'. Say why before it starts.
    if (known != UsbBus_Unknown && known != expected)
    {
        usb_bus_print(fs);
    }

    if (!wait_for_trigger())
    {
        usb_pio_stop();
        usb_clock_end();
        return;
    }

    usb_pio_run();

    uint32_t base_time = 0;
    uint32_t idle = 0;

    // a fresh window, the last state kept for the next capture
    memset(usb_bus.counts, 0, sizeof(usb_bus.counts));
    usb_bus.samples = 0;
    usb_bus.candidate = UsbBus_Unknown;

    while (1)
    {
        // A quiet bus (nothing attached, a suspended host) never fills the buffer: 'p' ends the capture.
        if (pio_sm_is_rx_fifo_empty(USB_SAMPLER_PIO, usb_sampler_sm))
        {
            usb_bus_watch(); // only samples; printing here could overrun the PIO FIFO

            if (++idle >= 4096)
            {
                idle = 0;

                if (stop_requested())
                {
                    printf("capture stopped\r\n");
                    break;
                }
            }

            continue;
        }

        uint32_t v = pio_sm_get(USB_SAMPLER_PIO, usb_sampler_sm);

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
            // in words: comparing the word index with BUFFER_SIZE (bytes) let a busy bus write up to
            // 384 KB past the end of the 128 KB buffer
            if (index < (BUFFER_WORDS - 4))
            {
                g_buffer[index++] = v;
            }
            else
            {
                break;
            }
        }
    }

    usb_pio_stop();
    usb_clock_end();
    process_buffer();

    if (usb_bus.state != UsbBus_Unknown && usb_bus.state != expected && usb_bus.state != known)
    {
        usb_bus_print(fs);
    }
}

// ---- Keylogger ---------------------------------------------------------------------------------
//
// okhi drains the sampler with one core and decodes with the other. Here the Bus Pirate keeps core
// 1, so a DMA channel drains the sampler's RX FIFO into a ring in the big buffer and the CPU decodes
// the ring at its own pace, printing included: nothing in the capture path waits for the CPU. The
// ring is 8192 words, about 2.5 s of a keyboard polled on two endpoints every 8 ms (about 3300
// words a second: a keep-alive every ms, an IN and its NAK or DATA and ACK per poll).
//
// The ring is decoded as a stream. Data words (bit 31 clear) gather until a size word (bit 31 set)
// closes the packet, which must bring exactly size / 31 + 1 data words: a lost word fails that check
// instead of shifting the packets after it. The IN that addresses a report and the report itself are
// paired across the whole stream: the old keylogger decoded batches of 256 packets with the pairing
// reset at each one, and lost the report whenever its IN ended a batch (4 of 1175 reports on the
// bench, 2026-11-03).

#define KL_RING_BITS 15 // the largest ring a DMA channel can wrap: 32 KB
#define KL_RING_BYTES (1u << KL_RING_BITS)
#define KL_RING_WORDS (KL_RING_BYTES / 4)
#define KL_RING_MARGIN 1024 // resynchronise this far before the DMA could overwrite unread words
#define KL_PKT_WORDS 40     // the longest packet decoded; a low speed packet takes 5
#define KL_PKT_BYTES ((KL_PKT_WORDS * 31) / 8 + 1)

#if PICO_RP2350
// re-triggers itself at the end of the count: the channel never stops
#define KL_DMA_COUNT_BITS DMA_CH0_TRANS_COUNT_COUNT_BITS
#define KL_DMA_COUNT                                                                                                   \
    ((DMA_CH0_TRANS_COUNT_MODE_VALUE_TRIGGER_SELF << DMA_CH0_TRANS_COUNT_MODE_LSB) | DMA_CH0_TRANS_COUNT_COUNT_BITS)
#else
// 2^32 - 1 words, weeks of traffic; the main loop re-arms the channel if it ever stops
#define KL_DMA_COUNT_BITS 0xffffffffu
#define KL_DMA_COUNT 0xffffffffu
#endif

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

// Per endpoint: the last report seen (repeats are held back) and the keys of the last report that
// was a keyboard report (a key is new only against these). One shared state read the empty report a
// second interface sends at every change of the first as "all keys up", and typed every overlapped
// key twice ("abcd" gave 8 characters with okhi-kbd's two-interface format, 2026-11-03).
typedef struct
{
    bool seen;
    uint8_t addr;
    uint8_t last[8];
    uint8_t held[6];
} keylog_ep_t;

static struct
{
    volatile uint32_t *ring;
    int dma;
    uint64_t base;      // words the DMA wrote in completed counts
    uint32_t last_left; // transfer count at the previous look
    uint64_t read;      // words decoded
    uint32_t pkt[KL_PKT_WORDS];
    int pkt_words;
    bool resync; // drop words until the next size word
    bool in_pending;
    uint8_t in_addr;
    uint8_t in_ep;
    keylog_ep_t eps[16];
    bool caps;
    unsigned int reports;
    unsigned int repeats;
    unsigned int ep0;
    unsigned int ignored;
    unsigned int packets;
    unsigned int resets;
    unsigned int errors;
    unsigned int error_kinds[6]; // by the highest CAPTURE_ERROR_* bit: stuffing, CRC, PID, SYNC, bit count, length
    unsigned int framing;
    unsigned int peak;
    unsigned int lost;
} kl;

static unsigned char keylog_tracker[KEYLOG_TRACKER_SIZE];
static unsigned int keylog_tracker_index = 0;

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

// A boot keyboard report: the reserved byte is 0, no error code (1 rollover, 2 POST fail, 3 undefined),
// no modifier usage in the key array, no key twice. Holes between keys are fine: plenty of keyboards
// keep a key in a fixed slot. Anything else is another interface or the phantom state, and decoding
// it would emit keys nobody pressed.
static bool keylog_plausible(const uint8_t *b)
{
    if (b[1] != 0)
    {
        return false;
    }

    for (int i = 2; i < 8; i++)
    {
        if (b[i] == 0)
        {
            continue;
        }

        if (b[i] < 0x04 || b[i] >= 0xE0)
        {
            return false;
        }

        for (int j = 2; j < i; j++)
        {
            if (b[j] == b[i])
            {
                return false;
            }
        }
    }

    return true;
}

// One report of an interrupt endpoint. Returns true if it printed (the DMA moved on meanwhile).
static bool keylog_report(uint8_t addr, uint8_t ep, const uint8_t *report)
{
    // Endpoint 0 carries the enumeration, 8 bytes at a time, never a keystroke: decoded, one
    // re-enumeration put 41 characters of descriptors in the tracker (2026-11-03).
    if (ep == 0)
    {
        kl.ep0++;
        return false;
    }

    keylog_ep_t *e = &kl.eps[ep & 0x0f];

    if (e->seen && e->addr != addr)
    {
        memset(e->held, 0, sizeof(e->held));
    }
    else if (e->seen && memcmp(e->last, report, 8) == 0)
    {
        // an idle rate repeat, or a resend after a lost ACK: no keystroke in it
        kl.repeats++;
        return false;
    }

    e->seen = true;
    e->addr = addr;
    memcpy(e->last, report, 8);

    printf("[0x%04X] IN 0x%02X/%X: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X", kl.reports, addr, ep,
           report[0], report[1], report[2], report[3], report[4], report[5], report[6], report[7]);
    kl.reports++;

    if (!keylog_plausible(report))
    {
        kl.ignored++;
        printf(" %s\r\n-----------\r\n", report[2] == 0x01 ? "<ROLLOVER>" : "<IGNORED>");
        return true;
    }

    printf("\r\n");

    bool shift = (report[0] & 0x22) != 0;

    for (int i = 2; i < 8; i++)
    {
        uint8_t k = report[i];

        if (k < 4 || k >= KEYLOG_HID_MAX || memchr(e->held, k, sizeof(e->held)))
        {
            continue;
        }

        if (k == 0x39)
        {
            kl.caps = !kl.caps;
            printf("<CAPS>");
            continue;
        }

        char c = keylog_hid_lower[k];

        if (c >= 'a' && c <= 'z')
        {
            if (shift != kl.caps)
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

    memcpy(e->held, &report[2], sizeof(e->held));
    return true;
}

// One word of the stream. Returns true if a report was printed.
static bool keylog_word(uint32_t w)
{
    if (!(w & 0x80000000))
    {
        if (kl.pkt_words < KL_PKT_WORDS)
        {
            kl.pkt[kl.pkt_words] = w;
        }

        kl.pkt_words++;
        return false;
    }

    uint32_t size = 0xffffffff - w;
    int words = kl.pkt_words;

    kl.pkt_words = 0;

    if (kl.resync)
    {
        // the end of a packet whose first words were lost
        kl.resync = false;
        return false;
    }

    kl.packets++;

    if (size == 0)
    {
        // a bus reset: a sustained SE0 with no samples
        kl.resets++;
        kl.in_pending = false;
        if (words)
        {
            kl.framing++;
        }
        return false;
    }

    if (size > 0xffff || words > KL_PKT_WORDS || words != (int)(size / 31) + 1)
    {
        kl.framing++;
        kl.in_pending = false;
        return false;
    }

    if (size == 1)
    {
        // a low speed keep-alive
        return false;
    }

    uint8_t out[KL_PKT_BYTES];
    uint32_t r = decode_samples(kl.pkt, words, size - 1, out, sizeof(out));
    int n = r & CAPTURE_SIZE_MASK;

    uint32_t error = (r & ~CAPTURE_SIZE_MASK) | check_packet(out, n, false);

    if (error)
    {
        // as in okhi: a damaged packet leaves the pairing alone, an IN still waits for its DATA
        kl.errors++;
        kl.error_kinds[__builtin_clz(error)]++;
        return false;
    }

    int pid = out[1] & 0x0f;

    if (pid == Pid_In)
    {
        int v = (out[3] << 8) | out[2];
        kl.in_addr = v & 0x7f;
        kl.in_ep = (v >> 7) & 0x0f;
        kl.in_pending = true;
        return false;
    }

    bool in_pending = kl.in_pending;
    kl.in_pending = false;

    if (in_pending && (pid == Pid_Data0 || pid == Pid_Data1) && n - 4 == 8)
    {
        return keylog_report(kl.in_addr, kl.in_ep, &out[2]);
    }

    return false;
}

// Words the DMA has written since the keylogger started.
static uint64_t keylog_written(void)
{
    uint32_t left = dma_hw->ch[kl.dma].transfer_count & KL_DMA_COUNT_BITS;

    if (left > kl.last_left)
    {
        // the count started again: the channel re-triggered (RP2350) or was re-armed (RP2040)
        kl.base += KL_DMA_COUNT_BITS;
    }

    kl.last_left = left;
    return kl.base + (KL_DMA_COUNT_BITS - left);
}

static void keylog_drain(void)
{
    uint64_t written = keylog_written();

    while (kl.read < written)
    {
        uint64_t lag = written - kl.read;

        if (lag > kl.peak)
        {
            kl.peak = (unsigned int)lag;
        }

        if (lag > KL_RING_WORDS - KL_RING_MARGIN)
        {
            // printing fell behind by most of the ring: skip ahead and lose words, not the ring
            uint64_t to = written - KL_RING_WORDS / 2;
            kl.lost += (unsigned int)(to - kl.read);
            kl.read = to;
            kl.pkt_words = 0;
            kl.resync = true;
            kl.in_pending = false;
            continue;
        }

        uint32_t w = kl.ring[kl.read & (KL_RING_WORDS - 1)];
        kl.read++;

        if (keylog_word(w))
        {
            written = keylog_written();
        }
    }

#if !PICO_RP2350
    if (!dma_channel_is_busy(kl.dma))
    {
        dma_channel_set_trans_count(kl.dma, KL_DMA_COUNT, true);
    }
#endif
}

static void keylog_run(void)
{
    printf("\r\nUSB low speed keylogger started, press 'q' to exit\r\n"
           "only USB 1.1 low speed boot protocol keyboards are decoded\r\n\r\n");

    memset(&kl, 0, sizeof(kl));
    memset(&usb_bus, 0, sizeof(usb_bus));
    memset(keylog_tracker, 0, sizeof(keylog_tracker));
    keylog_tracker_index = 0;

    // the ring: the 32 KB aligned block inside the 128 KB big buffer (a DMA ring wraps on its size)
    kl.ring = (volatile uint32_t *)(((uintptr_t)g_buffer + KL_RING_BYTES - 1) & ~(uintptr_t)(KL_RING_BYTES - 1));
    kl.dma = dma_claim_unused_channel(false);

    if (kl.dma < 0)
    {
        printf("no free DMA channel\r\n");
        return;
    }

    usb_clock_begin(false);

    if (!usb_pio_load(false))
    {
        usb_pio_stop();
        usb_clock_end();
        dma_channel_unclaim(kl.dma);
        return;
    }

    // the ring overwrites the buffer of the last 's' capture
    g_buffer_info.fs = false;
    g_buffer_info.count = 0;

    dma_channel_config c = dma_channel_get_default_config(kl.dma);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_ring(&c, true, KL_RING_BITS);
    channel_config_set_dreq(&c, pio_get_dreq(USB_SAMPLER_PIO, usb_sampler_sm, false));
    channel_config_set_high_priority(&c, true);
    kl.last_left = KL_DMA_COUNT_BITS;
    dma_channel_configure(kl.dma, &c, kl.ring, &USB_SAMPLER_PIO->rxf[usb_sampler_sm], KL_DMA_COUNT, true);

    usb_pio_run();

    char cmd = '\0';
    while (1)
    {
        keylog_drain();

        if (usb_bus_watch())
        {
            usb_bus_print(false);
        }

        if (rx_fifo_try_get(&cmd) && (cmd == 'q' || cmd == 'Q'))
        {
            break;
        }
    }

    usb_pio_stop();
    // clears EN before the abort, the sequence erratum RP2350-E5 asks for: a self-triggering
    // channel aborted with EN set may trigger again. Not seen with a plain abort either (three 's'
    // captures after 'k', no error, 2026-11-03), but it would steal the sampler's FIFO words.
    dma_channel_cleanup(kl.dma);
    dma_channel_unclaim(kl.dma);
    usb_clock_end();

    printf("\r\n\r\nkeys captured: %s\r\n", keylog_tracker);
    printf("reports: %u\r\n", kl.reports);
    printf("repeats held back: %u\r\n", kl.repeats);
    printf("endpoint 0 data: %u\r\n", kl.ep0);
    printf("reports ignored: %u\r\n", kl.ignored);
    printf("packets: %u\r\n", kl.packets);
    printf("bus resets: %u\r\n", kl.resets);
    printf("packet errors: %u\r\n", kl.errors);
    if (kl.errors)
    {
        printf("  stuffing %u, crc %u, pid %u, sync %u, bit count %u, length %u\r\n", kl.error_kinds[0],
               kl.error_kinds[1], kl.error_kinds[2], kl.error_kinds[3], kl.error_kinds[4], kl.error_kinds[5]);
    }
    printf("framing errors: %u\r\n", kl.framing);
    printf("ring peak backlog (words): %u\r\n", kl.peak);
    printf("words lost: %u\r\n", kl.lost);
    printf("keylogger stopped\r\n");
}

void usb_handler(struct command_result* res) {
    if (NULL == g_buffer)
    {
        printf("Buffer not allocated\r\n");
        return;
    }

    // psucmd_enable() also labels the VOUT pin, which psu_enable() alone left at OFF with 3.3 V on it.
    // The buffers must run at 3.3 V: at 5 V their input high threshold is above a USB high.
    uint32_t result = PSU_OK;

    if (!psu_status.enabled || fabsf(psu_status.voltage_requested_float - 3.33f) > 0.05f)
    {
        result = psucmd_enable(3.33f, 400.0f, false, 100);
    }

    if (result) {
            printf("\r\nPSU ERROR CODE %d\r\n", result);
    } else {
            printf("\r\nPSU 3V3 Enabled\r\n");
    }

    print_help();
    usb_bus_check(g_capture_speed == CaptureSpeed_Full);

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
            printf("capture start! 'p' to stop\r\n");
            cap();
            printf("capture end!, press 'b' to show the buffer\r\n");
        }
        else if (cmd == 'p')
        {
            printf("no capture running\r\n");
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
    usb_pio_stop();

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

    psucmd_disable();
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
