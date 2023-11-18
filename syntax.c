#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "command_attributes.h"
#include "bytecode.h"
#include "modes.h"
#include "ui/ui_prompt.h"
#include "ui/ui_parse.h"
#include "ui/ui_const.h"
#include "ui/ui_term.h"
#include "ui/ui_cmdln.h"
#include "syntax.h"
#include "bio.h"
#include "amux.h"

#define SYN_MAX_LENGTH 512

const struct command_attributes attributes_empty;
const struct command_response response_empty;
struct command_attributes attributes;
struct prompt_result result;
struct _bytecode out[SYN_MAX_LENGTH];
struct _bytecode in[SYN_MAX_LENGTH];
const struct _bytecode bytecode_empty;
uint32_t out_cnt=0;
uint32_t in_cnt=0;

struct _output_info{
    uint8_t previous_command;
    uint8_t previous_number_format;
    uint8_t row_length;
    uint8_t row_counter;    
};

void postprocess_mode_write(struct _bytecode *in, struct _output_info *info);
void postprocess_format_print_number(struct _bytecode *in, uint32_t *value, bool read);


bool syntax_compile(struct opt_args *args)
{
    uint32_t pos=0;
    uint32_t i;
    char c;
    bool error=false;
    uint32_t slot_cnt=0;

    out_cnt=0;
    in_cnt=0;


    for(i=0; i<SYN_MAX_LENGTH; i++)
    {
        out[i]=bytecode_empty;
        in[i]=bytecode_empty;
    }

    //we need to track pin functions to avoid blowing out any existing pins
    //if a conflict is found, the compiler can throw an error
    enum bp_pin_func pin_func[HW_PINS-2];
    for(i=1;i<HW_PINS-1; i++)
    {
        pin_func[i-1]=system_config.pin_func[i]; //=BP_PIN_IO;
    }

    if(cmdln_try_peek(0,&c))
    {
        if(c=='>') cmdln_try_discard(1);
        pos++;
    }
    
    while(cmdln_try_peek(0,&c))
    {
        pos++;

        if( c<=' ' || c>'~' )
        {
            //out of ascii range
            cmdln_try_discard(1);
            continue;
        } 
       
        //if number parse it
        if(c>='0' && c<='9')
        {
            ui_parse_get_int(&result, &out[out_cnt].out_data);
            if(result.error)
            {
                printf("Error parsing integer at position %d\r\n", pos);
                return true;
            }

            //if(system_config.write_with_read)
            //{
                //out[out_cnt].command=SYN_WRITE_READ;
            //}
            //else
            //{
                out[out_cnt].command=SYN_WRITE;
            //}
            
            out[out_cnt].number_format=result.number_format;

        }
        else if(c=='"')
        {
            cmdln_try_remove(&c); //remove "
            // sanity check! is there a terminating "?
            error=true;
            i=0;
            while(cmdln_try_peek(i,&c))
            {
                if(c=='"')
                {
                    error=false;
                    break;
                }
                i++;
            }

            if(error)
            {
                printf("Error: string missing terminating '\"'");
                return true;
            }

            uint8_t k,row_length;
            k=row_length=8;

            //attributes->has_string=true; //show ASCII chars
            //attributes->number_format=df_hex; //force hex display

            while(i--)
            {
                cmdln_try_remove(&c);
                //if(system_config.write_with_read)
                //{
                    //out[out_cnt].command=SYN_WRITE_READ;
                //}
                //else
                //{
                    out[out_cnt].command=SYN_WRITE;
                //}

                out[out_cnt].out_data=c;
                out[out_cnt].has_repeat=false;
                out[out_cnt].repeat=1;                
                out[out_cnt].number_format=df_ascii;  
                out[out_cnt].bits=system_config.num_bits; 
                
                out_cnt++;

                /*if(out_cnt>=SYN_MAX_LENGTH)
                {
                    printf("Syntax exceeds available space (%d slots)\r\n", SYN_MAX_LENGTH);
                    return true;
                } */                            

            }

            cmdln_try_remove(&c); // consume the final "
            continue;
        }
        else
        {   
            uint8_t cmd;
            
            switch(c)
            {
                case 'r': cmd=SYN_READ; break; //read
                case '[': cmd=SYN_START; system_config.write_with_read=false; break; //start
                case '{': cmd=SYN_START; system_config.write_with_read=true; break; //start with read write
                case ']': case '}': cmd=SYN_STOP; break; //stop
                case 'd': cmd=SYN_DELAY_US; break; //delay us
                case 'D': cmd=SYN_DELAY_MS; break; //delay ms
                case 'a': cmd=SYN_AUX_OUTPUT; out[out_cnt].out_data=0; break; //aux low
                case 'A': cmd=SYN_AUX_OUTPUT; out[out_cnt].out_data=1; break; //aux HIGH
                case '@': cmd=SYN_AUX_INPUT; break; //aux INPUT
                case 'v': cmd=SYN_ADC; break; //voltage report once
                //case 'f': cmd=SYN_FREQ; break; //measure frequency once
                default: 
                    printf("Unknown syntax '%c' at position %d\r\n",c,pos);
                    return true;
                    break;                    
            }     

            out[out_cnt].command=cmd;

            // parsing an int value from the command line sets the pointer to the next value
            // if it's another command, we need to do that manually now to keep the pointer
            // where the next parsing function expects it
            cmdln_try_discard(1);

        }

       if(ui_parse_get_dot(&out[out_cnt].bits))
       {
            out[out_cnt].has_bits=true;
       }
       else
       {
            out[out_cnt].has_bits=false;
            out[out_cnt].bits=system_config.num_bits;
       }

       if(ui_parse_get_colon(&out[out_cnt].repeat))
       {
            out[out_cnt].has_repeat=true;
       }
       else
       {
            out[out_cnt].has_repeat=false;
            out[out_cnt].repeat=1;
       }

       if(out[out_cnt].command >= SYN_AUX_OUTPUT)
       {
            if(out[out_cnt].has_bits==false)
            {
                printf("Error: missing IO number for command %c at position %d. Try %c.0[0xff\r\n",c,pos);
                return true;
            }

            if(out[out_cnt].bits>=count_of(bio2bufiopin))
            {
                printf("%sError:%s pin IO%d is invalid\r\n", ui_term_color_error(), ui_term_color_reset(), out[out_cnt].bits);
                return true;
            }

            if(out[out_cnt].command!=SYN_ADC && pin_func[out[out_cnt].bits]!=BP_PIN_IO)
            {
                printf("%sError:%s at position %d IO%d is already in use\r\n", ui_term_color_error(), ui_term_color_reset(), pos, out[out_cnt].bits);
                //printf("IO%d already in use. Error at position %d\r\n",c,pos);
                return true;                
            }

            //pin_func[out[out_cnt].bits]=cmd;
            //AUX high and low need to set function until changed to read again...
       }

        slot_cnt+=out[out_cnt].repeat;

        if(slot_cnt>=SYN_MAX_LENGTH)
        {
            printf("Syntax exceeds available space (%d slots)\r\n", SYN_MAX_LENGTH);
            return true;
        }

        out_cnt++;

    }

    in_cnt=0;

    return false;    
}

