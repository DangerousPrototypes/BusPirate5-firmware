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
#include "pirate/intercore_helpers.h"

// const structs are init'd with 0s, we'll make them here and copy in the main loop
static const struct command_result result_blank;

bool ui_process_syntax(void)
{
    if(syntax_compile())
    {
            printf("Syntax compile error\r\n");
            return true;
    }
    icm_core0_send_message(BP_ICM_DISABLE_LCD_UPDATES);
    if(syntax_run())
    {
            icm_core0_send_message(BP_ICM_ENABLE_LCD_UPDATES);
            printf("Syntax execution error\r\n");
            return true;
    }
    icm_core0_send_message(BP_ICM_ENABLE_LCD_UPDATES);
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

bool ui_process_commands(void){
    char c,d;
    struct _command_info_t cp;
    cp.nextptr=0;

/*    cmdln_info();
    cmdln_info_uint32();
    command_var_t arg;
    uint32_t value;
    if(cmdln_args_find_flag_uint32('t', &arg, &value)) printf("Value -t: %d\r\n", value);

    return false;*/    
    while(true){
        if(!cmdln_find_next_command(&cp)) return false;
        
        switch(cp.command[0]){
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
        struct command_result result=result_blank;
        //string 0 is the command
        // continue if we don't get anything? could be an empty chained command? should that be error?
        if(!cmdln_args_string_by_position(0, sizeof(command_string), command_string)){
            continue;
        }   

        enum COMMAND_TYPE {
            NONE=0,
            GLOBAL,
            MODE,
            DISPLAY 
        };

        // first search global commands
        uint32_t user_cmd_id=0;
        uint32_t command_type=NONE;
        for(int i=0; i<commands_count; i++){  
            if(strcmp(command_string, commands[i].command)==0){
                user_cmd_id=i;
                command_type=GLOBAL;
                //global help handler (optional, set config in commands.c)
                if(cmdln_args_find_flag('h')){
                    if(commands[user_cmd_id].help_text!=0x00){ 
                        printf("%s%s%s\r\n",ui_term_color_info(), t[commands[user_cmd_id].help_text], ui_term_color_reset());
                        return false;
                    }else{ // let app know we requested help
                        result.help_flag=true;
                    }
                }

                if(command_type==GLOBAL && system_config.mode==HIZ && !commands[user_cmd_id].allow_hiz && !result.help_flag){
                    printf("%s\r\n",hiz_error());
                    return true;            
                }
                commands[user_cmd_id].func(&result);
                goto cmd_ok;
            }
        }

        // if not global, search mode specific commands
        if(!command_type){
            if(modes[system_config.mode].mode_commands_count){
                for(int i=0; i< *modes[system_config.mode].mode_commands_count; i++){
                    if(strcmp(command_string, modes[system_config.mode].mode_commands[i].command)==0){
                        user_cmd_id=i;
                        command_type=MODE;
                        //mode help handler (optional, set config in modes command struct)
                        if(cmdln_args_find_flag('h')){ 
                            //show auto short help
                            if( modes[system_config.mode].mode_commands[user_cmd_id].allow_hiz
                            && (modes[system_config.mode].mode_commands[user_cmd_id].help_text!=0x00)){ 
                                printf("%s%s%s\r\n",ui_term_color_info(), t[modes[system_config.mode].mode_commands[user_cmd_id].help_text], ui_term_color_reset());
                                return false;
                            }else{ // let app know we requested help
                                result.help_flag=true;
                            }
                        }
                        modes[system_config.mode].mode_commands[user_cmd_id].func(&result);
                        goto cmd_ok;
                    }
                }
            }
        }
        
        //no such command, search storage for runnable scripts?
        
        // if nothing so far, hand off to mode parser
        if(!command_type){
            if(displays[system_config.display].display_command){
                if (displays[system_config.display].display_command(&result))
                    goto cmd_ok;
            }
            if(modes[system_config.mode].protocol_command){
                if (modes[system_config.mode].protocol_command(&result))
                    goto cmd_ok;
            }
        }

        // error no such command
        printf("%s", ui_term_color_notice());
        printf(t[T_CMDLN_INVALID_COMMAND], command_string);
        printf("%s\r\n", ui_term_color_reset());
        return true;
        
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

