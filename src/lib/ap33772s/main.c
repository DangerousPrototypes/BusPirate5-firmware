#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include "ap33772s.h"

#define DEFAULT_DEVICE_PATH "/dev/i2c-4"
#define DEFAULT_MONITOR_INTERVAL_MS 500

#define COMMAND_BUFFER_SIZE 512
#define HISTORY_MAX_ENTRIES 64


static int enable_raw_mode(void);
static void disable_raw_mode(void);


static void sleep_ms(int milliseconds)
{
    if (milliseconds <= 0) {
        return;
    }

    struct timespec req = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (long)(milliseconds % 1000) * 1000000L
    };

    while (nanosleep(&req, &req) == -1 && errno == EINTR) {
        continue;
    }
}

static volatile sig_atomic_t monitor_interrupted = 0;

static void monitor_sigint_handler(int signo)
{
    (void)signo;
    monitor_interrupted = 1;
}

static struct termios original_termios;
static bool raw_mode_active = false;

static bool ap33772s_debug_enabled = true;

static void ap33772s_set_debug(bool enable)
{
    ap33772s_debug_enabled = enable;
}

static int ap33772s_bus_read(void *ctx, uint8_t reg, uint8_t *data, size_t len)
{
    if (!ctx || !data || len == 0) {
        return -EINVAL;
    }

    int fd = *(int *)ctx;

    ssize_t written = write(fd, &reg, 1);
    if (written < 0) {
        return -errno;
    }
    if (written != 1) {
        return -EIO;
    }

    size_t total_read = 0;
    while (total_read < len) {
        ssize_t chunk = read(fd, data + total_read, len - total_read);
        if (chunk < 0) {
            return -errno;
        }
        if (chunk == 0) {
            return -EIO;
        }
        total_read += (size_t)chunk;
    }

    if (ap33772s_debug_enabled) {
        printf("I2C READ reg 0x%02X: ", reg);
        for (size_t i = 0; i < len; i++) {
            printf("%02X ", data[i]);
        }
        printf("\n");
    }

    return 0;
}

static int ap33772s_bus_write(void *ctx, uint8_t reg, const uint8_t *data, size_t len)
{
    if (!ctx || (len > 0 && !data)) {
        return -EINVAL;
    }

    int fd = *(int *)ctx;
    if (len > AP33772S_WRITE_BUFFER_LENGTH) {
        return -EINVAL;
    }

    uint8_t buffer[AP33772S_WRITE_BUFFER_LENGTH + 1];
    buffer[0] = reg;
    if (len > 0) {
        memcpy(&buffer[1], data, len);
    }

    if (ap33772s_debug_enabled) {
        printf("I2C WRITE reg 0x%02X: ", reg);
        for (size_t i = 0; i < len; i++) {
            printf("%02X ", buffer[i + 1]);
        }
        printf("\n");
    }

    ssize_t written = write(fd, buffer, len + 1);
    if (written < 0) {
        return -errno;
    }
    if ((size_t)written != len + 1) {
        return -EIO;
    }

    return 0;
}

static void ap33772s_delay_us(void *ctx, unsigned int usec)
{
    (void)ctx;
    if (usec == 0) {
        return;
    }

    struct timespec req = {
        .tv_sec = usec / 1000000U,
        .tv_nsec = (long)(usec % 1000000U) * 1000L
    };

    while (nanosleep(&req, &req) == -1 && errno == EINTR) {
        continue;
    }
}

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [--device <path>] [--interval <ms>] [command [args...]]\n"
            "       (omit command to enter interactive shell)\n\n"
            "Commands:\n"
            "  status                Show controller status, PDO list, and live measurements\n"
            "  opmode                Show controller operation mode flags\n"
            "  configure <value>     Write raw value into PDCONFIG register\n"
            "  monitor [ms]          Poll for status changes (default 500 ms)\n"
            "  set fixed <idx> <mA>  Request fixed PDO at index with target current\n"
            "  set pps <idx> <mV> <mA>  Request PPS profile\n"
            "  set avs <idx> <mV> <mA>  Request AVS profile\n"
            "  set max <idx>         Request maximum power for PDO\n"
            "  pdo <idx>             Show detailed information for a PDO\n"
            "  set protect <dr|otp|ocp|ovp|uvp> <on|off>  Toggle protection features\n"
            "  set output <on|off>   Enable or disable the NMOS output\n"
            "  set hardreset         Send hard reset command\n"
            "  set drswap            Send data role swap command\n"
            "  readreg <reg> <len>   Read and print register bytes\n"
            "  dbg <on|off>          Enable/disable I2C debug output\n"
            "  caps                  Refresh and print adapter PDO capabilities\n"
            "  help                  Show this message\n"
            "  exit                  Leave the interactive shell\n\n"
            "Examples:\n"
            "  %s status\n"
            "  %s --device /dev/i2c-1 monitor\n"
            "  %s set fixed 2 3000\n"
            "  %s\n",
            prog, prog, prog, prog, prog);
}

