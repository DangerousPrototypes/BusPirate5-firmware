/*
    display test for the universal programmer 'plank' (https:// xx) 
    
    (C) Chris van Dongen 2025-26

*/

//pinout for til305
#define UP_TIL305_COL1  UP_IO14
#define UP_TIL305_COL2  UP_IO10
#define UP_TIL305_COL3  UP_IO17
#define UP_TIL305_COL4  UP_IO23
#define UP_TIL305_COL5  UP_IO22
#define UP_TIL305_COLDP UP_IO16

#define UP_TIL305_ROW1  UP_IO11
#define UP_TIL305_ROW2  UP_IO21
#define UP_TIL305_ROW3  UP_IO12
#define UP_TIL305_ROW4  UP_IO13
#define UP_TIL305_ROW5  UP_IO20
#define UP_TIL305_ROW6  UP_IO19
#define UP_TIL305_ROW7  UP_IO18

#define UP_TIL305_PU    (0)
#define UP_TIL305_DIR   (0xFFFFFFFFl&(!(UP_TIL305_COL1|UP_TIL305_COL2|UP_TIL305_COL3|UP_TIL305_COL4|UP_TIL305_COL5|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7)))

//pinout for dl(r)1414
//
#define UP_DL1414_WR  UP_IO13
#define UP_DL1414_A1  UP_IO14
#define UP_DL1414_A0  UP_IO15

#define UP_DL1414_D0  UP_IO18
#define UP_DL1414_D1  UP_IO19
#define UP_DL1414_D2  UP_IO20
#define UP_DL1414_D3  UP_IO21
#define UP_DL1414_D4  UP_IO12
#define UP_DL1414_D5  UP_IO11
#define UP_DL1414_D6  UP_IO22

#define UP_DL1414_PU (0)
#define UP_DL1414_DIR (0)

void up_display_handler(struct command_result* res);