/* to make it faster later use this struct of functions to run the commands...
struct _syntax_run syn_run[]=
{
    SYN_WRITE=0,
    SYN_WRITE_READ,
    SYN_READ,
    SYN_START,
    SYN_STOP,
    SYN_DELAY_US,
    SYN_DELAY_MS,
    SYN_AUX_LOW,
    SYN_AUX_HIGH,
    SYN_AUX_INPUT,
    SYN_ADC,
    SYN_FREQ
}
*/
static const char labels[][5]={"AUXL","AUXH"};
bool syntax_run(void)
{
    uint32_t i, received;
    
    if(!out_cnt) return true;

    in_cnt=0;

    for(i=0;i<out_cnt;i++)
    {
        in[in_cnt]=out[i];

        switch(out[i].command) 
        {
            case SYN_WRITE:
                if(in_cnt+out[i].repeat >= SYN_MAX_LENGTH)
                {
                    in[in_cnt].error_message=t[T_SYNTAX_EXCEEDS_MAX_SLOTS];
                    in[in_cnt].error=SRES_ERROR;
                    return false;
                }                
                for(uint16_t j=0; j<out[i].repeat; j++) //TODO: generate multiple packets due to ack/nack needs
                {
                    if(j>0)
                    {
                        in_cnt++;
                        in[in_cnt]=out[i];
                    }

                    modes[system_config.mode].protocol_write(&in[in_cnt],NULL);
                }
                break;
            case SYN_WRITE_READ: break;
            case SYN_READ:        
                if(in_cnt+out[i].repeat >= SYN_MAX_LENGTH)
                {
                    in[in_cnt].error_message=t[T_SYNTAX_EXCEEDS_MAX_SLOTS];
                    in[in_cnt].error=SRES_ERROR;
                    return false;
                }      
                for(uint16_t j=0; j<out[i].repeat; j++)
                {
                    if(j>0)
                    {
                        in_cnt++;
                        in[in_cnt]=out[i];
                    }

                    modes[system_config.mode].protocol_read(&in[in_cnt], (i+1<out_cnt && j+1==out[i].repeat)?&out[i+1]:NULL);
                }
                break;
            case SYN_START:
                modes[system_config.mode].protocol_start(&in[in_cnt], NULL);
                break;
            case SYN_STOP:
                modes[system_config.mode].protocol_stop(&in[in_cnt], NULL);
                break;
            case SYN_DELAY_US:
                busy_wait_us_32(out[i].repeat);
                break; 
            case SYN_DELAY_MS:
                busy_wait_ms(out[i].repeat);
                break;
            case SYN_AUX_OUTPUT: 
            	bio_output(out[i].bits);
		        bio_put((uint8_t) out[i].bits,(bool)out[i].out_data);
                system_bio_claim(true, out[i].bits, BP_PIN_IO, labels[out[i].out_data]); //this should be moved to a cleanup function to reduce overhead
                system_set_active(true, out[i].bits, &system_config.aux_active);                
                break;
            case SYN_AUX_INPUT: 
                bio_input(out[i].bits);
                in[in_cnt].in_data=bio_get(out[i].bits);
                system_bio_claim(false, out[i].bits, BP_PIN_IO, 0);
                system_set_active(false, out[i].bits, &system_config.aux_active);                
                break;
            case SYN_ADC: 
        	    //sweep adc
	            in[in_cnt].in_data=hw_adc_bio(out[i].bits);           
                break;
            //case SYN_FREQ: break;                
            default:
                printf("Unknown internal code %d\r\n", out[i].command);
                return true;
                break;
        }
                
        if(in_cnt+1>=SYN_MAX_LENGTH)
        {
            in[in_cnt].error_message=t[T_SYNTAX_EXCEEDS_MAX_SLOTS];
            in[in_cnt].error=SRES_ERROR;
            return false;
        }    

        if(in[in_cnt].error >= SRES_ERROR)
        {
            in_cnt++;
            return false; //halt execution, but let the post process show the error.
        }

        in_cnt++;    
    }

    return false;
}



