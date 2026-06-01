#include "llmsession.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inputevent.h"
#include "llmdriver.h"
#include "llmruntime.h"
#include "the.h"
#include "proto.h"
#include "thedriver.h"
#include "transientui.h"
#include "vars.h"

typedef struct
{
   TransientUiKind kind;
   TransientUiReadvState readv;
   TransientUiDialogState dialog;
   TransientUiPopupState popup;
   TransientUiAction last_action;
} LlmTransientSession;

static LlmDriverScreenView previous_view;
static int previous_view_valid = 0;
static LlmTransientSession transient_session;

static const char *llm_popup_items[] =
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
      if (tolower((unsigned char)*left) !=
          tolower((unsigned char)*right))
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

static void llm_session_refresh(void)
{
   if (CURRENT_VIEW != NULL && CURRENT_FILE != NULL)
      pre_process_line(CURRENT_VIEW, CURRENT_VIEW->focus_line, (LINE *)NULL);
   build_screen(current_screen);
   display_screen(current_screen);
   if (the_driver_is_headless())
   {
      the_driver_set_current_screen(current_screen);
      if (CURRENT_VIEW != NULL)
         the_driver_set_screen_current_role(current_screen,
                                            CURRENT_VIEW->current_window);
   }
}

static void print_ack(int ok, short rc, const char *status)
{
   printf("{\"ok\":%d,\"rc\":%d,\"lastrc\":%d,\"status\":",
          ok ? 1 : 0, (int)rc, (int)lastrc);
   print_json_string(status);
   if (last_message != NULL)
   {
      fputs(",\"last_message\":", stdout);
      print_json_string((const char *)last_message);
   }
   fputs("}\n", stdout);
   fflush(stdout);
}

static void print_capabilities(void)
{
   CHARTYPE crexx_version[128];

   get_crexx_interpreter_version(crexx_version);
   fputs("{\"surface\":\"the\"", stdout);
   fputs(",\"driver\":\"llm\"", stdout);
   fputs(",\"curses\":false", stdout);
   fputs(",\"command_dispatcher\":\"full-the\"", stdout);
   fputs(",\"full_the_dispatcher\":true", stdout);
   fputs(",\"editor_runtime\":true", stdout);
   fputs(",\"real_buffers\":true", stdout);
   fputs(",\"profiles\":true", stdout);
   fputs(",\"prefix_commands\":\"full-runtime\"", stdout);
   fputs(",\"selection\":\"full-runtime-block-state\"", stdout);
   fputs(",\"buffers\":true", stdout);
   fputs(",\"file_ring\":true", stdout);
   fputs(",\"syntax_style_spans\":true", stdout);
#ifdef USE_SDSLH
   fputs(",\"parser_diagnostics\":\"first-class-snapshot-array\"", stdout);
   fputs(",\"sdslh\":true", stdout);
#else
   fputs(",\"parser_diagnostics\":\"unavailable-in-this-build\"", stdout);
   fputs(",\"sdslh\":false", stdout);
#endif
#ifdef USE_CREXX
   fputs(",\"crexx_macros\":true", stdout);
#else
   fputs(",\"crexx_macros\":false", stdout);
#endif
   fputs(",\"crexx\":", stdout);
   print_json_string((const char *)crexx_version);
   fputs(",\"transient_ui\":\"shared-transientui-protocol-adapter\"", stdout);
   fputs(",\"build_test_hooks\":\"external-shell-or-ctest\"", stdout);
   fputs(",\"inputs\":[\"look\",\"delta\",\"capabilities\",\"focus\",\"hit\",\"key\",\"text\",\"type\",\"command\",\"debug\",\"transient\",\"quit\"]", stdout);
   fputs(",\"view_modes\":[\"full\",\"filearea\",\"reserved\",\"prefix\",\"focus\"]", stdout);
   fputs(",\"transient_commands\":[\"transient readv [TEXT]\",\"transient dialog [TEXT]\",\"transient popup\",\"transient look\",\"transient key NAME\",\"transient text TEXT\",\"transient hit ROW COL\",\"transient result\",\"transient close\",\"transient cancel\"]", stdout);
   fputs(",\"debug_commands\":[\"describe-focus\",\"describe-row\",\"list-visible-rows\",\"dump-cursor-mapping\",\"dump-driver-ops\",\"explain-last-render\"]", stdout);
   fputs("}\n", stdout);
   fflush(stdout);
}

