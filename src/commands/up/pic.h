/*
    microchip pic functions for the universal programmer 'plank' (https:// xx) 
    
    (C) Chris van Dongen 2025-26

*/

// microchip pic

#define UP_PIC_8PIN_MCLR  UP_IO16
#define UP_PIC_8PIN_PDAT  UP_IO19
#define UP_PIC_8PIN_PCLK  UP_IO18
#define UP_PIC_8PIN_DIR   (0)
#define UP_PIC_8PIN_PU    (0)

#define PIC_CMD_LOADCFG   0x00
#define PIC_CMD_LOADD_PM  0x02
#define PIC_CMD_LOADD_DM  0x03
#define PIC_CMD_READD_PM  0x04
#define PIC_CMD_READD_DM  0x05
#define PIC_CMD_INCRADR   0x06
#define PIC_CMD_PROG      0x08
#define PIC_CMD_PROGSTART 0x18
#define PIC_CMD_PROGEND   0x0A
#define PIC_CMD_ERASE_PM  0x09
#define PIC_CMD_ERASE_DM  0x0B


void up_pic_handler(struct command_result* res);

//void picreadids(void);

