#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pirate.h"
#include "opt_args.h"
#include "fatfs/ff.h"
#include "storage.h"
#include "bytecode.h"
#include "mode/hwspi.h"
#include "../lib/sfud/inc/sfud.h"
#include "../lib/sfud/inc/sfud_def.h"
#include "mode/spiflash.h"
#include "ui/ui_cmdln.h"
#include "ui/ui_prompt.h"
#include "ui/ui_const.h"
#include "ui/ui_args.h"

//TODO: parsing based on position is so much better than dancing around 
// a global unknown pointer in the cmdln code
// combine these parsing functions with the ui_parse functions....

static const struct prompt_result empty_result;


bool ui_args_find_arg(char flag, arg_var_t *arg)
{
    uint32_t i=0;
    char flag_c, dash_c;
    arg->error=false;
    arg->has_arg=false;
    while(cmdln_try_peek(i, &dash_c) && cmdln_try_peek(i+1, &flag_c))
    {
        if(dash_c=='-' && flag_c==flag)
        {
            arg->has_arg=true;

            if(!(cmdln_try_peek(i+2, &dash_c) && cmdln_try_peek(i+3, &flag_c)))
            {
                //end of buffer, no value
                arg->has_value=false;
                return true;
            } else if(dash_c!=' ')
            {
                //malformed flag
                arg->error=true;
                return false;
            } else if(flag_c=='-')
            {
                //next command, no value
                arg->has_value=false;
                return true;
            }    
            arg->has_value=true; 
            arg->value_pos=i+3;
            return true; 
        }
        i++;
    }
    return false;
}

bool ui_args_get_string(uint32_t start_pos, uint32_t *end_pos, uint32_t max_len, char *string)
{
    char c;
    for(uint32_t i=0; i<max_len; i++)
    {
        //take a byte, if no byte break
        bool ok=cmdln_try_peek(start_pos,&c);
        if(!ok || c==0x00 || c==0x20 || i==(max_len-1))
        {
            string[i]=0x00;
            *end_pos=start_pos;
            if(i==0) return false;
            else return true;
        }
        string[i]=c;
        start_pos++;
    }
}

/*
bool ui_args_get_hex(struct prompt_result *result, uint32_t *value)
{
    char c;

    *result=empty_result;
    result->no_value=true;
    (*value)=0;

    while(cmdln_try_peek(0,&c)) //peek at next char
    {
        if(((c>='0')&&(c<='9')) )
        {
            (*value)<<=4;
            (*value)+=(c-0x30);
        }
        else if( ((c|0x20)>='a') && ((c|0x20)<='f') )
        {
            (*value)<<=4;
            c|=0x20;		// to lowercase
            (*value)+=(c-0x57);	// 0x61 ('a') -0xa
        }
        else
        {
            return false;
        }
        cmdln_try_discard(1);//discard
        result->success=true;
        result->no_value=false;  
    }

    return result->success;
}

bool ui_args_get_bin(struct prompt_result *result, uint32_t *value)
{
    char c;
    *result=empty_result;
    result->no_value=true;
    (*value)=0;

    while(cmdln_try_peek(0,&c)) //peek at next char
    {
        if( (c<'0')||(c>'1') )
        {
            return false;
        }
        (*value)<<=1;
        (*value)+=c-0x30;
        cmdln_try_discard(1);//discard
        result->success=true;
        result->no_value=false;   
    }

    return result->success;    
}
*/
bool ui_args_get_dec(uint32_t start_pos, struct prompt_result *result, uint32_t *value)
{
    char c;

    *result=empty_result;    
    result->no_value=true;
    (*value)=0;

    while(cmdln_try_peek(start_pos,&c)) //peek at next char
    {
        if( (c<'0') || (c>'9') ) //if there is a char, and it is in range
        {
            return false;
        }
        (*value)*=10;
        (*value)+=(c-0x30);    
        start_pos++;          
        result->success=true;
        result->no_value=false;
    }

    return result->success;
}

