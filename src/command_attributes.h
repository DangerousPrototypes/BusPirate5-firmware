struct command_response {
    bool error;
    uint32_t data;
};

struct command_attributes {
    bool has_value;
    bool has_dot;
    bool has_colon;
    bool has_string;
    uint8_t command;       // the actual command called
    uint8_t number_format; // DEC/HEX/BIN
    uint32_t value;        // integer value parsed from command line
    uint32_t dot;          // value after .
    uint32_t colon;        // value after :
};