#include "inputevent.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "getch.h"

#ifdef KEY_TAB
# define THE_INPUT_KEY_TAB KEY_TAB
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
   { "left", KEY_LEFT },
   { "right", KEY_RIGHT },
   { "up", KEY_UP },
   { "down", KEY_DOWN },
   { "home", KEY_HOME },
   { "end", KEY_END },
   { "pageup", KEY_PPAGE },
   { "pgup", KEY_PPAGE },
   { "pagedown", KEY_NPAGE },
   { "pgdn", KEY_NPAGE },
   { "enter", KEY_ENTER },
   { "return", KEY_RETURN },
   { "esc", KEY_ESC },
   { "escape", KEY_ESC },
   { "tab", THE_INPUT_KEY_TAB },
   { "backtab", KEY_BTAB },
   { "btab", KEY_BTAB },
   { "backspace", KEY_BACKSPACE },
   { "delete", KEY_DC },
   { "del", KEY_DC },
   { "insert", KEY_IC },
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
      case THE_INPUT_DEBUG_NONE:
      default:
         return "none";
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
         out->key_code = KEY_F((int)number);
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

int the_input_event_from_logical_hit(LogicalCursorZone zone,
                                     LINETYPE line_number, int row,
                                     int cell, TheInputEvent *out)
{
   if (out == NULL)
      return 0;
   *out = the_input_event_none();
   out->kind = THE_INPUT_LOGICAL_HIT;
   out->target.zone = zone;
   out->target.line_number = line_number;
   out->target.row = row;
   out->target.cell = cell;
   return zone != LOGICAL_CURSOR_ZONE_NONE && row >= 0 && cell >= 0;
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
