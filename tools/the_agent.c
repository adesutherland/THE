#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agentdriver.h"

static void copy_text(char *dest, size_t dest_len, const char *src)
{
   size_t len;

   if (dest == NULL || dest_len == 0)
      return;
   if (src == NULL)
      src = "";
   len = strlen(src);
   if (len >= dest_len)
      len = dest_len - 1;
   if (len > 0)
      memcpy(dest, src, len);
   dest[len] = '\0';
}

static char *trim(char *text)
{
   char *end;

   while (text != NULL && isspace((unsigned char)*text))
      text++;
   if (text == NULL || *text == '\0')
      return text;
   end = text + strlen(text) - 1;
   while (end > text && isspace((unsigned char)*end))
   {
      *end = '\0';
      end--;
   }
   return text;
}

static int ascii_equal_ci(const char *left, const char *right)
{
   if (left == NULL || right == NULL)
      return 0;
   while (*left != '\0' && *right != '\0')
   {
      if (tolower((unsigned char)*left) != tolower((unsigned char)*right))
         return 0;
      left++;
      right++;
   }
   return *left == '\0' && *right == '\0';
}

static int ascii_starts_ci(const char *text, const char *prefix)
{
   if (text == NULL || prefix == NULL)
      return 0;
   while (*prefix != '\0')
   {
      if (*text == '\0')
         return 0;
      if (tolower((unsigned char)*text) !=
          tolower((unsigned char)*prefix))
         return 0;
      text++;
      prefix++;
   }
   return 1;
}

static void print_json_string(const char *text)
{
   const unsigned char *ptr;

   putchar('"');
   if (text == NULL)
      text = "";
   for (ptr = (const unsigned char *)text; *ptr != '\0'; ptr++)
   {
      switch (*ptr)
      {
         case '\\':
            fputs("\\\\", stdout);
            break;
         case '"':
            fputs("\\\"", stdout);
            break;
         case '\n':
            fputs("\\n", stdout);
            break;
         case '\r':
            fputs("\\r", stdout);
            break;
         case '\t':
            fputs("\\t", stdout);
            break;
         default:
            if (*ptr < 0x20)
               printf("\\u%04x", *ptr);
            else
               putchar(*ptr);
            break;
      }
   }
   putchar('"');
}

static char *ack_logical_input(char *buffer, size_t buffer_len,
                               const char *input)
{
   char *text;

   copy_text(buffer, buffer_len, input);
   text = trim(buffer);
   if (ascii_starts_ci(text, "command "))
      text = trim(text + 8);
   return text;
}

static void print_unsupported_detail(const char *input)
{
   char buffer[4096];
   char *logical_input = ack_logical_input(buffer, sizeof(buffer), input);

   fputs(",\"unsupported\":{\"kind\":\"command\",\"input\":", stdout);
   print_json_string(logical_input);
   fputs(",\"surface\":\"the_agent\"", stdout);
   fputs(",\"reason\":\"the_agent uses a no-curses command subset, not the full THE command dispatcher\"", stdout);
   fputs(",\"capabilities_hint\":\"capabilities\"}", stdout);
}

static void print_ack(int ok, const AgentDriver *driver, const char *input)
{
   const char *status = agent_driver_status(driver);

   printf("{\"ok\":%d,\"status\":", ok ? 1 : 0);
   print_json_string(status);
   if (!ok && strcmp(status, "unsupported command") == 0)
      print_unsupported_detail(input);
   fputs("}\n", stdout);
   fflush(stdout);
}

static void print_simple_ack(const char *status)
{
   fputs("{\"ok\":1,\"status\":", stdout);
   print_json_string(status);
   fputs("}\n", stdout);
   fflush(stdout);
}

static void print_capabilities(void)
{
   fputs("{\"surface\":\"the_agent\"", stdout);
   fputs(",\"driver\":\"llm\"", stdout);
   fputs(",\"curses\":false", stdout);
   fputs(",\"command_dispatcher\":\"agent-subset\"", stdout);
   fputs(",\"full_the_dispatcher\":false", stdout);
   fputs(",\"sos_commands\":\"navigation-subset\"", stdout);
   fputs(",\"crexx_macros\":false", stdout);
   fputs(",\"prefix_commands\":false", stdout);
   fputs(",\"popup_dialogs\":false", stdout);
   fputs(",\"mouse\":false", stdout);
   fputs(",\"inputs\":[\"look\",\"capabilities\",\"focus\",\"key\",\"text\",\"type\",\"command\",\"debug\",\"quit\"]", stdout);
   fputs(",\"view_modes\":[\"full\",\"filearea\",\"reserved\",\"prefix\",\"focus\"]", stdout);
   fputs(",\"supported_commands\":[\"focus command\",\"focus filearea\",\"left\",\"right\",\"up\",\"down\",\"home\",\"end\",\"top\",\"bottom\",\"delete\",\"backspace\",\"goto N\",\"rows N\",\"cols N\",\"insert TEXT\",\"type TEXT\",\"save [PATH]\",\"write [PATH]\",\"sos NAVCOMMAND\"]", stdout);
   fputs(",\"supported_sos_commands\":[\"topedge\",\"bottomedge\",\"leftedge\",\"rightedge\",\"firstcol\",\"lastcol\",\"endchar\",\"qcmnd\",\"execute\"]", stdout);
   fputs(",\"debug_commands\":[\"describe-focus\",\"describe-row\",\"list-visible-rows\",\"dump-cursor-mapping\",\"dump-driver-ops\",\"explain-last-render\"]", stdout);
   fputs(",\"limitations\":[\"not wired to the full THE command dispatcher\",\"only SOS navigation commands are supported by the agent subset\",\"CREXX macros require the full editor integration surface\",\"popups/dialogs and mouse input are not modeled yet\"]", stdout);
   fputs(",\"use_crexx_for\":[\"full THE command execution\",\"full SOS command behavior\",\"macro/profile integration\"]", stdout);
   fputs(",\"use_agent_for\":[\"no-curses driver-boundary smoke\",\"logical snapshots\",\"normalized key/text input\"]", stdout);
   fputs("}\n", stdout);
   fflush(stdout);
}

static void usage(FILE *out)
{
   fputs("usage: the_agent [--rows N] [--cols N] [file]\n", out);
   fputs("stdin commands: look, capabilities, focus command|filearea,\n",
         out);
   fputs("                key NAME, text TEXT, command THE-COMMAND, quit\n", out);
}

static void parse_view_options(char *args, LlmDriverFormatOptions *options)
{
   char *token;

   llm_driver_format_options_init(options);
   if (args == NULL)
      return;
   token = strtok(args, " \t");
   while (token != NULL)
   {
      if (ascii_equal_ci(token, "full"))
         options->mode = LLM_DRIVER_VIEW_FULL;
      else if (ascii_equal_ci(token, "filearea"))
         options->mode = LLM_DRIVER_VIEW_FILEAREA;
      else if (ascii_equal_ci(token, "reserved"))
         options->mode = LLM_DRIVER_VIEW_RESERVED;
      else if (ascii_equal_ci(token, "prefix"))
         options->mode = LLM_DRIVER_VIEW_PREFIX;
      else if (ascii_equal_ci(token, "focus"))
         options->mode = LLM_DRIVER_VIEW_FOCUS;
      else if (ascii_equal_ci(token, "compact"))
         options->compact = 1;
      else if (ascii_starts_ci(token, "max="))
         options->max_text_cols = (int)strtol(token + 4, NULL, 10);
      else if (ascii_starts_ci(token, "first="))
         options->first_row = (int)strtol(token + 6, NULL, 10);
      else if (ascii_starts_ci(token, "rows="))
         options->row_count = (int)strtol(token + 5, NULL, 10);
      else if (ascii_starts_ci(token, "prefix="))
         options->include_prefix = strtol(token + 7, NULL, 10) != 0;
      else if (ascii_starts_ci(token, "command="))
         options->include_command = strtol(token + 8, NULL, 10) != 0;
      else if (ascii_starts_ci(token, "status="))
         options->include_status = strtol(token + 7, NULL, 10) != 0;
      else if (ascii_starts_ci(token, "cursor="))
         options->include_cursor = strtol(token + 7, NULL, 10) != 0;
      token = strtok(NULL, " \t");
   }
}

