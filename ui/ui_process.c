#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "pico/multicore.h"
#include "opt_args.h"
#include "commands.h"
#include "bytecode.h"
#include "modes.h"
#include "displays.h"
#include "mode/hiz.h"
#include "ui/ui_prompt.h"
#include "ui/ui_parse.h"
#include "ui/ui_term.h"
#include "ui/ui_cmdln.h"
#include "string.h"
#include "syntax.h"


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
            multicore_fifo_push_blocking(0xf0);
            multicore_fifo_pop_blocking();
            if(syntax_run())
            {
                    multicore_fifo_push_blocking(0xf1);
                    multicore_fifo_pop_blocking(); 
                    printf("Syntax execution error\r\n");
                    return true;
            }
            multicore_fifo_push_blocking(0xf1);
            multicore_fifo_pop_blocking();            
            if(syntax_post())
            {
                    printf("Syntax post process error\r\n");
                    return true;
            }
            //printf("Bus Syntax: Success\r\n");
            return false;
        }

        //MODE macros
        if(c=='(') //first character is (, mode macro
        {
            uint32_t temp;
            prompt_result result;

            cmdln_try_discard(1);
            ui_parse_get_macro(&result, &temp);   
            if(result.success)
            {
                modes[system_config.mode].protocol_macro(temp);
            }
            else
            {
                printf("%s\r\n",t[T_MODE_ERROR_PARSING_MACRO]);
                return true;
            }  

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
        bool mode_cmd=false;
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

        struct command_result result=result_blank;
        if(!cmd_valid)
        {
            if(displays[system_config.display].display_command)
            {
	        if (displays[system_config.display].display_command(&args[0], &result))
	    	    goto cmd_ok;
            }
            if(modes[system_config.mode].protocol_command)
            {
	        if (modes[system_config.mode].protocol_command(&args[0], &result))
	    	    goto cmd_ok;
            }
            printf("%s", ui_term_color_notice());
            printf(t[T_CMDLN_INVALID_COMMAND], args[0].c);
            printf("%s\r\n", ui_term_color_reset());
            return true;
        }
        //printf("Found: %s\r\n",cmd[user_cmd_id]);

        //no such command, search TF flash card for runnable scripts

        //do we have a command? good, get the opt args
        if(parse_help())
        {
            printf("%s\r\n",t[exec_new[user_cmd_id].help_text]);
            return false;
        }

        if(system_config.mode==HIZ && !exec_new[user_cmd_id].allow_hiz)
        {
            printf("%s\r\n",HiZerror());
            //printf("\r\n")
            return true;            
        }    

        if(exec_new[user_cmd_id].parsers)
        {
            for(int i=0; i<5;i++)
            {                
                if(exec_new[user_cmd_id].parsers[i].opt_parser==NULL)
                {
                    break;
                } 
                
                args[i]=empty_opt_args;
                args[i].max_len=OPTARG_STRING_LEN;
                exec_new[user_cmd_id].parsers[i].opt_parser(&args[i]);
            }
        }

        //printf("Opt arg: %s\r\n",args[0].c);    
        //execute the command
        exec_new[user_cmd_id].command(args, &result);
       
cmd_ok:
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

