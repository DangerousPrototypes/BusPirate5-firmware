typedef struct {
    const uint8_t width;   // width in bits
    const uint8_t height;  // height in bits
    const uint16_t offset; // start in font bitmap

} FONT_CHAR_INFO;

typedef struct {
    const uint8_t height_bytes;   // number of bytes per column, The Dot Factory doesn't pack the data so often bits are
                                  // wasted/skipped TODO: fix the dot factory...
    const uint8_t start_char;     // start character
    const uint8_t end_char;       // end character
    const uint8_t right_padding;  // pixels right padding needed
    const FONT_CHAR_INFO* lookup; // points to the character descriptors
    const uint8_t* bitmaps;       // points to the bitmaps

} FONT_INFO;