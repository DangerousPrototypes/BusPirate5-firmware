#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "translation/en-us.h"
//#include "translation/zh-cn.h"
#include "translation/pl-pl.h"
#include "translation/bs-ba.h"
#include "translation/it-it.h"

char **t;

void translation_init(void){
    t = (char **) &en_us;
}

//TODO: lots of changes to make this more automatic at compile time
void translation_set(uint32_t language)
{
    switch(language)
    {
        case 3:
            t = (char **) &it_it;
            break;
        case 2:
            t = (char **) &bs_ba;
            break;
        case 1:
            t = (char **) &pl_pl;
            break;
        /*case 2:
            t = (char **) &zn_ch;
            break;*/
        case 0:
        default:
            t = (char **) &en_us;
            break;
    }
}
