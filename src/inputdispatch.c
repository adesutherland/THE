#include "inputdispatch.h"

#include "the.h"
#include "proto.h"
#include "driverwindow.h"
#include "thedriver.h"
#include "vars.h"
#include "frontendpolicy.h"

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
