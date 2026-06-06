#include "inputevent.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "getch.h"

#ifdef THE_KEY_TAB
# define THE_INPUT_KEY_TAB THE_KEY_TAB
#else
# define THE_INPUT_KEY_TAB 0x9
#endif

typedef struct
{
   const char *name;
   int key_code;
} TheInputKeyName;

typedef struct
{
   const char *name;
   TheInputDebugCommand command;
} TheInputDebugName;

static const TheInputKeyName input_key_names[] =
{
   { "left", THE_KEY_LEFT },
   { "right", THE_KEY_RIGHT },
   { "up", THE_KEY_UP },
   { "down", THE_KEY_DOWN },
   { "home", THE_KEY_HOME },
   { "end", THE_KEY_END },
   { "pageup", THE_KEY_PPAGE },
   { "pgup", THE_KEY_PPAGE },
   { "pagedown", THE_KEY_NPAGE },
   { "pgdn", THE_KEY_NPAGE },
   { "enter", THE_KEY_ENTER },
   { "return", THE_KEY_RETURN },
   { "esc", THE_KEY_ESC },
   { "escape", THE_KEY_ESC },
   { "tab", THE_INPUT_KEY_TAB },
   { "backtab", THE_KEY_BTAB },
   { "btab", THE_KEY_BTAB },
   { "backspace", THE_KEY_BACKSPACE },
   { "delete", THE_KEY_DC },
   { "del", THE_KEY_DC },
   { "insert", THE_KEY_IC },
   { NULL, 0 }
};

static const TheInputDebugName input_debug_names[] =
{
   { "describe-focus", THE_INPUT_DEBUG_DESCRIBE_FOCUS },
   { "focus", THE_INPUT_DEBUG_DESCRIBE_FOCUS },
   { "describe-row", THE_INPUT_DEBUG_DESCRIBE_ROW },
   { "row", THE_INPUT_DEBUG_DESCRIBE_ROW },
   { "list-visible-rows", THE_INPUT_DEBUG_LIST_VISIBLE_ROWS },
   { "visible-rows", THE_INPUT_DEBUG_LIST_VISIBLE_ROWS },
   { "dump-cursor-mapping", THE_INPUT_DEBUG_DUMP_CURSOR_MAPPING },
   { "cursor-mapping", THE_INPUT_DEBUG_DUMP_CURSOR_MAPPING },
   { "dump-driver-ops", THE_INPUT_DEBUG_DUMP_DRIVER_OPS },
   { "driver-ops", THE_INPUT_DEBUG_DUMP_DRIVER_OPS },
   { "explain-last-render", THE_INPUT_DEBUG_EXPLAIN_LAST_RENDER },
   { "last-render", THE_INPUT_DEBUG_EXPLAIN_LAST_RENDER },
   { "utf-display", THE_INPUT_DEBUG_UTF_DISPLAY },
   { "utf", THE_INPUT_DEBUG_UTF_DISPLAY },
   { NULL, THE_INPUT_DEBUG_NONE }
};

static void input_copy_text(char *dest, size_t dest_len, const char *src)
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

static int input_ascii_equal_ci(const char *left, const char *right)
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

const char *the_input_kind_name(TheInputKind kind)
{
   switch (kind)
   {
      case THE_INPUT_TEXT:
         return "text";
      case THE_INPUT_KEY:
         return "key";
      case THE_INPUT_COMMAND:
         return "command";
      case THE_INPUT_LOGICAL_HIT:
         return "logical-hit";
      case THE_INPUT_DEBUG:
         return "debug";
      case THE_INPUT_NONE:
      default:
         return "none";
   }
}

const char *the_input_debug_command_name(TheInputDebugCommand command)
{
   switch (command)
   {
      case THE_INPUT_DEBUG_DESCRIBE_FOCUS:
         return "describe-focus";
      case THE_INPUT_DEBUG_DESCRIBE_ROW:
         return "describe-row";
      case THE_INPUT_DEBUG_LIST_VISIBLE_ROWS:
         return "list-visible-rows";
      case THE_INPUT_DEBUG_DUMP_CURSOR_MAPPING:
         return "dump-cursor-mapping";
      case THE_INPUT_DEBUG_DUMP_DRIVER_OPS:
         return "dump-driver-ops";
      case THE_INPUT_DEBUG_EXPLAIN_LAST_RENDER:
         return "explain-last-render";
      case THE_INPUT_DEBUG_UTF_DISPLAY:
         return "utf-display";
      case THE_INPUT_DEBUG_NONE:
      default:
         return "none";
   }
}

const char *the_input_logical_target_kind_name(TheInputLogicalTargetKind kind)
{
   switch (kind)
   {
      case THE_INPUT_TARGET_FILEAREA:
         return "filearea";
      case THE_INPUT_TARGET_PREFIX:
         return "prefix";
      case THE_INPUT_TARGET_COMMAND:
         return "command";
      case THE_INPUT_TARGET_PROMPT:
         return "prompt";
      case THE_INPUT_TARGET_STATUS:
         return "status";
      case THE_INPUT_TARGET_TABLINE:
         return "tabline";
      case THE_INPUT_TARGET_DIVIDER:
         return "divider";
      case THE_INPUT_TARGET_WINDOW:
         return "window";
      case THE_INPUT_TARGET_NONE:
      default:
         return "none";
   }
}

