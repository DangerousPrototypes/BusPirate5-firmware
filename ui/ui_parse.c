#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include <math.h>
#include "pirate.h"
#include "commands.h"
#include "ui/ui_prompt.h" //needed for prompt_result struct
#include "ui/ui_parse.h"
#include "ui/ui_const.h"
#include "ui/ui_cmdln.h"

static const struct prompt_result empty_result;

//temporary shim to get the parser going with args
bool ui_parse_get_int_args(opt_args *arg)
{
    struct prompt_result result;
    uint32_t value;
    bool temp=ui_parse_get_int(&result, &value);

    arg->error=result.error;
    arg->no_value=result.no_value;
    arg->number_format=result.number_format;
    arg->success=result.success;
    arg->i=value;

    return temp;
}


bool ui_parse_get_string(opt_args *result)
{
    char c;
    bool ok;
    result->no_value=true;

    for(result->len=0; result->len<result->max_len; result->len++)
    {
        //take a byte, if no byte break
        ok=cmdln_try_peek(0,&c);
        if(!ok || c==0x00 || c==0x20)
        {
            result->c[result->len]=0x00;
            cmdln_try_discard(1);
            if(result->len==0)
            {
                return false;
            }
            else
            {
                return true;
            }
        }
        result->no_value=false;
        result->c[result->len]=c;
        cmdln_try_discard(1);
    }
    result->c[result->max_len]=0x00;
    result->success=true; //really we should detect too long/incomplete string and return notice
    return true; 
}

bool ui_parse_get_hex(struct prompt_result *result, uint32_t *value)
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

bool ui_parse_get_bin(struct prompt_result *result, uint32_t *value)
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

bool ui_parse_get_dec(struct prompt_result *result, uint32_t *value)
{
    char c;

    *result=empty_result;    
    result->no_value=true;
    (*value)=0;

    while(cmdln_try_peek(0,&c)) //peek at next char
    {
        if( (c<'0') || (c>'9') ) //if there is a char, and it is in range
        {
            return false;
        }
        (*value)*=10;
        (*value)+=(c-0x30);    
        cmdln_try_discard(1);//discard          
        result->success=true;
        result->no_value=false;
    }

    return result->success;
}

// decodes value from the cmdline
// XXXXXX integer
// 0xXXXX hexadecimal
// 0bXXXX bin
// TODO: return format of number in struct
bool ui_parse_get_int(struct prompt_result *result, uint32_t *value)
{
    bool r1,r2;
    char p1,p2;

    *result=empty_result;
    *value=0;

    r1=cmdln_try_peek(0,&p1);
    r2=cmdln_try_peek(1,&p2);

    if( !r1 || (p1==0x00) )// no data, end of data, or no value entered on prompt
    {
        result->no_value=true;
        return false;
    }

    if( r2 && (p2|0x20)=='x') // HEX
    {
        cmdln_try_discard(2); //remove 0x
        ui_parse_get_hex(result, value);
        result->number_format=df_hex;// whatever from ui_const
    }
    else if( r2 && (p2|0x20)=='b' ) // BIN
    {
        cmdln_try_discard(2); //remove 0b
        ui_parse_get_bin(result, value);
        result->number_format=df_bin;// whatever from ui_const        
    }
    else  // DEC
    {   
        ui_parse_get_dec(result, value);
        result->number_format=df_dec;// whatever from ui_const        
    }

	return result->success;
}

// eats up the spaces and comma's from the cmdline
void ui_parse_consume_whitespace(void)
{
	while( (cmdln.rptr!=cmdln.wptr) && ((cmdln.buf[cmdln.rptr]==' ')||(cmdln.buf[cmdln.rptr]==',')) )
	{
		cmdln.rptr=cmdln_pu(cmdln.rptr+1); 
	}
}

bool ui_parse_get_macro(struct prompt_result *result, uint32_t* value)
{
    char c;
    bool r;

	//cmdln_try_discard(1); // advance 1 position '('
	ui_parse_get_int(result, value);	//get number
	r=cmdln_try_remove(&c); // advance 1 position ')'
	if(r && c==')')
	{
		result->success=true;
    }
    else
    {
        result->error=true;
    }
	return result->success;
}

// get the repeat from the commandline (if any) XX:repeat
bool ui_parse_get_colon(uint32_t *value)
{
	prompt_result result;
    ui_parse_get_delimited_sequence(&result, ':', value);
    return result.success;
}

// get the number of bits from the commandline (if any) XXX.numbit
bool ui_parse_get_dot(uint32_t *value)
{
	prompt_result result;
    ui_parse_get_delimited_sequence(&result, '.', value);	
    return result.success;
}

