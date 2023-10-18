#ifndef _TRANSLATION_BASE
#define _TRANSLATION_BASE
void translation_init(void);
void translation_set(uint32_t language);

extern char **t;

enum T_translations{
	T_ON=0,
	T_OFF,
	T_GND,
	T_INPUT,
	T_OUTPUT,
	T_EXIT,
	T_LOADED,
	T_SAVED,
	T_USE_PREVIOUS_SETTINGS,
	T_MODE_ERROR_NO_EFFECT,
	T_MODE_ERROR_NO_EFFECT_HIZ,
	T_MODE_NO_HELP_AVAILABLE,
	T_PRESS_ANY_KEY_TO_EXIT,
	T_MODE_MODE_SELECTION,
	T_MODE_MODE,
	T_MODE_NUMBER_DISPLAY_FORMAT,
	T_MODE_INVALID_OPTION,
	T_MODE_CHOOSE_AVAILABLE_PIN,
	T_MODE_ALL_PINS_IN_USE,
	T_MODE_PULLUP_RESISTORS,
	T_MODE_POWER_SUPPLY,
	T_MODE_DISABLED,
	T_MODE_ENABLED,
	T_MODE_BITORDER,
	T_MODE_BITORDER_MSB,
	T_MODE_BITORDER_LSB,
	T_MODE_DELAY,
	T_MODE_US,
	T_MODE_MS,
	T_MODE_ADC_VOLTAGE,
    T_MODE_ERROR_PARSING_MACRO,
    //FREQ
	T_MODE_PWM_GENERATE_FREQUENCY,
	T_MODE_FREQ_MEASURE_FREQUENCY,
	T_MODE_FREQ_FREQUENCY,
	T_MODE_FREQ_DUTY_CYCLE,
    //POWER SUPPLY
    T_PSU_DAC_ERROR,
    T_PSU_CURRENT_LIMIT_ERROR,
    T_PSU_SHORT_ERROR,
    T_PSU_ALREADY_DISABLED_ERROR,
	//SYNTAX
	T_SYNTAX_EXCEEDS_MAX_SLOTS,
	// SPI
	T_HWSPI_SPEED_MENU,
	T_HWSPI_SPEED_MENU_1,
	T_HWSPI_SPEED_PROMPT,
	T_HWSPI_BITS_MENU,
	T_HWSPI_BITS_MENU_1,
	T_HWSPI_BITS_PROMPT,
	T_HWSPI_CLOCK_POLARITY_MENU,
	T_HWSPI_CLOCK_POLARITY_MENU_1,
	T_HWSPI_CLOCK_POLARITY_MENU_2,
	T_HWSPI_CLOCK_POLARITY_PROMPT,
	T_HWSPI_CLOCK_PHASE_MENU,
	T_HWSPI_CLOCK_PHASE_MENU_1,
	T_HWSPI_CLOCK_PHASE_MENU_2,
	T_HWSPI_CLOCK_PHASE_PROMPT,
	T_HWSPI_CS_IDLE_MENU,
	T_HWSPI_CS_IDLE_MENU_1,
	T_HWSPI_CS_IDLE_MENU_2,
	T_HWSPI_CS_IDLE_PROMPT,
	T_HWSPI_ACTUAL_SPEED_KHZ,
	T_HWSPI_CS_SELECT,
	T_HWSPI_CS_DESELECT,
	// UART
	T_UART_SPEED_MENU,
	T_UART_SPEED_MENU_1,
	T_UART_SPEED_PROMPT,
	T_UART_PARITY_MENU,
	T_UART_PARITY_MENU_1,
	T_UART_PARITY_MENU_2,
	T_UART_PARITY_MENU_3,
	T_UART_PARITY_PROMPT,
	T_UART_DATA_BITS_MENU,
	T_UART_DATA_BITS_MENU_1,
	T_UART_DATA_BITS_PROMPT,
	T_UART_STOP_BITS_MENU,
	T_UART_STOP_BITS_MENU_1,
	T_UART_STOP_BITS_MENU_2,
	T_UART_STOP_BITS_PROMPT,
	T_UART_BLOCKING_MENU,
	T_UART_BLOCKING_MENU_1,
	T_UART_BLOCKING_MENU_2,
	T_UART_BLOCKING_PROMPT,
	T_UART_ACTUAL_SPEED_BAUD,
	T_UART_BAUD,
	// I2C
	T_HWI2C_SPEED_MENU,
	T_HWI2C_SPEED_MENU_1,
	T_HWI2C_SPEED_PROMPT,
	T_HWI2C_DATA_BITS_MENU,
	T_HWI2C_DATA_BITS_MENU_1,
	T_HWI2C_DATA_BITS_MENU_2,
	T_HWI2C_DATA_BITS_PROMPT,
	T_HWI2C_START,
	T_HWI2C_STOP,
	T_HWI2C_ACK,
	T_HWI2C_NACK,
	T_HWI2C_NO_PULLUP_DETECTED,
	T_HWI2C_TIMEOUT,
	T_HWI2C_I2C_ERROR,
	//T_HWI2C_
	// LEDs
	T_HWLED_DEVICE_MENU,
	T_HWLED_DEVICE_MENU_1,
	T_HWLED_DEVICE_MENU_2,
    T_HWLED_DEVICE_MENU_3,
	T_HWLED_DEVICE_PROMPT,
	T_HWLED_NUM_LEDS_MENU,
	T_HWLED_NUM_LEDS_MENU_1,
	T_HWLED_NUM_LEDS_PROMPT,
	T_HWLED_RESET,
	T_HWLED_FRAME_START,
	T_HWLED_FRAME_STOP,
	//HELP
	T_HELP_TITLE,
	T_HELP_BLANK,
	T_HELP_1_2,
	T_HELP_1_3,
	T_HELP_1_4,
	T_HELP_1_5,
	T_HELP_1_6,
	T_HELP_1_7,
	T_HELP_1_8,
	T_HELP_1_9,
	T_HELP_1_22,
	T_HELP_1_10,
	T_HELP_1_11,
	T_HELP_1_23,
	T_HELP_1_12,
	T_HELP_1_13,
	T_HELP_1_14,
	T_HELP_1_15,
	T_HELP_1_16,
	T_HELP_1_17,
	T_HELP_1_18,
	T_HELP_1_19,
	T_HELP_1_20,
	T_HELP_1_21,
	T_HELP_2_1,
	T_HELP_2_3,
	T_HELP_2_4,
	T_HELP_2_5,
	T_HELP_2_6,
	T_HELP_2_7,
	T_HELP_2_8,
	T_HELP_2_9,
	T_HELP_2_10,
	T_HELP_2_11,
	T_HELP_2_12,
	T_HELP_2_13,
	T_HELP_2_14,
	T_HELP_2_15,
	T_HELP_2_16,
	T_HELP_2_17,
	T_HELP_2_18,
	T_HELP_2_19,
	T_HELP_2_20,
	T_HELP_2_21,
	T_HELP_2_22,
	T_INFO_FIRMWARE,
	T_INFO_BOOTLOADER,
	T_INFO_WITH,
	T_INFO_RAM,
	T_INFO_FLASH,
	T_INFO_SN,
	T_INFO_WEBSITE,
	T_INFO_SD_CARD,
	T_INFO_FILE_SYSTEM,
	T_NOT_DETECTED,
	T_INFO_AVAILABLE_MODES,
	T_INFO_CURRENT_MODE,
	T_INFO_POWER_SUPPLY,
	T_INFO_CURRENT_LIMIT,
	T_INFO_PULLUP_RESISTORS,
	T_INFO_FREQUENCY_GENERATORS,
	T_INFO_DISPLAY_FORMAT,
	T_INFO_DATA_FORMAT,
	T_INFO_BITS,
	T_INFO_BITORDER,
    // CONFIG
	T_CONFIG_FILE,
	T_CONFIG_CONFIGURATION_OPTIONS,
	T_CONFIG_LANGUAGE,
	T_CONFIG_ANSI_COLOR_MODE,
	T_CONFIG_ANSI_TOOLBAR_MODE,
	T_CONFIG_LANGUAGE_ENGLISH,
	T_CONFIG_LANGUAGE_CHINESE,
	T_CONFIG_DISABLE,
	T_CONFIG_ENABLE,
    T_CONFIG_SCREENSAVER,
    T_CONFIG_SCREENSAVER_5,
    T_CONFIG_SCREENSAVER_10,
    T_CONFIG_SCREENSAVER_15,
    T_CONFIG_LEDS_EFFECT,
    T_CONFIG_LEDS_EFFECT_SOLID,
    T_CONFIG_LEDS_EFFECT_ANGLEWIPE,
    T_CONFIG_LEDS_EFFECT_CENTERWIPE,
    T_CONFIG_LEDS_EFFECT_CLOCKWISEWIPE,
    T_CONFIG_LEDS_EFFECT_TOPDOWNWIPE,
    T_CONFIG_LEDS_EFFECT_SCANNER,
	T_CONFIG_LEDS_EFFECT_CYCLE,
    T_CONFIG_LEDS_COLOR,
    T_CONFIG_LEDS_COLOR_RAINBOW,
    T_CONFIG_LEDS_COLOR_RED,
    T_CONFIG_LEDS_COLOR_ORANGE,
    T_CONFIG_LEDS_COLOR_YELLOW,
    T_CONFIG_LEDS_COLOR_GREEN,
    T_CONFIG_LEDS_COLOR_BLUE,
    T_CONFIG_LEDS_COLOR_PURPLE,
    T_CONFIG_LEDS_COLOR_PINK,
    T_CONFIG_LEDS_BRIGHTNESS,
    T_CONFIG_LEDS_BRIGHTNESS_10,
    T_CONFIG_LEDS_BRIGHTNESS_20,
    T_CONFIG_LEDS_BRIGHTNESS_30,
    T_CONFIG_LEDS_BRIGHTNESS_40,
    T_CONFIG_LEDS_BRIGHTNESS_50,
    T_CONFIG_LEDS_BRIGHTNESS_100,
	T_LAST_ITEM_ALWAYS_AT_THE_END //LEAVE THIS ITEM AT THE END!!! It helps the compiler report errors if there are missing translations
};