static void print_view(char *args, int delta)
{
   static char out[262144];
   LlmDriverFormatOptions options;
   LlmDriverScreenView view;

   parse_view_options(args, &options);
   llm_session_refresh();
   if (!llm_runtime_screen_view(current_screen, &view))
   {
      print_ack(0, RC_INVALID_ENVIRON, "snapshot unavailable");
      return;
   }
   if (delta && previous_view_valid)
      llm_driver_format_delta_view(&previous_view, &view, &options, out,
                                   sizeof(out));
   else
      llm_driver_format_semantic_view_with_options(&view, &options, out,
                                                   sizeof(out));
   previous_view = view;
   previous_view_valid = 1;
   fputs(out, stdout);
   fflush(stdout);
}

static int apply_key_name(const char *name)
{
   TheInputEvent input;
   int key = -1;

   if (!the_input_event_from_key_name(name, &input)
   ||  !the_input_event_to_legacy_key(&input, &key))
      return 0;
   process_key(key, FALSE);
   llm_session_refresh();
   return 1;
}

static int apply_text_bytes(const char *text)
{
   const unsigned char *ptr;

   if (text == NULL)
      text = "";
   for (ptr = (const unsigned char *)text; *ptr != '\0'; ptr++)
      process_key((int)*ptr, FALSE);
   llm_session_refresh();
   return 1;
}

static int focus_command(const char *name)
{
   if (CURRENT_VIEW == NULL)
      return 0;
   if (ascii_equal_ci(name, "command"))
   {
      CURRENT_VIEW->current_window = WINDOW_COMMAND;
      if (the_driver_is_headless())
         the_driver_set_screen_current_role(current_screen, WINDOW_COMMAND);
      (void)THEcursor_cmdline(current_screen, CURRENT_VIEW,
                              CURRENT_VIEW->cmdline_col + 1);
      return 1;
   }
   if (ascii_equal_ci(name, "prefix"))
   {
      CURRENT_VIEW->current_window = WINDOW_PREFIX;
      if (the_driver_is_headless())
         the_driver_set_screen_current_role(current_screen, WINDOW_PREFIX);
      if (the_driver != NULL && the_driver->move_prefix_cursor != NULL)
         the_driver->move_prefix_cursor(current_screen,
                                        CURRENT_VIEW->current_row, 0);
      return 1;
   }
   if (ascii_equal_ci(name, "filearea") || ascii_equal_ci(name, "file"))
   {
      CURRENT_VIEW->current_window = WINDOW_FILEAREA;
      if (the_driver_is_headless())
         the_driver_set_screen_current_role(current_screen, WINDOW_FILEAREA);
      (void)THEcursor_goto(CURRENT_VIEW->focus_line > 0
                              ? CURRENT_VIEW->focus_line : 1,
                           CURRENT_VIEW->current_column > 0
                              ? CURRENT_VIEW->current_column : 1);
      return 1;
   }
   return 0;
}

static int apply_logical_hit(char *args)
{
   TheInputLogicalTargetKind target_kind;
   char *target;
   char *line_text;
   char *row_text;
   char *cell_text;
   char *screen_text;
   char *window_text;
   long line_number;
   long row;
   long cell;
   long screen_arg = -1;
   long window_id = -1;

   if (CURRENT_VIEW == NULL || args == NULL)
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
   if (screen_text != NULL && !parse_long_token(screen_text, &screen_arg))
      return 0;
   if (window_text != NULL && !parse_long_token(window_text, &window_id))
      return 0;
   (void)screen_arg;
   (void)window_id;
   if (target_kind == THE_INPUT_TARGET_COMMAND)
   {
      CURRENT_VIEW->current_window = WINDOW_COMMAND;
      CURRENT_VIEW->cmdline_col = (int)cell;
      if (the_driver_is_headless())
         the_driver_set_screen_current_role(current_screen, WINDOW_COMMAND);
      (void)THEcursor_cmdline(current_screen, CURRENT_VIEW,
                              CURRENT_VIEW->cmdline_col + 1);
      return 1;
   }
   if (target_kind == THE_INPUT_TARGET_PREFIX)
   {
      CURRENT_VIEW->current_window = WINDOW_PREFIX;
      if (the_driver_is_headless())
         the_driver_set_screen_current_role(current_screen, WINDOW_PREFIX);
      if (the_driver != NULL && the_driver->move_prefix_cursor != NULL)
         the_driver->move_prefix_cursor(current_screen, (short)row,
                                        (short)cell);
      return 1;
   }
   if (target_kind == THE_INPUT_TARGET_FILEAREA)
   {
      LINETYPE target_line = (LINETYPE)line_number;

      CURRENT_VIEW->current_window = WINDOW_FILEAREA;
      if (the_driver_is_headless())
         the_driver_set_screen_current_role(current_screen, WINDOW_FILEAREA);
      if (target_line <= 0)
         target_line = CURRENT_VIEW->current_line + (LINETYPE)row;
      (void)THEcursor_goto(target_line, (LENGTHTYPE)cell + 1);
      return 1;
   }
   return 0;
}

