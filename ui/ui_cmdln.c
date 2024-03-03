#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "ui/ui_cmdln.h"

// the command line struct with buffer and several pointers
struct _command_line cmdln;

void cmdln_init(void)
{
	for(uint32_t i=0; i<UI_CMDBUFFSIZE; i++) cmdln.buf[i]=0x00;
	cmdln.wptr=0;
    cmdln.rptr=0;
    cmdln.histptr=0;
    cmdln.cursptr=0;  
}
//pointer update, rolls over
uint32_t cmdln_pu(uint32_t i)
{
    return ((i)&(UI_CMDBUFFSIZE-1));
}

bool cmdln_try_add(char *c)
{
    //TODO: leave one space for 0x00 command seperator????
    if(cmdln_pu(cmdln.wptr+1)==cmdln_pu(cmdln.rptr)) 
    {
        return false;
    }
    cmdln.buf[cmdln.wptr]=(*c); 
    cmdln.wptr=cmdln_pu(cmdln.wptr+1);
    return true;
}

bool cmdln_try_remove(char *c)
{
    if(cmdln_pu(cmdln.rptr) == cmdln_pu(cmdln.wptr))
    {
        return false;
    }

    (*c)=cmdln.buf[cmdln.rptr]; 
    cmdln.rptr=cmdln_pu(cmdln.rptr+1);
    return true;
}

bool cmdln_try_peek(uint32_t i, char *c)
{
    if(cmdln_pu(cmdln.rptr+i)==cmdln_pu(cmdln.wptr))
    {
        return false;
    }

    (*c)=cmdln.buf[cmdln_pu(cmdln.rptr+i)]; 
    return true;
}

bool cmdln_try_discard(uint32_t i)
{
    //this isn't very effective, maybe just not use it??
    //if(cmdln_pu(cmdln.rptr+i) == cmdln_pu(cmdln.wprt))
    //{
     //   return false;
    //}
    cmdln.rptr=cmdln_pu(cmdln.rptr+i);
    return true;
}

bool cmdln_next_buf_pos(void)
{
    cmdln.rptr=cmdln.wptr; 
    cmdln.cursptr=cmdln.wptr; 
    cmdln.histptr=0;
}