bool syntax_post(void)
{
    uint32_t i, received;
    static struct _output_info info;
    
    if(!in_cnt) return true;

    info.previous_command=0xff; //set invalid command so output display works

    for(i=0;i<in_cnt;i++)
    {
        switch(in[i].command)
        {
            case SYN_WRITE:
                postprocess_mode_write(&in[i], &info);
                //printf("write %d\r\n",&in[i]);
                break;
            case SYN_DELAY_US:
            case SYN_DELAY_MS:            
                printf("\r\n%s%s:%s %s%d%s%s",
                    ui_term_color_notice(),t[T_MODE_DELAY], ui_term_color_reset(),
                    ui_term_color_num_float(), in[i].repeat, ui_term_color_reset(),
                    (in[i].command==SYN_DELAY_US? t[T_MODE_US] : t[T_MODE_MS])
                );
                break;    
            case SYN_READ:  
                postprocess_mode_write(&in[i], &info);
                break;                 
            case SYN_START:
                if(in[i].data_message) printf("\r\n%s", in[i].data_message);
                break;
            case SYN_STOP:  
                if(in[i].data_message) printf("\r\n%s", in[i].data_message);
                break;   
            case SYN_AUX_OUTPUT:
                printf("\r\nIO%s%d%s set to%s OUTPUT: %s%d%s", 
                ui_term_color_num_float(), in[i].bits, ui_term_color_notice(),ui_term_color_reset(),
                ui_term_color_num_float(), (in[i].out_data), ui_term_color_reset());     
                break;
            case SYN_AUX_INPUT:
		        printf("\r\nIO%s%d%s set to%s INPUT: %s%d%s", 
			    ui_term_color_num_float(), in[i].bits, ui_term_color_notice(),ui_term_color_reset(),
			    ui_term_color_num_float(), in[i].in_data, ui_term_color_reset());
                break;   
            case SYN_ADC:      
                received = (6600*in[i].in_data) / 4096;           
                printf("\r\n%s%s IO%d:%s %s%d.%d%sV",
                    ui_term_color_info(), t[T_MODE_ADC_VOLTAGE], in[i].bits, ui_term_color_reset(), ui_term_color_num_float(),
                    ((received)/1000), (((received)%1000)/100),
                    ui_term_color_reset()); 
                break;
            //case SYN_FREQ:      
                
                //break;
            case SYN_WRITE_READ:                                  
            default:
                printf("\r\nUnimplemented command '%c'", in[i].command+0x30);
                //return true;
                break;
        }
        info.previous_command=in[i].command;

        if(in[i].error)
        {
            printf(" (%s)",in[i].error_message);
        }
    }
    printf("\r\n");
    in_cnt=0;
    return false;
}


