#ifndef __LOGICANALYZER_BOARD_SETTINGS__

    #define __LOGICANALYZER_BOARD_SETTINGS__

    #include "pico/stdlib.h"
    #include "LogicAnalyzer_Build_Settings.h"

    //Board definitions

    //This defines the name sent to the software
    //#define BOARD_NAME "PICO"
    //If defined the device supports complex, fast and external triggers
    //#define SUPPORTS_COMPLEX_TRIGGER
    //Stablishes the channel base GPIO
    //#define INPUT_PIN_BASE 2
    //Complex/fast/ext trigger output pin
    //#define COMPLEX_TRIGGER_OUT_PIN 0
    //Complex/fast/ext trigger input pin
    //#define COMPLEX_TRIGGER_IN_PIN 1
    //If defined, the onboard led is a led connected to a GPIO
    //#define GPIO_LED
    //If defined, the onboard led is a led connected to a CYGW module (for the Pico W)
    //#define CYGW_LED
    //If defined, the onboard led is a RGB led connected to a GPIO
    //#define WS2812_LED
    //Defines the used GPIO used for the GPIO and WS2812 led types
    //#define LED_IO 25
    //If defined enables the Pico W WiFi module
    //#define USE_CYGW_WIFI

    #if defined (BUILD_PICO)

        #define BOARD_NAME "PICO"
        #define SUPPORTS_COMPLEX_TRIGGER
        #define INPUT_PIN_BASE 2
        #define COMPLEX_TRIGGER_OUT_PIN 0
        #define COMPLEX_TRIGGER_IN_PIN 1
        #define GPIO_LED
        //#define CYGW_LED
        //#define WS2812_LED
        #define LED_IO 25
        //#define USE_CYGW_WIFI

    #elif defined (BUILD_PICO_W)

        #define BOARD_NAME "W"
        #define SUPPORTS_COMPLEX_TRIGGER
        #define INPUT_PIN_BASE 2
        #define COMPLEX_TRIGGER_OUT_PIN 0
        #define COMPLEX_TRIGGER_IN_PIN 1
        //#define GPIO_LED
        #define CYGW_LED
        //#define WS2812_LED
        //#define LED_IO 25
        //#define USE_CYGW_WIFI

    #elif defined (BUILD_PICO_W_WIFI)

        #define BOARD_NAME "WIFI"
        #define SUPPORTS_COMPLEX_TRIGGER
        #define INPUT_PIN_BASE 2
        #define COMPLEX_TRIGGER_OUT_PIN 0
        #define COMPLEX_TRIGGER_IN_PIN 1
        //#define GPIO_LED
        #define CYGW_LED
        //#define WS2812_LED
        //#define LED_IO 25
        #define USE_CYGW_WIFI

    #elif defined (BUILD_ZERO)

        #define BOARD_NAME "ZERO"
        #define SUPPORTS_COMPLEX_TRIGGER
        #define INPUT_PIN_BASE 0
        #define COMPLEX_TRIGGER_OUT_PIN 17
        #define COMPLEX_TRIGGER_IN_PIN 18
        //#define GPIO_LED
        //#define CYGW_LED
        #define WS2812_LED
        #define LED_IO 16
        //#define USE_CYGW_WIFI

    #elif defined (BUILD_BUSPIRATE5)

        #define BOARD_NAME "BP5"
        //#define SUPPORTS_COMPLEX_TRIGGER
        #define INPUT_PIN_BASE 8
        //#define COMPLEX_TRIGGER_OUT_PIN 17
        //#define COMPLEX_TRIGGER_IN_PIN 18
        //#define GPIO_LED
        //#define CYGW_LED
        //#define WS2812_LED
        //#define LED_IO RGB_CDO
        //#define USE_CYGW_WIFI

    #endif

#endif