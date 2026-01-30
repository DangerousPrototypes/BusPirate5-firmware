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
    
    if(request->debug) {
        const char* submode_names[] = {"WS2812", "APA102", "ONBOARD"};
        printf("[LED %s] Transaction: start=%d write=%d stop=%d\r\n",
               submode_names[hwled_mode_config.device],
               request->start_main, request->bytes_write, request->stop_main);
    }
    
    // Handle START condition
    if(request->start_main) {
        switch(hwled_mode_config.device) {
            case M_LED_WS2812:
                // WS2812: 65us reset at start
                hwled_wait_idle();
                if(request->debug) printf("  WS2812: Reset timing (65us)\r\n");
                break;
                
            case M_LED_APA102:
                // APA102: Send start frame (32 bits of 0)
                pio_sm_put_blocking(pio_config.pio, pio_config.sm, 0x00000000);
                if(request->debug) printf("  APA102: Start frame (0x00000000)\r\n");
                break;
                
            case M_LED_WS2812_ONBOARD:
                // Onboard: No special start needed
                if(request->debug) printf("  ONBOARD: Ready\r\n");
                break;
        }
    }
    
    // Handle WRITE
    if(request->bytes_write > 0) {
        const uint8_t *data = (const uint8_t *)data_write;
        uint32_t data_len = flatbuffers_uint8_vec_len(data_write);
        
        switch(hwled_mode_config.device) {
            case M_LED_WS2812:
                // WS2812: Send RGB bytes (24 bits per LED)
                for(uint32_t i = 0; i < data_len && i < request->bytes_write; i++) {
                    pio_sm_put_blocking(pio_config.pio, pio_config.sm, (data[i] << 8u));
                    bytes_written++;
                }
                if(request->debug) {
                    printf("  WS2812: Wrote %d bytes (%d LEDs)\r\n", 
                           bytes_written, bytes_written / 3);
                }
                break;
                
            case M_LED_APA102:
                // APA102: Send 32-bit frames (brightness + RGB)
                for(uint32_t i = 0; i < data_len && i < request->bytes_write; i++) {
                    pio_sm_put_blocking(pio_config.pio, pio_config.sm, data[i]);
                    bytes_written++;
                }
                if(request->debug) {
                    printf("  APA102: Wrote %d bytes (%d LEDs)\r\n",
                           bytes_written, bytes_written / 4);
                }
                break;
                
            case M_LED_WS2812_ONBOARD:
                // Onboard: Use rgb_put() for internal RGB LEDs
                // Data should be 4 bytes (combined 32-bit color)
                if(data_len >= 4) {
                    uint32_t color = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
                    rgb_put(color);
                    bytes_written = 4;
                    if(request->debug) {
                        printf("  ONBOARD: Color=0x%08X\r\n", color);
                    }
                } else if(request->debug) {
                    printf("  ONBOARD: Need 4 bytes, got %d\r\n", data_len);
                }
                break;
        }
    }
    
    // Handle STOP condition
    if(request->stop_main) {
        switch(hwled_mode_config.device) {
            case M_LED_WS2812:
                // WS2812: 65us reset at end
                hwled_wait_idle();
                if(request->debug) printf("  WS2812: Reset timing (65us)\r\n");
                break;
                
            case M_LED_APA102:
                // APA102: Send end frame (32 bits of 1)
                pio_sm_put_blocking(pio_config.pio, pio_config.sm, 0xFFFFFFFF);
                if(request->debug) printf("  APA102: End frame (0xFFFFFFFF)\r\n");
                break;
                
            case M_LED_WS2812_ONBOARD:
                // Onboard: No special stop needed
                if(request->debug) printf("  ONBOARD: Done\r\n");
                break;
        }
    }
    
    return bytes_written;
}