// get trailing information for a command, for example :10 or .10
bool ui_parse_get_delimited_sequence(struct prompt_result *result, char delimiter, uint32_t* value)
{
    char c;
    *result=empty_result;
	
	if(cmdln_try_peek(0,&c))	// advance one, did we reach the end?
	{
		if(c==delimiter)		// we have a change in bits \o/
		{
            //check that the next char is actually numeric before continue
            // prevents eating consecutive .... s
            if(cmdln_try_peek(1,&c))
            {
                if(c>='0' && c<='9')
                {
                    cmdln_try_discard(1); //discard delimiter
                    ui_parse_get_int(result, value);
                    result->success=true;
                    return true;
                }
            }

		}
	}
	result->no_value=true;
	return false;
}

// some commands have trailing attributes like m 6, o 4 etc
// get as many as specified or error....
bool ui_parse_get_attributes(struct prompt_result *result, uint32_t* attr, uint8_t len)
{
	*result=empty_result;
	result->no_value=true;

	for(uint8_t i=0;i<len;i++)
	{
		ui_parse_consume_whitespace(); // eat whitechars
		ui_parse_get_uint32(result, &attr[i]);
		if(result->error || result->no_value)
		{
			return false;

		}
	}

	return true;
}

// get a float from user input
bool ui_parse_get_float(struct prompt_result *result, float* value)
{
	uint32_t number=0;
	uint32_t decimal=0;
	int j=0;
    bool r;
    char c;
	*result = empty_result; // initialize result with empty result

    r=cmdln_try_peek(0,&c);
    if(!r || c==0x00) // user pressed enter only
	{
		result->no_value=true;
	}
	else if( r && ((c|0x20)=='x') ) // exit
	{
		result->exit=true;
	}
	else if( ((c>='0')&&(c<='9')) || (c=='.') || (c=',') )// 1-9 decimal
    {
        if((c>='0')&&(c<='9')) //there is a number before the . or , 
        {
            ui_parse_get_dec(result, &number);
        }
        
        r=cmdln_try_peek(0,&c);
        if(r && (c=='.' || c==','))
        {
            cmdln_try_discard(1); //discard seperator
            while(cmdln_try_peek(0,&c)) //peek at next char
            {
                if( (c<'0') || (c>'9') ) //if there is a char, and it is in range
                {
                    break;
                }

                decimal*=10;
                decimal+=(c-0x30);
                cmdln_try_discard(1);//discard  
                j++;//track digits so we can find the proper divider later...           
            }
        }
		
		(*value)=(float)number;
		(*value)+=( (float)decimal / (float)pow(10,j) );

		result->success=true;

	}
    else
    { // bad result (not number)
		result->error=true;
        return false;
	}

	return true;
}

bool ui_parse_get_uint32(struct prompt_result *result, uint32_t* value)
{
    bool r;
    char c;

	*result = empty_result; // initialize result with empty result

    r=cmdln_try_peek(0,&c);
    if(!r || c==0x00) // user pressed enter only
	{
		result->no_value=true;
	}
	else if( r && ((c|0x20)=='x') ) // exit
	{
		result->exit=true;
	}
	else if((c>='0')&&(c<='9'))// 1-9 decimal
    {
        return ui_parse_get_dec(result, value);
    }
    else // bad result (not number)
    { 
        result->error=true;
    }
    cmdln_try_discard(1); //discard
	return true;
}

bool ui_parse_get_units(struct prompt_result *result, char* units, uint8_t length, uint8_t* unit_type)
{
    char c;
    bool r;
    uint8_t i=0;
	*result=empty_result;

	//get the trailing type
    ui_parse_consume_whitespace();

    for(i=0; i<length; i++)
    {
        units[i]=0x00;
    }

    i=0;
	while(cmdln_try_peek(0,&c))
    {
        if( (i<length) && c!=0x00 && c!=' ' )
        {
            units[i]=(c|0x20); //to lower case
            i++;
            cmdln_try_discard(1);
        }
        else
        {
            break;
        } 
    }
	units[length-1]=0x00;

	//TODO: write our own little string compare...
	if(units[0]=='n'&&units[1]=='s'){
		//ms
		(*unit_type)=freq_ns;	
	}else if(units[0]=='u'&&units[1]=='s'){
		//us
		(*unit_type)=freq_us;
	}else if(units[0]=='m'&&units[1]=='s'){
		//ms
		(*unit_type)=freq_ms;
	}else if(units[0]=='h'&&units[1]=='z'){
		//hz
		(*unit_type)=freq_hz;
	}else if(units[0]=='k'&&units[1]=='h'&&units[2]=='z'){
		//khz
		(*unit_type)=freq_khz;
	}else if(units[0]=='m'&&units[1]=='h'&&units[2]=='z'){
		//mhz
		(*unit_type)=freq_mhz;
	}else if(units[0]=='%'){
		//%
		(*unit_type)=freq_percent;
	}else{
		result->no_value=true;
		return false;
	}	

	result->success=true;
	return true;
}


