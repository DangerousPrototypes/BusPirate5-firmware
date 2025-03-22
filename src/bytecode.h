// struct __attribute__((packed, aligned(sizeof(uint64_t)))) _bytecode_output{
struct _bytecode {
    // NOTE: Increasing the size of this structure can have SIGNIFICANT impact on RAM usage.
    //       Be aware of padding bytes.
    uint8_t number_format;
    uint8_t command; // 255 command options, write/write_return
    uint8_t error; // mode flags errors. One bit to halt execution? Other bits for warnings? ccan override the halt from
                   // configuration menu?
    // use bits to make this take only 28 bytes per structure, instead of 32, to save an additional 8k of RAM.
    uint8_t read_with_write : 1; // used only in syntax.c, in non-timing critical path; set only in hwspi.c?
    uint8_t has_bits : 1;        // used only in syntax.c, in non-timing critical path.
    uint8_t has_repeat : 1;      // BUGBUG -- Value is only set, never read?

    const char* error_message;
    const char* data_message;
    // The following fields appear to be overloaded by the individual protocols.  Thus,
    // they have NO DEFINED MEANING outside of the protocol-specific convention, which
    // is allowed to vary between protocols.
    //
    // As one example, HWLED protocol uses only the `out_data` field, and even then,
    // the value stored depends on the protocol:
    //     * WS2812: 24 least significant bits: RGB value (top 8 bits are forced to be zero before used).
    //     * APA102: 24 least significant bits: RGB value (top 8 bits are currently forced to 0xFF for full-brightness).
    uint32_t bits;     // 0-32 bits?
    uint32_t repeat;   // 0-0xffff repeat
    uint32_t out_data; // 32 data bits (to be sent over wire(s))
    uint32_t in_data;  // 32 data bits (read from the wire(s))
};
static_assert(
    sizeof(struct _bytecode) <= 28,
    "sizeof(struct _bytecode) has increased.  This will impact RAM.  Review to ensure this is not avoidable.");

struct _bytecode_output {
    uint8_t number_format;
    uint8_t command; // 255 command options, write/write_return
    uint32_t bits;   // 0-32 bits?
    uint32_t repeat; // 0-0xffff repeat
    uint32_t data;   // 32 data bits
    bool has_repeat;
    bool has_bits;
};

// need a way to generate multiple results from a single repeated command
// track by command ID? sequence number?
struct _bytecode_result {
    struct _bytecode_output output;
    uint8_t error; // mode flags errors. One bit to halt execution? Other bits for warnings? ccan override the halt from
                   // configuration menu?
    const char* error_message;
    uint32_t data; // up to 32bits results? BUT: how to deal with repeated reads????
    const char* message;
};

typedef enum {
    SSTATUS_OK,
    SSTATUS_ERROR
} SYNTAX_STATUS;

enum SYNTAX_ERRORS {
    SERR_NONE = 0,
    SERR_DEBUG,
    SERR_INFO,
    SERR_WARN,
    SERR_ERROR
};

enum SYNTAX {
    SYN_WRITE = 0,
    SYN_READ,
    SYN_START,
    SYN_STOP,
    SYN_START_ALT,
    SYN_STOP_ALT,
    SYN_TICK_CLOCK,
    SYN_SET_CLK_HIGH, /// here
    SYN_SET_CLK_LOW,
    SYN_SET_DAT_HIGH,
    SYN_SET_DAT_LOW,
    SYN_READ_DAT, // here
    SYN_DELAY_US,
    SYN_DELAY_MS,
    SYN_AUX_OUTPUT_HIGH,
    SYN_AUX_OUTPUT_LOW,
    SYN_AUX_INPUT,
    SYN_ADC,
    // SYN_FREQ
};