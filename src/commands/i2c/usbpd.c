#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include <string.h>
#include "pirate.h"
#include "ui/ui_term.h"
#include "command_struct.h"
#include "ui/ui_help.h"
#include "lib/bp_args/bp_cmd.h"
#include "binmode/fala.h"
#include "pirate/hwi2c_pio.h"
#include "lib/ap33772s/ap33772s.h"
#include "lib/ap33772s/ap33772s_int.h"

enum usbpd_actions_enum {
    USBPD_STATUS=0,
    USBPD_REQUEST,
    USBPD_RESET,
};

static const char* const usage[] = {
    "usbpd [status|request|reset]\r\n\t[-p <PDO index>] [-v <mV>] [-i <mA>] [-h(elp)]",
    "show USB PD status:%s usbpd status",
    "request a fixed voltage PDO profile:%s usbpd request -p 1",
    "request a PPS/AVS voltage profile:%s usbpd request -p 2 -v 9000 -i 1500",
    "send USB PD hard reset:%s usbpd reset"
};

static const bp_command_action_t usbpd_action_defs[] = {
    { USBPD_STATUS,  "status",  T_HELP_I2C_USBPD_STATUS },
    { USBPD_REQUEST, "request", T_HELP_I2C_USBPD_REQUEST },
    { USBPD_RESET,   "reset",   T_HELP_I2C_USBPD_RESET },
};

static const bp_command_opt_t usbpd_opts[] = {
    { "pdo",     'p', BP_ARG_REQUIRED, "index", T_HELP_I2C_USBPD_PDO_INDEX },
    { "voltage", 'v', BP_ARG_REQUIRED, "mV",    T_HELP_I2C_USBPD_VOLTAGE },
    { "current", 'i', BP_ARG_REQUIRED, "mA",    T_HELP_I2C_USBPD_CURRENT },
    { 0 }
};

const bp_command_def_t usbpd_def = {
    .name         = "usbpd",
    .description  = T_HELP_I2C_USBPD,
    .actions      = usbpd_action_defs,
    .action_count = count_of(usbpd_action_defs),
    .opts         = usbpd_opts,
    .usage        = usage,
    .usage_count  = count_of(usage),
};

static void print_measurements(ap33772s_ref dev)
{
    int value = 0;
    if (ap33772s_read_voltage(dev, &value) == 0) {
        printf("VBUS voltage: %d mV\r\n", value);
    } 
    if (ap33772s_read_current(dev, &value) == 0) {
        printf("VBUS current: %d mA\r\n", value);
    }
    if (ap33772s_read_vreq(dev, &value) == 0) {
        printf("Requested V : %d mV\r\n", value);
    }      
    if (ap33772s_read_ireq(dev, &value) == 0) {
        printf("Requested I : %d mA\r\n", value);
    }    
    if (ap33772s_read_temperature(dev, &value) == 0) {
        printf("Temperature : %d C\r\n", value);
    }
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
            printf("Failed to read PD_MSGRLT\r\n");
            return;
        }
        if (result != 0x00) {
            break;
        }
        busy_wait_ms(100);
    }

    printf("PD request %s (PD_MSGRLT=0x%02X)\r\n", decode_pd_result(result), result);

    if (result == 0x01) {
        int voltage = 0;
        if (ap33772s_read_vreq(dev, &voltage) == 0) {
            printf("Negotiated voltage: %d mV\r\n", voltage);
        }
    }
}

static int handle_status(ap33772s_ref dev)
{
    uint8_t status = 0;
    int ret = ap33772s_read_status(dev, &status);
    if (ret < 0) {
        fprintf(stderr, "Failed to read status register: %s\r\n", strerror(-ret));
        return ret;
    }

    printf("Status flags: 0x%02X (", status);
    print_status_flags(status);
    printf(")\r\n");
    return 0;
}

