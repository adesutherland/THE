#include "inputdispatch.h"

#include "the.h"
#include "proto.h"
#include "driverwindow.h"
#include "thedriver.h"
#include "vars.h"
#include "frontendpolicy.h"
#include "panelui.h"

short the_input_dispatch_command(const char *command, int restricted)
{
   if (command == NULL)
      return RC_INVALID_OPERAND;
   if (restricted && !the_frontend_policy_command_allowed(command))
   {
      display_error(0, (CHARTYPE *)
                    "Command unavailable in restricted frontend sessions",
                    FALSE);
      return RC_INVALID_ENVIRON;
   }
   return restricted ? command_line_restricted((CHARTYPE *)command)
                     : command_line((CHARTYPE *)command, COMMAND_ONLY_FALSE);
}

short the_input_dispatch_action(const TheFrontendAction *action)
{
   TheFrontendAction prepared;
   char command[THE_FRONTEND_ACTION_COMMAND_MAX + 1];
   char error[256];

   if (action == NULL)
      return RC_INVALID_OPERAND;
   prepared = *action;
   if (!the_frontend_policy_prepare_action(&prepared, error, sizeof(error)))
   {
      display_error(0, (CHARTYPE *)error, FALSE);
      return RC_INVALID_ENVIRON;
   }
   if (!the_frontend_action_format_command(&prepared, command,
                                           sizeof(command)))
      return RC_INVALID_OPERAND;
   {
      /* Frontend actions name editor built-ins. User aliases must not
       * replace Save, Open, Close or the other generated action verbs. */
      short rc = command_line_builtin_single((CHARTYPE *)command);
      if (rc == RC_OK && number_of_files > 0 && vd_current != NULL)
      {
         the_frontend_policy_apply_current_file();
         if (prepared.id == THE_FRONTEND_ACTION_FILE_OPEN
         ||  prepared.id == THE_FRONTEND_ACTION_FILE_CREATE
         ||  prepared.id == THE_FRONTEND_ACTION_BUFFER_SWITCH)
         {
            rc = command_line_builtin_single((CHARTYPE *)"set coloring on auto");
         }
      }
      return rc;
   }
}

int the_input_dispatch_logical_hit(const TheInputLogicalTarget *target)
{
   if (CURRENT_VIEW == NULL || target == NULL)
      return 0;
   return THEcursor_logical_target(target) == RC_OK;
}

short the_input_dispatch_workbench_panel(const TheInputEvent *input)
{
   if (!input || input->kind != THE_INPUT_WORKBENCH_PANEL) return RC_INVALID_OPERAND;
   return the_workbench_panel_operation(the_input_event_workbench_panel_id(input),
      the_input_event_workbench_panel_generation(input),the_input_event_workbench_panel_operation(input));
}
static int form_modal(void) { return the_panel_active!=NULL; }
short the_input_dispatch_workbench_form(const TheInputEvent *input)
{
   TheWorkbenchFormRequest request;
   if (!the_input_event_workbench_form(input,&request)) return RC_INVALID_OPERAND;
   the_workbench_forms_modal_lookup(form_modal);
   return the_workbench_form_request(&request);
}

short the_input_dispatch_workbench_filefields(const TheInputEvent *input)
{
   TheWorkbenchFileFieldsRequest request;
   if(!the_input_event_workbench_filefields(input,&request)) return RC_INVALID_OPERAND;
   the_editor_filefields_initialize();
   return the_workbench_filefields_request(&request);
}