static int parse_int_arg(const char *text, int *value)
{
    char *end = NULL;
    errno = 0;
    long parsed = strtol(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0') {
        return -EINVAL;
    }
    if (parsed > INT_MAX || parsed < INT_MIN) {
        return -ERANGE;
    }
    *value = (int)parsed;
    return 0;
}

static void print_status_flags(uint8_t status)
{
    if (status == 0) {
        printf("none");
        return;
    }

    bool first = true;
    struct {
        uint8_t mask;
        const char *label;
    } flags[] = {
        {AP33772S_MASK_STARTED, "STARTED"},
        {AP33772S_MASK_READY, "READY"},
        {AP33772S_MASK_NEWPDO, "NEWPDO"},
        {AP33772S_MASK_UVP, "UVP"},
        {AP33772S_MASK_OVP, "OVP"},
        {AP33772S_MASK_OCP, "OCP"},
        {AP33772S_MASK_OTP, "OTP"},
    };

    for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); ++i) {
        if (status & flags[i].mask) {
            if (!first) {
                printf(", ");
            }
            printf("%s", flags[i].label);
            first = false;
        }
    }
}

static void print_opmode_flags(uint8_t opmode)
{
    bool first = true;
    struct {
        uint8_t mask;
        const char *label;
    } flags[] = {
        {1u << 7, "CCFLIP"},
        {1u << 6, "DR"},
        {1u << 5, "DATARL"},
        {1u << 1, "PDMOD"},
        {1u << 0, "LGCYMOD"},
    };

    for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); ++i) {
        if (opmode & flags[i].mask) {
            if (!first) {
                printf(", ");
            }
            printf("%s", flags[i].label);
            first = false;
        }
    }

    if (first) {
        printf("none");
    }
}

static void print_measurements(ap33772s_ref dev)
{
    int value = 0;
    if (ap33772s_read_voltage(dev, &value) == 0) {
        printf("VBUS voltage : %d mV\n", value);
    }
    if (ap33772s_read_current(dev, &value) == 0) {
        printf("VBUS current : %d mA\n", value);
    }
    if (ap33772s_read_temperature(dev, &value) == 0) {
        printf("NTC temp    : %d C\n", value);
    }
    if (ap33772s_read_vreq(dev, &value) == 0) {
        printf("Requested V : %d mV\n", value);
    }
    if (ap33772s_read_ireq(dev, &value) == 0) {
        printf("Requested I : %d mA\n", value);
    }
}


static int handle_caps(ap33772s_ref dev, const char *header)
{   
    int ret = ap33772s_get_power_capabilities(dev);
    if (ret < 0) {
        fprintf(stderr, "Failed to refresh PDO list: %s\n", strerror(-ret));
        return ret;
    }

    if (header && *header) {
        printf("%s\n", header);
    }

    int16_t num_pdo = ap33772s_get_num_pdo(dev);
    if (num_pdo < 0) {
        num_pdo = 0;
    }

    if (num_pdo == 0) {
        printf("No source PDOs reported. Ensure the adapter has completed PD negotiation.\n");
        return 0;
    }

    printf("Available PDO entries: %d\n", num_pdo);
    ap33772s_print_profiles(dev);
    return 0;
}