static int handle_opmode(ap33772s_ref dev)
{
    uint8_t opmode = 0;
    int ret = ap33772s_read_opmode(dev, &opmode);
    if (ret < 0) {
        fprintf(stderr, "Failed to read operation mode register: %s\r\n", strerror(-ret));
        return ret;
    }

    //printf("OPMODE flags: 0x%02X (", opmode);
    //print_opmode_flags(opmode);
    //printf(")\r\n");

    const char *cc_line = (opmode & (1u << 7)) ? "CC2" : "CC1 or unattached";
    const char *data_role = (opmode & (1u << 5)) ? "DFP" : "UFP";
    const char *source = (opmode & (1u << 1)) ? "PD source connected" :
                         (opmode & (1u << 0)) ? "Legacy source connected" :
                                                 "No source detected";
    const char *derating = (opmode & (1u << 6)) ? "active" : "normal";

    printf("Source      : %s\r\n", source);
    printf("CC line     : %s\r\n", cc_line);
    printf("Data role   : %s\r\n", data_role);
    printf("De-rating   : %s\r\n", derating);

    return 0;
}
#if 0
static int handle_configure(ap33772s_ref dev, int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "configure requires <value>\r\n");
        return -EINVAL;
    }

    int value = 0;
    if (parse_int_arg(argv[1], &value) < 0 || value < 0 || value > 0xFF) {
        fprintf(stderr, "Invalid value for configure; must be 0-255\r\n");
        return -EINVAL;
    }

    int ret = ap33772s_write_pdconfig(dev, (uint8_t)value);
    if (ret < 0) {
        fprintf(stderr, "Failed to write PDCONFIG: %s\r\n", strerror(-ret));
        return ret;
    }

    printf("PDCONFIG set to 0x%02X\r\n", value & 0xFF);
    return 0;
}
#endif

