#pragma once

// NOTE: GENERATED FILE -- DO NOT EDIT DIRECTLY
//       This file is auto-generated by `src/translations/json2h.py`.
//       Entirely new strings should be added to `src/translations/en-us.h`
//       New or modified translations of a string should be added to the
//       language-specific JSON file, which can be found in directory
//       `src/translations/templates/`.

#include "translation/base.h"
static char const * const pl_pl[T_LAST_ITEM_ALWAYS_AT_THE_END]={
    [ T_ON                             ] = "Wł.",
    [ T_OFF                            ] = "Wył.",
    [ T_GND                            ] = NULL,
    [ T_INPUT                          ] = NULL,
    [ T_OUTPUT                         ] = NULL,
    [ T_EXIT                           ] = "Wyjście",
    [ T_LOADED                         ] = "Załadowano",
    [ T_SAVED                          ] = "Zapisano",
    [ T_HZ                             ] = NULL,
    [ T_KHZ                            ] = NULL,
    [ T_MHZ                            ] = NULL,
    [ T_SPEED                          ] = NULL,
    [ T_WARN_VOUT_VREF_LOW             ] = NULL,
    [ T_USE_PREVIOUS_SETTINGS          ] = "Użyć poprzednich ustawień?",
    [ T_MODE_ERROR_NO_EFFECT           ] = "BŁĄD: polecenie nie ma tutaj efektu",
    [ T_MODE_ERROR_NO_EFFECT_HIZ       ] = "Polecenienie ma efektu w trybie HiZ, naciśnij 'm' aby wybrać tryb",
    [ T_MODE_NO_HELP_AVAILABLE         ] = "Brak dostępnej pomocy",
    [ T_PRESS_ANY_KEY_TO_EXIT          ] = "Naciśnij jakiś klawisz aby wyjść",
    [ T_PRESS_ANY_KEY                  ] = NULL,
    [ T_PRESS_BUTTON                   ] = NULL,
    [ T_PRESS_X_TO_EXIT                ] = NULL,
    [ T_MODE_MODE_SELECTION            ] = "Wybór trybu",
    [ T_MODE_MODE                      ] = "Tryb",
    [ T_MODE_NUMBER_DISPLAY_FORMAT     ] = "Numeryczny format wyśwetlania",
    [ T_MODE_INVALID_OPTION            ] = "Nieprawidłowa opcja",
    [ T_MODE_CHOOSE_AVAILABLE_PIN      ] = "Wybierz dostępny pin:",
    [ T_MODE_ALL_PINS_IN_USE           ] = "Wszystkie piny w użyciu",
    [ T_MODE_PULLUP_RESISTORS          ] = "Rezystory Pull-up",
    [ T_MODE_POWER_SUPPLY              ] = "Zasilanie",
    [ T_MODE_DISABLED                  ] = "Wyłączone",
    [ T_MODE_DISPLAY                   ] = NULL,
    [ T_MODE_DISPLAY_SELECTION         ] = NULL,
    [ T_MODE_ENABLED                   ] = "Włączone",
    [ T_MODE_BITORDER                  ] = "Kolejność bitów",
    [ T_MODE_BITORDER_MSB              ] = NULL,
    [ T_MODE_BITORDER_LSB              ] = NULL,
    [ T_MODE_DELAY                     ] = "Opóźnienie",
    [ T_MODE_US                        ] = NULL,
    [ T_MODE_MS                        ] = NULL,
    [ T_MODE_ADC_VOLTAGE               ] = NULL,
    [ T_MODE_ERROR_PARSING_MACRO       ] = "Błąd parsowania makra",
    [ T_MODE_ERROR_NO_MACROS_AVAILABLE ] = "Brak dostępnych makr",
    [ T_MODE_ERROR_MACRO_NOT_DEFINED   ] = "Makro nie zdefiniowane",
    [ T_MODE_TICK_CLOCK                ] = NULL,
    [ T_MODE_SET_CLK                   ] = NULL,
    [ T_MODE_SET_DAT                   ] = NULL,
    [ T_MODE_READ_DAT                  ] = NULL,
    [ T_MODE_NO_VOUT_VREF_ERROR        ] = NULL,
    [ T_MODE_NO_VOUT_VREF_HINT         ] = NULL,
    [ T_MODE_NO_PULLUP_ERROR           ] = NULL,
    [ T_MODE_NO_PULLUP_HINT            ] = NULL,
    [ T_MODE_PWM_GENERATE_FREQUENCY    ] = "Wygeneruj częstotliwość",
    [ T_MODE_FREQ_MEASURE_FREQUENCY    ] = "Pomiar częstotliwości",
    [ T_MODE_FREQ_FREQUENCY            ] = "Częstotliwość",
    [ T_MODE_FREQ_DUTY_CYCLE           ] = "Cykl pracy częstotliwości",
    [ T_PSU_DAC_ERROR                  ] = "Błąd DAC PSU, uruchom self-test",
    [ T_PSU_CURRENT_LIMIT_ERROR        ] = "Natężenie ponad limit, wyłączono zasilanie",
    [ T_PSU_SHORT_ERROR                ] = "Potencjalne zwarcie, wyłączono zasilanie",
    [ T_PSU_ALREADY_DISABLED_ERROR     ] = "Zasilanie już wyłączone",
    [ T_SYNTAX_EXCEEDS_MAX_SLOTS       ] = "Wynik przekracza dostępną przestrzeń (%d slotów)",
    [ T_HWSPI_SPEED_MENU               ] = "Prędkość SPI",
    [ T_HWSPI_SPEED_MENU_1             ] = "1 do 62500kHz",
    [ T_HWSPI_SPEED_PROMPT             ] = NULL,
    [ T_HWSPI_BITS_MENU                ] = "Bity danych",
    [ T_HWSPI_BITS_MENU_1              ] = "4 do 8 bitów",
    [ T_HWSPI_BITS_PROMPT              ] = "Bity/Bitów (%s%d*%s)",
    [ T_HWSPI_CLOCK_POLARITY_MENU      ] = "Polaryzacja zegara",
    [ T_HWSPI_CLOCK_POLARITY_MENU_1    ] = NULL,
    [ T_HWSPI_CLOCK_POLARITY_MENU_2    ] = NULL,
    [ T_HWSPI_CLOCK_POLARITY_PROMPT    ] = "Polaryzacja",
    [ T_HWSPI_CLOCK_PHASE_MENU         ] = "Faza zegara",
    [ T_HWSPI_CLOCK_PHASE_MENU_1       ] = NULL,
    [ T_HWSPI_CLOCK_PHASE_MENU_2       ] = NULL,
    [ T_HWSPI_CLOCK_PHASE_PROMPT       ] = "Faza",
    [ T_HWSPI_CS_IDLE_MENU             ] = NULL,
    [ T_HWSPI_CS_IDLE_MENU_1           ] = NULL,
    [ T_HWSPI_CS_IDLE_MENU_2           ] = NULL,
    [ T_HWSPI_CS_IDLE_PROMPT           ] = NULL,
    [ T_HWSPI_ACTUAL_SPEED_KHZ         ] = "Faktyczna prędkość",
    [ T_HWSPI_CS_SELECT                ] = "CS Włączone",
    [ T_HWSPI_CS_DESELECT              ] = "CS Wyłączone",
    [ T_UART_SPEED_MENU                ] = "Prędkość UART",
    [ T_UART_SPEED_MENU_1              ] = "1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200 itd.",
    [ T_UART_SPEED_PROMPT              ] = "Szerokość transmisji (%s%d*%s)",
    [ T_UART_PARITY_MENU               ] = "Parzystość",
    [ T_UART_PARITY_MENU_1             ] = "Brak",
    [ T_UART_PARITY_MENU_2             ] = "Parzyste",
    [ T_UART_PARITY_MENU_3             ] = "Nieparzyste",
    [ T_UART_PARITY_PROMPT             ] = "Parzystość",
    [ T_UART_DATA_BITS_MENU            ] = "Bity danych",
    [ T_UART_DATA_BITS_MENU_1          ] = "5 do 8 bitów",
    [ T_UART_DATA_BITS_PROMPT          ] = "(%s%d*%s) Bity/Bitów",
    [ T_UART_STOP_BITS_MENU            ] = "Bity zatrzymania",
    [ T_UART_STOP_BITS_MENU_1          ] = NULL,
    [ T_UART_STOP_BITS_MENU_2          ] = NULL,
    [ T_UART_STOP_BITS_PROMPT          ] = "Bity",
    [ T_UART_BLOCKING_MENU             ] = "Używaj funkcji blokujących?",
    [ T_UART_BLOCKING_MENU_1           ] = "Nie",
    [ T_UART_BLOCKING_MENU_2           ] = "Tak",
    [ T_UART_BLOCKING_PROMPT           ] = "Blokowanie",
    [ T_UART_FLOW_CONTROL_MENU         ] = NULL,
    [ T_UART_FLOW_CONTROL_MENU_1       ] = NULL,
    [ T_UART_FLOW_CONTROL_MENU_2       ] = NULL,
    [ T_UART_FLOW_CONTROL_PROMPT       ] = NULL,
    [ T_UART_INVERT_MENU               ] = NULL,
    [ T_UART_INVERT_MENU_1             ] = NULL,
    [ T_UART_INVERT_MENU_2             ] = NULL,
    [ T_UART_INVERT_PROMPT             ] = NULL,
    [ T_UART_GLITCH_TRG_MENU           ] = NULL,
    [ T_UART_GLITCH_TRG_MENU_1         ] = NULL,
    [ T_UART_GLITCH_TRG_PROMPT         ] = NULL,
    [ T_UART_GLITCH_DLY_MENU           ] = NULL,
    [ T_UART_GLITCH_DLY_MENU_1         ] = NULL,
    [ T_UART_GLITCH_DLY_PROMPT         ] = NULL,
    [ T_UART_GLITCH_VRY_MENU           ] = NULL,
    [ T_UART_GLITCH_VRY_MENU_1         ] = NULL,
    [ T_UART_GLITCH_VRY_PROMPT         ] = NULL,
    [ T_UART_GLITCH_LNG_MENU           ] = NULL,
    [ T_UART_GLITCH_LNG_MENU_1         ] = NULL,
    [ T_UART_GLITCH_LNG_PROMPT         ] = NULL,
    [ T_UART_GLITCH_CYC_MENU           ] = NULL,
    [ T_UART_GLITCH_CYC_MENU_1         ] = NULL,
    [ T_UART_GLITCH_CYC_PROMPT         ] = NULL,
    [ T_UART_GLITCH_FAIL_MENU          ] = NULL,
    [ T_UART_GLITCH_FAIL_MENU_1        ] = NULL,
    [ T_UART_GLITCH_FAIL_PROMPT        ] = NULL,
    [ T_UART_GLITCH_CNT_MENU           ] = NULL,
    [ T_UART_GLITCH_CNT_MENU_1         ] = NULL,
    [ T_UART_GLITCH_CNT_PROMPT         ] = NULL,
    [ T_UART_GLITCH_NORDY_MENU         ] = NULL,
    [ T_UART_GLITCH_NORDY_MENU_1       ] = NULL,
    [ T_UART_GLITCH_NORDY_PROMPT       ] = NULL,
    [ T_UART_GLITCH_NORDY_DISABLED     ] = NULL,
    [ T_UART_GLITCH_NORDY_ENABLED      ] = NULL,
    [ T_UART_GLITCH_GLITCHED           ] = NULL,
    [ T_UART_GLITCH_CANCELLED          ] = NULL,
    [ T_UART_GLITCH_DONE               ] = NULL,
    [ T_UART_TOOL_TIMEOUT              ] = NULL,
    [ T_UART_GLITCH_UNKNOWN            ] = NULL,
    [ T_UART_GLITCH_SETUP_ERR          ] = NULL,
    [ T_UART_ACTUAL_SPEED_BAUD         ] = "Faktyczna szerokość transmisji",
    [ T_UART_BAUD                      ] = "szerokość transmisji",
    [ T_UART_OPEN                      ] = "UART OTWARTY",
    [ T_UART_OPEN_WITH_READ            ] = "UART OTWARTY (ASYNC READ)",
    [ T_UART_CLOSE                     ] = "UART ZAMKNIĘTY",
    [ T_UART_NO_DATA_READ              ] = "Brak danych do odczytu",
    [ T_UART_NO_DATA_TIMEOUT           ] = "Odczyt danych przekroczył limit czasu",
    [ T_HWI2C_SPEED_MENU               ] = "Prędkość I2C",
    [ T_HWI2C_SPEED_MENU_1             ] = "1kHz do 1000kHz",
    [ T_HWI2C_SPEED_PROMPT             ] = NULL,
    [ T_HWI2C_DATA_BITS_MENU           ] = "Bity danych",
    [ T_HWI2C_DATA_BITS_MENU_1         ] = NULL,
    [ T_HWI2C_DATA_BITS_MENU_2         ] = NULL,
    [ T_HWI2C_DATA_BITS_PROMPT         ] = "Bitów",
    [ T_HWI2C_CLOCK_STRETCH_MENU       ] = NULL,
    [ T_HWI2C_CLOCK_STRETCH_PROMPT     ] = NULL,
    [ T_HWI2C_START                    ] = NULL,
    [ T_HWI2C_REPEATED_START           ] = NULL,
    [ T_HWI2C_STOP                     ] = NULL,
    [ T_HWI2C_ACK                      ] = NULL,
    [ T_HWI2C_NACK                     ] = NULL,
    [ T_HWI2C_NO_PULLUP_DETECTED       ] = "Nie wykryto rezystorów pull-up. Użyj P aby je włączyć",
    [ T_HWI2C_NO_VOUT_DETECTED         ] = NULL,
    [ T_HWI2C_TIMEOUT                  ] = "Przekroczenie limitu czasu I2C",
    [ T_HWI2C_I2C_ERROR                ] = "Błąd magistrali I2C",
    [ T_HELP_I2C_TCS34725              ] = NULL,
    [ T_HW2WIRE_SPEED_MENU             ] = NULL,
    [ T_HW2WIRE_RST_LOW                ] = NULL,
    [ T_HW2WIRE_RST_HIGH               ] = NULL,
    [ T_HW2WIRE_SNIFF                  ] = NULL,
    [ T_HW3WIRE_SPEED_MENU_1           ] = NULL,
    [ T_HWLED_DEVICE_MENU              ] = "Rodzaj LED",
    [ T_HWLED_DEVICE_MENU_1            ] = NULL,
    [ T_HWLED_DEVICE_MENU_2            ] = NULL,
    [ T_HWLED_DEVICE_MENU_3            ] = "LEDy na płytce (18 SK6812s)",
    [ T_HWLED_DEVICE_PROMPT            ] = "Typ",
    [ T_HWLED_NUM_LEDS_MENU            ] = "Ilość LEDów w pasku",
    [ T_HWLED_NUM_LEDS_MENU_1          ] = "1 do 10000",
    [ T_HWLED_NUM_LEDS_PROMPT          ] = "LEDy (%s%d*%s)",
    [ T_HWLED_RESET                    ] = NULL,
    [ T_HWLED_FRAME_START              ] = "RAMKA STARTu (0x00000000)",
    [ T_HWLED_FRAME_STOP               ] = "RAMKA STOPu (0xFFFFFFFF)",
    [ T_HW1WIRE_RESET                  ] = NULL,
    [ T_HW1WIRE_PRESENCE_DETECT        ] = "Wykryto urządzenie",
    [ T_HW1WIRE_NO_DEVICE              ] = "Nie wykryto urządzenia",
    [ T_IR_RX_SENSOR_MENU              ] = NULL,
    [ T_IR_RX_SENSOR_MENU_LEARNER      ] = NULL,
    [ T_IR_RX_SENSOR_MENU_BARRIER      ] = NULL,
    [ T_IR_RX_SENSOR_MENU_38K_DEMOD    ] = NULL,
    [ T_IR_RX_SENSOR_MENU_56K_DEMOD    ] = NULL,
    [ T_IR_TX_SPEED_MENU               ] = NULL,
    [ T_IR_TX_SPEED_MENU_1             ] = NULL,
    [ T_IR_TX_SPEED_PROMPT             ] = NULL,
    [ T_IR_PROTOCOL_MENU               ] = NULL,
    [ T_IR_PROTOCOL_MENU_RAW           ] = NULL,
    [ T_IR_PROTOCOL_MENU_RC5           ] = NULL,
    [ T_IR_PROTOCOL_MENU_NEC           ] = NULL,
    [ T_IR_CMD_TV_BGONE                ] = NULL,
    [ T_IR_CMD_IRRX                    ] = NULL,
    [ T_IR_CMD_IRTX                    ] = NULL,
    [ T_HELP_IRTX_FILE_FLAG            ] = NULL,
    [ T_HELP_IRRX_FILE_FLAG            ] = NULL,
    [ T_HELP_IRRX_SENSOR_FLAG          ] = NULL,
    [ T_JTAG_BLUETAG_OPTIONS           ] = NULL,
    [ T_JTAG_BLUETAG_JTAG              ] = NULL,
    [ T_JTAG_BLUETAG_SWD               ] = NULL,
    [ T_JTAG_BLUETAG_FLAGS             ] = NULL,
    [ T_JTAG_BLUETAG_CHANNELS          ] = NULL,
    [ T_JTAG_BLUETAG_VERSION           ] = NULL,
    [ T_JTAG_BLUETAG_DISABLE           ] = NULL,
    [ T_JTAG_BLUETAG_DESCRIPTION       ] = NULL,
    [ T_I2S_SPEED_MENU                 ] = NULL,
    [ T_I2S_SPEED_MENU_1               ] = NULL,
    [ T_I2S_SPEED_PROMPT               ] = NULL,
    [ T_I2S_DATA_BITS_MENU             ] = NULL,
    [ T_I2S_DATA_BITS_MENU_1           ] = NULL,
    [ T_I2S_DATA_BITS_PROMPT           ] = NULL,
    [ T_CMDLN_INVALID_COMMAND          ] = "Niepoprawne polecenie: %s. Użyj ? aby użyskać pomoc.",
    [ T_CMDLN_NO_HELP                  ] = "Pomoc dla tego polecenia jest aktualnie niedostępna.",
    [ T_CMDLN_LS                       ] = "ls {katalog} - listuje pliki w aktualnej lokacji albo {katalogu}.",
    [ T_CMDLN_CD                       ] = "cd {katalog} - przechodzi do {katalogu}.",
    [ T_CMDLN_MKDIR                    ] = "mkdir {katalog} - tworzy {katalog}.",
    [ T_CMDLN_RM                       ] = "rm {plik/katalog} - usuwa plik lub (zawartość) katalog/katalogu.",
    [ T_CMDLN_CAT                      ] = "cat {plik} - wyświetla zawartość {pliku}.",
    [ T_CMDLN_MODE                     ] = "m - zmienia tryb protokołu, pokazuje menu wyboru.",
    [ T_CMDLN_PSU_EN                   ] = "W - włącza zasilanie na płytce, pokazuje menu konfiguracji.",
    [ T_CMDLN_REBOOT                   ] = NULL,
    [ T_CMDLN_BOOTLOAD                 ] = "$ - resetuje i wchodzi w tryb bootloadera do aktualizacji.",
    [ T_CMDLN_INT_FORMAT               ] = "= {wartość} - konwertuje {wartość} do BIN/DEC/HEX/ASCII.",
    [ T_CMDLN_INT_INVERSE              ] = "| {wartość} - inwertuje bity {wartośći}.",
    [ T_CMDLN_HELP                     ] = "? - pokazuje pomoc dla polecenia i składni.",
    [ T_CMDLN_CONFIG_MENU              ] = "c - otwieramenu konfiguacji Bus Pirate'a.",
    [ T_CMDLN_FREQ_ONE                 ] = "f {IOx} - jednorazowo mierzy częstotliwość na pinie {IOx}.",
    [ T_CMDLN_FREQ_CONT                ] = "F {IOx} - mierzy częstotliwość na pinie {IOx} w sposób ciągły.",
    [ T_CMDLN_PWM_CONFIG               ] = "G - konfiguruje generator częstotliwości (PWM) na dostępnym pinie IOx.",
    [ T_CMDLN_PWM_DIS                  ] = "g {IOx} - wyłącza generator częstotliwości (PWM) na pinie {IOx}.",
    [ T_CMDLN_HELP_MODE                ] = "h - pokazuje ekran pomocy specyficzny dla trybu.",
    [ T_CMDLN_HELP_DISPLAY             ] = NULL,
    [ T_CMDLN_INFO                     ] = "i - pokazuje informację o Bus Pirate'cie i ekran statusu.",
    [ T_CMDLN_BITORDER_MSB             ] = "l - ustawia kolejność wyjścia na Najbardziej Znaczący Bit (MSB).",
    [ T_CMDLN_BITORDER_LSB             ] = "L - ustawia kolejność wyjścia na Najmniej Znaczący Bit (LSB).",
    [ T_CMDLN_DISPLAY_FORMAT           ] = "o - konfiguruje format wyświetlania numerów.",
    [ T_CMDLN_PULLUPS_EN               ] = "P - włącza rezystory pull-up na płytce.",
    [ T_CMDLN_PULLUPS_DIS              ] = "p - wyłącza rezystory pull-up na płytce.",
    [ T_CMDLN_PSU_DIS                  ] = "w - wyłącza zasilanie na płytce.",
    [ T_CMDLN_ADC_CONT                 ] = "V {IOx} - mierzy napięcie na pinie {IOx} w sposób ciągły. Pomiń numer pinu aby mierzyć na wszystkich pinach.",
    [ T_CMDLN_ADC_ONE                  ] = "v {IOx} - jednorazowo mierzy napięciena pinie {IOx}. Pomiń numer pinu aby zmierzyć jednorazowo na wszystkich pinach.",
    [ T_CMDLN_SELFTEST                 ] = "~ - przeprowadza self-test fabryczny. Odłącz wszystkie urządzenia i zmień na tryb HiZ przed rozpoczęciem testu.",
    [ T_CMDLN_AUX_IN                   ] = "@ {IOx} - ustawia pin {IOx} jako wejściowy, czyta stan pinu.",
    [ T_CMDLN_AUX_LOW                  ] = "a {IOx} - ustawia pin {IOx} jako wyjściowy i stan niski.",
    [ T_CMDLN_AUX_HIGH                 ] = "A {IOx} - ustawia pin {IOx} jako wyjściowy i stan wysoki.",
    [ T_CMDLN_DUMP                     ] = "dump {plik} {urządzenie} - zrzuca zawarość pamięci flash {urządzenia} do {pliku}. Uwaga: aktualnie eksperymentalna funkcja, która działa tylko z 25LC020 w trybie SPI.",
    [ T_CMDLN_LOAD                     ] = "load {plik} {urządzenie} - wczytuje zawartość {pliku} do pamięci flash {urządzenia}. Uwaga: aktualnie eksperymentalna funkcja, która działa tylko z 25LC020 w trybie SPI.",
    [ T_CMDLN_DISPLAY                  ] = NULL,
    [ T_CMDLN_LOGIC                    ] = NULL,
    [ T_CMDLN_HEX                      ] = NULL,
    [ T_HELP_SECTION_IO                ] = NULL,
    [ T_HELP_SECTION_CAPTURE           ] = NULL,
    [ T_HELP_SECTION_CONFIGURE         ] = NULL,
    [ T_HELP_SECTION_SYSTEM            ] = NULL,
    [ T_HELP_SECTION_FILES             ] = NULL,
    [ T_HELP_SECTION_PROGRAMS          ] = NULL,
    [ T_HELP_SECTION_MODE              ] = NULL,
    [ T_HELP_SECTION_SYNTAX            ] = NULL,
    [ T_HELP_SECTION_HELP              ] = NULL,
    [ T_HELP_GREATER_THAN              ] = "Uruchom Składnię Magistrali (patrz Bus Syntax)",
    [ T_HELP_SYNTAX_ADC                ] = "Mierzy volty na IO.x\t",
    [ T_HELP_CMD_LS                    ] = "Listuj pliki",
    [ T_HELP_CMD_CD                    ] = "Zmień katalog",
    [ T_HELP_CMD_MKDIR                 ] = "Twórz katalog",
    [ T_HELP_CMD_RM                    ] = "Usuń plik/katalog",
    [ T_HELP_CMD_PAUSE                 ] = NULL,
    [ T_HELP_CMD_FORMAT                ] = NULL,
    [ T_HELP_CMD_CAT                   ] = "Pokaż zawartość pliku",
    [ T_HELP_CMD_HEX                   ] = NULL,
    [ T_HELP_CAPTURE_SCOPE             ] = NULL,
    [ T_HELP_CAPTURE_LA                ] = NULL,
    [ T_HELP_CMD_FLASH                 ] = NULL,
    [ T_HELP_CMD_LABEL                 ] = NULL,
    [ T_HELP_CMD_IMAGE                 ] = NULL,
    [ T_HELP_CMD_DUMP                  ] = NULL,
    [ T_HELP_1_2                       ] = "Konwertuje x/odwraca x\t",
    [ T_HELP_1_3                       ] = "Wewnętrzny test (self-test)\t\t",
    [ T_HELP_SYSTEM_REBOOT             ] = NULL,
    [ T_HELP_1_5                       ] = "Przejdź do bootloadera\t",
    [ T_HELP_1_6                       ] = "Opóźnij 1 us/MS (d:4 aby powtórzyć)",
    [ T_HELP_1_7                       ] = "Ustaw stan pinu IO.x (low/HI/READ)",
    [ T_HELP_COMMAND_AUX               ] = "Ustaw stan pinu IO x (low/HI/READ)",
    [ T_HELP_COMMAND_DISPLAY           ] = NULL,
    [ T_HELP_1_9                       ] = "Menu konfiguracyjne\t",
    [ T_HELP_1_22                      ] = "Pokaż wolty na pinie IOx (jednorazowo/CIĄGLE)",
    [ T_HELP_1_10                      ] = "Pokaż wolty na wszystkich pinach IOs (jednorazowo/CIĄGLE)",
    [ T_HELP_1_11                      ] = "Zmierz częstotliwość na pinie IOx (jednorazowo/CIĄGLE)",
    [ T_HELP_1_23                      ] = "Monitoruj częstotliwość (wył./WŁ.)\t",
    [ T_HELP_1_12                      ] = "Generuj częstotliwość (wył./WŁ.)",
    [ T_HELP_HELP_GENERAL              ] = NULL,
    [ T_HELP_HELP_DISPLAY              ] = NULL,
    [ T_HELP_HELP_COMMAND              ] = NULL,
    [ T_HELP_1_14                      ] = "Wersja/Status\t",
    [ T_HELP_1_15                      ] = "Kolejność bitów (msb/LSB)\t",
    [ T_HELP_1_16                      ] = "Zmień tryb\t\t",
    [ T_HELP_1_17                      ] = "Ustaw format wyświetlania numerów",
    [ T_HELP_1_18                      ] = "Rezystory Pull-up (wył./WŁ.)",
    [ T_HELP_1_19                      ] = "-\t\t\t",
    [ T_HELP_1_20                      ] = "Pokaż wolty/stany\t",
    [ T_HELP_1_21                      ] = "Zasilanie (wył./WŁ.)\t",
    [ T_HELP_2_1                       ] = "Mode macro x/list all\t",
    [ T_HELP_2_3                       ] = "Uruchom",
    [ T_HELP_2_4                       ] = "Zatrzymaj",
    [ T_HELP_2_7                       ] = "Wyślij stringa",
    [ T_HELP_2_8                       ] = NULL,
    [ T_HELP_2_9                       ] = NULL,
    [ T_HELP_2_10                      ] = "Wyślij wartość",
    [ T_HELP_2_11                      ] = "Przeczytaj",
    [ T_HELP_SYN_CLOCK_HIGH            ] = NULL,
    [ T_HELP_SYN_CLOCK_LOW             ] = NULL,
    [ T_HELP_SYN_CLOCK_TICK            ] = NULL,
    [ T_HELP_SYN_DATA_HIGH             ] = NULL,
    [ T_HELP_SYN_DATA_LOW              ] = NULL,
    [ T_HELP_SYN_DATA_READ             ] = NULL,
    [ T_HELP_2_18                      ] = "Bit przeczytany",
    [ T_HELP_2_19                      ] = "Powtórz np. r:10",
    [ T_HELP_2_20                      ] = "Ilość bitów do zapisu/odczytu np. 0x55.2",
    [ T_HELP_2_21                      ] = "Makra użytkownika x/pokaż wszystkie",
    [ T_HELP_2_22                      ] = "Makra użytkownika przypisz x",
    [ T_HELP_HINT                      ] = "Multiplikuj polecenia przy użyciu ; || &&.\r\n\r\nPatrz -h dla pomocy polecenia: ls -h",
    [ T_HELP_FLASH                     ] = NULL,
    [ T_HELP_FLASH_ERASE               ] = NULL,
    [ T_HELP_FLASH_WRITE               ] = NULL,
    [ T_HELP_FLASH_READ                ] = NULL,
    [ T_HELP_FLASH_VERIFY              ] = NULL,
    [ T_HELP_FLASH_TEST                ] = NULL,
    [ T_HELP_FLASH_PROBE               ] = NULL,
    [ T_HELP_FLASH_INIT                ] = NULL,
    [ T_HELP_FLASH_FILE_FLAG           ] = NULL,
    [ T_HELP_FLASH_ERASE_FLAG          ] = NULL,
    [ T_HELP_FLASH_VERIFY_FLAG         ] = NULL,
    [ T_HELP_I2C_EEPROM                ] = NULL,
    [ T_HELP_SPI_EEPROM                ] = NULL,
    [ T_HELP_1WIRE_EEPROM              ] = NULL,
    [ T_HELP_EEPROM_DUMP               ] = NULL,
    [ T_HELP_EEPROM_ERASE              ] = NULL,
    [ T_HELP_EEPROM_WRITE              ] = NULL,
    [ T_HELP_EEPROM_READ               ] = NULL,
    [ T_HELP_EEPROM_VERIFY             ] = NULL,
    [ T_HELP_EEPROM_TEST               ] = NULL,
    [ T_HELP_EEPROM_LIST               ] = NULL,
    [ T_HELP_EEPROM_DEVICE_FLAG        ] = NULL,
    [ T_HELP_EEPROM_FILE_FLAG          ] = NULL,
    [ T_HELP_EEPROM_VERIFY_FLAG        ] = NULL,
    [ T_HELP_EEPROM_START_FLAG         ] = NULL,
    [ T_HELP_EEPROM_BYTES_FLAG         ] = NULL,
    [ T_HELP_EEPROM_ADDRESS_FLAG       ] = NULL,
    [ T_HELP_EEPROM_PROTECT            ] = NULL,
    [ T_HELP_EEPROM_PROTECT_FLAG       ] = NULL,
    [ T_HELP_EEPROM_SPI_WPEN_FLAG      ] = NULL,
    [ T_HELP_EEPROM_SPI_TEST_FLAG      ] = NULL,
    [ T_INFO_FIRMWARE                  ] = NULL,
    [ T_INFO_BOOTLOADER                ] = NULL,
    [ T_INFO_WITH                      ] = "z",
    [ T_INFO_RAM                       ] = NULL,
    [ T_INFO_FLASH                     ] = NULL,
    [ T_INFO_SN                        ] = NULL,
    [ T_INFO_WEBSITE                   ] = NULL,
    [ T_INFO_TF_CARD                   ] = "Karta SD/TF",
    [ T_INFO_FILE_SYSTEM               ] = "System Plików",
    [ T_NOT_DETECTED                   ] = "Nie Wykryto",
    [ T_INFO_AVAILABLE_MODES           ] = "Dostępne tryby",
    [ T_INFO_CURRENT_MODE              ] = "Aktywny tryb",
    [ T_INFO_POWER_SUPPLY              ] = "Zasilanie",
    [ T_INFO_CURRENT_LIMIT             ] = "Limit natężenia",
    [ T_INFO_PULLUP_RESISTORS          ] = "Rezystory pull-up",
    [ T_INFO_FREQUENCY_GENERATORS      ] = "Generatory częstotliwości",
    [ T_INFO_DISPLAY_FORMAT            ] = "Format wyświetlania",
    [ T_INFO_DATA_FORMAT               ] = "Format danych",
    [ T_INFO_BITS                      ] = "bity/bitów",
    [ T_INFO_BITORDER                  ] = "kolejność bitów",
    [ T_CONFIG_FILE                    ] = "Plik konfiguracyjny",
    [ T_CONFIG_CONFIGURATION_OPTIONS   ] = "Opcje konfiguracyjne",
    [ T_CONFIG_LANGUAGE                ] = NULL,
    [ T_CONFIG_ANSI_COLOR_MODE         ] = "Tryb kolorowy ANSI",
    [ T_CONFIG_ANSI_COLOR_FULLCOLOR    ] = NULL,
    [ T_CONFIG_ANSI_COLOR_256          ] = NULL,
    [ T_CONFIG_ANSI_TOOLBAR_MODE       ] = "Tryb ANSI toolbar",
    [ T_CONFIG_LANGUAGE_ENGLISH        ] = NULL,
    [ T_CONFIG_LANGUAGE_CHINESE        ] = NULL,
    [ T_CONFIG_LANGUAGE_POLISH         ] = NULL,
    [ T_CONFIG_LANGUAGE_BOSNIAN        ] = NULL,
    [ T_CONFIG_LANGUAGE_ITALIAN        ] = NULL,
    [ T_CONFIG_DISABLE                 ] = "Wyłącz",
    [ T_CONFIG_ENABLE                  ] = "Włącz",
    [ T_CONFIG_SCREENSAVER             ] = "Wygaszacz ekranu LCD",
    [ T_CONFIG_SCREENSAVER_5           ] = "5 minut",
    [ T_CONFIG_SCREENSAVER_10          ] = "10 minut",
    [ T_CONFIG_SCREENSAVER_15          ] = "15 minut",
    [ T_CONFIG_LEDS_EFFECT             ] = "Efekt LED",
    [ T_CONFIG_LEDS_EFFECT_SOLID       ] = "Stały",
    [ T_CONFIG_LEDS_EFFECT_ANGLEWIPE   ] = NULL,
    [ T_CONFIG_LEDS_EFFECT_CENTERWIPE  ] = NULL,
    [ T_CONFIG_LEDS_EFFECT_CLOCKWISEWIPE ] = NULL,
    [ T_CONFIG_LEDS_EFFECT_TOPDOWNWIPE ] = NULL,
    [ T_CONFIG_LEDS_EFFECT_SCANNER     ] = "Skaner",
    [ T_CONFIG_LEDS_EFFECT_GENTLE_GLOW ] = NULL,
    [ T_CONFIG_LEDS_EFFECT_CYCLE       ] = NULL,
    [ T_CONFIG_LEDS_COLOR              ] = "Kolor LED",
    [ T_CONFIG_LEDS_COLOR_RAINBOW      ] = "Tęcza",
    [ T_CONFIG_LEDS_COLOR_RED          ] = "Czerwony",
    [ T_CONFIG_LEDS_COLOR_ORANGE       ] = "Pomarańczowy",
    [ T_CONFIG_LEDS_COLOR_YELLOW       ] = "Żółty",
    [ T_CONFIG_LEDS_COLOR_GREEN        ] = "Zielony",
    [ T_CONFIG_LEDS_COLOR_BLUE         ] = "Niebieski",
    [ T_CONFIG_LEDS_COLOR_PURPLE       ] = "Fioletowy",
    [ T_CONFIG_LEDS_COLOR_PINK         ] = "Różowy",
    [ T_CONFIG_LEDS_COLOR_WHITE        ] = NULL,
    [ T_CONFIG_LEDS_BRIGHTNESS         ] = "Jasność LEDów",
    [ T_CONFIG_LEDS_BRIGHTNESS_10      ] = NULL,
    [ T_CONFIG_LEDS_BRIGHTNESS_20      ] = NULL,
    [ T_CONFIG_LEDS_BRIGHTNESS_30      ] = NULL,
    [ T_CONFIG_LEDS_BRIGHTNESS_40      ] = NULL,
    [ T_CONFIG_LEDS_BRIGHTNESS_50      ] = NULL,
    [ T_CONFIG_LEDS_BRIGHTNESS_100     ] = "100% ***UWAGA: uszkodzi port USB bez zewnętrznego zasilacza***",
    [ T_CONFIG_BINMODE_SELECT          ] = NULL,
    [ T_HELP_DUMMY_COMMANDS            ] = NULL,
    [ T_HELP_DUMMY_INIT                ] = NULL,
    [ T_HELP_DUMMY_TEST                ] = NULL,
    [ T_HELP_DUMMY_FLAGS               ] = NULL,
    [ T_HELP_DUMMY_B_FLAG              ] = NULL,
    [ T_HELP_DUMMY_I_FLAG              ] = NULL,
    [ T_HELP_DUMMY_FILE_FLAG           ] = NULL,
    [ T_HELP_SLE4442                   ] = NULL,
    [ T_HELP_SLE4442_INIT              ] = NULL,
    [ T_HELP_SLE4442_DUMP              ] = NULL,
    [ T_HELP_SLE4442_UNLOCK            ] = NULL,
    [ T_HELP_SLE4442_WRITE             ] = NULL,
    [ T_HELP_SLE4442_ERASE             ] = NULL,
    [ T_HELP_SLE4442_PSC               ] = NULL,
    [ T_HELP_SLE4442_PROTECT           ] = NULL,
    [ T_HELP_SLE4442_ADDRESS_FLAG      ] = NULL,
    [ T_HELP_SLE4442_VALUE_FLAG        ] = NULL,
    [ T_HELP_SLE4442_CURRENT_PSC_FLAG  ] = NULL,
    [ T_HELP_SLE4442_NEW_PSC_FLAG      ] = NULL,
    [ T_HELP_SLE4442_FILE_FLAG         ] = NULL,
    [ T_HELP_GCMD_W                    ] = NULL,
    [ T_HELP_GCMD_W_DISABLE            ] = NULL,
    [ T_HELP_GCMD_W_ENABLE             ] = NULL,
    [ T_HELP_GCMD_W_VOLTS              ] = NULL,
    [ T_HELP_GCMD_W_CURRENT_LIMIT      ] = NULL,
    [ T_HELP_GCMD_P                    ] = NULL,
    [ T_HELP_HELP                      ] = NULL,
    [ T_HELP_SYS_COMMAND               ] = NULL,
    [ T_HELP_SYS_DISPLAY               ] = NULL,
    [ T_HELP_SYS_MODE                  ] = NULL,
    [ T_HELP_SYS_HELP                  ] = NULL,
    [ T_HELP_GCMD_SELFTEST             ] = NULL,
    [ T_HELP_GCMD_SELFTEST_CMD         ] = NULL,
    [ T_HELP_GCMD_SELFTEST_H_FLAG      ] = NULL,
    [ T_HELP_AUXIO                     ] = NULL,
    [ T_HELP_AUXIO_LOW                 ] = NULL,
    [ T_HELP_AUXIO_HIGH                ] = NULL,
    [ T_HELP_AUXIO_INPUT               ] = NULL,
    [ T_HELP_AUXIO_IO                  ] = NULL,
    [ T_HELP_DISK_HEX                  ] = NULL,
    [ T_HELP_DISK_HEX_FILE             ] = NULL,
    [ T_HELP_DISK_HEX_ADDR             ] = NULL,
    [ T_HELP_DISK_HEX_ASCII            ] = NULL,
    [ T_HELP_DISK_HEX_SIZE             ] = NULL,
    [ T_HELP_DISK_HEX_OFF              ] = NULL,
    [ T_HELP_DISK_HEX_PAGER_OFF        ] = NULL,
    [ T_HELP_DISK_HEX_QUIET            ] = NULL,
    [ T_HELP_DISK_CAT                  ] = NULL,
    [ T_HELP_DISK_CAT_FILE             ] = NULL,
    [ T_HELP_DISK_MKDIR                ] = NULL,
    [ T_HELP_DISK_MKDIR_DIR            ] = NULL,
    [ T_HELP_DISK_CD                   ] = NULL,
    [ T_HELP_DISK_CD_DIR               ] = NULL,
    [ T_HELP_DISK_RM                   ] = NULL,
    [ T_HELP_DISK_RM_FILE              ] = NULL,
    [ T_HELP_DISK_RM_DIR               ] = NULL,
    [ T_HELP_DISK_LS                   ] = NULL,
    [ T_HELP_DISK_LS_DIR               ] = NULL,
    [ T_HELP_DISK_FORMAT               ] = NULL,
    [ T_HELP_DISK_FORMAT_CMD           ] = NULL,
    [ T_HELP_DISK_LABEL                ] = NULL,
    [ T_HELP_DISK_LABEL_GET            ] = NULL,
    [ T_HELP_DISK_LABEL_SET            ] = NULL,
    [ T_HELP_VADC                      ] = NULL,
    [ T_HELP_VADC_SINGLE               ] = NULL,
    [ T_HELP_VADC_CONTINUOUS           ] = NULL,
    [ T_HELP_VADC_IO                   ] = NULL,
    [ T_HELP_I2C_SCAN                  ] = NULL,
    [ T_HELP_I2C_SCAN_VERBOSE          ] = NULL,
    [ T_HELP_FLAG                      ] = NULL,
    [ T_HELP_I2C_SI7021                ] = NULL,
    [ T_HELP_I2C_MS5611                ] = NULL,
    [ T_HELP_I2C_TSL2561               ] = NULL,
    [ T_HELP_I2C_SHT3X                 ] = NULL,
    [ T_HELP_I2C_SHT4X                 ] = NULL,
    [ T_HELP_1WIRE_SCAN                ] = NULL,
    [ T_HELP_1WIRE_DS18B20             ] = NULL,
    [ T_HELP_UART_BRIDGE               ] = NULL,
    [ T_HELP_UART_BRIDGE_EXIT          ] = NULL,
    [ T_HELP_UART_BRIDGE_TOOLBAR       ] = NULL,
    [ T_HELP_UART_BRIDGE_SUPPRESS_LOCAL_ECHO ] = NULL,
    [ T_HELP_UART_NMEA                 ] = NULL,
    [ T_HELP_UART_GLITCH_EXIT          ] = NULL,
    [ T_HELP_SECTION_SCRIPT            ] = NULL,
    [ T_HELP_CMD_SCRIPT                ] = NULL,
    [ T_HELP_CMD_BUTTON                ] = NULL,
    [ T_HELP_CMD_MACRO                 ] = NULL,
    [ T_HELP_CMD_TUTORIAL              ] = NULL,
    [ T_HELP_CMD_PAUSE_KEY             ] = NULL,
    [ T_HELP_CMD_PAUSE_BUTTON          ] = NULL,
    [ T_HELP_CMD_PAUSE_EXIT            ] = NULL,
    [ T_HELP_BUTTON                    ] = NULL,
    [ T_HELP_BUTTON_SHORT              ] = NULL,
    [ T_HELP_BUTTON_LONG               ] = NULL,
    [ T_HELP_BUTTON_FILE               ] = NULL,
    [ T_HELP_BUTTON_HIDE               ] = NULL,
    [ T_HELP_BUTTON_EXIT               ] = NULL,
    [ T_HELP_LOGIC                     ] = NULL,
    [ T_HELP_LOGIC_START               ] = NULL,
    [ T_HELP_LOGIC_STOP                ] = NULL,
    [ T_HELP_LOGIC_HIDE                ] = NULL,
    [ T_HELP_LOGIC_SHOW                ] = NULL,
    [ T_HELP_LOGIC_NAV                 ] = NULL,
    [ T_HELP_LOGIC_INFO                ] = NULL,
    [ T_HELP_LOGIC_FREQUENCY           ] = NULL,
    [ T_HELP_LOGIC_OVERSAMPLE          ] = NULL,
    [ T_HELP_LOGIC_DEBUG               ] = NULL,
    [ T_HELP_LOGIC_SAMPLES             ] = NULL,
    [ T_HELP_LOGIC_TRIGGER_PIN         ] = NULL,
    [ T_HELP_LOGIC_TRIGGER_LEVEL       ] = NULL,
    [ T_HELP_LOGIC_LOW_CHAR            ] = NULL,
    [ T_HELP_LOGIC_HIGH_CHAR           ] = NULL,
    [ T_HELP_CMD_CLS                   ] = NULL,
    [ T_HELP_SECTION_TOOLS             ] = NULL,
    [ T_HELP_CMD_LOGIC                 ] = NULL,
    [ T_HELP_CMD_SMPS                  ] = NULL,
    [ T_INFRARED_CMD_TEST              ] = NULL,
    [ T_UART_CMD_TEST                  ] = NULL,
    [ T_SPI_CMD_SNIFF                  ] = NULL,
    [ T_HELP_UART_GLITCH               ] = NULL,
    [ T_HELP_UART_GLITCH_CONFIG        ] = NULL,
    [ T_I2C_SNIFF                      ] = NULL,
    [ T_I2C_SNIFF_QUIET                ] = NULL,
    [ T_HELP_DDR5                      ] = NULL,
    [ T_HELP_DDR5_PROBE                ] = NULL,
    [ T_HELP_DDR5_DUMP                 ] = NULL,
    [ T_HELP_DDR5_WRITE                ] = NULL,
    [ T_HELP_DDR5_READ                 ] = NULL,
    [ T_HELP_DDR5_VERIFY               ] = NULL,
    [ T_HELP_DDR5_LOCK                 ] = NULL,
    [ T_HELP_DDR5_UNLOCK               ] = NULL,
    [ T_HELP_DDR5_CRC                  ] = NULL,
    [ T_HELP_DDR5_FILE_FLAG            ] = NULL,
    [ T_HELP_DDR5_BLOCK_FLAG           ] = NULL,

};
