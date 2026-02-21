struct hex_config_t {
    uint32_t start_address; // start address for the hex dump
    uint32_t requested_bytes; // user requested number of bytes to read
    uint32_t max_size_bytes; // maximum size of the device in bytes
    uint32_t _aligned_start; // aligned start address for the hex dump
    uint32_t _aligned_end;   // aligned end address for the hex dump
    uint32_t _total_read_bytes; // total number of bytes read
    uint32_t rows_printed; // number of rows printed
    uint32_t rows_terminal; // number of rows in the terminal
    bool pager_off; // enable/disable paging
    bool header_verbose; //show the address range in the header
    bool quiet; // disable address and ASCII dump
};

struct bp_command_def;

void ui_hex_init_config(struct hex_config_t *config);
bool ui_hex_get_args_config(const struct bp_command_def *def, struct hex_config_t *config);
void ui_hex_align_config(struct hex_config_t *config);
void ui_hex_header_config(struct hex_config_t *config);
bool ui_hex_row_config(struct hex_config_t *config, uint32_t address, uint8_t *buf, uint32_t buf_size);

#define UI_HEX_HELP_START T_HELP_EEPROM_START_FLAG
#define UI_HEX_HELP_BYTES T_HELP_EEPROM_BYTES_FLAG
#define UI_HEX_HELP_QUIET T_HELP_DISK_HEX_QUIET

/**
 * Shared opt entries for any command using the hex display library.
 * Insert into your bp_command_opt_t[] array before the {0} sentinel.
 * Example:
 *   static const bp_command_opt_t my_opts[] = {
 *       UI_HEX_OPTS
 *       { "file", 'f', BP_ARG_REQUIRED, "file", T_HELP_MY_FILE },
 *       { 0 }
 *   };
 */
#define UI_HEX_OPTS \
    { "start",   's', BP_ARG_REQUIRED, "addr",  UI_HEX_HELP_START, NULL }, \
    { "bytes",   'b', BP_ARG_REQUIRED, "count", UI_HEX_HELP_BYTES, NULL }, \
    { "quiet",   'q', BP_ARG_NONE,     NULL,    UI_HEX_HELP_QUIET, NULL }, \
    { "nopager", 'c', BP_ARG_NONE,     NULL,    T_HELP_DISK_HEX_PAGER_OFF, NULL },