void ap33772s_print_profile_custom(ap33772s_ref dev, int idx)
{
    struct ap33772s_pdo_info info;
    if (ap33772s_get_pdo_info(dev, idx + 1, &info) < 0 || !info.detected) {
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

    printf("PDO %d (%s %s): ", idx + 1, domain, type);

    if (info.supply == AP33772S_PDO_SUPPLY_FIXED) {
        printf("%d mV, %d-%d mA", info.voltage_max_mv, info.current_min_ma, info.current_max_ma);
        const char *peak = NULL;
        switch (info.peak_current_code & 0x3) {
            case 1:
                peak = "150% peak";
            case 2:
                peak = "200% peak";
            case 3:
                peak = "300% peak";
        }

        if (peak && peak[0] != '\0') {
            printf(", %s", peak);
        }
    } else {
        const char *min_desc = info.supply == AP33772S_PDO_SUPPLY_AVS ?
                               ap33772s_avs_voltage_min_desc(info.voltage_min_code) :
                               ap33772s_pps_voltage_min_desc(info.voltage_min_code);
        printf("%s to %d mV, %d-%d mA", min_desc, info.voltage_max_mv,
               info.current_min_ma, info.current_max_ma);
    }

    printf("\r\n");
}

static int handle_caps(ap33772s_ref dev)
{   
    int ret = ap33772s_get_power_capabilities(dev);
    if (ret < 0) {
        printf("Failed to refresh PDO list:\r\n");
        return ret;
    }

    int16_t num_pdo = ap33772s_get_num_pdo(dev);
    if (num_pdo < 0) {
        num_pdo = 0;
    }

    if (num_pdo == 0) {
        printf("No source PDOs reported. Ensure the adapter has completed PD negotiation.\r\n");
        return 0;
    }
    handle_opmode(dev);
    print_measurements(dev);
    
    printf("\r\nAvailable PDO entries: %d\r\n", num_pdo);
    //ap33772s_print_profiles(dev);
    for (int i = 0; i < dev->num_pdo && i < AP33772S_MAX_PDO_ENTRIES; ++i) {
        ap33772s_print_profile_custom(dev, i);
    }

    return 0;
}

int ap33772s_bus_read(void *ctx, uint8_t reg, uint8_t *data, size_t len){
    if(i2c_read_reg(AP33772S_ADDRESS<<1,(uint8_t[]){reg},1,data,len)){
        pio_i2c_stop_timeout(0xffffff);
        return -1;
    };
    return 0;
}
int ap33772s_bus_write(void *ctx, uint8_t reg, const uint8_t *data, size_t len){
    if(i2c_write_reg(AP33772S_ADDRESS<<1,(uint8_t[]){reg},1,data,len)){
        pio_i2c_stop_timeout(0xffffff);
        return -1;
    };
    return 0;
}
void ap33772s_delay_us(void *ctx, unsigned int usec){
    busy_wait_us(usec);
}

void usbpd_profiles_print(ap33772s_ref dev){
    if (!dev) {
        return;
    }
    for (int i = 0; i < dev->num_pdo && i < AP33772S_MAX_PDO_ENTRIES; ++i) {
        ap33772s_print_profile_custom(dev, i);
    }
}



void usbpd_handler(struct command_result* res) {
    if(bp_cmd_help_check(&usbpd_def, res->help_flag)) {
        return;
    }

    uint32_t action;
    if (!bp_cmd_get_action(&usbpd_def, &action)) {
        bp_cmd_help_show(&usbpd_def);
        return;
    }    

    const struct ap33772s_bus_delegate delegate = {
        .read = ap33772s_bus_read,
        .write = ap33772s_bus_write,
        .delay_us = ap33772s_delay_us,
        .ctx = 0,
    };    

    struct ap33772s dev_instance;
    struct ap33772s *dev = &dev_instance;
    dev->pps_index = -1;
    dev->avs_index = -1;
    dev->index_avs = -1;
    dev->bus = delegate;  

    //always refresh PDOs
    int ret = ap33772s_get_power_capabilities(dev);
    if (ret < 0) {
        printf("Failed to refresh PDO list\r\n");
        return;
    }

    int16_t num_pdo = ap33772s_get_num_pdo(dev);
    if (num_pdo < 0) {
        num_pdo = 0;
    }    

    // check which action to perform
    if(action == USBPD_REQUEST){
        //-p profile, -v mv, -i ma
        uint32_t pdo_index = 0;
        uint32_t req_mv = 0;
        uint32_t req_ma = 0;
        if(!bp_cmd_get_uint32(&usbpd_def, 'p', &pdo_index)){
            printf("Specify PDO index to request with -p <index>\r\n");
            return;
        }
        
        //validate index
        if(pdo_index < 1 || pdo_index > num_pdo){
            printf("Invalid PDO index specified: %d\r\n\r\n", pdo_index);
            usbpd_profiles_print(dev);
            return;
        }

        //get profile type
        struct ap33772s_pdo_info info;
        if (ap33772s_get_pdo_info(dev, pdo_index, &info) < 0 || !info.detected) {
            printf("Failed to get PDO %d info\r\n", pdo_index);
            return;
        }

        // need to specify voltage for non-fixed PDOs
        if(info.supply == AP33772S_PDO_SUPPLY_FIXED){
            //set voltage to max
            req_mv = info.voltage_max_mv;
        }else{
            if(!bp_cmd_get_uint32(&usbpd_def, 'v', &req_mv)){
                printf("Specify requested voltage in mV with -v <mV> for PPS/AVS PDOs\r\n");
                return;
            }
            //validate voltage
            if(req_mv < info.voltage_min_mv || req_mv > info.voltage_max_mv){
                printf("Requested voltage %d mV is out of range for PDO %d (%d mV - %d mV)\r\n", req_mv, pdo_index, info.voltage_min_mv, info.voltage_max_mv);
                return;
            }
        }

        //if specified, must be within limits, else max
        if(!bp_cmd_get_uint32(&usbpd_def, 'i', &req_ma)){
            //not specified, use max
            req_ma = info.current_max_ma;
        }else{
            //validate current
            if(req_ma < info.current_min_ma || req_ma > info.current_max_ma){
                printf("Requested current %d mA is out of range for PDO %d (%d mA - %d mA)\r\n", req_ma, pdo_index, info.current_min_ma, info.current_max_ma);
                return;
            }            
        }

        //perform request
        printf("Requesting PDO %d, %dmV @ %dmA\r\n", pdo_index, req_mv, req_ma);
        switch(info.supply){
            case AP33772S_PDO_SUPPLY_FIXED:
                ret = ap33772s_request_fixed_pdo(dev, pdo_index, req_ma);
                break;
            case AP33772S_PDO_SUPPLY_PPS:
                ret = ap33772s_request_pps(dev, pdo_index, req_mv, req_ma);
                break;
            case AP33772S_PDO_SUPPLY_AVS:
                ret = ap33772s_request_avs(dev, pdo_index, req_mv, req_ma);
                break;
            default:
                printf("Unsupported PDO supply type\r\n");
                return;
        }

        if (ret < 0) {
            printf("Failed to request PDO %d\r\n", pdo_index);
            return;
        }
        busy_wait_ms(2);
        report_pd_request_result(dev);
        printf("\r\n");
        print_measurements(dev);
        return;
    }

    //hard reset, check response
    if(action == USBPD_RESET){
        printf("Sending PD hard reset...\r\n");
        ret = ap33772s_send_hard_reset(dev);
        if (ret < 0) {
            printf("Failed to send PD hard reset\r\n");
            return;
        }
        printf("PD hard reset sent successfully\r\n");
        return;
    }

    //status
    handle_opmode(dev);
    print_measurements(dev);
    printf("\r\nAvailable PDO entries: %d\r\n", num_pdo);
    //ap33772s_print_profiles(dev);
    for (int i = 0; i < dev->num_pdo && i < AP33772S_MAX_PDO_ENTRIES; ++i) {
        ap33772s_print_profile_custom(dev, i);
    }



}