//TODO: rename help to friendly names, define commands somewhere outside the code
#if(0)
#define T_HELP_CONVERT "text"
#define BP_CMD_CONVERT "=X/|X"
#define BP_CMD_REVERSE "~"
	{0,"=X/|X",T_HELP_1_2},	
	{0,"~", T_HELP_1_3},
	{0,"#", T_HELP_1_4},
	{0,"$", T_HELP_1_5},
	{0,"&/%", T_HELP_1_6},
	{0,"a/A/@.x", T_HELP_1_7},
	{0,"b", T_HELP_1_8},
	{0,"c/C", T_HELP_1_9},
	{0,"v.x/V.x", T_HELP_1_22},
	{0,"v/V", T_HELP_1_10},
	{0,"f.x/F.x", T_HELP_1_11},
	{0,"f/F", T_HELP_1_23},
	{0,"g.x/G", T_HELP_1_12},
	{0,"h/H/?", T_HELP_1_13},
	{0,"i", T_HELP_1_14},
	{0,"l/L", T_HELP_1_15},
	{0,"m", T_HELP_1_16},
	{0,"o", T_HELP_1_17},
	{0,"p/P", T_HELP_1_18},
	{0,"q", T_HELP_1_19},
	{0,"s", T_HELP_1_20},
	{0,"w/W", T_HELP_1_21}
#endif


#endif