void postprocess_mode_write(struct _bytecode *in, struct _output_info *info)
{
    uint32_t repeat;
    uint32_t value;
    uint8_t row_length;
    bool new_line=false;

    //how many numbers per row
    row_length=8;
    if(in->number_format==df_bin || system_config.display_format==df_ascii)
    {
        row_length=4;
    }

    //if number format changed, make a new row
    if(
        in->number_format!=info->previous_number_format ||
        in->command!=info->previous_command
        )
    {
        new_line=true;
        info->row_counter=info->row_length=row_length;
        info->previous_number_format=in->number_format;
    }    

    if(in->command==SYN_WRITE)//(!system_config.write_with_read)
    {
        value=in->out_data;
        repeat=1;
        if(new_line)
        {
            printf("\r\n%sTX:%s ", ui_term_color_info(), ui_term_color_reset());
        }
    }    

    if(in->command==SYN_READ)//(!system_config.write_with_read)
    {
        repeat=1;
        value=in->in_data;
        if(new_line)
        {
            printf("\r\n%sRX:%s ", ui_term_color_info(), ui_term_color_reset());
        }        
    }        

    while(repeat--)
    {
        postprocess_format_print_number(in, &value, (in->command==SYN_READ));

        info->row_counter--;
        if(in->data_message)
        {
            printf(" %s ",in->data_message);
        }
        else
        {
            printf(" ");
        }

        if(!info->row_counter)
        {
            printf("\r\n    ");
            info->row_counter=row_length;
        } 
    }    
}

