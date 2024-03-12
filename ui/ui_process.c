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

bool ui_process_syntax(void)
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

bool ui_process_macro(void)
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

bool ui_process_commands(void)
{
    char c,d;

    struct _command_info_t cp;
    cp.nextptr=0;
    
    while(true)
    {
        if(!cmdln_find_next_command(&cp)) return false;
        
        switch(cp.command[0])
        {
            case '[':
            case '>':
            case '{':
            case ']':
            case '}':
                return ui_process_syntax(); //first character is { [ or >, process as syntax
                break;
            case '(':
                return ui_process_macro(); //first character is (, mode macro
                break;

        }
        //process as a command
        char command_string[MAX_COMMAND_LENGTH];
 
        //string 0 is the command
        // continue if we don't get anything? could be an empty chained command? should that be error?
        if(!cmdln_args_string_by_position(0, sizeof(command_string), command_string))
        {
            continue;
        }   

        bool mode_cmd, cmd_valid =false;
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

        //global help handler (optional, set config in commands.c)
        //ui_args_find_flag_novalue('h', &arg);
        command_var_t arg;
        cmdln_find_flag('h', &arg );
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

        switch(cp.delimiter) //next action based on the command delimiter
        {
            case 0: return false; //done
            case ';': break; //next command
            case '|': if(!result.error) return false; break; //perform next command if last failed
            case '&': if(result.error) return false; break; //perform next if last success
        }
    }
    return false;
}

