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
    if(request->debug) printf("[LED] Performing transaction\r\n");
    
    uint32_t bytes_written = 0;
    uint32_t device = hwled_mode_config.device;
    
    // Handle START condition using device-specific function
    if(request->start_main || request->start_alt) {
        if(request->debug) printf("[LED] START\r\n");
        led_devices[device].start();
    }
    
    // Handle WRITE using device-specific function
    if(request->bytes_write > 0) {
        const uint8_t *data = (const uint8_t *)data_write;
        
        if(request->debug) printf("[LED] Writing %d bytes\r\n", request->bytes_write);
        
        // All LED devices require 32-bit packed values:
        // WS2812/ONBOARD: 3 bytes per pixel (RGB) - packed in lower 24 bits
        // APA102: 4 bytes per pixel (brightness + RGB) - packed in full 32 bits
        uint32_t bytes_per_pixel = (device == M_LED_APA102) ? 4 : 3;
        uint32_t i = 0;
        
        while(i < request->bytes_write) {
            uint32_t color = 0;
            // Pack bytes into 32-bit value
            for(uint32_t j = 0; j < bytes_per_pixel && i < request->bytes_write; j++) {
                color |= ((uint32_t)data[i++] << (8 * ((bytes_per_pixel-1) - j)));
            }
            led_devices[device].write(color);
            bytes_written += bytes_per_pixel;
        }
    }
    
    // Handle STOP condition using device-specific function
    if(request->stop_main || request->stop_alt) {
        if(request->debug) printf("[LED] STOP\r\n");
        led_devices[device].stop();
    }
    
    return bytes_written;
}