static short apply_real_command(char *command)
{
   short rc;

   if (command == NULL)
      command = "";
   rc = command_line((CHARTYPE *)command, COMMAND_ONLY_FALSE);
   llm_session_refresh();
   return rc;
}

static void print_debug(char *args)
{
   char view_args[64];
   size_t i;

   if (args == NULL)
      args = "";
   if (ascii_equal_ci(args, "dump-driver-ops"))
   {
      fputs("{\"debug\":\"dump-driver-ops\",\"ops\":[", stdout);
      for (i = 0; i < the_driver_log_count(); i++)
      {
         const char *entry = the_driver_log_entry(i);

         if (i > 0)
            fputc(',', stdout);
         print_json_string(entry);
      }
      fputs("]}\n", stdout);
      fflush(stdout);
      return;
   }

   copy_text(view_args, sizeof(view_args), "focus compact");
   print_view(view_args, 0);
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

static void build_transient_snapshot(const LlmTransientSession *session,
                                     TransientUiSnapshot *snapshot)
{
   static const char *dialog_prompt[] =
   {
      "Full-runtime modal adapter",
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
         snapshot, 2, 4, 10, 40, "THE DIALOG", dialog_prompt, 2,
         session->dialog.edit.text, session->dialog.edit.cursor_cell, 1,
         buttons, 2, &session->dialog);
   else if (session->kind == TRANSIENT_UI_KIND_POPUP)
      transient_ui_snapshot_build_popup(snapshot, 2, 6, &session->popup,
                                        llm_popup_items);
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

static void print_transient_result(const LlmTransientSession *session)
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

static void print_transient(char *args)
{
   TransientUiSnapshot snapshot;
   char out[32768];

   args = trim(args);
   if (args == NULL || *args == '\0' || ascii_equal_ci(args, "look"))
   {
      build_transient_snapshot(&transient_session, &snapshot);
      transient_ui_format_snapshot(&snapshot, out, sizeof(out));
      fputs(out, stdout);
      fputc('\n', stdout);
      fflush(stdout);
      return;
   }
   if (ascii_starts_ci(args, "readv"))
   {
      char *text = trim(args + 5);

      transient_session.kind = TRANSIENT_UI_KIND_READV;
      transient_ui_readv_state_init(&transient_session.readv,
                                    text != NULL && *text != '\0'
                                       ? text : "runtime input",
                                    -1, 0, 50);
      transient_session.last_action = TRANSIENT_UI_ACTION_NONE;
      print_ack(1, RC_OK, "transient readv");
      return;
   }
   if (ascii_starts_ci(args, "dialog"))
   {
      char *text = trim(args + 6);

      transient_session.kind = TRANSIENT_UI_KIND_DIALOG;
      transient_ui_dialog_state_init(&transient_session.dialog, 1, 2, 0,
                                     text != NULL && *text != '\0'
                                        ? text : "runtime edit");
      transient_session.last_action = TRANSIENT_UI_ACTION_NONE;
      print_ack(1, RC_OK, "transient dialog");
      return;
   }
   if (ascii_equal_ci(args, "popup"))
   {
      transient_session.kind = TRANSIENT_UI_KIND_POPUP;
      transient_ui_popup_state_init(&transient_session.popup, 5, 24, 4, 32,
                                    0, 4, llm_popup_items);
      transient_session.last_action = TRANSIENT_UI_ACTION_NONE;
      print_ack(1, RC_OK, "transient popup");
      return;
   }
   if (ascii_starts_ci(args, "key "))
   {
      TransientUiKey key;

      if (!transient_key_from_name(trim(args + 4), &key))
      {
         print_ack(0, RC_INVALID_OPERAND, "bad transient key");
         return;
      }
      if (transient_session.kind == TRANSIENT_UI_KIND_READV)
         transient_session.last_action = transient_ui_readv_handle_key(
            &transient_session.readv, key);
      else if (transient_session.kind == TRANSIENT_UI_KIND_DIALOG)
         transient_session.last_action = transient_ui_dialog_handle_key(
            &transient_session.dialog, key);
      else if (transient_session.kind == TRANSIENT_UI_KIND_POPUP)
         transient_session.last_action = transient_ui_popup_handle_key(
            &transient_session.popup, llm_popup_items, key);
      else
         transient_session.last_action = TRANSIENT_UI_ACTION_NONE;
      print_transient_result(&transient_session);
      return;
   }
   if (ascii_starts_ci(args, "text "))
   {
      char *text = trim(args + 5);
      int ok = 0;

      if (transient_session.kind == TRANSIENT_UI_KIND_READV)
         ok = transient_ui_readv_insert_text(&transient_session.readv, text);
      else if (transient_session.kind == TRANSIENT_UI_KIND_DIALOG)
         ok = transient_ui_readv_insert_text(&transient_session.dialog.edit,
                                             text);
      transient_session.last_action = TRANSIENT_UI_ACTION_NONE;
      if (ok)
         print_transient_result(&transient_session);
      else
         print_ack(1, RC_OK, "transient text ignored");
      return;
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
         print_ack(0, RC_INVALID_OPERAND, "bad transient hit");
         return;
      }
      build_transient_snapshot(&transient_session, &snapshot);
      if (transient_session.kind == TRANSIENT_UI_KIND_DIALOG)
         transient_session.last_action = transient_ui_dialog_handle_hit(
            &transient_session.dialog, &snapshot, (int)row, (int)col);
      else if (transient_session.kind == TRANSIENT_UI_KIND_POPUP)
         transient_session.last_action = transient_ui_popup_handle_hit(
            &transient_session.popup, &snapshot, (int)row, (int)col);
      else if (transient_session.kind == TRANSIENT_UI_KIND_READV)
      {
         TransientUiHitTarget hit;

         transient_session.last_action = transient_ui_hit_test(
            &snapshot, (int)row, (int)col, &hit)
            ? TRANSIENT_UI_ACTION_FOCUS_CHANGED : TRANSIENT_UI_ACTION_NONE;
      }
      else
         transient_session.last_action = TRANSIENT_UI_ACTION_NONE;
      print_transient_result(&transient_session);
      return;
   }
   if (ascii_equal_ci(args, "result"))
   {
      print_transient_result(&transient_session);
      return;
   }
   if (ascii_equal_ci(args, "cancel"))
   {
      transient_session.last_action = TRANSIENT_UI_ACTION_CANCEL;
      print_transient_result(&transient_session);
      memset(&transient_session, 0, sizeof(transient_session));
      return;
   }
   if (ascii_equal_ci(args, "close") || ascii_equal_ci(args, "clear"))
   {
      memset(&transient_session, 0, sizeof(transient_session));
      print_ack(1, RC_OK, "transient cleared");
      return;
   }
   print_ack(0, RC_INVALID_OPERAND, "bad transient command");
}

int llm_session_run_protocol(void)
{
   char line[4096];

   previous_view_valid = 0;
   memset(&transient_session, 0, sizeof(transient_session));
   (void)focus_command("filearea");
   llm_session_refresh();
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
         print_ack(1, RC_OK, "bye");
         break;
      }
      if (ascii_equal_ci(command, "capabilities")
      ||  ascii_equal_ci(command, "capability")
      ||  ascii_equal_ci(command, "debug capabilities"))
      {
         print_capabilities();
         continue;
      }
      if (ascii_equal_ci(command, "look")
      ||  ascii_starts_ci(command, "look ")
      ||  ascii_equal_ci(command, "delta")
      ||  ascii_starts_ci(command, "delta "))
      {
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
         print_view(args, delta);
         continue;
      }
      if (ascii_starts_ci(command, "focus "))
      {
         int ok = focus_command(trim(command + 6));

         llm_session_refresh();
         print_ack(ok, ok ? RC_OK : RC_INVALID_OPERAND,
                   ok ? "focus changed" : "unknown focus target");
         continue;
      }
      if (ascii_starts_ci(command, "hit "))
      {
         int ok = apply_logical_hit(trim(command + 4));

         llm_session_refresh();
         print_ack(ok, ok ? RC_OK : RC_INVALID_OPERAND,
                   ok ? "hit applied" : "bad hit");
         continue;
      }
      if (ascii_starts_ci(command, "key "))
      {
         int ok = apply_key_name(trim(command + 4));

         print_ack(ok, ok ? RC_OK : RC_INVALID_OPERAND,
                   ok ? "key applied" : "bad key");
         continue;
      }
      if (ascii_starts_ci(command, "text "))
      {
         print_ack(apply_text_bytes(command + 5), RC_OK, "text applied");
         continue;
      }
      if (ascii_starts_ci(command, "type "))
      {
         print_ack(apply_text_bytes(command + 5), RC_OK, "text applied");
         continue;
      }
      if (ascii_starts_ci(command, "command "))
      {
         short rc = apply_real_command(trim(original + 8));

         print_ack(rc >= 0, rc, rc >= 0 ? "command dispatched"
                                         : "command failed");
         continue;
      }
      if (ascii_starts_ci(command, "debug "))
      {
         print_debug(trim(command + 6));
         continue;
      }
      if (ascii_starts_ci(command, "transient"))
      {
         print_transient(trim(command + 9));
         continue;
      }
      {
         short rc = apply_real_command(original);

         print_ack(rc >= 0, rc, rc >= 0 ? "command dispatched"
                                         : "command failed");
      }
   }
   return 0;
}
