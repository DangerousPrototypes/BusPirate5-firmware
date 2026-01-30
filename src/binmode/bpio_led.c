#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "bytecode.h"
#include "command_struct.h"
#include "bpio_reader.h"
#include "bpio_led.h"
#include "bpio_transactions.h"
#include "mode/hwled.h"
#include "pirate/rgb.h"
#include "ui/ui_format.h"
#include "hardware/pio.h"
#include "pio_config.h"

// External references from hwled.c
extern led_mode_config_t hwled_mode_config;
extern struct _pio_config pio_config;
extern const led_device_funcs led_devices[];

bool bpio_led_configure(bpio_mode_configuration_t *bpio_mode_config) {
    // Validate submode (0=WS2812, 1=APA102, 2=WS2812_ONBOARD)
    if(bpio_mode_config->submode > 2) {
        if(bpio_mode_config->debug) {
            printf("LED: Invalid submode %d (0=WS2812, 1=APA102, 2=ONBOARD)\r\n", 
                   bpio_mode_config->submode);
        }
        return false;
    }
    
    // Let hwled configure the device
    return hwled_bpio_configure(bpio_mode_config);
}

uint32_t bpio_led_transaction(struct bpio_data_request_t *request, flatbuffers_uint8_vec_t data_write, uint8_t *data_read) {
    uint32_t bytes_written = 0;
    uint32_t device = hwled_mode_config.device;
    
    if(request->debug) {
        const char* submode_names[] = {"WS2812", "APA102", "ONBOARD"};
        printf("[LED %s] Transaction: start=%d write=%d stop=%d\r\n",
               submode_names[device],
               request->start_main, request->bytes_write, request->stop_main);
    }
    
    // Handle START condition using device-specific function
    if(request->start_main) {
        led_devices[device].start();
        if(request->debug) {
            printf("  %s: Start\r\n", (device == M_LED_WS2812) ? "WS2812" : 
                                      (device == M_LED_APA102) ? "APA102" : "ONBOARD");
        }
    }
    
    // Handle WRITE using device-specific function
    if(request->bytes_write > 0) {
        const uint8_t *data = (const uint8_t *)data_write;
        uint32_t data_len = flatbuffers_uint8_vec_len(data_write);
        
        // Special handling for onboard RGB (needs 4-byte packed format)
        if(device == M_LED_WS2812_ONBOARD) {
            if(data_len >= 4) {
                uint32_t color = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
                led_devices[device].write(color);
                bytes_written = 4;
                if(request->debug) {
                    printf("  ONBOARD: Color=0x%08X\r\n", color);
                }
            } else if(request->debug) {
                printf("  ONBOARD: Need 4 bytes, got %d\r\n", data_len);
            }
        } else {
            // WS2812 and APA102: write bytes to PIO
            for(uint32_t i = 0; i < data_len && i < request->bytes_write; i++) {
                if(device == M_LED_WS2812) {
                    // WS2812: Shift data left by 8 bits
                    led_devices[device].write(data[i] << 8u);
                } else {
                    // APA102: Send data as-is
                    led_devices[device].write(data[i]);
                }
                bytes_written++;
            }
            if(request->debug) {
                printf("  %s: Wrote %d bytes (%d LEDs)\r\n",
                       (device == M_LED_WS2812) ? "WS2812" : "APA102",
                       bytes_written,
                       (device == M_LED_WS2812) ? bytes_written / 3 : bytes_written / 4);
            }
        }
    }
    
    // Handle STOP condition using device-specific function
    if(request->stop_main) {
        led_devices[device].stop();
        if(request->debug) {
            printf("  %s: Stop\r\n", (device == M_LED_WS2812) ? "WS2812" : 
                                      (device == M_LED_APA102) ? "APA102" : "ONBOARD");
        }
    }
    
    return bytes_written;
}
