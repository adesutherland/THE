#ifndef THE_FRONTENDACTION_H
#define THE_FRONTENDACTION_H

#include <stddef.h>

#define THE_FRONTEND_ACTION_ARGUMENT_MAX 2048
#define THE_FRONTEND_ACTION_COMMAND_MAX 2080

typedef enum
{
   THE_FRONTEND_ACTION_NONE = 0,
   THE_FRONTEND_ACTION_FILE_OPEN,
   THE_FRONTEND_ACTION_FILE_CREATE,
   THE_FRONTEND_ACTION_FILE_SAVE,
   THE_FRONTEND_ACTION_FILE_CLOSE,
   THE_FRONTEND_ACTION_BUFFER_SWITCH,
   THE_FRONTEND_ACTION_EDIT_UNDO
} TheFrontendActionId;

typedef struct
{
   TheFrontendActionId id;
   char argument[THE_FRONTEND_ACTION_ARGUMENT_MAX + 1];
} TheFrontendAction;

typedef struct
{
   TheFrontendActionId id;
   const char *name;
   const char *menu;
   const char *label;
   int requires_argument;
} TheFrontendActionDefinition;

size_t the_frontend_action_definition_count(void);
const TheFrontendActionDefinition *the_frontend_action_definition_at(
   size_t index);
const TheFrontendActionDefinition *the_frontend_action_definition(
   TheFrontendActionId id);
const char *the_frontend_action_name(TheFrontendActionId id);
int the_frontend_action_id_from_name(const char *name,
                                     TheFrontendActionId *id);
int the_frontend_action_init(TheFrontendActionId id, const char *argument,
                             TheFrontendAction *out);
int the_frontend_action_from_name(const char *name, const char *argument,
                                  TheFrontendAction *out);
int the_frontend_action_format_command(const TheFrontendAction *action,
                                       char *out, size_t out_len);

#endif