int the_input_logical_target_kind_from_name(const char *name,
                                            TheInputLogicalTargetKind *kind)
{
   TheInputLogicalTargetKind parsed = THE_INPUT_TARGET_NONE;

   if (name == NULL)
      return 0;
   if (input_ascii_equal_ci(name, "filearea")
   ||  input_ascii_equal_ci(name, "file")
   ||  input_ascii_equal_ci(name, "file-area"))
      parsed = THE_INPUT_TARGET_FILEAREA;
   else if (input_ascii_equal_ci(name, "prefix"))
      parsed = THE_INPUT_TARGET_PREFIX;
   else if (input_ascii_equal_ci(name, "command")
   ||       input_ascii_equal_ci(name, "cmd")
   ||       input_ascii_equal_ci(name, "cmdline")
   ||       input_ascii_equal_ci(name, "commandline"))
      parsed = THE_INPUT_TARGET_COMMAND;
   else if (input_ascii_equal_ci(name, "prompt"))
      parsed = THE_INPUT_TARGET_PROMPT;
   else if (input_ascii_equal_ci(name, "status"))
      parsed = THE_INPUT_TARGET_STATUS;
   else if (input_ascii_equal_ci(name, "tabline")
   ||       input_ascii_equal_ci(name, "tabs")
   ||       input_ascii_equal_ci(name, "tab")
   ||       input_ascii_equal_ci(name, "filetabs")
   ||       input_ascii_equal_ci(name, "file-tabs"))
      parsed = THE_INPUT_TARGET_TABLINE;
   else if (input_ascii_equal_ci(name, "divider"))
      parsed = THE_INPUT_TARGET_DIVIDER;
   else if (input_ascii_equal_ci(name, "window")
   ||       input_ascii_equal_ci(name, "win"))
      parsed = THE_INPUT_TARGET_WINDOW;
   else
      return 0;

   if (kind != NULL)
      *kind = parsed;
   return 1;
}

static LogicalCursorZone input_zone_from_target_kind(TheInputLogicalTargetKind kind)
{
   switch (kind)
   {
      case THE_INPUT_TARGET_FILEAREA:
         return LOGICAL_CURSOR_ZONE_FILEAREA;
      case THE_INPUT_TARGET_PREFIX:
         return LOGICAL_CURSOR_ZONE_PREFIX;
      case THE_INPUT_TARGET_COMMAND:
         return LOGICAL_CURSOR_ZONE_COMMAND;
      case THE_INPUT_TARGET_PROMPT:
         return LOGICAL_CURSOR_ZONE_PROMPT;
      case THE_INPUT_TARGET_STATUS:
         return LOGICAL_CURSOR_ZONE_STATUS;
      case THE_INPUT_TARGET_TABLINE:
      case THE_INPUT_TARGET_DIVIDER:
      case THE_INPUT_TARGET_WINDOW:
      case THE_INPUT_TARGET_NONE:
      default:
         return LOGICAL_CURSOR_ZONE_NONE;
   }
}

static TheInputLogicalTargetKind input_target_kind_from_zone(LogicalCursorZone zone)
{
   switch (zone)
   {
      case LOGICAL_CURSOR_ZONE_FILEAREA:
         return THE_INPUT_TARGET_FILEAREA;
      case LOGICAL_CURSOR_ZONE_PREFIX:
         return THE_INPUT_TARGET_PREFIX;
      case LOGICAL_CURSOR_ZONE_COMMAND:
         return THE_INPUT_TARGET_COMMAND;
      case LOGICAL_CURSOR_ZONE_PROMPT:
         return THE_INPUT_TARGET_PROMPT;
      case LOGICAL_CURSOR_ZONE_STATUS:
         return THE_INPUT_TARGET_STATUS;
      case LOGICAL_CURSOR_ZONE_NONE:
      default:
         return THE_INPUT_TARGET_NONE;
   }
}

TheInputEvent the_input_event_none(void)
{
   TheInputEvent input;

   memset(&input, 0, sizeof(input));
   input.kind = THE_INPUT_NONE;
   input.key_code = -1;
   return input;
}

int the_input_event_from_text(uint32_t codepoint, TheInputEvent *out)
{
   if (out == NULL)
      return 0;
   *out = the_input_event_none();
   out->kind = THE_INPUT_TEXT;
   out->codepoint = codepoint;
   if (codepoint <= 0x7Fu)
      out->key_code = (int)codepoint;
   return 1;
}