static void print_pdo_detail(ap33772s_ref dev, int index)
{
    struct ap33772s_pdo_info info;
    int ret = ap33772s_get_pdo_info(dev, index, &info);
    if (ret < 0) {
        fprintf(stderr, "Unable to decode PDO %d: %s\n", index, strerror(-ret));
        return;
    }
    if (!info.detected) {
        printf("PDO %d is not reported by the source.\n", index);
        return;
    }

    const char *domain = info.is_epr ? "EPR" : "SPR";
    const char *type = NULL;
    switch (info.supply) {
    case AP33772S_PDO_SUPPLY_FIXED:
        type = "Fixed";
        break;
    case AP33772S_PDO_SUPPLY_PPS:
        type = "PPS";
        break;
    case AP33772S_PDO_SUPPLY_AVS:
        type = "AVS";
        break;
    default:
        type = "Unknown";
        break;
    }

    printf("PDO %d (%s %s)\n", index, domain, type);
    printf("  Raw value     : 0x%04X\n", info.raw);
    printf("  Voltage max   : %d mV\n", info.voltage_max_mv);

    if (info.supply == AP33772S_PDO_SUPPLY_FIXED) {
        printf("  Voltage min   : %d mV\n", info.voltage_min_mv);
        printf("  Peak capability: %s\n", ap33772s_fixed_peak_current_desc(info.peak_current_code));
    } else {
        const char *min_desc = info.supply == AP33772S_PDO_SUPPLY_AVS ?
                               ap33772s_avs_voltage_min_desc(info.voltage_min_code) :
                               ap33772s_pps_voltage_min_desc(info.voltage_min_code);
        if (info.voltage_min_mv > 0) {
            printf("  Voltage min   : %d mV (%s)\n", info.voltage_min_mv, min_desc);
        } else {
            printf("  Voltage min   : %s\n", min_desc);
        }
    }

    printf("  Current range : %d-%d mA (code %u)\n", info.current_min_ma, info.current_max_ma,
           info.current_code);
}

static const char *decode_pd_result(uint8_t value)
{
    switch (value) {
    case 0x00:
        return "pending";
    case 0x01:
        return "accepted";
    case 0x02:
        return "rejected";
    case 0x03:
        return "failed";
    default:
        return "unknown";
    }
}

static void report_pd_request_result(ap33772s_ref dev)
{
    const int max_attempts = 20; // ~2 seconds
    uint8_t result = 0;
    int ret = 0;

    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        ret = ap33772s_read_pd_msg_result(dev, &result);
        if (ret < 0) {
            fprintf(stderr, "Failed to read PD_MSGRLT: %s\n", strerror(-ret));
            return;
        }
        if (result != 0x00) {
            break;
        }
        sleep_ms(100);
    }

    printf("  PD request %s (PD_MSGRLT=0x%02X)\n", decode_pd_result(result), result);

    if (result == 0x01) {
        int voltage = 0;
        if (ap33772s_read_vreq(dev, &voltage) == 0) {
            printf("  Negotiated voltage: %d mV\n", voltage);
        }
    }
}

static int handle_status(ap33772s_ref dev)
{
    uint8_t status = 0;
    int ret = ap33772s_read_status(dev, &status);
    if (ret < 0) {
        fprintf(stderr, "Failed to read status register: %s\n", strerror(-ret));
        return ret;
    }

    printf("Status flags: 0x%02X (", status);
    print_status_flags(status);
    printf(")\n");
    return 0;
}

static int handle_opmode(ap33772s_ref dev)
{
    uint8_t opmode = 0;
    int ret = ap33772s_read_opmode(dev, &opmode);
    if (ret < 0) {
        fprintf(stderr, "Failed to read operation mode register: %s\n", strerror(-ret));
        return ret;
    }

    printf("OPMODE flags: 0x%02X (", opmode);
    print_opmode_flags(opmode);
    printf(")\n");

    const char *cc_line = (opmode & (1u << 7)) ? "CC2" : "CC1 or unattached";
    const char *data_role = (opmode & (1u << 5)) ? "DFP" : "UFP";
    const char *source = (opmode & (1u << 1)) ? "PD source connected" :
                         (opmode & (1u << 0)) ? "Legacy source connected" :
                                                 "No source detected";
    const char *derating = (opmode & (1u << 6)) ? "active" : "normal";

    printf("  CC line   : %s\n", cc_line);
    printf("  Data role : %s\n", data_role);
    printf("  Source    : %s\n", source);
    printf("  De-rating : %s\n", derating);

    return 0;
}

static int handle_configure(ap33772s_ref dev, int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "configure requires <value>\n");
        return -EINVAL;
    }

    int value = 0;
    if (parse_int_arg(argv[1], &value) < 0 || value < 0 || value > 0xFF) {
        fprintf(stderr, "Invalid value for configure; must be 0-255\n");
        return -EINVAL;
    }

    int ret = ap33772s_write_pdconfig(dev, (uint8_t)value);
    if (ret < 0) {
        fprintf(stderr, "Failed to write PDCONFIG: %s\n", strerror(-ret));
        return ret;
    }

    printf("PDCONFIG set to 0x%02X\n", value & 0xFF);
    return 0;
}