static int apply_ascii_text(AgentDriver *driver, const char *text)
{
   TheInputEvent input;
   const unsigned char *ptr;
   int ok = 1;

   if (text == NULL)
      text = "";
   for (ptr = (const unsigned char *)text; *ptr != '\0'; ptr++)
   {
      if (*ptr >= 0x80)
      {
         char command[THE_INPUT_COMMAND_MAX + 1];
         snprintf(command, sizeof(command), "type %s", text);
         if (!the_input_event_from_command(command, &input))
            return 0;
         return agent_driver_apply_input(driver, &input);
      }
      if (!the_input_event_from_text((uint32_t)*ptr, &input)
      ||  !agent_driver_apply_input(driver, &input))
         ok = 0;
   }
   return ok;
}

static int apply_command_line(AgentDriver *driver, char *line)
{
   TheInputEvent input;
   char *text = trim(line);

   if (text == NULL || *text == '\0')
      return 1;
   if (ascii_starts_ci(text, "key "))
   {
      char *name = trim(text + 4);
      if (!the_input_event_from_key_name(name, &input))
         return 0;
      return agent_driver_apply_input(driver, &input);
   }
   if (ascii_starts_ci(text, "text "))
      return apply_ascii_text(driver, text + 5);
   if (ascii_starts_ci(text, "type "))
      return apply_ascii_text(driver, text + 5);
   if (ascii_starts_ci(text, "command "))
   {
      if (!the_input_event_from_command(trim(text + 8), &input))
         return 0;
      return agent_driver_apply_input(driver, &input);
   }
   if (ascii_starts_ci(text, "debug "))
   {
      if (!the_input_event_from_debug_command(trim(text + 6), &input))
         return 0;
      return agent_driver_apply_input(driver, &input);
   }
   if (!the_input_event_from_command(text, &input))
      return 0;
   return agent_driver_apply_input(driver, &input);
}

int main(int argc, char **argv)
{
   AgentDriver driver;
   int rows = 24;
   int cols = 80;
   const char *path = NULL;
   int i;
   char line[4096];

   for (i = 1; i < argc; i++)
   {
      if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
      {
         usage(stdout);
         return 0;
      }
      if (strcmp(argv[i], "--rows") == 0 && i + 1 < argc)
      {
         rows = atoi(argv[++i]);
         continue;
      }
      if (strcmp(argv[i], "--cols") == 0 && i + 1 < argc)
      {
         cols = atoi(argv[++i]);
         continue;
      }
      path = argv[i];
   }

   agent_driver_init(&driver, rows, cols);
   if (path != NULL && !agent_driver_load_file(&driver, path))
   {
      fputs("the_agent: failed to load file\n", stderr);
      agent_driver_free(&driver);
      return 1;
   }

   while (fgets(line, sizeof(line), stdin) != NULL)
   {
      char original[4096];
      char *command;

      line[strcspn(line, "\r\n")] = '\0';
      copy_text(original, sizeof(original), line);
      command = trim(line);
      if (command == NULL || *command == '\0')
         continue;
      if (ascii_equal_ci(command, "quit") || ascii_equal_ci(command, "exit"))
      {
         print_simple_ack("bye");
         break;
      }
      if (ascii_equal_ci(command, "help"))
      {
         usage(stdout);
         continue;
      }
      if (ascii_equal_ci(command, "capabilities")
      ||  ascii_equal_ci(command, "capability")
      ||  ascii_equal_ci(command, "debug capabilities"))
      {
         print_capabilities();
         continue;
      }
      if (ascii_equal_ci(command, "look")
      ||  ascii_starts_ci(command, "look "))
      {
         LlmDriverFormatOptions options;
         char out[65536];
         char args[4096];

         args[0] = '\0';
         if (ascii_starts_ci(command, "look "))
            copy_text(args, sizeof(args), command + 5);
         parse_view_options(args, &options);
         agent_driver_format(&driver, &options, out, sizeof(out));
         fputs(out, stdout);
         fflush(stdout);
         continue;
      }
      print_ack(apply_command_line(&driver, original), &driver, original);
   }

   agent_driver_free(&driver);
   return 0;
}