int the_input_event_from_key_name(const char *name, TheInputEvent *out)
{
   size_t i;

   if (out == NULL || name == NULL)
      return 0;
   *out = the_input_event_none();
   if ((name[0] == 'f' || name[0] == 'F') && isdigit((unsigned char)name[1]))
   {
      char *end = NULL;
      long number = strtol(name + 1, &end, 10);

      if (end != NULL && *end == '\0' && number >= 1 && number <= 64)
      {
         out->kind = THE_INPUT_KEY;
         out->key_code = THE_KEY_F((int)number);
         return 1;
      }
   }

   for (i = 0; input_key_names[i].name != NULL; i++)
   {
      if (input_ascii_equal_ci(name, input_key_names[i].name))
      {
         out->kind = THE_INPUT_KEY;
         out->key_code = input_key_names[i].key_code;
         return 1;
      }
   }
   return 0;
}

int the_input_event_from_legacy_key(int key_code, TheInputEvent *out)
{
   if (out == NULL || key_code < 0)
      return 0;
   if (key_code >= 0x20 && key_code <= 0x7e)
      return the_input_event_from_text((uint32_t)key_code, out);
   *out = the_input_event_none();
   out->kind = THE_INPUT_KEY;
   out->key_code = key_code;
   return 1;
}

int the_input_event_from_command(const char *command, TheInputEvent *out)
{
   if (out == NULL)
      return 0;
   *out = the_input_event_none();
   out->kind = THE_INPUT_COMMAND;
   input_copy_text(out->command, sizeof(out->command), command);
   return out->command[0] != '\0';
}

int the_input_event_from_logical_target(TheInputLogicalTargetKind kind,
                                        LINETYPE line_number, int row,
                                        int cell, int screen, int window_id,
                                        TheInputEvent *out)
{
   if (out == NULL)
      return 0;
   *out = the_input_event_none();
   out->kind = THE_INPUT_LOGICAL_HIT;
   out->target.kind = kind;
   out->target.zone = input_zone_from_target_kind(kind);
   out->target.line_number = line_number;
   out->target.row = row;
   out->target.cell = cell;
   out->target.screen = screen;
   out->target.window_id = window_id;
   return kind != THE_INPUT_TARGET_NONE && row >= 0 && cell >= 0;
}

int the_input_event_from_logical_hit(LogicalCursorZone zone,
                                     LINETYPE line_number, int row,
                                     int cell, TheInputEvent *out)
{
   if (out == NULL)
      return 0;
   *out = the_input_event_none();
   out->kind = THE_INPUT_LOGICAL_HIT;
   out->target.kind = input_target_kind_from_zone(zone);
   out->target.zone = zone;
   out->target.line_number = line_number;
   out->target.row = row;
   out->target.cell = cell;
   return out->target.kind != THE_INPUT_TARGET_NONE && row >= 0 && cell >= 0;
}

int the_input_event_from_debug_command(const char *name,
                                       TheInputEvent *out)
{
   size_t i;

   if (out == NULL || name == NULL)
      return 0;
   *out = the_input_event_none();
   for (i = 0; input_debug_names[i].name != NULL; i++)
   {
      if (input_ascii_equal_ci(name, input_debug_names[i].name))
      {
         out->kind = THE_INPUT_DEBUG;
         out->debug_command = input_debug_names[i].command;
         return 1;
      }
   }
   return 0;
}

int the_input_event_to_legacy_key(const TheInputEvent *input, int *key_code)
{
   if (input == NULL || key_code == NULL)
      return 0;
   if (input->kind != THE_INPUT_TEXT
   &&  input->kind != THE_INPUT_KEY)
      return 0;
   if (input->key_code < 0)
      return 0;
   *key_code = input->key_code;
   return 1;
}

int the_input_legacy_key_is_mouse(int key_code)
{
   return key_code == THE_KEY_MOUSE;
}

void the_input_queue_init(TheInputQueue *queue)
{
   if (queue != NULL)
      memset(queue, 0, sizeof(*queue));
}

int the_input_queue_push(TheInputQueue *queue, TheInputEvent input)
{
   if (queue == NULL || queue->count >= THE_INPUT_QUEUE_MAX)
      return 0;
   queue->items[queue->tail] = input;
   queue->tail = (queue->tail + 1) % THE_INPUT_QUEUE_MAX;
   queue->count++;
   return 1;
}

int the_input_queue_pop(TheInputQueue *queue, TheInputEvent *input)
{
   if (queue == NULL || queue->count == 0)
      return 0;
   if (input != NULL)
      *input = queue->items[queue->head];
   queue->head = (queue->head + 1) % THE_INPUT_QUEUE_MAX;
   queue->count--;
   return 1;
}

int the_input_queue_pop_legacy_key(TheInputQueue *queue, int *key_code)
{
   TheInputEvent input;

   if (queue == NULL || key_code == NULL || queue->count == 0)
      return 0;
   input = queue->items[queue->head];
   if (!the_input_event_to_legacy_key(&input, key_code))
      return 0;
   queue->head = (queue->head + 1) % THE_INPUT_QUEUE_MAX;
   queue->count--;
   return 1;
}
