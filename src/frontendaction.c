#include "frontendaction.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const TheFrontendActionDefinition frontend_actions[] =
{
   { THE_FRONTEND_ACTION_FILE_CREATE, "file.create", "File", "New", 1 },
   { THE_FRONTEND_ACTION_FILE_OPEN, "file.open", "File", "Open", 1 },
   { THE_FRONTEND_ACTION_FILE_SAVE, "file.save", "File", "Save", 0 },
   { THE_FRONTEND_ACTION_FILE_CLOSE, "file.close", "File", "Close", 0 },
   { THE_FRONTEND_ACTION_BUFFER_SWITCH, "buffer.switch", NULL,
     "Switch", 1 },
   { THE_FRONTEND_ACTION_EDIT_UNDO, "edit.undo", "Edit", "Undo", 0 }
};

static int frontend_action_argument_is_safe(const char *argument)
{
   const unsigned char *cursor = (const unsigned char *)argument;

   if (argument == NULL || *argument == '\0')
      return 0;
   while (*cursor != '\0')
   {
      if (*cursor == '"' || iscntrl(*cursor))
         return 0;
      cursor++;
   }
   return 1;
}

size_t the_frontend_action_definition_count(void)
{
   return sizeof(frontend_actions) / sizeof(frontend_actions[0]);
}

const TheFrontendActionDefinition *the_frontend_action_definition_at(
   size_t index)
{
   if (index >= the_frontend_action_definition_count())
      return NULL;
   return &frontend_actions[index];
}

const TheFrontendActionDefinition *the_frontend_action_definition(
   TheFrontendActionId id)
{
   size_t i;

   for (i = 0; i < the_frontend_action_definition_count(); i++)
   {
      if (frontend_actions[i].id == id)
         return &frontend_actions[i];
   }
   return NULL;
}

const char *the_frontend_action_name(TheFrontendActionId id)
{
   const TheFrontendActionDefinition *definition =
      the_frontend_action_definition(id);

   return definition != NULL ? definition->name : "none";
}

int the_frontend_action_id_from_name(const char *name,
                                     TheFrontendActionId *id)
{
   size_t i;

   if (name == NULL)
      return 0;
   for (i = 0; i < the_frontend_action_definition_count(); i++)
   {
      if (strcmp(frontend_actions[i].name, name) == 0)
      {
         if (id != NULL)
            *id = frontend_actions[i].id;
         return 1;
      }
   }
   return 0;
}

int the_frontend_action_init(TheFrontendActionId id, const char *argument,
                             TheFrontendAction *out)
{
   const TheFrontendActionDefinition *definition =
      the_frontend_action_definition(id);
   size_t len = argument != NULL ? strlen(argument) : 0;

   if (out == NULL || definition == NULL)
      return 0;
   memset(out, 0, sizeof(*out));
   if (definition->requires_argument)
   {
      if (!frontend_action_argument_is_safe(argument)
      ||  len > THE_FRONTEND_ACTION_ARGUMENT_MAX)
         return 0;
      memcpy(out->argument, argument, len + 1);
   }
   else if (len != 0)
      return 0;
   out->id = id;
   return 1;
}

int the_frontend_action_from_name(const char *name, const char *argument,
                                  TheFrontendAction *out)
{
   TheFrontendActionId id;

   return the_frontend_action_id_from_name(name, &id)
       && the_frontend_action_init(id, argument, out);
}

int the_frontend_action_format_command(const TheFrontendAction *action,
                                       char *out, size_t out_len)
{
   int len;

   if (action == NULL || out == NULL || out_len == 0
   ||  the_frontend_action_definition(action->id) == NULL)
      return 0;
   switch (action->id)
   {
      case THE_FRONTEND_ACTION_FILE_OPEN:
      case THE_FRONTEND_ACTION_FILE_CREATE:
      case THE_FRONTEND_ACTION_BUFFER_SWITCH:
         if (!frontend_action_argument_is_safe(action->argument))
            return 0;
         len = snprintf(out, out_len, "edit \"%s\"", action->argument);
         break;
      case THE_FRONTEND_ACTION_FILE_SAVE:
         len = snprintf(out, out_len, "save");
         break;
      case THE_FRONTEND_ACTION_FILE_CLOSE:
         len = snprintf(out, out_len, "quit");
         break;
      case THE_FRONTEND_ACTION_EDIT_UNDO:
         len = snprintf(out, out_len, "sos undo");
         break;
      case THE_FRONTEND_ACTION_NONE:
      default:
         return 0;
   }
   return len > 0 && (size_t)len < out_len;
}
