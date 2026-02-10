/*
    spirom stuff for the universal programmer 'plank' (https:// xx) 
    
    (C) Chris van Dongen 2025-26

*/

// 8-pin 25 spirom
#define UP_SPIROM_8PIN_CS     UP_IO13
#define UP_SPIROM_8PIN_MISO   UP_IO14
#define UP_SPIROM_8PIN_WP     UP_IO15
#define UP_SPIROM_8PIN_MOSI   UP_IO17
#define UP_SPIROM_8PIN_CLK    UP_IO18
#define UP_SPIROM_8PIN_HOLD   UP_IO19

#define UP_SPIROM_8PIN_PU   (0)
#define UP_SPIROM_8PIN_DIR  (UP_SPIROM_8PIN_MISO)


void up_spirom_handler(struct command_result* res);

