#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "modes.h"
#include "hardware/uart.h"
#include "mode/hiz.h"
#include "amux.h"
#include "auxpinfunc.h"
#include "font/font.h"
#include "ui/ui_lcd.h"
#include "ui/ui_prompt.h"
#include "ui/ui_parse.h"
#include "ui/ui_info.h"
#include "ui/ui_format.h"
#include "ui/ui_init.h"
#include "ui/ui_const.h"
#include "ui/ui_term.h"
#include "ui/ui_config.h"
#include "ui/ui_mode.h"
#include "pwm.h"
#include "freq.h"
#include "adc.h"
#include "psu.h"
#include "pullups.h"
#include "helpers.h"
#include "ui/ui_cmdln.h"
//#include "ui/ui_process.h"
#include "storage.h"
#include "string.h"
#include "syntax.h"
//#include "postprocess.h"

// const structs are init'd with 0s, we'll make them here and copy in the main loop
static const struct command_result result_blank;
static const struct opt_args empty_opt_args;
static struct opt_args args[5];

bool parse_help(void)
{
    char c,d;
    if(cmdln_try_peek(0,&c) && cmdln_try_peek(1,&d) && c=='-' && d=='h')
    {
        return true;
    }
    return false;
}

bool ui_process_commands(void)
{
    char c,d;

    if(!cmdln_try_peek(0,&c))
    {
        return false;
    }

    while(cmdln_try_peek(0,&c))
    {
        if(c==' ') //discard whitespace
        {
            cmdln_try_discard(1);
            continue;
        }

        if(c=='[' || c=='>' || c=='{') //first character is { [ or >, process as syntax
        {
        if(syntax_compile(&args[0]))
        {
                printf("Syntax compile error\r\n");
                return true;
        }
        if(syntax_run())
        {
                printf("Syntax execution error\r\n");
                return true;
        }
        if(syntax_post())
        {
                printf("Syntax post process error\r\n");
                return true;
        }
        printf("Bus Syntax: Success\r\n");
        return false;
        }
        
        //process as a command

        args[0]=empty_opt_args;
        args[0].max_len=OPTARG_STRING_LEN;
        ui_parse_get_string(&args[0]);
        //printf("Command: %s\r\n", args[0].c);

        if(args[0].no_value)
        {
            return false;
        }

        bool cmd_valid=false;
        uint32_t user_cmd_id=0;
        for(int i=0; i<count_of_cmd; i++)
        {  
            if(strcmp(args[0].c, cmd[i])==0)
            {
                user_cmd_id=i;
                cmd_valid=true;
                break;
            }
        }

        if(!cmd_valid)
        {
            printf("Invalid command: %s. Type ? for help.\r\n", args[0].c);
            return true;
        }
        //printf("Found: %s\r\n",cmd[user_cmd_id]);

        //no such command, search SD card for runnable scripts

        //do we have a command? good, get the opt args
        if(parse_help())
        {
            printf("%s\r\n",exec_new[user_cmd_id].help_text);
            return false;
        }

        if(system_config.mode==HIZ && !exec_new[user_cmd_id].allow_hiz)
        {
            printf("%s\r\n",HiZerror());
            //printf("\r\n")
            return true;            
        }    

        args[0]=empty_opt_args;
        args[0].max_len=OPTARG_STRING_LEN;
        if(exec_new[user_cmd_id].opt1_parser)
            exec_new[user_cmd_id].opt1_parser(&args[0]);
        //printf("Opt arg: %s\r\n",args[0].c);    
        //execute the command
        struct command_result result=result_blank;
        exec_new[user_cmd_id].command(&args, &result);
        
        printf("%s\r\n", ui_term_color_reset());

        while(cmdln_try_peek(0,&c))
        {
            if(c==';') //next command
            {
                cmdln_try_discard(1);
                break;
            }

            if(!cmdln_try_peek(1,&d))
            {
                return false;
            }

            if(c=='|' && d=='|') //perform next command if last failed
            {
                cmdln_try_discard(2);
                if(!result.error) return false;
                break;

            }
            else if(c=='&' && d=='&') // perform next command if previous was successful
            {
                cmdln_try_discard(2);
                if(result.error) return false;
                break;
            }

            cmdln_try_discard(1);
        }
    }

    return false;
}

