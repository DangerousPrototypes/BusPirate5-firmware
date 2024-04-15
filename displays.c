#include <stdint.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "bytecode.h"
#include "opt_args.h"
#include "commands.h"
#include "displays.h"

#include "display/default.h"
#ifdef	BP_USE_SCOPE
    #include "display/scope.h"
#endif


struct _display displays[MAXDISPLAY]={
{
	noperiodic,				// service to regular poll whether a byte ahs arrived
	disp_default_setup,		// setup UI
	disp_default_setup_exc,	// real setup
	disp_default_cleanup,	// cleanup for HiZ
	disp_default_settings,	// display settings 
	0,					    // display small help about the protocol
	"Default",				// friendly name (promptname)
	0,			// scope specific commands
	disp_default_lcd_update,// screen write
},
#ifdef BP_USE_SCOPE
{
    scope_periodic,			// service to regular poll whether a byte ahs arrived
    scope_setup,			// setup UI
    scope_setup_exc,		// real setup
    scope_cleanup,			// cleanup for HiZ
    scope_settings,			// display settings
    scope_help,				// display small help about the protocol
    "Scope",				// friendly name (promptname)
	scope_commands,			// scope specific commands
	scope_lcd_update,		// scope screen write
},
#endif
};
/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */


