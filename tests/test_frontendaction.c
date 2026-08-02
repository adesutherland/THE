#include <stdio.h>
#include <string.h>

#include "frontendaction.h"

static int failures;

static void expect_true(const char *label, int value)
{
   if (!value)
   {
      fprintf(stderr, "FAIL %s\n", label);
      failures++;
   }
}

static void expect_command(const char *label, const char *name,
                           const char *argument, const char *expected)
{
   TheFrontendAction action;
   char command[THE_FRONTEND_ACTION_COMMAND_MAX + 1];

   if (!the_frontend_action_from_name(name, argument, &action)
   ||  !the_frontend_action_format_command(&action, command,
                                           sizeof(command))
   ||  strcmp(command, expected) != 0)
   {
      fprintf(stderr, "FAIL %s: expected '%s'\n", label, expected);
      failures++;
   }
}

int main(void)
{
   TheFrontendAction action;
   TheFrontendActionId id = THE_FRONTEND_ACTION_NONE;
   const TheFrontendActionDefinition *definition;

   expect_true("registry count", the_frontend_action_definition_count() == 6);
   expect_true("lookup save",
               the_frontend_action_id_from_name("file.save", &id)
               && id == THE_FRONTEND_ACTION_FILE_SAVE);
   definition = the_frontend_action_definition(id);
   expect_true("save metadata", definition != NULL
               && strcmp(definition->menu, "File") == 0
               && strcmp(definition->label, "Save") == 0);

   expect_command("open", "file.open", "/tmp/a file.txt",
                  "edit \"/tmp/a file.txt\"");
   expect_command("create", "file.create", "new.txt",
                  "edit \"new.txt\"");
   expect_command("switch", "buffer.switch", "src/the.c",
                  "edit \"src/the.c\"");
   expect_command("save", "file.save", "", "save");
   expect_command("close", "file.close", "", "quit");
   expect_command("undo", "edit.undo", "", "sos undo");

   expect_true("reject unknown",
               !the_frontend_action_from_name("file.delete", "x", &action));
   expect_true("reject missing path",
               !the_frontend_action_from_name("file.open", "", &action));
   expect_true("reject quote",
               !the_frontend_action_from_name("file.open", "bad\"name", &action));
   expect_true("reject newline",
               !the_frontend_action_from_name("file.open", "bad\nname", &action));
   expect_true("reject save argument",
               !the_frontend_action_from_name("file.save", "path", &action));

   if (failures != 0)
      return 1;
   puts("frontend action tests passed");
   return 0;
}