static int handle_set(ap33772s_ref dev, int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "set: missing arguments\n");
        return -EINVAL;
    }

    const char *mode = argv[1];
    int ret = 0;

    if (strcmp(mode, "fixed") == 0) {
        if (argc < 4) {
            fprintf(stderr, "set fixed requires <index> <mA>\n");
            return -EINVAL;
        }
        int pdo_index = 0;
        int current_ma = 0;
        if ((ret = parse_int_arg(argv[2], &pdo_index)) < 0 ||
            (ret = parse_int_arg(argv[3], &current_ma)) < 0) {
            fprintf(stderr, "Invalid numeric argument for set fixed\n");
            return ret;
        }
        ret = ap33772s_request_fixed_pdo(dev, (uint8_t)pdo_index, current_ma);
        if (ret < 0) {
            fprintf(stderr, "Failed to request fixed PDO: %s\n", strerror(-ret));
            return ret;
        }
        printf("Requested fixed PDO %d with %d mA\n", pdo_index, current_ma);
    } else if (strcmp(mode, "pps") == 0 || strcmp(mode, "avs") == 0) {
        bool is_pps = (mode[0] == 'p');
        if (argc < 5) {
            fprintf(stderr, "set %s requires <index> <mV> <mA>\n", mode);
            return -EINVAL;
        }
        int pdo_index = 0;
        int voltage_mv = 0;
        int current_ma = 0;
        if ((ret = parse_int_arg(argv[2], &pdo_index)) < 0 ||
            (ret = parse_int_arg(argv[3], &voltage_mv)) < 0 ||
            (ret = parse_int_arg(argv[4], &current_ma)) < 0) {
            fprintf(stderr, "Invalid numeric argument for set %s\n", mode);
            return ret;
        }
        if (is_pps) {
            ret = ap33772s_request_pps(dev, (uint8_t)pdo_index, voltage_mv, current_ma);
            if (ret < 0) {
                fprintf(stderr, "Failed to request PPS PDO: %s\n", strerror(-ret));
                return ret;
            }
            printf("Requested PPS PDO %d with %d mV / %d mA\n", pdo_index, voltage_mv, current_ma);
        } else {
            ret = ap33772s_request_avs(dev, (uint8_t)pdo_index, voltage_mv, current_ma);
            if (ret < 0) {
                fprintf(stderr, "Failed to request AVS PDO: %s\n", strerror(-ret));
                return ret;
            }
            printf("Requested AVS PDO %d with %d mV / %d mA\n", pdo_index, voltage_mv, current_ma);
        }
    } else if (strcmp(mode, "max") == 0) {
        if (argc < 3) {
            fprintf(stderr, "set max requires <index>\n");
            return -EINVAL;
        }
        int pdo_index = 0;
        if ((ret = parse_int_arg(argv[2], &pdo_index)) < 0) {
            fprintf(stderr, "Invalid PDO index for set max\n");
            return ret;
        }
        ret = ap33772s_request_max_power(dev, (uint8_t)pdo_index);
        if (ret < 0) {
            fprintf(stderr, "Failed to request max power PDO %d: %s\n", pdo_index, strerror(-ret));
            return ret;
        }
        printf("Requested max power for PDO %d\n", pdo_index);
    } else if (strcmp(mode, "protect") == 0) {
        if (argc < 4) {
            fprintf(stderr, "set protect requires <dr|otp|ocp|ovp|uvp> <on|off>\n");
            return -EINVAL;
        }

        const char *feature = argv[2];
        const char *state = argv[3];
        bool enable = false;
        if (strcmp(state, "on") == 0 || strcmp(state, "1") == 0 || strcmp(state, "enable") == 0) {
            enable = true;
        } else if (strcmp(state, "off") == 0 || strcmp(state, "0") == 0 || strcmp(state, "disable") == 0) {
            enable = false;
        } else {
            fprintf(stderr, "set protect state must be on/off\n");
            return -EINVAL;
        }

        struct {
            const char *name;
            uint8_t mask;
            const char *label;
        } options[] = {
            {"dr",  AP33772S_CONFIG_DR_EN,  "De-rating"},
            {"otp", AP33772S_CONFIG_OTP_EN, "Over-temperature"},
            {"ocp", AP33772S_CONFIG_OCP_EN, "Over-current"},
            {"ovp", AP33772S_CONFIG_OVP_EN, "Over-voltage"},
            {"uvp", AP33772S_CONFIG_UVP_EN, "Under-voltage"},
        };

        uint8_t mask = 0;
        const char *label = NULL;
        for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); ++i) {
            if (strcmp(feature, options[i].name) == 0) {
                mask = options[i].mask;
                label = options[i].label;
                break;
            }
        }

        if (mask == 0) {
            fprintf(stderr, "Unknown protection feature '%s'\n", feature);
            return -EINVAL;
        }

        uint8_t config = 0;
        ret = ap33772s_read_config(dev, &config);
        if (ret < 0) {
            fprintf(stderr, "Failed to read CONFIG register: %s\n", strerror(-ret));
            return ret;
        }

        if (enable) {
            config |= mask;
        } else {
            config &= (uint8_t)~mask;
        }

        ret = ap33772s_write_config(dev, config);
        if (ret < 0) {
            fprintf(stderr, "Failed to write CONFIG register: %s\n", strerror(-ret));
            return ret;
        }

        printf("Protection %s %s (CONFIG=0x%02X)\n", label, enable ? "enabled" : "disabled", config);
    } else if (strcmp(mode, "output") == 0) {
        if (argc < 3) {
            fprintf(stderr, "set output requires <on|off>\n");
            return -EINVAL;
        }
        const char *state = argv[2];
        bool enable = false;
        if (strcmp(state, "on") == 0 || strcmp(state, "1") == 0) {
            enable = true;
        } else if (strcmp(state, "off") == 0 || strcmp(state, "0") == 0) {
            enable = false;
        } else {
            fprintf(stderr, "set output argument must be on/off\n");
            return -EINVAL;
        }
        ret = ap33772s_set_output(dev, enable);
        if (ret < 0) {
            fprintf(stderr, "Failed to change output state: %s\n", strerror(-ret));
            return ret;
        }
        printf("Output %s\n", enable ? "enabled" : "disabled");
    } else if (strcmp(mode, "hardreset") == 0) {
        ret = ap33772s_send_hard_reset(dev);
        if (ret < 0) {
            fprintf(stderr, "Failed to send hard reset: %s\n", strerror(-ret));
            return ret;
        }
        printf("Hard reset sent\n");
    } else if (strcmp(mode, "drswap") == 0) {
        ret = ap33772s_send_data_role_swap(dev);
        if (ret < 0) {
            fprintf(stderr, "Failed to send data role swap: %s\n", strerror(-ret));
            return ret;
        }
        printf("Data role swap sent\n");
    } else {
        fprintf(stderr, "Unknown set mode '%s'\n", mode);
        return -EINVAL;
    }

    //print_measurements(dev);
    return 0;
}

