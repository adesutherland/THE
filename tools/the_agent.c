#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agentdriver.h"
#include "transientui.h"

typedef struct
{
   TransientUiKind kind;
   TransientUiReadvState readv;
   TransientUiDialogState dialog;
   TransientUiPopupState popup;
   TransientUiAction last_action;
} AgentTransientSession;

static const char *agent_popup_items[] =
{
   "Open",
   "Save",
   "-----",
   "Close"
};

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

static int parse_long_token(const char *token, long *value)
{
   char *end = NULL;
   long parsed;

   if (token == NULL || *token == '\0' || value == NULL)
      return 0;
   parsed = strtol(token, &end, 10);
   if (end == token || end == NULL || *end != '\0')
      return 0;
   *value = parsed;
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
   fputs(",\"sos_commands\":\"navigation-and-edit-subset\"", stdout);
   fputs(",\"crexx_macros\":false", stdout);
   fputs(",\"prefix_commands\":\"agent-editing-subset\"", stdout);
   fputs(",\"selection\":true", stdout);
   fputs(",\"undo_redo\":true", stdout);
   fputs(",\"buffers\":true", stdout);
   fputs(",\"project_files\":true", stdout);
   fputs(",\"delta_views\":true", stdout);
   fputs(",\"transient_ui\":true", stdout);
   fputs(",\"mouse\":\"logical-hit-subset\"", stdout);
   fputs(",\"build_test_hooks\":\"external-shell-or-ctest\"", stdout);
   fputs(",\"inputs\":[\"look\",\"delta\",\"capabilities\",\"focus\",\"hit\",\"key\",\"text\",\"type\",\"command\",\"debug\",\"transient\",\"quit\"]", stdout);
   fputs(",\"view_modes\":[\"full\",\"filearea\",\"reserved\",\"prefix\",\"focus\"]", stdout);
   fputs(",\"supported_commands\":[\"focus command\",\"focus filearea\",\"focus prefix\",\"hit TARGET LINE ROW CELL [SCREEN WINDOW]\",\"left\",\"right\",\"up\",\"down\",\"home\",\"end\",\"pageup\",\"pagedown\",\"tab\",\"backtab\",\"top\",\"bottom\",\"delete\",\"backspace\",\"goto N\",\"find TEXT\",\"find-next\",\"find-prev\",\"replace OLD NEW\",\"replace /OLD/NEW/\",\"replace-all OLD NEW\",\"rows N\",\"cols N\",\"insert TEXT\",\"type TEXT\",\"setline TEXT\",\"insertline TEXT\",\"appendline TEXT\",\"deleteline\",\"duplicateline\",\"prefix LINE COMMAND\",\"prefix-clear [LINE|all]\",\"prefix-execute\",\"select L1 C1 L2 C2\",\"selection-copy\",\"selection-delete\",\"selection-replace TEXT\",\"undo\",\"redo\",\"buffer-open PATH\",\"buffer-switch INDEX|PATH\",\"buffer-list\",\"buffer-close[!] [INDEX|PATH]\",\"project-list [DIR]\",\"open PATH\",\"open! PATH\",\"new\",\"new!\",\"save [PATH]\",\"write [PATH]\",\"sos COMMAND\"]", stdout);
   fputs(",\"supported_prefix_commands\":[\"d|del|delete\",\"dup|copy\",\"r TEXT\",\"i TEXT\",\"a TEXT\"]", stdout);
   fputs(",\"transient_commands\":[\"transient readv [TEXT]\",\"transient dialog [TEXT]\",\"transient popup\",\"transient look\",\"transient key NAME\",\"transient text TEXT\",\"transient hit ROW COL\",\"transient result\",\"transient close\"]", stdout);
   fputs(",\"supported_sos_commands\":[\"topedge\",\"bottomedge\",\"leftedge\",\"rightedge\",\"firstcol\",\"lastcol\",\"endchar\",\"firstchar\",\"delchar\",\"cuadelchar\",\"delback\",\"cuadelback\",\"delend\",\"delword\",\"prefix\",\"tabfieldf\",\"tabfieldb\",\"qcmnd\",\"execute\"]", stdout);
   fputs(",\"debug_commands\":[\"describe-focus\",\"describe-row\",\"list-visible-rows\",\"dump-cursor-mapping\",\"dump-driver-ops\",\"explain-last-render\"]", stdout);
   fputs(",\"outside_llm_headless_target\":[", stdout);
   fputs("{\"name\":\"full THE command dispatcher\",\"reason\":\"requires the full editor command/profile runtime and is intentionally separate from the no-curses agent command subset\"}", stdout);
   fputs(",{\"name\":\"CREXX macros\",\"reason\":\"require CREXX and the full THE macro/profile integration surface, not the driver-boundary agent target\"}", stdout);
   fputs(",{\"name\":\"terminal mouse packets\",\"reason\":\"the agent path consumes logical hit targets from snapshots instead of terminal escape sequences\"}", stdout);
   fputs(",{\"name\":\"build and test execution\",\"reason\":\"agents should run shell, CMake, or CTest directly outside THE; embedding process execution would couple the editor driver to the host automation layer\"}", stdout);
   fputs("]", stdout);
   fputs(",\"use_agent_for\":[\"no-curses editing sessions\",\"logical snapshots and deltas\",\"normalized key/text input\",\"logical hit input\",\"prefix subset editing\",\"selection operations\",\"buffer/project listing\",\"transient modal demos\"]", stdout);
   fputs("}\n", stdout);
   fflush(stdout);
}