// decodes value from the cmdline
// XXXXXX integer
// 0xXXXX hexadecimal
// 0bXXXX bin
bool ui_args_get_int(uint32_t start_pos, struct prompt_result *result, uint32_t *value)
{
    bool r1,r2;
    char p1,p2;

    *result=empty_result;
    *value=0;

    r1=cmdln_try_peek(start_pos,&p1);
    r2=cmdln_try_peek(start_pos+1,&p2);

    if( !r1 || (p1==0x00) )// no data, end of data, or no value entered on prompt
    {
        result->no_value=true;
        return false;
    }

    if( r2 && (p2|0x20)=='x') // HEX
    {
        //cmdln_try_discard(2); //remove 0x
        start_pos+=2;
        //ui_args_get_hex(result, value);
        result->number_format=df_hex;// whatever from ui_const
    }
    else if( r2 && (p2|0x20)=='b' ) // BIN
    {
        //cmdln_try_discard(2); //remove 0b
        start_pos+=2;
        //ui_args_get_bin(result, value);
        result->number_format=df_bin;// whatever from ui_const        
    }
    else  // DEC
    {   
        ui_args_get_dec(start_pos, result, value);
        result->number_format=df_dec;// whatever from ui_const        
    }
	return result->success;
}

bool ui_args_find_flag_uint32(char flag, arg_var_t *arg, uint32_t *value)
{
    struct prompt_result result;

    if(!ui_args_find_arg(flag, arg)) return false;
    if(arg->has_value)
    {
        if(!ui_args_get_int(arg->value_pos, &result, value))
        {
            //printf("Unknown parsing error\r\n");
            arg->error=true;
            return false;            
        }
    }
    return true;
}

bool ui_args_find_flag_string(char flag, arg_var_t *arg, uint32_t max_len, char *value)
{
    if(!ui_args_find_arg(flag, arg)) return false;

    if(arg->has_value)
    {
        uint32_t end_pos;
        if(!ui_args_get_string(arg->value_pos, &end_pos, max_len, value))
        {
            arg->error=true;
            return false;
        }
    }
    return true;
}

bool ui_args_find_flag_novalue(char flag, arg_var_t *arg)
{
    if(!ui_args_find_arg(flag, arg)) return false;
    return true;
}

bool ui_args_find_string_discard(arg_var_t *arg, bool discard, uint32_t max_len, char *value)
{
    uint32_t i=0;
    char c;
    arg->error=false;
    arg->has_value=false;
    value[0]=0x00; //null terminate for those who need it

    //the read pointer should be at the end of the command
    //next we consume 1 or more spaces,
    //then copy text to the buffer until space or - or 0x00
    while(cmdln_try_peek(i, &c))
    {
        if(c=='-'||c==0x00||c==';'||c=='&'||c=='|')
        {
            return false;
        }

        if(c!=' ')
        {
            uint32_t end_pos;
            if(ui_args_get_string(i, &end_pos, max_len, value))
            {
                arg->has_value=true;
                arg->value_pos=i;
                if(discard)
                {
                    if(!cmdln_try_discard(end_pos)) printf("Error discarding in ui_args.c\r\n");
                }
                return true;
            }
            else
            {
                value[0]=0x00;
                return false;
            }
        }

        i++;
    }
    return false;
}

bool ui_args_find_string(arg_var_t *arg, uint32_t max_len, char *value)
{
    return ui_args_find_string_discard(arg, false, max_len, value);
}

bool ui_args_find_uint32(arg_var_t *arg, uint32_t *value)
{
    struct prompt_result result;
    uint32_t i=0;
    char c;
    arg->error=false;
    arg->has_value=false;

    //the read pointer should be at the end of the command
    //next we consume 1 or more spaces,
    //then copy text to the buffer until space or - or 0x00
    while(cmdln_try_peek(i, &c))
    {
        if(c=='-'||c==0x00)
        {
            return false;
        }

        if(c!=' ')
        {
            if(ui_args_get_int(i, &result, value))
            {
                arg->has_value=true;
                arg->value_pos=i;
                return true;
            }
            else
            {
                return false;
            }
        }

        i++;
    }
    return false;
}