static int handle_monitor(ap33772s_ref dev, int interval_ms)
{
    struct sigaction previous_sa;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = monitor_sigint_handler;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, &previous_sa) == -1) {
        perror("sigaction");
    }

    monitor_interrupted = 0;

    printf("Monitoring status changes every %d ms (Ctrl+C to exit)\n", interval_ms);
    int ret = 0;

    while (!monitor_interrupted) {
        uint8_t status = 0;
        ret = ap33772s_read_status(dev, &status);
        if (ret < 0) {
            fprintf(stderr, "Monitor read failed: %s\n", strerror(-ret));
            break;
        }

        if (status != 0) {
            printf("[event] status=0x%02X (", status);
            print_status_flags(status);
            printf(")\n");

            if (status & (AP33772S_MASK_NEWPDO | AP33772S_MASK_READY)) {
                handle_caps(dev, "Source PDO capabilities:");
            }


            print_measurements(dev);
            fflush(stdout);
        }

        if (monitor_interrupted) {
            break;
        }

        sleep_ms(interval_ms);
    }

    if (monitor_interrupted) {
        printf("Monitor interrupted by user.\n");
        ret = 0;
    }

    if (sigaction(SIGINT, &previous_sa, NULL) == -1) {
        perror("sigaction");
    }

    return ret;
}

static int tokenize_line(char *line, char **argv, int max_tokens)
{
    int count = 0;
    char *save_ptr = NULL;
    char *token = strtok_r(line, "\t \r\n", &save_ptr);
    while (token && count < max_tokens) {
        argv[count++] = token;
        token = strtok_r(NULL, "\t \r\n", &save_ptr);
    }
    return count;
}

static void print_command_help(const char *prog)
{
    print_usage(prog);
}

enum command_result {
    CMD_RESULT_OK = 0,
    CMD_RESULT_ERROR = -1,
    CMD_RESULT_EXIT = 1
};

