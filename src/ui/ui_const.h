static const int ui_const_input_output[] = { T_INPUT, T_OUTPUT };

static enum ui_const_freq_measures {
    freq_ns,
    freq_us,
    freq_ms,
    freq_hz,
    freq_khz,
    freq_mhz,
    freq_percent
} ui_const_freq_measures;

// Lower case for detecting user input
static const char ui_const_freq_slugs[][4] = { "ns", "us", "ms", "hz", "khz", "mhz", "%" };
// Pretty labels to display frequency and period
static const char ui_const_freq_labels[][4] = { "ns", "us", "ms", "Hz", "kHz", "MHz", "%" };
// short version of freq labels to show in constrained spaces (eg toolbar)
static const char ui_const_freq_labels_short[][1] = { "n", "u", "m", "H", "K", "M", "%" };
// global constants
static const char ui_const_bit_orders[][4] = { "MSB", "LSB" };

static const char ui_const_pin_states[][6] = { "OFF", "ON", "GND", "PWM", "FREQ", "ERR" };

static enum ui_const_display_formats_enum {
    df_auto,
    df_hex,
    df_dec,
    // df_oct,
    df_bin,
    df_ascii
} ui_const_display_formats_enum;

static const char ui_const_display_formats[][6] = { "Auto",
                                                    "HEX",
                                                    "DEC",
                                                    //"OCT",
                                                    "BIN",
                                                    "ASCII" };

static const char ascii_hex[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };