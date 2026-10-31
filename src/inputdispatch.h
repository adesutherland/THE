#ifndef THE_INPUTDISPATCH_H
#define THE_INPUTDISPATCH_H

#include "inputevent.h"

int the_input_dispatch_logical_hit(const TheInputLogicalTarget *target);
short the_input_dispatch_action(const TheFrontendAction *action);
short the_input_dispatch_workbench_panel(const TheInputEvent *input);
short the_input_dispatch_workbench_form(const TheInputEvent *input);
short the_input_dispatch_command(const char *command, int restricted);

#endif