static int execute_command(ap33772s_ref dev, int argc, char **argv, int *monitor_interval_ms, const char *prog)
{
    if (argc == 0) {
        return CMD_RESULT_OK;
    }

    const char *cmd = argv[0];

    if (strcmp(cmd, "status") == 0) {
        int ret = handle_status(dev);
        return ret < 0 ? CMD_RESULT_ERROR : CMD_RESULT_OK;
    }

    if (strcmp(cmd, "opmode") == 0) {
        int ret = handle_opmode(dev);
        return ret < 0 ? CMD_RESULT_ERROR : CMD_RESULT_OK;
    }

    if (strcmp(cmd, "configure") == 0) {
        int ret = handle_configure(dev, argc, argv);
        return ret < 0 ? CMD_RESULT_ERROR : CMD_RESULT_OK;
    }

    if (strcmp(cmd, "pdo") == 0) {
        if (argc < 2) {
            fprintf(stderr, "pdo requires <index>\n");
            return CMD_RESULT_ERROR;
        }
        int index = 0;
        if (parse_int_arg(argv[1], &index) < 0 || index < 1 || index > AP33772S_MAX_PDO_ENTRIES) {
            fprintf(stderr, "pdo index must be between 1 and %d\n", AP33772S_MAX_PDO_ENTRIES);
            return CMD_RESULT_ERROR;
        }
        int ret = ap33772s_refresh_pdo(dev, index);
        if (ret < 0) {
            fprintf(stderr, "Failed to refresh PDO %d: %s\n", index, strerror(-ret));
            return CMD_RESULT_ERROR;
        }
        print_pdo_detail(dev, index);
        return CMD_RESULT_OK;
    }

    if (strcmp(cmd, "monitor") == 0) {
        int interval = *monitor_interval_ms;
        if (argc >= 2) {
            int value = 0;
            if (parse_int_arg(argv[1], &value) < 0 || value <= 0) {
                fprintf(stderr, "monitor: invalid interval\n");
                return CMD_RESULT_ERROR;
            }
            interval = value;
            *monitor_interval_ms = value;
        }

        bool resume_raw = raw_mode_active;
        if (resume_raw) {
            disable_raw_mode();
            printf("\n");
        }

        int ret = handle_monitor(dev, interval);

        if (resume_raw) {
            if (enable_raw_mode() < 0) {
                fprintf(stderr, "Failed to re-enable advanced line editing; switching to basic input.\n");
                raw_mode_active = false;
                return ret < 0 ? CMD_RESULT_ERROR : CMD_RESULT_OK;
            }
        }

        return ret < 0 ? CMD_RESULT_ERROR : CMD_RESULT_OK;
    }

    if (strcmp(cmd, "set") == 0) {
        int ret = handle_set(dev, argc, argv);
        if (ret == 0 && argc >= 2 &&
            (strcmp(argv[1], "fixed") == 0 || strcmp(argv[1], "pps") == 0 ||
             strcmp(argv[1], "avs") == 0 || strcmp(argv[1], "max") == 0)) {
            report_pd_request_result(dev);
        }
        return ret < 0 ? CMD_RESULT_ERROR : CMD_RESULT_OK;
    }

    if (strcmp(cmd, "readreg") == 0) {
        if (argc < 3) {
            fprintf(stderr, "readreg requires <reg> <len>\n");
            return CMD_RESULT_ERROR;
        }
        int reg = 0, len = 0;
        if (parse_int_arg(argv[1], &reg) < 0 || parse_int_arg(argv[2], &len) < 0 || reg < 0 || reg > 255 || len < 1 || len > 128) {
            fprintf(stderr, "Invalid arguments for readreg\n");
            return CMD_RESULT_ERROR;
        }
        uint8_t buf[128];
        int ret = ap33772s_read_bytes(dev, (uint8_t)reg, buf, (size_t)len);
        if (ret < 0) {
            fprintf(stderr, "Failed to read register: %s\n", strerror(-ret));
            return CMD_RESULT_ERROR;
        }
        printf("Register 0x%02X (%d bytes): ", reg, len);
        for (int i = 0; i < len; i++) {
            printf("%02X ", buf[i]);
        }
        printf("\n");
        return CMD_RESULT_OK;
    }

    if (strcmp(cmd, "dbg") == 0) {
        if (argc < 2) {
            fprintf(stderr, "dbg requires on/off\n");
            return CMD_RESULT_ERROR;
        }
        if (strcmp(argv[1], "on") == 0) {
            ap33772s_set_debug(true);
            printf("Debug enabled\n");
        } else if (strcmp(argv[1], "off") == 0) {
            ap33772s_set_debug(false);
            printf("Debug disabled\n");
        } else {
            fprintf(stderr, "dbg argument must be on/off\n");
            return CMD_RESULT_ERROR;
        }
        return CMD_RESULT_OK;
    }

    if (strcmp(cmd, "caps") == 0) {
        int ret = handle_caps(dev, "Source PDO capabilities:");
        return ret < 0 ? CMD_RESULT_ERROR : CMD_RESULT_OK;
    }

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
        print_command_help(prog);
        return CMD_RESULT_OK;
    }

    if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
        return CMD_RESULT_EXIT;
    }

    fprintf(stderr, "Unknown command '%s'\n", cmd);
    return CMD_RESULT_ERROR;
}