// represent d in the current display mode. If numbits=8 also display the ascii representation 
void postprocess_format_print_number(struct _bytecode *in, uint32_t *value, bool read)
{
	uint32_t mask, i, d, j;
    uint8_t num_bits, num_nibbles, display_format;
    bool color_flip;

    d=(*value);
    
    num_bits=in->bits;

    //maybe just tell it if we're reading or writing, instead of this convoluted logic pretzel
    if(!read && 
        (system_config.display_format==df_auto || 
        system_config.display_format==df_ascii || 
        in->number_format==df_ascii )
    )
    {
        display_format = in->number_format;
    }
    else
    {
        display_format = system_config.display_format;
    }

	if (num_bits<32)
    {
        mask=((1<<num_bits)-1);
    }
	else 
    {
        mask=0xFFFFFFFF;
    }
	d&=mask;

    if(display_format==df_ascii)
    {
        if((char)d>=' ' && (char)d<='~')
        {
            printf("'%c' ", (char)d);
        }
        else
        {
            printf("''  ");
        } 
    }

    //TODO: move this part to second function/third functions so we can reuse it from other number print places with custom specs that aren't in attributes
	switch(display_format)
	{
        case df_ascii: //drop through and show hex
        case df_auto:
		case df_hex:
            num_nibbles=num_bits/4;
            if(num_bits%4) num_nibbles++;
            if(num_nibbles&0b1) num_nibbles++;
            color_flip=true;
            printf("%s0x%s", "","");
            for(i=num_nibbles*4; i>0; i-=8)
            {
                printf("%s", (color_flip?ui_term_color_num_float():ui_term_color_reset()));          
                color_flip=!color_flip;
                printf("%c", ascii_hex[((d >> (i-4)) & 0x0F)]);
                printf("%c", ascii_hex[((d >> (i-8)) & 0x0F)]);

            }
            printf("%s", ui_term_color_reset());
			break;
		case df_dec:
            printf("%d", d);	
			break;
		case df_bin:	
            j=num_bits%4;
            if(j==0) j=4;
            color_flip=false;
            printf("%s0b%s", "", ui_term_color_num_float());
			for(i=0; i<num_bits; i++)
			{
                if(!j)
                {
                    if(color_flip)
                    {
                        color_flip=!color_flip;
                        printf("%s", ui_term_color_num_float());
                    }
                    else
                    {
                        color_flip=!color_flip;
                        printf("%s", ui_term_color_reset());
                    }
                    j=4;
                }
                j--;

				mask=1<<(num_bits-i-1);
				if(d&mask)
					printf("1");
				else
					printf("0");
			}
            printf("%s", ui_term_color_reset());
			break;
	}
	
    if(num_bits!=8)
    {
        printf(".%d", num_bits);
    }

    //if( attributes->has_string)
    //{
        //printf(" %s\'%c\'", ui_term_color_reset(), d);
    //}

    //printf("\r\n");
}



/*
//A. syntax begins with bus start [ or /
//B. some kind of final byte before stop flag? look ahead?
//1. Compile loop: process all commands to bytecode
//2. Run loop: run the bytecode all at once
//3. Post-process loop: process output into UI

//mode commands:
//start
//stop
//write
//read

//global commands
//delay
//aux pins
//adc
//pwm?
//freq?

struct __attribute__((packed, aligned(sizeof(uint64_t)))) _bytecode_output{
	uint8_t command; //255 command options
	uint8_t bits; //0-32 bits?
	uint16_t repeat; //0-0xffff repeat
	uint32_t data; //32 data bits
};


//need a way to generate multiple results from a single repeated command
//track by command ID? sequence number?
struct _bytecode_result{
	uint8_t error; //mode flags errors. One bit to halt execution? Other bits for warnings? ccan override the halt from configuration menu?
	uint8_t command; //copied from above for post-process
	uint8_t bits;  //copied from above for post-process
	uint16_t repeat; //copied from above for post-process
	uint32_t data; //copied from above for post-process
	uint32_t result; //up to 32bits results? BUT: how to deal with repeated reads????
}
*/