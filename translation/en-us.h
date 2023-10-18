// en-us is our base language translation file
#include "translation/base.h"
static char const * const en_us[]={
	[T_ON]="ON",
	[T_OFF]="OFF",
	[T_GND]="GND",
	[T_INPUT]="INPUT",
	[T_OUTPUT]="OUTPUT",
	[T_EXIT]="Exit",
	[T_LOADED]="Loaded",
	[T_SAVED]="Saved",
	[T_USE_PREVIOUS_SETTINGS]="Use previous settings?",
	[T_MODE_ERROR_NO_EFFECT]="ERROR: command has no effect here",
	[T_MODE_ERROR_NO_EFFECT_HIZ]="Command has no effect in HiZ mode, press 'm' to choose a mode",
	[T_MODE_NO_HELP_AVAILABLE]="No help available",
	[T_PRESS_ANY_KEY_TO_EXIT]="Press any key to exit",
	[T_MODE_MODE_SELECTION]="Mode selection",
	[T_MODE_MODE]="Mode",
	[T_MODE_NUMBER_DISPLAY_FORMAT]="Number display format",
	[T_MODE_INVALID_OPTION]="Invalid option",
	[T_MODE_CHOOSE_AVAILABLE_PIN]="Choose available pin:",
	[T_MODE_ALL_PINS_IN_USE]="All pins in use",
	[T_MODE_PULLUP_RESISTORS]="Pull-up resistors",
	[T_MODE_POWER_SUPPLY]="Power supply",
	[T_MODE_DISABLED]="Disabled",
	[T_MODE_ENABLED]="Enabled",
	[T_MODE_BITORDER]="Bitorder",
	[T_MODE_BITORDER_MSB]="MSB",
	[T_MODE_BITORDER_LSB]="LSB",
	[T_MODE_DELAY]="Delay",
	[T_MODE_US]="us",
	[T_MODE_MS]="ms",
	[T_MODE_ADC_VOLTAGE]="Volts on",
	[T_MODE_ERROR_PARSING_MACRO]="Error parsing macro",
	//FREQ
	[T_MODE_PWM_GENERATE_FREQUENCY]="Generate frequency",
    [T_MODE_FREQ_MEASURE_FREQUENCY]="Frequency measurement",
    [T_MODE_FREQ_FREQUENCY]="Frequency",
	[T_MODE_FREQ_DUTY_CYCLE]="Duty cycle",
    //POWER SUPPLY
    [T_PSU_DAC_ERROR]="PSU DAC error, please run self-test",
    [T_PSU_CURRENT_LIMIT_ERROR]="Current over limit, power supply disabled",
    [T_PSU_SHORT_ERROR]="Potential short circuit, power supply disabled",
    [T_PSU_ALREADY_DISABLED_ERROR]="Power supply already disabled",
    //SYNTAX
	[T_SYNTAX_EXCEEDS_MAX_SLOTS]="Result exceeds available space (%d slots)",
	
	//SPI
	[T_HWSPI_SPEED_MENU]="SPI speed",
	[T_HWSPI_SPEED_MENU_1]="1 to 62500KHz",
	[T_HWSPI_SPEED_PROMPT]="KHz (%s%dKHz*%s)",
	[T_HWSPI_BITS_MENU]="Data bits",
	[T_HWSPI_BITS_MENU_1]="4 to 8 bits",
	[T_HWSPI_BITS_PROMPT]="Bits (%s%d*%s)",
	[T_HWSPI_CLOCK_POLARITY_MENU]="Clock polarity",
	[T_HWSPI_CLOCK_POLARITY_MENU_1]="Idle LOW",
	[T_HWSPI_CLOCK_POLARITY_MENU_2]="Idle HIGH",
	[T_HWSPI_CLOCK_POLARITY_PROMPT]="Polarity",
	[T_HWSPI_CLOCK_PHASE_MENU]="Clock phase",
	[T_HWSPI_CLOCK_PHASE_MENU_1]="LEADING edge",
	[T_HWSPI_CLOCK_PHASE_MENU_2]="TRAILING edge",		
	[T_HWSPI_CLOCK_PHASE_PROMPT]="Phase",
	[T_HWSPI_CS_IDLE_MENU]="Chip select",
	[T_HWSPI_CS_IDLE_MENU_1]="Active HIGH (CS)",
	[T_HWSPI_CS_IDLE_MENU_2]="Active LOW (/CS)",
	[T_HWSPI_CS_IDLE_PROMPT]="CS",
	[T_HWSPI_ACTUAL_SPEED_KHZ]="Actual speed",
	[T_HWSPI_CS_SELECT]="CS Enabled",
	[T_HWSPI_CS_DESELECT]="CS Disabled",
    //UART
	[T_UART_SPEED_MENU]="UART speed",
	[T_UART_SPEED_MENU_1]="1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200 etc",	
	[T_UART_SPEED_PROMPT]="Baud (%s%d*%s)",
	[T_UART_PARITY_MENU]="Parity",
	[T_UART_PARITY_MENU_1]="None",
	[T_UART_PARITY_MENU_2]="Even",
	[T_UART_PARITY_MENU_3]="Odd",
	[T_UART_PARITY_PROMPT]="Parity",
	[T_UART_DATA_BITS_MENU]="Data bits",
	[T_UART_DATA_BITS_MENU_1]="5 to 8 bits",
	[T_UART_DATA_BITS_PROMPT]="Bits (%s%d*%s)",
	[T_UART_STOP_BITS_MENU]="Stop bits",
	[T_UART_STOP_BITS_MENU_1]="1",
	[T_UART_STOP_BITS_MENU_2]="2",		
	[T_UART_STOP_BITS_PROMPT]="Bits",
	[T_UART_BLOCKING_MENU]="Use blocking functions?",
	[T_UART_BLOCKING_MENU_1]="No",
	[T_UART_BLOCKING_MENU_2]="Yes",		
	[T_UART_BLOCKING_PROMPT]="Block",
	[T_UART_ACTUAL_SPEED_BAUD]="Actual speed",
	[T_UART_BAUD]="baud",
    //I2C
	[T_HWI2C_SPEED_MENU]="I2C speed",
	[T_HWI2C_SPEED_MENU_1]="1KHz to 1000KHz",
	[T_HWI2C_SPEED_PROMPT]="KHz (%s%dKHz*%s)",
	[T_HWI2C_DATA_BITS_MENU]="Data bits",
	[T_HWI2C_DATA_BITS_MENU_1]="8",
	[T_HWI2C_DATA_BITS_MENU_2]="10",
	[T_HWI2C_DATA_BITS_PROMPT]="Bits",
	[T_HWI2C_START]="I2C START",
	[T_HWI2C_STOP]="I2C STOP",
	[T_HWI2C_ACK]="ACK",
	[T_HWI2C_NACK]="NACK",
	[T_HWI2C_NO_PULLUP_DETECTED]="no pull-up detected. Use P to enable onboard pull-up resistors",
	[T_HWI2C_TIMEOUT]="I2C timeout",
	[T_HWI2C_I2C_ERROR]="I2C bus error",
    //LEDs
	[T_HWLED_DEVICE_MENU]="LED type",
	[T_HWLED_DEVICE_MENU_1]="WS2812/SK6812/'NeoPixel' (single wire interface)",
	[T_HWLED_DEVICE_MENU_2]="APA102/SK9822 (clock and data interface)",
    [T_HWLED_DEVICE_MENU_3]="Onboard LEDs (16 SK6812s)",
	[T_HWLED_DEVICE_PROMPT]="Type",
	[T_HWLED_NUM_LEDS_MENU]="Number of LEDs in the strip",
	[T_HWLED_NUM_LEDS_MENU_1]="1 to 10000",
	[T_HWLED_NUM_LEDS_PROMPT]="LEDs (%s%d*%s)",
	[T_HWLED_RESET]="RESET",
	[T_HWLED_FRAME_START]="START FRAME (0x00000000)",
	[T_HWLED_FRAME_STOP]="STOP FRAME (0xFFFFFFFF)",
	//HELP
    [T_HELP_TITLE]="General Commands\t\t\tProtocol Interaction",
	[T_HELP_BLANK]="\t\t\t",
	[T_HELP_1_2]="Converts X/reverse X\t",
	[T_HELP_1_3]="Selftest\t\t",
	[T_HELP_1_4]="Reset the Bus Pirate\t",
	[T_HELP_1_5]="Jump to bootloader\t",
	[T_HELP_1_6]="Delay 1 us/MS\t\t",
	[T_HELP_1_7]="Set IO.x state (low/HI/READ)",
	[T_HELP_1_8]="-\t\t\t",
	[T_HELP_1_9]="Configuration menu\t",
	[T_HELP_1_22]="Show volts on IOx (once/CONT)",
	[T_HELP_1_10]="Show volts all IOs (once/CONT)",
	[T_HELP_1_11]="Measure freq on IOx (once/CONT)",
	[T_HELP_1_23]="Monitor freq (off/ON)\t",
	[T_HELP_1_12]="Generate frequency (off/ON)",
	[T_HELP_1_13]="Help (general/PROTOCOL)\t",
	[T_HELP_1_14]="Versioninfo/statusinfo\t",
	[T_HELP_1_15]="Bitorder (msb/LSB)\t",
	[T_HELP_1_16]="Change mode\t\t",
	[T_HELP_1_17]="Set number display format",
	[T_HELP_1_18]="Pull-up resistors (off/ON)",
	[T_HELP_1_19]="-\t\t\t",
	[T_HELP_1_20]="Show volts/states\t",
	[T_HELP_1_21]="Power supply (off/ON)\t",
	[T_HELP_2_1]="Mode macro x/list all",
	[T_HELP_2_3]="Start",
	[T_HELP_2_4]="Stop",
	[T_HELP_2_5]="Start with read",
	[T_HELP_2_6]="Stop",
	[T_HELP_2_7]="Send string",
	[T_HELP_2_8]="",
	[T_HELP_2_9]="",
	[T_HELP_2_10]="Send value",
	[T_HELP_2_11]="Read",
	[T_HELP_2_12]="CLK hi",
	[T_HELP_2_13]="CLK lo",
	[T_HELP_2_14]="CLK tick",
	[T_HELP_2_15]="DAT hi",
	[T_HELP_2_16]="DAT lo",
	[T_HELP_2_17]="DAT read",
	[T_HELP_2_18]="Bit read",
	[T_HELP_2_19]="Repeat e.g. r:10",
	[T_HELP_2_20]="Bits to read/write e.g. 0x55.2",
	[T_HELP_2_21]="User macro x/list all",
	[T_HELP_2_22]="User macro assign x",
    //INFO
	[T_INFO_FIRMWARE]="Firmware",
	[T_INFO_BOOTLOADER]="Bootloader",
	[T_INFO_WITH]="with",
	[T_INFO_RAM]="RAM",
	[T_INFO_FLASH]="FLASH",
	[T_INFO_SN]="S/N",
	[T_INFO_WEBSITE]="https://BusPirate.com/",
	[T_INFO_SD_CARD]="SD Card",
	[T_INFO_FILE_SYSTEM]="File System",
	[T_NOT_DETECTED]="Not Detected",
	[T_INFO_AVAILABLE_MODES]="Available modes",
	[T_INFO_CURRENT_MODE]="Active mode",
	[T_INFO_POWER_SUPPLY]="Power supply",
	[T_INFO_CURRENT_LIMIT]="Current limit",
	[T_INFO_PULLUP_RESISTORS]="Pull-up resistors",
	[T_INFO_FREQUENCY_GENERATORS]="Frequency generators",
	[T_INFO_DISPLAY_FORMAT]="Display format",
	[T_INFO_DATA_FORMAT]="Data format",
	[T_INFO_BITS]="bits",
	[T_INFO_BITORDER]="bitorder",
	//CONFIG MENU
	[T_CONFIG_FILE]="Configuration file",
	[T_CONFIG_CONFIGURATION_OPTIONS]="Configuration options",
	[T_CONFIG_LANGUAGE]="Language",
	[T_CONFIG_ANSI_COLOR_MODE]="ANSI color mode",
	[T_CONFIG_ANSI_TOOLBAR_MODE]="ANSI toolbar mode",
	[T_CONFIG_LANGUAGE_ENGLISH]="English",
	[T_CONFIG_LANGUAGE_CHINESE]="Chinese (simplified)",
	[T_CONFIG_DISABLE]="Disable",
	[T_CONFIG_ENABLE]="Enable",
    [T_CONFIG_SCREENSAVER]="LCD screensaver",
    [T_CONFIG_SCREENSAVER_5]="5 minutes",
    [T_CONFIG_SCREENSAVER_10]="10 minutes",
    [T_CONFIG_SCREENSAVER_15]="15 minutes",
    [T_CONFIG_LEDS_EFFECT]="LED effect",
    [T_CONFIG_LEDS_EFFECT_SOLID]="Solid",
    [T_CONFIG_LEDS_EFFECT_ANGLEWIPE]="Angle wipe",
    [T_CONFIG_LEDS_EFFECT_CENTERWIPE]="Center wipe",
    [T_CONFIG_LEDS_EFFECT_CLOCKWISEWIPE]="Clockwise wipe",
    [T_CONFIG_LEDS_EFFECT_TOPDOWNWIPE]="Top side wipe",
    [T_CONFIG_LEDS_EFFECT_SCANNER]="Scanner",
	[T_CONFIG_LEDS_EFFECT_CYCLE]="Party mode",
    [T_CONFIG_LEDS_COLOR]="LED color",
    [T_CONFIG_LEDS_COLOR_RAINBOW]="Rainbow",
    [T_CONFIG_LEDS_COLOR_RED]="Red",
    [T_CONFIG_LEDS_COLOR_ORANGE]="Orange",
    [T_CONFIG_LEDS_COLOR_YELLOW]="Yellow",
    [T_CONFIG_LEDS_COLOR_GREEN]="Green",
    [T_CONFIG_LEDS_COLOR_BLUE]="Blue",
    [T_CONFIG_LEDS_COLOR_PURPLE]="Purple",
    [T_CONFIG_LEDS_COLOR_PINK]="Pink",
    [T_CONFIG_LEDS_BRIGHTNESS]="LED brightness",
    [T_CONFIG_LEDS_BRIGHTNESS_10]="10%",
    [T_CONFIG_LEDS_BRIGHTNESS_20]="20%",
    [T_CONFIG_LEDS_BRIGHTNESS_30]="30%",
    [T_CONFIG_LEDS_BRIGHTNESS_40]="40%",
    [T_CONFIG_LEDS_BRIGHTNESS_50]="50%",
    [T_CONFIG_LEDS_BRIGHTNESS_100]="100% ***WARNING: will damage USB port without external power supply***"

};

static_assert(count_of(en_us)==T_LAST_ITEM_ALWAYS_AT_THE_END, "Missing english translations!");