static int interactive_loop_basic(ap33772s_ref dev, int *monitor_interval_ms, const char *prog)
{
    char line[COMMAND_BUFFER_SIZE];
    char *argv[16];

    while (true) {
        printf("ap33772s> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        int argc = tokenize_line(line, argv, (int)(sizeof(argv) / sizeof(argv[0])));
        int result = execute_command(dev, argc, argv, monitor_interval_ms, prog);
        if (result == CMD_RESULT_EXIT) {
            break;
        }
    }

    return 0;
}


static int enable_raw_mode(void)
{
    if (!isatty(STDIN_FILENO)) {
        return -1;
    }

    if (tcgetattr(STDIN_FILENO, &original_termios) == -1) {
        return -1;
    }

    struct termios raw = original_termios;
    raw.c_lflag &= ~(ICANON | ECHO | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        return -1;
    }

    raw_mode_active = true;
    return 0;
}

static void disable_raw_mode(void)
{
    if (raw_mode_active) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
        raw_mode_active = false;
    }
}

static void render_prompt(const char *prompt, const char *buffer, size_t len, size_t *last_render_len)
{
    size_t prompt_len = strlen(prompt);
    const char carriage_return = '\r';
    const char clear_seq[] = "\033[K";

    write(STDOUT_FILENO, &carriage_return, 1);
    write(STDOUT_FILENO, prompt, prompt_len);
    if (len > 0) {
        write(STDOUT_FILENO, buffer, len);
    }

    write(STDOUT_FILENO, clear_seq, sizeof(clear_seq) - 1);

    if (last_render_len) {
        *last_render_len = prompt_len + len;
    }
    fflush(stdout);
}

static int interactive_loop(ap33772s_ref dev, int *monitor_interval_ms, const char *prog)
{
    printf("Interactive mode. Type 'help' for commands, 'exit' to quit.\n");

    if (enable_raw_mode() < 0) {
        fprintf(stderr, "Note: advanced line editing unavailable; using basic input.\n");
        return interactive_loop_basic(dev, monitor_interval_ms, prog);
    }

    const char *prompt = "ap33772s> ";
    char buffer[COMMAND_BUFFER_SIZE] = {0};
    size_t len = 0;
    size_t last_render_len = 0;

    char *history[HISTORY_MAX_ENTRIES] = {0};
    size_t history_len = 0;
    size_t history_index = 0;
    char saved_buffer[COMMAND_BUFFER_SIZE] = {0};
    bool saved_valid = false;

    char *argv[16];

    render_prompt(prompt, buffer, len, &last_render_len);

    while (true) {
        unsigned char c = 0;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) {
            printf("\n");
            break;
        }

        if (c == '\r' || c == '\n') {
            write(STDOUT_FILENO, "\r\n", 2);
            buffer[len] = '\0';

            char line_copy[COMMAND_BUFFER_SIZE];
            memcpy(line_copy, buffer, len + 1);
            int argc = tokenize_line(line_copy, argv, (int)(sizeof(argv) / sizeof(argv[0])));

            if (argc > 0) {
                if (history_len == 0 || strcmp(history[history_len - 1], buffer) != 0) {
                    if (history_len == HISTORY_MAX_ENTRIES) {
                        free(history[0]);
                        memmove(history, history + 1, sizeof(char *) * (HISTORY_MAX_ENTRIES - 1));
                        history_len--;
                    }
                    history[history_len++] = strdup(buffer);
                }
            }

            history_index = history_len;
            saved_valid = false;

            int result = execute_command(dev, argc, argv, monitor_interval_ms, prog);
            if (result == CMD_RESULT_EXIT) {
                for (size_t i = 0; i < history_len; ++i) {
                    free(history[i]);
                }
                disable_raw_mode();
                return 0;
            }

            len = 0;
            buffer[0] = '\0';
            last_render_len = 0;
            render_prompt(prompt, buffer, len, &last_render_len);
            continue;
        }

        if (c == 3) {
            printf("^C\n");
            len = 0;
            buffer[0] = '\0';
            history_index = history_len;
            saved_valid = false;
            last_render_len = 0;
            render_prompt(prompt, buffer, len, &last_render_len);
            continue;
        }

        if (c == 4) {
            if (len == 0) {
                printf("\n");
                break;
            }
            continue;
        }

        if (c == 127 || c == 8) {
            if (len > 0) {
                len--;
                buffer[len] = '\0';
                last_render_len = 0;
                render_prompt(prompt, buffer, len, &last_render_len);
            }
            continue;
        }

        if (c == 27) {
            unsigned char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) <= 0) {
                continue;
            }
            if (read(STDIN_FILENO, &seq[1], 1) <= 0) {
                continue;
            }
            if (seq[0] == '[') {
                if (seq[1] == 'A') {
                    if (history_len > 0) {
                        if (history_index == history_len && !saved_valid) {
                            memcpy(saved_buffer, buffer, len + 1);
                            saved_valid = true;
                        }
                        if (history_index > 0) {
                            history_index--;
                            strncpy(buffer, history[history_index], COMMAND_BUFFER_SIZE - 1);
                            buffer[COMMAND_BUFFER_SIZE - 1] = '\0';
                            len = strlen(buffer);
                            last_render_len = 0;
                            render_prompt(prompt, buffer, len, &last_render_len);
                        }
                    }
                } else if (seq[1] == 'B') {
                    if (history_len > 0 && history_index < history_len) {
                        history_index++;
                        if (history_index == history_len) {
                            if (saved_valid) {
                                strncpy(buffer, saved_buffer, COMMAND_BUFFER_SIZE - 1);
                                buffer[COMMAND_BUFFER_SIZE - 1] = '\0';
                                len = strlen(buffer);
                            } else {
                                buffer[0] = '\0';
                                len = 0;
                            }
                        } else {
                            strncpy(buffer, history[history_index], COMMAND_BUFFER_SIZE - 1);
                            buffer[COMMAND_BUFFER_SIZE - 1] = '\0';
                            len = strlen(buffer);
                        }
                        last_render_len = 0;
                        render_prompt(prompt, buffer, len, &last_render_len);
                    }
                }
            }
            continue;
        }

        if (c >= 32 && c < 127) {
            if (len + 1 < COMMAND_BUFFER_SIZE) {
                buffer[len++] = (char)c;
                buffer[len] = '\0';
                last_render_len = 0;
                render_prompt(prompt, buffer, len, &last_render_len);
            }
            continue;
        }
    }

    for (size_t i = 0; i < history_len; ++i) {
        free(history[i]);
    }
    disable_raw_mode();
    return 0;
}