static void usage(FILE *out)
{
   fputs("usage: the_agent [--rows N] [--cols N] [file]\n", out);
   fputs("stdin commands: look, capabilities, focus command|filearea,\n",
         out);
   fputs("                hit TARGET LINE ROW CELL [SCREEN WINDOW],\n", out);
   fputs("                key NAME, text TEXT, command THE-COMMAND,\n", out);
   fputs("                delta, transient readv|dialog|popup, open/save/find/replace/line commands, quit\n", out);
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

static int apply_logical_hit(AgentDriver *driver, char *args)
{
   TheInputLogicalTargetKind target_kind;
   TheInputEvent input;
   char *target;
   char *line_text;
   char *row_text;
   char *cell_text;
   char *screen_text;
   char *window_text;
   long line_number;
   long row;
   long cell;
   long screen = -1;
   long window_id = -1;

   if (driver == NULL || args == NULL)
      return 0;
   target = strtok(args, " \t");
   line_text = strtok(NULL, " \t");
   row_text = strtok(NULL, " \t");
   cell_text = strtok(NULL, " \t");
   screen_text = strtok(NULL, " \t");
   window_text = strtok(NULL, " \t");
   if (!the_input_logical_target_kind_from_name(target, &target_kind)
   ||  !parse_long_token(line_text, &line_number)
   ||  !parse_long_token(row_text, &row)
   ||  !parse_long_token(cell_text, &cell))
      return 0;
   if (screen_text != NULL && !parse_long_token(screen_text, &screen))
      return 0;
   if (window_text != NULL && !parse_long_token(window_text, &window_id))
      return 0;
   if (line_number < 0 || row < 0 || cell < 0)
      return 0;
   if (!the_input_event_from_logical_target(target_kind, (LINETYPE)line_number,
                                            (int)row, (int)cell,
                                            (int)screen, (int)window_id,
                                            &input))
      return 0;
   return agent_driver_apply_input(driver, &input);
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
   if (ascii_starts_ci(text, "hit "))
      return apply_logical_hit(driver, trim(text + 4));
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

static int transient_key_from_name(const char *name, TransientUiKey *out)
{
   if (out != NULL)
      *out = TRANSIENT_UI_KEY_NONE;
   if (name == NULL || out == NULL)
      return 0;
   if (ascii_equal_ci(name, "tab"))
      *out = TRANSIENT_UI_KEY_TAB;
   else if (ascii_equal_ci(name, "backtab"))
      *out = TRANSIENT_UI_KEY_BACKTAB;
   else if (ascii_equal_ci(name, "up"))
      *out = TRANSIENT_UI_KEY_UP;
   else if (ascii_equal_ci(name, "down"))
      *out = TRANSIENT_UI_KEY_DOWN;
   else if (ascii_equal_ci(name, "left"))
      *out = TRANSIENT_UI_KEY_LEFT;
   else if (ascii_equal_ci(name, "right"))
      *out = TRANSIENT_UI_KEY_RIGHT;
   else if (ascii_equal_ci(name, "pageup"))
      *out = TRANSIENT_UI_KEY_PAGEUP;
   else if (ascii_equal_ci(name, "pagedown"))
      *out = TRANSIENT_UI_KEY_PAGEDOWN;
   else if (ascii_equal_ci(name, "home"))
      *out = TRANSIENT_UI_KEY_HOME;
   else if (ascii_equal_ci(name, "end"))
      *out = TRANSIENT_UI_KEY_END;
   else if (ascii_equal_ci(name, "enter") || ascii_equal_ci(name, "return"))
      *out = TRANSIENT_UI_KEY_ENTER;
   else if (ascii_equal_ci(name, "esc") || ascii_equal_ci(name, "escape"))
      *out = TRANSIENT_UI_KEY_ESCAPE;
   else if (ascii_equal_ci(name, "backspace"))
      *out = TRANSIENT_UI_KEY_BACKSPACE;
   else if (ascii_equal_ci(name, "delete"))
      *out = TRANSIENT_UI_KEY_DELETE;
   else if (ascii_equal_ci(name, "quit"))
      *out = TRANSIENT_UI_KEY_QUIT;
   else
      return 0;
   return 1;
}

static void build_transient_snapshot(const AgentTransientSession *session,
                                     TransientUiSnapshot *snapshot)
{
   static const char *dialog_prompt[] =
   {
      "Agent modal demo",
      "Use key, text, or hit commands"
   };
   TransientUiButtonSpec buttons[] =
   {
      { " OK ", 7, 4, 4 },
      { " CANCEL ", 7, 14, 8 }
   };

   if (session == NULL || snapshot == NULL)
      return;
   if (session->kind == TRANSIENT_UI_KIND_READV)
      transient_ui_snapshot_build_readv(snapshot, 2, 2, 50, &session->readv);
   else if (session->kind == TRANSIENT_UI_KIND_DIALOG)
      transient_ui_snapshot_build_dialog(
         snapshot, 2, 4, 10, 40, "AGENT DIALOG", dialog_prompt, 2,
         session->dialog.edit.text, session->dialog.edit.cursor_cell, 1,
         buttons, 2, &session->dialog);
   else if (session->kind == TRANSIENT_UI_KIND_POPUP)
      transient_ui_snapshot_build_popup(snapshot, 2, 6, &session->popup,
                                        agent_popup_items);
   else
      transient_ui_snapshot_init(snapshot, TRANSIENT_UI_KIND_NONE, 0, 0, 0, 0);
}

static const char *transient_action_name(TransientUiAction action)
{
   switch (action)
   {
      case TRANSIENT_UI_ACTION_FOCUS_CHANGED:
         return "focus-changed";
      case TRANSIENT_UI_ACTION_ACCEPT:
         return "accept";
      case TRANSIENT_UI_ACTION_CANCEL:
         return "cancel";
      case TRANSIENT_UI_ACTION_NONE:
      default:
         return "none";
   }
}

static void print_transient_result(const AgentTransientSession *session)
{
   fputs("{\"ok\":1,\"kind\":\"", stdout);
   fputs(transient_ui_kind_name(session != NULL ? session->kind
                                                : TRANSIENT_UI_KIND_NONE),
         stdout);
   fputs("\",\"action\":\"", stdout);
   fputs(transient_action_name(session != NULL ? session->last_action
                                               : TRANSIENT_UI_ACTION_NONE),
         stdout);
   fputs("\"", stdout);
   if (session != NULL && session->kind == TRANSIENT_UI_KIND_READV)
   {
      fputs(",\"text\":", stdout);
      print_json_string(session->readv.text);
   }
   else if (session != NULL && session->kind == TRANSIENT_UI_KIND_DIALOG)
   {
      fputs(",\"edit_text\":", stdout);
      print_json_string(session->dialog.edit.text);
      printf(",\"selected_button\":%d", session->dialog.selected_button);
   }
   else if (session != NULL && session->kind == TRANSIENT_UI_KIND_POPUP)
      printf(",\"selected_item\":%d", session->popup.selected_item);
   fputs("}\n", stdout);
   fflush(stdout);
}

static int handle_transient_command(AgentTransientSession *session,
                                    char *command)
{
   TransientUiSnapshot snapshot;
   char out[32768];
   char *args;

   if (session == NULL || command == NULL
   ||  !ascii_starts_ci(command, "transient"))
      return 0;
   args = trim(command + 9);
   if (args == NULL || *args == '\0' || ascii_equal_ci(args, "look"))
   {
      build_transient_snapshot(session, &snapshot);
      transient_ui_format_snapshot(&snapshot, out, sizeof(out));
      fputs(out, stdout);
      fputc('\n', stdout);
      fflush(stdout);
      return 1;
   }
   if (ascii_starts_ci(args, "readv"))
   {
      char *text = trim(args + 5);

      session->kind = TRANSIENT_UI_KIND_READV;
      transient_ui_readv_state_init(&session->readv,
                                    text != NULL && *text != '\0'
                                       ? text : "agent input",
                                    -1, 0, 50);
      session->last_action = TRANSIENT_UI_ACTION_NONE;
      print_simple_ack("transient readv");
      return 1;
   }
   if (ascii_starts_ci(args, "dialog"))
   {
      char *text = trim(args + 6);

      session->kind = TRANSIENT_UI_KIND_DIALOG;
      transient_ui_dialog_state_init(&session->dialog, 1, 2, 0,
                                     text != NULL && *text != '\0'
                                        ? text : "agent edit");
      session->last_action = TRANSIENT_UI_ACTION_NONE;
      print_simple_ack("transient dialog");
      return 1;
   }
   if (ascii_equal_ci(args, "popup"))
   {
      session->kind = TRANSIENT_UI_KIND_POPUP;
      transient_ui_popup_state_init(&session->popup, 5, 24, 4, 32, 0, 4,
                                    agent_popup_items);
      session->last_action = TRANSIENT_UI_ACTION_NONE;
      print_simple_ack("transient popup");
      return 1;
   }
   if (ascii_starts_ci(args, "key "))
   {
      TransientUiKey key;

      if (!transient_key_from_name(trim(args + 4), &key))
      {
         print_ack(0, NULL, command);
         return 1;
      }
      if (session->kind == TRANSIENT_UI_KIND_READV)
         session->last_action = transient_ui_readv_handle_key(&session->readv,
                                                              key);
      else if (session->kind == TRANSIENT_UI_KIND_DIALOG)
         session->last_action = transient_ui_dialog_handle_key(&session->dialog,
                                                               key);
      else if (session->kind == TRANSIENT_UI_KIND_POPUP)
         session->last_action = transient_ui_popup_handle_key(
            &session->popup, agent_popup_items, key);
      else
         session->last_action = TRANSIENT_UI_ACTION_NONE;
      print_transient_result(session);
      return 1;
   }
   if (ascii_starts_ci(args, "text "))
   {
      char *text = trim(args + 5);
      int ok = 0;

      if (session->kind == TRANSIENT_UI_KIND_READV)
         ok = transient_ui_readv_insert_text(&session->readv, text);
      else if (session->kind == TRANSIENT_UI_KIND_DIALOG)
         ok = transient_ui_readv_insert_text(&session->dialog.edit, text);
      session->last_action = TRANSIENT_UI_ACTION_NONE;
      if (ok)
         print_transient_result(session);
      else
         print_simple_ack("transient text ignored");
      return 1;
   }
   if (ascii_starts_ci(args, "hit "))
   {
      char *row_text = strtok(args + 4, " \t");
      char *col_text = strtok(NULL, " \t");
      long row;
      long col;

      if (!parse_long_token(row_text, &row)
      ||  !parse_long_token(col_text, &col))
      {
         print_ack(0, NULL, command);
         return 1;
      }
      build_transient_snapshot(session, &snapshot);
      if (session->kind == TRANSIENT_UI_KIND_DIALOG)
         session->last_action = transient_ui_dialog_handle_hit(
            &session->dialog, &snapshot, (int)row, (int)col);
      else if (session->kind == TRANSIENT_UI_KIND_POPUP)
         session->last_action = transient_ui_popup_handle_hit(
            &session->popup, &snapshot, (int)row, (int)col);
      else if (session->kind == TRANSIENT_UI_KIND_READV)
      {
         TransientUiHitTarget hit;

         session->last_action = transient_ui_hit_test(
            &snapshot, (int)row, (int)col, &hit)
            ? TRANSIENT_UI_ACTION_FOCUS_CHANGED : TRANSIENT_UI_ACTION_NONE;
      }
      else
         session->last_action = TRANSIENT_UI_ACTION_NONE;
      print_transient_result(session);
      return 1;
   }
   if (ascii_equal_ci(args, "result"))
   {
      print_transient_result(session);
      return 1;
   }
   if (ascii_equal_ci(args, "close") || ascii_equal_ci(args, "clear"))
   {
      memset(session, 0, sizeof(*session));
      print_simple_ack("transient cleared");
      return 1;
   }
   print_ack(0, NULL, command);
   return 1;
}

int main(int argc, char **argv)
{
   AgentDriver driver;
   AgentTransientSession transient;
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

   memset(&transient, 0, sizeof(transient));
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
      if (handle_transient_command(&transient, command))
         continue;
      if (ascii_equal_ci(command, "look")
      ||  ascii_starts_ci(command, "look ")
      ||  ascii_equal_ci(command, "delta")
      ||  ascii_starts_ci(command, "delta "))
      {
         LlmDriverFormatOptions options;
         char out[65536];
         char args[4096];
         int delta = ascii_equal_ci(command, "delta")
                  || ascii_starts_ci(command, "delta ");

         args[0] = '\0';
         if (ascii_starts_ci(command, "look "))
         {
            copy_text(args, sizeof(args), command + 5);
            if (ascii_equal_ci(trim(args), "delta")
            ||  ascii_starts_ci(trim(args), "delta "))
            {
               char *delta_args = trim(args) + 5;

               delta = 1;
               memmove(args, trim(delta_args), strlen(trim(delta_args)) + 1);
            }
         }
         else if (ascii_starts_ci(command, "delta "))
            copy_text(args, sizeof(args), command + 6);
         parse_view_options(args, &options);
         if (delta)
            agent_driver_format_delta(&driver, &options, out, sizeof(out));
         else
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
