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
#include "ui/ui_args.h"


// const structs are init'd with 0s, we'll make them here and copy in the main loop
static const struct command_result result_blank;

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
            if(syntax_compile())
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
        char command_string[MAX_COMMAND_LENGTH];
        arg_var_t arg;
        ui_args_find_string_discard(&arg, true, sizeof(command_string), command_string);
        if(!arg.has_value)
        {
            return false;
        }

        bool cmd_valid=false;
        bool mode_cmd=false;
        uint32_t user_cmd_id=0;
        for(int i=0; i<commands_count; i++)
        {  
            if(strcmp(command_string, commands[i].command)==0)
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
	        if (displays[system_config.display].display_command(&result))
	    	    goto cmd_ok;
            }
            if(modes[system_config.mode].protocol_command)
            {
	        if (modes[system_config.mode].protocol_command(&result))
	    	    goto cmd_ok;
            }
            printf("%s", ui_term_color_notice());
            printf(t[T_CMDLN_INVALID_COMMAND], command_string);
            printf("%s\r\n", ui_term_color_reset());
            return true;
        }
        //no such command, search TF flash card for runnable scripts

        //global help handler TODO: make optional
        ui_args_find_flag_novalue('h', &arg);
        if(arg.has_arg && (commands[user_cmd_id].help_text!=0x00))
        {
            printf("%s\r\n",t[commands[user_cmd_id].help_text]);
            return false;
        }

        if(system_config.mode==HIZ && !commands[user_cmd_id].allow_hiz)
        {
            printf("%s\r\n",hiz_error());
            return true;            
        }    

        //execute the command
        commands[user_cmd_id].func(&result);
       
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