int main(int argc, char **argv)
{
    const char *device_path = DEFAULT_DEVICE_PATH;
    int monitor_interval_ms = DEFAULT_MONITOR_INTERVAL_MS;

    int argi = 1;
    while (argi < argc && strncmp(argv[argi], "--", 2) == 0) {
        const char *opt = argv[argi++];
        if (strcmp(opt, "--device") == 0) {
            if (argi >= argc) {
                fprintf(stderr, "--device requires a path\n");
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
            device_path = argv[argi++];
        } else if (strcmp(opt, "--interval") == 0) {
            if (argi >= argc) {
                fprintf(stderr, "--interval requires a value\n");
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
            if (parse_int_arg(argv[argi++], &monitor_interval_ms) < 0 || monitor_interval_ms <= 0) {
                fprintf(stderr, "Invalid --interval value\n");
                return EXIT_FAILURE;
            }
        } else if (strcmp(opt, "--help") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        } else {
            fprintf(stderr, "Unknown option '%s'\n", opt);
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    int remaining = argc - argi;

    int fd = open(device_path, O_RDWR);
    if (fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    if (ioctl(fd, I2C_SLAVE, AP33772S_ADDRESS) < 0) {
        perror("ioctl I2C_SLAVE");
        close(fd);
        return EXIT_FAILURE;
    }

    struct ap33772s_bus_delegate delegate = {
        .read = ap33772s_bus_read,
        .write = ap33772s_bus_write,
        .delay_us = ap33772s_delay_us,
        .ctx = &fd,
    };

    ap33772s_ref device = ap33772s_init(&delegate);
    if (!device) {
        perror("ap33772s_init");
        close(fd);
        return EXIT_FAILURE;
    }

    int exit_code = EXIT_SUCCESS;
    if (remaining == 0) {
        interactive_loop(device, &monitor_interval_ms, argv[0]);
    } else {
        int result = execute_command(device, remaining, &argv[argi], &monitor_interval_ms, argv[0]);
        if (result == CMD_RESULT_ERROR) {
            exit_code = EXIT_FAILURE;
        }
    }

    ap33772s_destroy(device);
    close(fd);
    return exit_code;
}
