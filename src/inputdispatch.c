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
   return command_line((CHARTYPE *)command, COMMAND_ONLY_FALSE);
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
      short rc = command_line((CHARTYPE *)command, COMMAND_ONLY_FALSE);
      if (rc == RC_OK && number_of_files > 0 && vd_current != NULL)
         the_frontend_policy_apply_current_file();
      return rc;
   }
}

int the_input_dispatch_logical_hit(const TheInputLogicalTarget *target)
{
   if (CURRENT_VIEW == NULL || target == NULL)
      return 0;

   if (target->kind == THE_INPUT_TARGET_COMMAND)
   {
      CURRENT_VIEW->current_window = WINDOW_COMMAND;
      CURRENT_VIEW->cmdline_col = target->cell;
      if (the_driver_is_headless())
         the_driver_set_screen_current_role(current_screen, WINDOW_COMMAND);
      (void)THEcursor_cmdline(current_screen, CURRENT_VIEW,
                              CURRENT_VIEW->cmdline_col + 1);
      return 1;
   }

   if (target->kind == THE_INPUT_TARGET_PREFIX)
   {
      CURRENT_VIEW->current_window = WINDOW_PREFIX;
      if (the_driver_is_headless())
         the_driver_set_screen_current_role(current_screen, WINDOW_PREFIX);
      if (the_driver != NULL && the_driver->move_prefix_cursor != NULL)
         the_driver->move_prefix_cursor(current_screen, (short)target->row,
                                        (short)target->cell);
      return 1;
   }

   if (target->kind == THE_INPUT_TARGET_FILEAREA)
   {
      LINETYPE target_line = target->line_number;

      CURRENT_VIEW->current_window = WINDOW_FILEAREA;
      if (the_driver_is_headless())
         the_driver_set_screen_current_role(current_screen, WINDOW_FILEAREA);
      if (target_line <= 0)
         target_line = CURRENT_VIEW->current_line + (LINETYPE)target->row;
      (void)THEcursor_goto(target_line, (LENGTHTYPE)target->cell + 1);
      return 1;
   }

   return 0;
}
